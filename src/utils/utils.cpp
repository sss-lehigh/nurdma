//
// Created by jacob on 11/20/18.
//

#include "utils.h"
#include <unistd.h>
#include <netdb.h>
#include <cstddef>
#include <cstdio>
#include <fstream>
#include <numaif.h>
#include <numa.h>
#include <byteswap.h>

int get_numa_zone(const std::string &dev_name) {
    int zone;
    std::string filename("/sys/class/infiniband/");
    filename += dev_name + std::string("/device/numa_node");
    std::ifstream infile(filename.data());
    if (!infile) {
        return -1;
    }
    infile >> zone;
    infile.close();
    return zone;
}

int bind_to_numa_zone(int rqstd_numa, const char* dev_name) {
    int numa;
    struct bitmask *node_mask = nullptr;
    struct bitmask *run_mask = nullptr;
    struct bitmask *mem_mask = nullptr;

    // check that NUMA is enabled
    if (numa_available() < 0) {
        return -1;
    }

    node_mask = numa_allocate_nodemask();
    if (rqstd_numa < 0)
        numa = get_numa_zone(dev_name);
    else
        numa = rqstd_numa;
    if (numa < 0) {
        return 1;
    }
    numa_bitmask_setbit(node_mask, static_cast<unsigned int>(numa));
    numa_bind(node_mask);
    numa_bitmask_free(node_mask);
    numa_bitmask_free(run_mask);
    numa_bitmask_free(mem_mask);
    usleep(100);
    return numa;
}

int enforce_numa_zone(void *ptr, int zone) {
    long pagesize, tmp, rc;
    int status[1];
    status[0] = -1;
    pagesize = sysconf(_SC_PAGESIZE);
    tmp = (long) ptr & -pagesize;
    rc = move_pages(0 /*self memory */, 1, (void **) &tmp, nullptr, status, 0);
    //std::cerr << ptr << " exists on NUMA" << status[0] << std::endl;
    if (status[0] != zone) {
        //std::cerr << "moving " << ptr << " to NUMA" << zone << std::endl;
        if (status[0] < 0) {
            std::cerr << "NUMA lookup failed with error: " << strerror((int) -rc) << std::endl;
        }
        move_pages(0 /*self memory */, 1, (void **) &tmp, &zone, status, MPOL_MF_MOVE);
        while (status[0] != zone) {
            move_pages(0 /*self memory */, 1, (void **) &tmp, &zone, status, MPOL_MF_MOVE);
            std::cerr << "could not move " << ptr << " to NUMA " << zone << "; retrying..." << std::endl;
            if (status[0] < 0) {
                std::cerr << "NUMA lookup failed with error code " << status[0] << ": " << strerror(-status[0]) << std::endl;
            }
        }
    }
    return 0;
}

int enforce_numa_zone_all(void *ptr, int zone) {
    long pagesize, tmp, rc;
    int status[1];
    status[0] = -1;
    pagesize = sysconf(_SC_PAGESIZE);
    tmp = (long) ptr & -pagesize;
    rc = move_pages(0 /*self memory */, 1, (void **) &tmp, nullptr, status, 0);
    //std::cerr << ptr << " exists on NUMA" << status[0] << std::endl;
    if (status[0] != zone) {
        //std::cerr << "moving " << ptr << " to NUMA" << zone << std::endl;
        if (rc < 0) {
            std::cerr << "NUMA lookup failed with error code " << -rc << ": " << strerror((int) -rc) << std::endl;
        }
        rc = move_pages(0 /*self memory */, 1, (void **) &tmp, &zone, status, MPOL_MF_MOVE_ALL);
        while (status[0] != zone) {
            std::cerr << "could not move " << ptr << " to NUMA " << zone << "; retrying..." << std::endl;
            if (status[0] < 0) {
                std::cerr << "move_pages failed with error code " << status[0] << ": " << strerror(-status[0]) << std::endl;
            }
            else if (rc < 0) {
                std::cerr << "move_pages failed with error code " << -rc << ": " << strerror((int) -rc) << std::endl;
            }
            rc = move_pages(0 /*self memory */, 1, (void **) &tmp, &zone, status, MPOL_MF_MOVE_ALL);
        }
    }
    return 0;
}

int get_numa_zone(void *ptr) {
    long pagesize;
    long tmp;
    int status[1];
    status[0] = -1;
    pagesize = sysconf(_SC_PAGESIZE);
    tmp = (long) ptr & -pagesize;
    (int) move_pages(0 /*self memory */, 1, (void **) &tmp, nullptr, status, 0);
    return status[0];
}

