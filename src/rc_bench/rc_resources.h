//
// Created by jacob on 10/30/18.
//

#ifndef RDMA_RDMA_H
#define RDMA_RDMA_H

#include <cstddef>
#include <string>
#include <vector>
#include <infiniband/verbs.h>
#include <thread>
#include "../utils/utils.h"

class rc_resources {
public:
    rc_resources();
    void initOpenClosed(size_t n_machines, size_t n_workers, const char *rqstd_servername, size_t n_clients,
                            const char *rqstd_dev_name, int rqstd_ib_port, size_t rqstd_obj_size, size_t rqstd_buf_size,
                            int rqstd_numa, int rqstd_tcp_port);
    void initDelivery(size_t n_clients, const char *rqstd_servername, int rqstd_tcp_port,
                          const char *rqstd_dev_name, int rqstd_ib_port, int rqstd_numa);

    int getNumClients();
    bool isClient();

    // openloop and closeloop benchmark
    void client_openloop(int client_id, int iters, int postlist, int sendbatch, int mode, int think);
    void poll_cq(int client_id, int iters, int sendbatch);
    std::vector<double> client_closedloop(int client_id, int iters, int postlist, int sendbatch, int mode, int think);
    void worker(int worker_id, int mode, int think, int timeout);

    // delivery benchmark
    void client_delivery(int client_id, int iters);
    void server_delivery(int server_id, int iters);

    int connectQPs();
    bool syncSocket();
    void signalWorkersDone();
    void printShmemBuf();
    void checkNumaLocality();
    void flushShmemBuf();

    ~rc_resources();
private:
    size_t num_clients;
    size_t num_workers;
    int numa;
    size_t obj_size;

    std::string servername;
    int tcp_port;
    int sock;

    struct cm_con_data_t remote_props;
    std::string dev_name;
    int ib_port;
    struct ibv_device_attr dev_attr;
    struct ibv_context *ib_ctx;
    struct ibv_port_attr port_attr;
    struct ibv_pd *pd;
    struct ibv_mr *mr;
    std::vector<struct ibv_qp*> qps;
    std::vector<struct ibv_cq*> cqs;
    size_t shmem_buf_size;
    unsigned char *shmem_buf;
    std::vector<bool*> client_flags;
    std::vector<bool*> worker_flags;
    std::vector<int> worker_rands;

    bool check_atomic_suport();

    int setup_tcp_socket(std::string servername, int port);
    int setup_ib_dev();
    int setup_shmem_buf();
    int setup_qps();

    int modify_qp_to_init(ibv_qp *qp);
    int modify_qp_to_rtr(ibv_qp *qp, uint32_t remote_qpn, uint16_t dlid);
    int modify_qp_to_rts(ibv_qp *qp);

    void prepare_requests(ibv_send_wr *srs, ibv_sge *sges, int tid, int mode, int offset, int postlist);
    void prepare_shmembuf(int tid, int mode);

};


#endif //RDMA_RDMA_H
