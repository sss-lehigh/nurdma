//
// Created by jacob on 11/19/18.
//

#include <fstream>
#include <getopt.h>
#include <thread>
#include <future>
#include "rapidjson/document.h"
#include "rapidjson/filereadstream.h"
#include "rc_resources.h"

#define CLIENT_NUMA -1 // -1 is RDMA-local

namespace json = rapidjson;

json::Document parse_config(const char *filename) {
    json::Document d;
    FILE *fp = fopen(filename, "r");
    assert(fp != nullptr);
    char readBuffer[10000];
    std::cerr << "parsing file: " << filename << std::endl;
    json::FileReadStream is(fp, readBuffer, sizeof(readBuffer));
    if(d.ParseStream(is).HasParseError()) {
        std::cerr << "parse error: " << d.ParseStream(is).GetParseError() << std::endl;
        exit(1);
    }
    fclose(fp);
    return d;
}

int runClientsOpenloop(rc_resources &resources, int iters, int postlist, int sendbatch, int mode, int think) {
    std::vector<std::thread> client_threads;
    std::vector<std::thread> polling_threads;
    std::cerr << "initial contents of client buffer: " << std::endl;
    resources.printShmemBuf();
    for (int i = 0; i < resources.getNumClients(); ++i) {
        client_threads.emplace_back(&rc_resources::client_openloop, &resources, i, iters, postlist, sendbatch, mode, think);
        polling_threads.emplace_back(&rc_resources::poll_cq, &resources, i, iters, sendbatch);
    }
    for (auto &thread : client_threads)
        thread.join();
    for (auto &thread : polling_threads)
        thread.join();
    std::cerr << "clients finished; syncing with server..." << std::endl;
    if (!resources.syncSocket()) {
        std::cerr << "sync error after RDMA ops" << std::endl;
        return 1;
    }
    std::cerr << "contents of client buffer: " << std::endl;
    resources.printShmemBuf();
    return 0;
}

int runClientsClosedloop(rc_resources &resources, int iters, int postlist, int sendbatch, int mode,
                         int think) {
    std::vector<std::future<std::vector<double>>> client_futures;
    char c;
    int i;
    double avg_latency = 0, avg_std_dev = 0;
    std::cerr << "initial contents of client buffer: " << std::endl;
    resources.printShmemBuf();
    for (i = 0; i < resources.getNumClients(); ++i) {
        client_futures.push_back(std::move(std::async(&rc_resources::client_closedloop, &resources, i, iters, postlist, sendbatch, mode, think)));
    }
    std::vector<std::vector<double>> results;
    for (auto &future : client_futures)
        results.push_back(future.get());
    if (!resources.syncSocket()) {
        std::cerr << "sync error after RDMA ops" << std::endl;
        return 1;
    }
    std::cerr << "contents of client buffer: " << std::endl;
    resources.printShmemBuf();

    i = 0;
    for (auto &r : results) {
        std::cout << i++ << "\t" << r[0] << "ns\t" << r[1] << "ns" << std::endl;
        avg_latency += r[0];
        avg_std_dev += r[1];
    }
    avg_latency /= results.size();
    avg_std_dev /= results.size();
    std::cout << "total\t" << avg_latency << "ns\t" << avg_std_dev << "ns" << std::endl;
    return 0;
}

int runWorkers(rc_resources &resources, int num_workers, int mode, int think, int timeout) {
    std::vector<std::thread> server_threads;
    std::cerr << "initial contents of server buffer: " << std::endl;
    resources.printShmemBuf();
    resources.checkNumaLocality();
    // resources.flushShmemBuf();
    for (int i = 0; i < num_workers; ++i) {
        server_threads.emplace_back(&rc_resources::worker, &resources, i, mode, think, timeout);
    }
    if (!resources.syncSocket()) {
        std::cerr << "sync error after RDMA ops" << std::endl;
        return 1;
    }
    resources.signalWorkersDone();
    for (auto &t : server_threads)
        t.join();
    std::cerr << "contents of server buffer: " << std::endl;
    resources.printShmemBuf();
//    resources.checkNumaLocality();
    return 0;
}

int main(int argc, char *argv[]) {
    int opt = 0, rc = 0;
    std::string configfile, servername;
    bool isClient = false;

    while (true) {
        struct option long_options[] = {
                {"config",     required_argument, nullptr, 0},
                {"servername", optional_argument, nullptr, 1},
                {nullptr, 0,                      nullptr, 0}
        };
        opt = getopt_long(argc, argv, "", long_options, nullptr);
        if (opt == -1)
            break;
        switch (opt) {
            case 0:
                configfile = std::string(optarg);
                break;
            case 1:
                servername = std::string(optarg);
                isClient = true;
                break;
            default:
                return 1;
        }
    }

    json::Document config = parse_config(configfile.data());
    rc_resources resources = rc_resources();
    resources.initOpenClosed(0, config["workers"].GetUint64(), servername.data(), config["num_clients"].GetUint64(),
                             config["ib_dev"].GetString(), config["ib_port"].GetInt(), config["obj_size"].GetUint64(),
                             config["buf_size"].GetUint64(), config["numa"].GetInt(), config["tcp_port"].GetInt());
    rc = resources.connectQPs();
    assert(rc == 0);

    if (isClient && config["experiment"].GetString() == std::string("open")) {
        rc = runClientsOpenloop(resources, config["iters"].GetInt(), config["postlist"].GetInt(),
                                config["sendbatch"].GetInt(), config["client_mode"].GetInt(),
                                config["think"].GetInt());
    } else if (isClient && config["experiment"].GetString() == std::string("closed")) {
        rc = runClientsClosedloop(resources, config["iters"].GetInt(), config["postlist"].GetInt(),
                                  config["sendbatch"].GetInt(), config["client_mode"].GetInt(),
                                  config["think"].GetInt());

    } else {
        rc = runWorkers(resources, config["workers"].GetInt(), config["worker_mode"].GetInt(), config["think"].GetInt(),
                        config["timeout"].GetInt());
    }
    assert(rc == 0);
}