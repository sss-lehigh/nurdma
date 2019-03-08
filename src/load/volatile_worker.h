#ifndef VOLATILE_WORKER_H
#define VOLATILE_WORKER_H

#include <thread>
#include <random>
#include <numa.h>
#include <sched.h>
#include <iostream>
#include "worker.h"

/* a thread reading or writing to memory */
class VolatileWorker : public Worker {
    public:
    /* constructor for independent buffers */
    VolatileWorker(int size, float ratio, volatile bool *flag) : Worker(size, ratio, flag) {
        this->volatileBuf = (volatile double *) buf;
    }

    /* constructor for shared buffer */
    VolatileWorker(double *buf, int size, float ratio, volatile bool *flag) : Worker(buf, size, ratio, flag) {
        this->volatileBuf = (volatile double *) buf;
    }

    /* creates and launches a new worker thread */
    void start(int cpu, int numa) {
        t = new std::thread(work, this, cpu, numa);
    }

    /* stops a worker */
    void stop() {
        t->join();
    }

    ~VolatileWorker() {
        if (!shared)
            delete buf;
    }

    private:
    volatile double *volatileBuf;

    static int work(VolatileWorker *w, int cpu, int numa) {
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

        // do work, access buffer through volatile double pointer
        unsigned int r;
        for (int i = 0; !*(w->done); ++i) {
            r = w->rands[i % NUMRANDS];
            if (r < w->writeRatio * RAND_MAX) {
                w->volatileBuf[(i + r) % w->bufSize] = r;
            } else {
                w->result = w->volatileBuf[(i + r) % w->bufSize];
            }
        }
    }
};
#endif