#include <getopt.h>
#include <fstream>
#include <future>
#include <thread>
#include "rapidjson/document.h"
#include "rapidjson/filereadstream.h"
#include "rc_resources.h"
#include "../utils/utils.h"

#define CLIENT_NUMA -1  // -1 is RDMA-local
#define DEVNAME "mlx4_0"

namespace json = rapidjson;

json::Document parse_config(const char *filename) {
  json::Document d;
  FILE *fp = fopen(filename, "r");
  assert(fp != nullptr);
  char readBuffer[10000];
  std::cerr << "parsing file: " << filename << std::endl;
  json::FileReadStream is(fp, readBuffer, sizeof(readBuffer));
  if (d.ParseStream(is).HasParseError()) {
    std::cerr << "parse error: " << d.ParseStream(is).GetParseError()
              << std::endl;
    exit(1);
  }
  fclose(fp);
  return d;
}

/**
 * @brief Launches clients in an open loop and outputs throughput statistics
 *
 * @param resources
 * @param iters
 * @param postlist
 * @param sendbatch
 * @param mode
 * @param think
 * @return int
 */
int runClientsOpenloop(rc_resources &resources, int iters, int postlist,
                       int sendbatch, int mode, int think, float write_ratio) {
  // prints out buffer contents for visual inspection
  std::cerr << "initial contents of client buffer: " << std::endl;
  resources.printShmemBuf();

  // sanity check
  resources.checkNumaLocality();

  // launch a thread (and polling thread) for each client
  std::vector<std::thread> client_threads;
  std::vector<std::thread> polling_threads;
  for (int i = 0; i < resources.getNumClients(); ++i) {
    client_threads.emplace_back(&rc_resources::client_openloop, &resources, i,
                                iters, postlist, sendbatch, mode, think, write_ratio);
    polling_threads.emplace_back(&rc_resources::poll_cq, &resources, i, iters,
                                 sendbatch);
  }

  // wait for clients and polling threads to initialize then sync with the server to ensure that everything finished initializing on server
  resources.clientWait();
  std::cerr << "clients initialized, checking with server..." << std::endl;
  if (!resources.syncSocket()) {
    std::cerr << "sync error after setup" << std::endl;
    return 1;
  }
  resources.start(); // allow initialized clients to execute

  // wait for threads to complete the threads
  for (auto &thread : client_threads) thread.join();
  for (auto &thread : polling_threads) thread.join();
  
  // sanity check
  resources.checkNumaLocality();

  // sync with the server to ensure that everything finished
  std::cerr << "clients finished; syncing with server..." << std::endl;
  if (!resources.syncSocket()) {
    std::cerr << "sync error after RDMA ops" << std::endl;
    return 1;
  }

  // output updated client buffer contents
  std::cerr << "contents of client buffer: " << std::endl;
  resources.printShmemBuf();
  return 0;
}

/**
 * @brief Runs clients in a closed loop and collects statistics about latency
 * 
 * @param resources 
 * @param iters 
 * @param postlist 
 * @param sendbatch 
 * @param mode 
 * @param think 
 * @return int 
 */
int runClientsClosedloop(rc_resources &resources, int iters, int postlist,
                         int sendbatch, int mode, int think, float write_ratio) {
  std::cerr << "initial contents of client buffer: " << std::endl;
  resources.printShmemBuf();

  // reinitialize client barrier because we don't need as many as default
  resources.reinitClientBarrier(resources.getNumClients());

  // sanity check
  resources.checkNumaLocality();

  // launch each client (will poll for own completion)
  char c;
  int i;
  double avg_latency = 0, avg_std_dev = 0;
  std::vector<std::future<std::vector<double>>> client_futures;
  for (i = 0; i < resources.getNumClients(); ++i) {
    client_futures.push_back(
        std::move(std::async(std::launch::async, &rc_resources::client_closedloop, &resources, i,
                             iters, postlist, sendbatch, mode, think, write_ratio)));
  }

  // wait for clients to initialize then sync with the server to ensure that everything finished initializing on server
  std::cerr << "clients initialized, checking with server..." << std::endl;
  if (!resources.syncSocket()) {
    std::cerr << "sync error after setup" << std::endl;
    return 1;
  }
  resources.start(); // allow initialized clients to execute

  // collect results from each client
  std::vector<std::vector<double>> results;
  for (auto &future : client_futures) results.push_back(future.get());

  // sanity check
  resources.checkNumaLocality();

  // sync with server
  if (!resources.syncSocket()) {
    std::cerr << "sync error after RDMA ops" << std::endl;
    return 1;
  }
  std::cerr << "contents of client buffer: " << std::endl;
  resources.printShmemBuf();

  // calculate overall average latency and std deviation
  i = 0;
  for (auto &r : results) {
    std::cout << i++ << "\t" << r[0] << "ns\t" << r[1] << "ns" << std::endl;
    avg_latency += r[0];
    avg_std_dev += r[1];
  }
  avg_latency /= results.size();
  avg_std_dev /= results.size();
  std::cout << "total\t" << avg_latency << "ns\t" << avg_std_dev << "ns"
            << std::endl;
  return 0;
}

