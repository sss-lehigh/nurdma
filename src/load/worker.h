#ifndef WORKER_H
#define WORKER_H

#include <thread>
#include <random>
#include <numa.h>
#include <sched.h>
#include <iostream>

#define NUMRANDS 100000

/* a thread reading or writing to memory */
class Worker {
    public:
    /* constructor for independent buffers */
    Worker(int size, float ratio, volatile bool *flag): bufSize(size), writeRatio(ratio), shared(false), done(flag) {
        for (int i = 0; i < NUMRANDS; ++i) {
            rands[i] = (unsigned int) std::rand();
        }
        buf = new double[bufSize];
    }

    /* constructor for shared buffer */
    Worker(double *buf, int size, float ratio, volatile bool *flag) : bufSize(size), buf(buf), writeRatio(ratio), shared(true), done(flag) {
        for (int i = 0; i < NUMRANDS; ++i) {
            rands[i] = (unsigned int) std::rand();
        }
    }

    /* creates and launches a new worker thread */
    void start(int cpu, int numa) {
        t = new std::thread(work, this, cpu, numa);
    }

    /* stops a worker */
    void stop() {
        t->join();
    }

    ~Worker() {
        if (!shared)
            delete buf;
    }

    double *buf;
    int bufSize;
    unsigned int rands[NUMRANDS];
    float writeRatio;
    std::thread *t;
    double result;
    bool shared;
    volatile bool *done;

    static int work(Worker *w, int cpu, int numa) {
        // set affinity
        if (cpu > 0) {
            // set thread affinity
            cpu_set_t cpuSet;
            CPU_ZERO(&cpuSet);
            CPU_SET(cpu, &cpuSet);
            if (sched_setaffinity(0, sizeof(cpuSet), &cpuSet)) {
                perror("sched_setaffinity");
            }
        }

        // bind memory
        if (numa > 0 && !w->shared) {
            struct bitmask *numaMask = numa_allocate_nodemask();
            if (numa_bitmask_setbit(numaMask, numa)) {
                perror("could not set bitmask");
                exit(1);
            }
            numa_set_membind(numaMask);
            numa_free_nodemask(numaMask);
        }

        // do work
        unsigned int r;
        for (int i = 0; !*(w->done); ++i) {
            r = w->rands[i % NUMRANDS];
            if (r < w->writeRatio * RAND_MAX) {
                w->buf[(i + r) % w->bufSize] = r;
            } else {
                w->result = w->buf[(i + r) % w->bufSize];
            }
        }
    }
};
#endif