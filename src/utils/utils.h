//
// Created by jacob on 10/30/18.
//

#ifndef RDMA_UTILS_H
#define RDMA_UTILS_H

#include <unistd.h>
#include <netdb.h>
#include <cstddef>
#include <cstdio>
#include <iostream>
#include <numaif.h>
#include <numa.h>
#include <byteswap.h>

#if __BYTE_ORDER == __LITTLE_ENDIAN

static inline uint64_t
htonll(uint64_t x) {
    return bswap_64 (x);
}

static inline uint64_t
ntohll(uint64_t x) {
    return bswap_64 (x);
}

#elif __BYTE_ORDER == __BIG_ENDIAN

static inline uint64_t
htonll (uint64_t x)
{
    return x;
}

static inline uint64_t
ntohll (uint64_t x)
{
    return x;
}
#else
#error __BYTE_ORDER is neither __LITTLE_ENDIAN nor __BIG_ENDIAN
#endif

#define BILLION 1000000000L
#define QKEY 0x77B3A57

struct cm_con_data_t {
    uint64_t addr;                /* Buffer address */
    uint32_t rkey;                /* Remote key */
    uint32_t qp_num;              /* QP number */
    uint16_t lid;                 /* LID of the IB port */
    uint8_t gid[16];              /* gid */
} __attribute__ ((packed));

int get_numa_zone(const std::string &dev_name);

int bind_to_numa_zone(int rqstd_numa, const char* dev_name);

int enforce_numa_zone(void *ptr, int zone);

int enforce_numa_zone_all(void *ptr, int zone);

int get_numa_zone(void *ptr);

int sock_connect(const char *servername, int port);

int sock_sync_data(int sock, int xfer_size, char *local_data, char *remote_data);

#endif //RDMA_UTILS_H