/**
 * @brief Runs worker threads (intended for server machine) to simulate load working 
 *        on the remotely accessible buffer
 * 
 * @param resources 
 * @param num_workers 
 * @param mode 
 * @param think 
 * @param timeout 
 * @return int 
 */
int runWorkers(rc_resources &resources, int num_workers, int mode, int think,
               int timeout, float write_ratio) {
  std::vector<std::thread> server_threads;
  std::cerr << "initial contents of server buffer: " << std::endl;
  resources.printShmemBuf();
  resources.checkNumaLocality();

  // NB: uncomment following line for testing caching behavior of RDMA ops
  // resources.flushShmemBuf();

  // launch the worker threads
  for (int i = 0; i < num_workers; ++i) {
    server_threads.emplace_back(&rc_resources::worker, &resources, i, mode,
                                think, timeout, write_ratio);
  }
  
  // wait for worker threads to initialize then sync with the client to ensure that everything finished initializing on client
  resources.workerWait();
  std::cerr << "clients initialized, checking with client..." << std::endl;
  if (!resources.syncSocket()) {
    std::cerr << "sync error after setup" << std::endl;
    return 1;
  }
  resources.start();

  // wait for signal from client that the test is complete
  if (!resources.syncSocket()) {
    std::cerr << "sync error after RDMA ops" << std::endl;
    return 1;
  }
  resources.signalWorkersDone();
  for (auto &t : server_threads) t.join();
  std::cerr << "contents of server buffer: " << std::endl;
  resources.printShmemBuf();
  resources.checkNumaLocality();
  return 0;
}

/**
 * @brief Reads configuration file and launches the corresponding jobs according to arguments
 * 
 * @param argc 
 * @param argv 
 * @return int 
 */
int main(int argc, char *argv[]) {
  int opt = 0, rc = 0;
  std::string configfile, servername;
  bool isClient = false;

  // get program arguments
  while (true) {
    struct option long_options[] = {
        {"config", required_argument, nullptr, 0},
        {"servername", optional_argument, nullptr, 1},
        {nullptr, 0, nullptr, 0}};
    opt = getopt_long(argc, argv, "", long_options, nullptr);
    if (opt == -1) break;
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

  // read in configuration file
  json::Document config = parse_config(configfile.data());

  // initialize and connect rc_resources
  rc_resources resources = rc_resources();
  resources.initOpenClosed(
      0, config["workers"].GetUint64(), servername.data(),
      config["num_clients"].GetUint64(), config["ib_dev"].GetString(),
      config["ib_port"].GetInt(), config["obj_size"].GetUint64(),
      config["buf_size"].GetUint64(), (isClient ? get_numa_zone(DEVNAME) : config["numa"].GetInt()),
      config["tcp_port"].GetInt());
  rc = resources.connectQPs();
  assert(rc == 0);

  // XXX: hot fix to not break old configs w/o write_ratio
  float write_ratio = 0.9;
  if (config.HasMember("write_ratio")) {
    std::cerr << "write ratio: " << write_ratio << std::endl;
    write_ratio = config["write_ratio"].GetFloat();
  }

  // decide what to do based on configuration and whether or not to run as the client
  if (isClient && config["experiment"].GetString() == std::string("open")) {
    rc = runClientsOpenloop(
        resources, config["iters"].GetInt(), config["postlist"].GetInt(),
        config["sendbatch"].GetInt(), config["client_mode"].GetInt(),
        config["think"].GetInt(), write_ratio);
  } else if (isClient &&
             config["experiment"].GetString() == std::string("closed")) {
    rc = runClientsClosedloop(
        resources, config["iters"].GetInt(), config["postlist"].GetInt(),
        config["sendbatch"].GetInt(), config["client_mode"].GetInt(),
        config["think"].GetInt(), write_ratio);
  } else {
    rc = runWorkers(resources, config["workers"].GetInt(),
                    config["worker_mode"].GetInt(), config["think"].GetInt(),
                    config["timeout"].GetInt(), write_ratio);
  }
  assert(rc == 0);
}