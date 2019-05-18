# Abstract

Remote direct memory access (RDMA) and non-uniform memory access (NUMA) are critical technologies of modern high-performance computing platforms. RDMA allows nodes to directly access memory on remote machines. Multiprocessor architectures implement NUMA to scale up memory access performance. When paired together, these technologies exhibit performance penalties under certain configurations. This paper explores these configurations to provide quantitative findings on the impact of NUMA for RDMA-based systems. One of the consequences of ultra-fast networks is that known implications of NUMA locality now constitute a higher relative impact on the performance of RDMA-enabled distributed systems. Our study quantifies its role and uncovers unexpected behavior. In summary, poor NUMA locality of remotely accessible memory can lead to an automatic 20% performance degradation. Additionally, local workloads operating on remotely accessible memory can lead to 300% performance gap depending on memory locality. Surprisingly, configurations demonstrating this result contradict the presumed impact of NUMA locality. Our findings are validated using two generations of RDMA cards, a synthetic benchmark, and the popular application Memcached ported for RDMA.

# Building and running

This project requires CMake 3.0 and the following:
* libnuma-dev
* libpthread
* Mellanox OFED driver (v 4.5-1.0.1.0)

To build, assume the project's root directory is at  `$PROJ` make a new directory for the binary in the project's root directory (`mkdir $PROJ/bin`). Change directories to `bin` and use `cmake` to generate the Makefile (`cd $PROJ/bin; cmake ..`). Finally, from `$PROJ/bin` make the project (`make`).

Individual configuration files can be run using `./rc_bench` on both the client and the server. Alternatively, entire directories of configurations can be run using `run_all_rc.py` in the  `$PROJ/scripts` directory. It is possible to remotely launch the experiments using `$PROJ/scripts/run.sh`.

# Experimental results

We have included all of our experimental data in `$PROJ/results`. All results presented in our paper correspond to this raw data. There are also additional experiments that were not included in the final draft.