int sock_connect(const char *servername, int port) {
    struct timespec t1{}, t2{}, retry{.tv_sec=10, .tv_nsec=0};
    struct addrinfo *resolved_addr = nullptr;
    struct addrinfo *iterator;
    char service[6];
    int sockfd = -1;
    int listenfd = 0;
    struct addrinfo hints = {
            .ai_flags = AI_PASSIVE,
            .ai_family = AF_INET,
            .ai_socktype = SOCK_STREAM
    };
    if (sprintf(service, "%d", port) < 0)
        goto sock_connect_exit;
    /* Resolve DNS address, use sockfd as temp storage */
    sockfd = getaddrinfo(servername, service, &hints, &resolved_addr);
    if (sockfd < 0) {
        fprintf(stderr, "%s for %s: %d\n", gai_strerror(sockfd), servername, port);
        goto sock_connect_exit;
    }
    /* Search through results and find the one we want */
    for (iterator = resolved_addr; iterator; iterator = iterator->ai_next) {
        sockfd = socket(iterator->ai_family, iterator->ai_socktype,
                        iterator->ai_protocol);
        if (sockfd >= 0) {
            int enable = 1;
            if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int)) < 0) {
                fprintf(stderr, "Could not set sockopt SO_REUSEADDR\n");
                goto sock_connect_exit;
            }
            if (servername) {
                /* Client client_mode. Initiate connection to remote */
                std::cerr << "trying to connect to " << servername << std::endl;
                clock_gettime(CLOCK_REALTIME, &t1);
                while (connect(sockfd, iterator->ai_addr, iterator->ai_addrlen)
                       && (BILLION * (t2.tv_sec - t1.tv_sec) + t2.tv_nsec - t1.tv_nsec) < (BILLION * (retry.tv_sec) + retry.tv_nsec)) {
                    std::cerr << ".";
                    clock_gettime(CLOCK_REALTIME, &t2);
                }
                if ((BILLION * (t2.tv_sec - t1.tv_sec) + t2.tv_nsec - t1.tv_nsec)
                    >= (BILLION * (retry.tv_sec) + retry.tv_nsec)) {
                    std::cerr << std::endl;
                    close(sockfd);
                    sockfd = -1;
                }
            } else {
                /* Server client_mode. Set up listening socket an accept a connection */
                listenfd = sockfd;
                sockfd = -1;
                if (bind(listenfd, iterator->ai_addr, iterator->ai_addrlen) < 0) {
                    perror("bind failed");
                    goto sock_connect_exit;
                }
                if (listen(listenfd, 1) < 0) {
                    perror("listen failed");
                    goto sock_connect_exit;
                }
                fd_set read_set{};
                FD_ZERO(&read_set);
                FD_SET(listenfd, &read_set);
                int s = select(FD_SETSIZE, &read_set, nullptr, nullptr, new timeval{.tv_sec=10,.tv_usec=0});
                if (s < 0) {
                    perror("select failed");
                    goto sock_connect_exit;
                }
                else if (s == 0) {
                    std::cerr << "network timed out" << std::endl;
                    goto sock_connect_exit;
                }
                if (FD_ISSET(listenfd, &read_set)) {
                    sockfd = accept(listenfd, nullptr, nullptr);
                    if (sockfd < 0) {
                        perror("accept failed");
                    }
                }
                else {
                    std::cerr << "well fuck" << std::endl;
                }

            }
        }
    }
    sock_connect_exit:
    if (listenfd)
        close(listenfd);
    if (resolved_addr)
        freeaddrinfo(resolved_addr);
    if (sockfd < 0) {
        perror("Something went wrong");
        if (servername)
            std::cerr << "Couldn't connect to " << servername << ": " << port << std::endl;
        else {
            std::cerr << "did not receive any incoming connections" << std::endl;
        }
    }
    return sockfd;
}


int sock_sync_data(int sock, int xfer_size, char *local_data, char *remote_data) {
    int rc;
    int read_bytes = 0;
    int total_read_bytes = 0;
    //std::cerr << local_data << std::endl;
    rc = (int) write(sock, local_data, (size_t) xfer_size);
    if (rc < xfer_size)
        fprintf(stderr, "Failed writing data during sock_sync_data\n");
    else
        rc = 0;
    while (!rc && total_read_bytes < xfer_size) {
        //std::cerr << "reading data in sock_sync_data" << std::endl;
        read_bytes = (int) read(sock, remote_data, (size_t) xfer_size);
        if (read_bytes > 0)
            total_read_bytes += read_bytes;
        else {
            perror("Error reading on socket");
            rc = read_bytes;
        }
    }
    return rc;
}

