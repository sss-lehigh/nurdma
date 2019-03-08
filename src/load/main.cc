#include "worker.h"
#include "volatile_worker.h"
#include <iostream>
#include <vector>
#include <atomic>
#include <numa.h>

#define NONUMA -1

/* a simple program to put stress on memory system */
int main(int argc, char ** argv) {
    /* options: <buf_size> <num_workers> <write_ratio> <mode> [numa] */
    if (argc < 5 || argc > 6) {
        std::cerr << "invalid program arguments" << std::endl;
        std::cerr << "usage: " << argv[0] << " <buf_size> <num_workers> <write_ratio> <mode> [numa]" << std::endl;
        exit(1);
    }

    // setup
    int bufSize = atoi(argv[1]);
    int numWorkers = atoi(argv[2]);
    float writeRatio = atof(argv[3]);
    int mode = atoi(argv[4]);
    int numa = -1;
    if (argc == 6) {
        numa = atoi(argv[5]);
    }

    /* 
    mode
        1: all workers share a buffer
        2: each worker has individual buffer
        3: all workers share a volatile buffer
        4: each worker has individual volatile buffer
    */
    double *buf = nullptr;
    if (mode == 1 || mode == 3) {
        buf = new double[bufSize];
    }

    // create workers
    std::vector<Worker> workers;
    volatile bool done = false;
    for (int i = 0; i < numWorkers; ++i) {
        if (buf == nullptr)
            workers.push_back((mode < 3 ? Worker(bufSize, writeRatio, &done) : VolatileWorker(bufSize, writeRatio, &done)));
        else
            workers.push_back((mode < 3 ? Worker(buf, bufSize, writeRatio, &done) : VolatileWorker(buf, bufSize, writeRatio, &done)));
    }

    // start workers
    int numCPU = numa_num_configured_cpus();
    if (numa < 0) {
        for (int i = 0; i < numWorkers; ++i) {
            workers[i].start(i % numCPU, NONUMA);
        } 
    } else {
        if (numa_run_on_node(numa)) {
            perror("numa_run_on_node");
            exit(1);
        }
        struct bitmask *mask = numa_allocate_cpumask();
        if (numa_node_to_cpus(numa, mask)) {
            perror("could not set cpumask");
            exit(1);
        }
        for (int i = 0, j = 0; i < numWorkers; ++i) {
            while (!numa_bitmask_isbitset(mask, j))
                j = (j + 1) % numCPU;
            workers[i].start(j, numa);
            j = (j + 1) % numCPU;
        }
        numa_free_cpumask(mask);
    }

    // run until input from user
    char c;
    std::cin >> c;
    done = true;

    for (int i = 0; i < numWorkers; ++i) {
        workers[i].stop();
    }

    if (buf != nullptr)
        delete buf;

    return 0;
}
