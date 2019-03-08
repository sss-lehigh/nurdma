//
// Created by jacob on 10/30/18.
//

#include <cstdio>
#include <unistd.h>
#include <netdb.h>
#include <iostream>
#include <numa.h>
#include <fstream>
#include <cmath>
#include <future>
#include <random>
#include <atomic>
#include <infiniband/verbs.h>
#include <emmintrin.h>
#include "rc_resources.h"

#define MAX_RD_ATOMIC 1
#define BINDNUMA true
#define WORKER_LTNCY false
#define WORKER_NUMA_CHK false
#define WORKER_WRITE_RATIO 0.9
#define CLIENT_WRITE_RATIO 0.9
#define WRKR_RANDS 1000000
#define QP_WR_CAP 4096
#define SGE_CAP 30

rc_resources::rc_resources()
        : numa(), remote_props(), dev_attr(), ib_ctx(), port_attr(), pd(), mr(), shmem_buf() {}

void rc_resources::initOpenClosed(size_t n_machines, size_t n_workers, const char *rqstd_servername, size_t n_clients,
                                  const char *rqstd_dev_name, int rqstd_ib_port, size_t rqstd_obj_size, size_t rqstd_buf_size,
                                  int rqstd_numa, int rqstd_tcp_port) {
    num_clients = n_clients;
    num_workers = n_workers;
#if BINDNUMA
    numa = bind_to_numa_zone(rqstd_numa, rqstd_dev_name);
#else
    numa = rqstd_numa;
#endif

    servername = rqstd_servername;
    tcp_port = rqstd_tcp_port;
    sock = setup_tcp_socket(rqstd_servername, rqstd_tcp_port);
    if (sock < 0) {
        std::cerr << "socket setup failed" << std::endl;
        exit(1);
    }

    // setup RDMA environment
    dev_name = rqstd_dev_name;
    ib_port = rqstd_ib_port;
    obj_size = rqstd_obj_size;
    shmem_buf_size = rqstd_buf_size;
    if (setup_ib_dev()) {
        std::cerr << "ib_dev setup failed" << std::endl;
        exit(1);
    }
    std::cerr << "ib_dev setup complete" << std::endl;
    if (setup_shmem_buf()) {
        std::cerr << "shmem_buf setup failed" << std::endl;
        exit(1);
    }
    std::cerr << "shmem_buf setup complete" << std::endl;
    if (setup_qps()) {
        std::cerr << "qp setup failed" << std::endl;
        exit(1);
    }
    std::cerr << "qp setup complete" << std::endl;
    for (int i = 0; i < num_clients; ++i)
        client_flags.push_back(new bool(false));
    for (int i = 0; i < num_workers; ++i)
        worker_flags.push_back(new bool(false));

    for (int i = 0; i < WRKR_RANDS; ++i) {
        worker_rands.push_back(std::rand());
    }

    // enforce numa locality
    // enforce_numa_zone_all(shmem_buf, numa);
    enforce_numa_zone(&num_clients, numa);
    enforce_numa_zone(&numa, numa);
    enforce_numa_zone(&remote_props, numa);
    enforce_numa_zone(&sock, numa);
    enforce_numa_zone(&dev_name, numa);
    enforce_numa_zone(&dev_attr, numa);
    enforce_numa_zone(&ib_port, numa);
    enforce_numa_zone(&port_attr, numa);
    enforce_numa_zone(ib_ctx, numa);
    enforce_numa_zone(pd, numa);
    enforce_numa_zone(mr, numa);
    enforce_numa_zone(&shmem_buf_size, numa);
    enforce_numa_zone(&cqs, numa);
    for (auto &cq : cqs)
        enforce_numa_zone(cq, numa);
    enforce_numa_zone(&qps, numa);
    for (auto &qp : qps)
        enforce_numa_zone(qp, numa);
    enforce_numa_zone(&client_flags, numa);
    for (auto &f : client_flags)
        enforce_numa_zone(f, numa);
    enforce_numa_zone(&worker_flags, numa);
    for (auto &f : worker_flags)
        enforce_numa_zone(f, numa);
    for (auto &r : worker_rands)
        enforce_numa_zone(&r, numa);
    enforce_numa_zone(&worker_rands, numa);
    std::cerr << "numa enforcement complete" << std::endl;
}

void rc_resources::initDelivery(size_t n_clients, const char *rqstd_servername, int rqstd_tcp_port,
                                const char *rqstd_dev_name, int rqstd_ib_port, int rqstd_numa) {
    num_clients = n_clients;
    num_workers = 0;
#if BINDNUMA
    numa = bind_to_numa_zone(rqstd_numa, rqstd_dev_name);
#else
    numa = rqstd_numa;
#endif

    servername = rqstd_servername;
    tcp_port = rqstd_tcp_port;
    sock = setup_tcp_socket(rqstd_servername, rqstd_tcp_port);
    if (sock < 0) {
        std::cerr << "could not set up socket" << std::endl;
        exit(1);
    }

    // setup RDMA environment
    dev_name = rqstd_dev_name;
    ib_port = rqstd_ib_port;
    obj_size = 2 * sizeof(timespec);
    shmem_buf_size = 0;
    if (setup_ib_dev()) {
        std::cerr << "ib_dev setup failed" << std::endl;
        exit(1);
    }
    std::cerr << "ib_dev setup complete" << std::endl;
    if (setup_shmem_buf()) {
        std::cerr << "shmem_buf setup failed" << std::endl;
        exit(1);
    }
    std::cerr << "shmem_buf setup complete" << std::endl;
    if (setup_qps()) {
        std::cerr << "qp setup failed" << std::endl;
        exit(1);
    }
    std::cerr << "qp setup complete" << std::endl;
    for (int i = 0; i < num_clients; ++i)
        client_flags.push_back(new bool(false));
    for (int i = 0; i < num_workers; ++i)
        worker_flags.push_back(new bool(false));

    // enforce numa locality
    // enforce_numa_zone_all(shmem_buf, numa);
    enforce_numa_zone(&num_clients, numa);
    enforce_numa_zone(&numa, numa);
    enforce_numa_zone(&remote_props, numa);
    enforce_numa_zone(&sock, numa);
    enforce_numa_zone(&dev_name, numa);
    enforce_numa_zone(&dev_attr, numa);
    enforce_numa_zone(&ib_port, numa);
    enforce_numa_zone(&port_attr, numa);
    enforce_numa_zone(ib_ctx, numa);
    enforce_numa_zone(pd, numa);
    enforce_numa_zone(mr, numa);
    enforce_numa_zone(&shmem_buf_size, numa);
    enforce_numa_zone(&cqs, numa);
    for (auto &cq : cqs)
        enforce_numa_zone(cq, numa);
    enforce_numa_zone(&qps, numa);
    for (auto &qp : qps)
        enforce_numa_zone(qp, numa);
    enforce_numa_zone(&client_flags, numa);
    for (auto &f : client_flags)
        enforce_numa_zone(f, numa);
    enforce_numa_zone(&worker_flags, numa);
    for (auto &f : worker_flags)
        enforce_numa_zone(f, numa);
    std::cerr << "numa enforcement complete" << std::endl;
}

int rc_resources::setup_tcp_socket(std::string servername, int port) {
    // set up socket
    if (!servername.empty()) {
        sock = sock_connect(servername.data(), port);
        if (sock < 0) {
            std::cerr << "failed to establish TCP connection to server " << servername << ", port " << port
                      << std::endl;
        }
    } else {
        std::cout << "waiting on port " << port << " for TCP connection" << std::endl;
        sock = sock_connect(nullptr, port);
        if (sock < 0) {
            std::cerr << "failed to establish TCP connection with client on port " << port << std::endl;
        }
    }
    return sock;
}

int rc_resources::setup_ib_dev() {
    struct ibv_device **dev_list = nullptr;
    struct ibv_device *ib_dev = nullptr;
    int i, num_devices, rc = 0;

    // initialize device context
    dev_list = ibv_get_device_list(&num_devices);
    if (!dev_list) {
        std::cerr << "failed to get IB device list" << std::endl;
        rc = 1;
        goto setup_ib_dev_exit;
    }
    if (!num_devices) {
        std::cerr << "no devices found on this machine" << std::endl;
        rc = 1;
        goto setup_ib_dev_exit;
    }
    for (i = 0; i < num_devices; ++i) {
        if (dev_name.empty()) { // use first device found if no device requested
            dev_name = std::string(ibv_get_device_name(dev_list[i]));
            std::cerr << "device not specified, using first one found: " << dev_name << std::endl;
        }
        if (ibv_get_device_name(dev_list[i]) == dev_name) {
            ib_dev = dev_list[i];
            break;
        }
    }
    if (!ib_dev) {
        std::cerr << "IB device " << dev_name << " wasn't found" << std::endl;
        rc = 1;
        goto setup_ib_dev_exit;
    }

    std::cerr << dev_name << " on NUMA " << get_numa_zone(dev_name) << std::endl;
    ib_ctx = ibv_open_device(ib_dev);
    if (!ib_ctx) {
        std::cerr << "failed to open device " << dev_name << std::endl;
        rc = 1;
        goto setup_ib_dev_exit;
    }
    dev_attr = {}; // query device properties
    port_attr = {};
    ibv_query_device(ib_ctx, &dev_attr);
    if (ibv_query_port (ib_ctx, (uint8_t) ib_port, &port_attr)) {
        std::cerr << "ibv_query_port on port " << ib_port << " failed" << std::endl;
        rc = 1;
        goto setup_ib_dev_exit;
    }
    setup_ib_dev_exit:
    if (rc) {
        if (ib_ctx) {
            ibv_close_device(ib_ctx);
        }
        if (dev_list) {
            ibv_free_device_list(dev_list);
        }
        ib_ctx = nullptr;
        dev_attr = {};
        port_attr = {};
    }
    return rc;
}

int rc_resources::setup_shmem_buf() {
    ibv_access_flags mr_flags;
    int rc = 0;

    pd = ibv_alloc_pd(ib_ctx);
    if (!pd) {
        std::cerr << "ibv_alloc_pd failed" << std::endl;
    }

    shmem_buf_size = shmem_buf_size > num_clients * obj_size ? shmem_buf_size : num_clients * obj_size;

    // what is true difference between numa_alloc and malloc -> move
#if BINDNUMA
    shmem_buf = (unsigned char *) numa_alloc(shmem_buf_size);
#else
    shmem_buf = (unsigned char*) malloc(shmem_buf_size);
    enforce_numa_zone(shmem_buf, numa);
#endif

    if (!shmem_buf) {
        std::cerr << "fail to allocate " << shmem_buf_size << " bytes on numa zone " << numa << std::endl;
        rc = 1;
        goto setup_shmem_buf_exit;
    }
    if (servername.empty())
        memset(shmem_buf, 255, shmem_buf_size);
    else
        memset(shmem_buf, 0, shmem_buf_size);

    mr_flags = static_cast<ibv_access_flags >( IBV_ACCESS_LOCAL_WRITE | IBV_ACCESS_REMOTE_READ | IBV_ACCESS_REMOTE_WRITE | IBV_ACCESS_REMOTE_ATOMIC);
    mr = ibv_reg_mr(pd, shmem_buf, shmem_buf_size, mr_flags);
    if (!mr) {
        std::cerr << "ibv_reg_mr failed with mr_flags=0x" << mr_flags << std::endl;
        rc = 1;
        goto setup_shmem_buf_exit;
    }

    setup_shmem_buf_exit:
    if (rc) {
        if (mr) {
            ibv_dereg_mr(mr);
        }
        if (shmem_buf) {
            numa_free(shmem_buf, shmem_buf_size);
        }
        if (pd) {
            ibv_dealloc_pd(pd);
        }
    }
    return rc;
}

int rc_resources::setup_qps() {
    ibv_qp_init_attr qp_init_attr = {};
    int i, rc = 0;

    qp_init_attr.qp_type = IBV_QPT_RC;          // QP type is reliable connected
    qp_init_attr.sq_sig_all = 0;                // do not automatically signal completion
    qp_init_attr.cap.max_send_wr = (uint32_t) dev_attr.max_qp_wr;
    qp_init_attr.cap.max_recv_wr = (uint32_t) 1;
    qp_init_attr.cap.max_send_sge = (uint32_t) 1;
    qp_init_attr.cap.max_recv_sge = (uint32_t) 1;

    int num_cqe = dev_attr.max_cqe;
    for (i = 0; i < num_clients; ++i) {
        struct ibv_cq *temp_cqptr = nullptr;
        struct ibv_qp *temp_qpptr = nullptr;
        while (temp_cqptr == nullptr) {
            temp_cqptr = ibv_create_cq(ib_ctx, num_cqe, nullptr, nullptr, 0); // create cq that can handle max cqes
            if (temp_cqptr == nullptr && errno != EINVAL) {
                perror("CQ creation unsuccessful\n");
                rc = 1;
                goto setup_qps_exit;
            }
            else if (temp_cqptr == nullptr)
                --num_cqe;
        }
        cqs.push_back(temp_cqptr);
        qp_init_attr.send_cq = temp_cqptr;
        qp_init_attr.recv_cq = temp_cqptr;
        while (temp_qpptr == nullptr) {
            temp_qpptr = ibv_create_qp(pd, &qp_init_attr);
            if (temp_qpptr == nullptr && qp_init_attr.cap.max_send_wr == 0) {
                perror("QP creation unsuccessful");
                rc = 1;
                goto setup_qps_exit;
            }
            else if (temp_qpptr == nullptr) {
                --qp_init_attr.cap.max_send_wr;
            }
        }
        do {
            ibv_destroy_qp(temp_qpptr);
            ++qp_init_attr.cap.max_send_sge;
            temp_qpptr = ibv_create_qp(pd, &qp_init_attr);
            if (temp_qpptr == nullptr) {
                --qp_init_attr.cap.max_send_sge;
                temp_qpptr = ibv_create_qp(pd, &qp_init_attr);
                break;
            }
        } while (temp_qpptr != nullptr);
        qps.push_back(temp_qpptr);
    }

    std::cerr << "Real CQE maximum: " << num_cqe << std::endl;
    std::cerr << "Real Send QP WR maximum: " << qp_init_attr.cap.max_send_wr << std::endl;
    std::cerr << "Real Send QP SGE maximum: " << qp_init_attr.cap.max_send_sge << std::endl;

    setup_qps_exit:
    if (rc) {
        for (auto &qp : qps) {
            ibv_destroy_qp(qp);
        }
        for (auto &cq : cqs) {
            ibv_destroy_cq(cq);
        }
    }
    return rc;
}

int rc_resources::modify_qp_to_init(struct ibv_qp *qp) {
    struct ibv_qp_attr attr = {};
    int flags;
    int rc;
    memset(&attr, 0, sizeof(attr));
    attr.qp_state = IBV_QPS_INIT;
    attr.port_num = (uint8_t) ib_port;
    attr.pkey_index = 0;
    attr.qp_access_flags = IBV_ACCESS_LOCAL_WRITE | IBV_ACCESS_REMOTE_READ |
                           IBV_ACCESS_REMOTE_WRITE | IBV_ACCESS_REMOTE_ATOMIC;
    flags = IBV_QP_STATE | IBV_QP_PKEY_INDEX | IBV_QP_PORT | IBV_QP_ACCESS_FLAGS;
    rc = ibv_modify_qp(qp, &attr, flags);
    if (rc) {
        perror("failed to modify QP state to INIT");
    }
    return rc;
}

int rc_resources::modify_qp_to_rtr(struct ibv_qp *qp, uint32_t remote_qpn, uint16_t dlid) {
    struct ibv_qp_attr attr = {};
    int flags;
    int rc;
    memset(&attr, 0, sizeof(attr));
    attr.qp_state = IBV_QPS_RTR;
    attr.path_mtu = IBV_MTU_256;
    attr.dest_qp_num = remote_qpn;
    attr.rq_psn = 0;
    attr.max_dest_rd_atomic = MAX_RD_ATOMIC;
    attr.min_rnr_timer = 0x12;
    attr.ah_attr.is_global = 0;
    attr.ah_attr.dlid = dlid;
    attr.ah_attr.sl = 0;
    attr.ah_attr.src_path_bits = 0;
    attr.ah_attr.port_num = (uint8_t) ib_port;
    flags = IBV_QP_STATE | IBV_QP_AV | IBV_QP_PATH_MTU | IBV_QP_DEST_QPN |
            IBV_QP_RQ_PSN | IBV_QP_MAX_DEST_RD_ATOMIC | IBV_QP_MIN_RNR_TIMER;
    rc = ibv_modify_qp(qp, &attr, flags);
    if (rc)
        std::cerr << "failed to modify QP state to RTR - " << strerror(rc) << std::endl;
    return rc;
}

int rc_resources::modify_qp_to_rts(struct ibv_qp *qp) {
    struct ibv_qp_attr attr = {};
    int flags;
    int rc;
    memset(&attr, 0, sizeof(attr));
    attr.qp_state = IBV_QPS_RTS;
    attr.timeout = 0x12;
    attr.retry_cnt = 6;
    attr.rnr_retry = 0;
    attr.sq_psn = 0;
    attr.max_rd_atomic = MAX_RD_ATOMIC;
    flags = IBV_QP_STATE | IBV_QP_TIMEOUT | IBV_QP_RETRY_CNT |
            IBV_QP_RNR_RETRY | IBV_QP_SQ_PSN | IBV_QP_MAX_QP_RD_ATOMIC;
    rc = ibv_modify_qp(qp, &attr, flags);
    if (rc)
        std::cerr << "failed to modify QP state to RTS" << std::endl;
    return rc;
}

int rc_resources::connectQPs() {
    struct cm_con_data_t local_con{}, remote_con{}, tmp_con{};
    int i;
    char ch;
    union ibv_gid my_gid = {};

    for (i = 0; i < num_clients; ++i) {
        local_con.addr = ntohll((uintptr_t) shmem_buf);
        local_con.rkey =  htonl(mr->rkey);
        local_con.qp_num = htonl(qps[i]->qp_num);
        local_con.lid = port_attr.lid;
        memcpy(local_con.gid, (void *) &my_gid, 16);
        if (sock_sync_data(sock, sizeof(struct cm_con_data_t), (char *) &local_con, (char *) &tmp_con) < 0) {
            perror("failed to exchange connection data");
            return 1;
        }
        remote_con = {
                addr : ntohll((uintptr_t) tmp_con.addr),
                rkey : ntohl(tmp_con.rkey),
                qp_num : ntohl(tmp_con.qp_num),
                lid : tmp_con.lid
        };
        memcpy(remote_con.gid, tmp_con.gid, 16);
        remote_props = remote_con;
        if (modify_qp_to_init(qps[i]))
            return 1;
        if (modify_qp_to_rtr(qps[i], remote_con.qp_num, remote_con.lid))
            return 1;
        if (modify_qp_to_rts(qps[i]))
            return 1;
        if (sock_sync_data(sock, 1, (char *) "Q", &ch)) {
            return 1;
        }
    }
    return 0;
}

bool rc_resources::syncSocket() {
    char ch = 'a';
    return sock_sync_data(sock, 1, (char *) "Q", &ch) == 0;
}

bool rc_resources::isClient() {
    return !servername.empty();
}

void rc_resources::client_openloop(int client_id, int iters, int postlist,
                                          int sendbatch, int mode, int think) {
    /* send variables */
    int ops[] = {IBV_WR_RDMA_READ, IBV_WR_RDMA_WRITE, IBV_WR_ATOMIC_CMP_AND_SWP, IBV_WR_ATOMIC_FETCH_AND_ADD};
    int offset, opcode;
    struct ibv_send_wr srs[postlist];
    struct ibv_sge sges[postlist];
    struct ibv_send_wr *bad_wr = nullptr, *to_send = nullptr;

    /* backoff variables and misc*/
    struct timespec t1 = {}, t2 = {}, backoff = {};
    int r, i, j, rc = 0;
    int divisible, todo, failures = 0, x = 0, y = 1;

    // check that atomics are supported
    if (mode > 2 && dev_attr.atomic_cap == IBV_ATOMIC_NONE) {
        goto client_openloop_exit;
    }

    offset = (uint32_t) obj_size * client_id;
    prepare_requests(srs, sges, client_id, mode, offset, postlist);
    prepare_shmembuf(client_id, mode);

    divisible = iters % postlist == 0;
    todo = (divisible ? iters / postlist : iters / postlist + 1);

    if (mode == 2)
        std::srand((uint) client_id);

    for (i = 0; i < todo; ++i) {
        to_send = &srs[0];
        switch (mode) {
            case 0: // read
                opcode = 0;
                break;
            case 1: // write
                opcode = 1;
                break;
            case 2: // choose read/write based on FB's ETC workload
                r = std::rand();
                if (r < RAND_MAX * CLIENT_WRITE_RATIO)
                    opcode = 1;
                else
                    opcode = 0;
                if (opcode == 1)
                    memset(shmem_buf + offset, 0, obj_size);
                break;
            case 3: // compare and swap
                opcode = 2;
                break;
            case 4: // fetch and add
                opcode = 3;
                break;
            default:
                opcode = 0;
        }
        for (j = 0; j < postlist; ++j) {
            srs[j].send_flags = (((i * postlist) + j + 1) % sendbatch == 0 ? IBV_SEND_SIGNALED : 0);
            srs[j].opcode = (enum ibv_wr_opcode) ops[opcode];
        }

        int last_wr;
        if (!divisible && i == todo - 1) {
            last_wr = (iters % postlist) - 1;
            srs[last_wr].next = nullptr;
            srs[last_wr].send_flags = IBV_SEND_SIGNALED;
        } else if (i == todo - 1) {
            last_wr = postlist - 1;
            srs[last_wr].send_flags = IBV_SEND_SIGNALED;
        }

        backoff.tv_sec = 0;
        backoff.tv_nsec = 32;
        do {
            clock_gettime(CLOCK_REALTIME, &t1);
            do {
                clock_gettime(CLOCK_REALTIME, &t2);
            } while ((BILLION * (t2.tv_sec - t1.tv_sec) + t2.tv_nsec - t1.tv_nsec) < backoff.tv_nsec);
            rc = ibv_post_send(qps[client_id], to_send, &bad_wr);
            if (rc) {
                ++failures;
                backoff.tv_nsec *= 2;
                to_send = bad_wr;
            }
        } while (rc == ENOMEM);
        if (rc) {
            std::cerr << "Client " << client_id << " failed iteration " << i << " with code " << rc << " (trying again)" << std::endl;
        }

        for (j = 0; j < think; ++j) {
            x = y + 1;
            y = x + 1;
        }
    }
    fprintf(stderr, "%d, total failures: %d\n", x + y, failures);

    // wait for polling thread
    client_openloop_exit:
    sleep(1);
    *client_flags[client_id] = true;
}

void rc_resources::poll_cq(int client_id, int iters, int sendbatch) {
    struct ibv_wc wc = {};
    struct timespec t1 = {}, t2 = {};
    double throughput;
    int total_expected, received_this_period, total_received, epoch;
    volatile bool *done;
    int poll_result;

    total_expected = iters;
    received_this_period = 0;
    total_received = 0;
    epoch = 0;
    done = client_flags[client_id];
    clock_gettime(CLOCK_REALTIME, &t1);
    do {
        poll_result = ibv_poll_cq(cqs[client_id], 1, &wc);
        if (poll_result < 0) {
            /* poll CQ failed */
            std::cerr << "poll CQ failed with error " << wc.status << "(" << ibv_wc_status_str(wc.status) << ")" << std::endl;
            break;
        } else if (poll_result > 0) {
            /* CQE found */
            total_received += sendbatch;
            received_this_period += sendbatch;
            if (wc.status != IBV_WC_SUCCESS) {
                std::cerr << "got bad completion with status: 0x" << wc.status
                          << " (" << ibv_wc_status_str(wc.status) << ") vendor syndrome: 0x"
                          << wc.vendor_err << std::endl;
                break;
            }
        }
        clock_gettime(CLOCK_REALTIME, &t2);
        if (BILLION * (t2.tv_sec - t1.tv_sec) + t2.tv_nsec - t1.tv_nsec >= BILLION / 100) {
            ++epoch;
            throughput = received_this_period /
                         ((BILLION * (t2.tv_sec - t1.tv_sec) + t2.tv_nsec - t1.tv_nsec) / (double) BILLION);
            printf("%d\t%d\t%f\n", (client_id), epoch, throughput);
            received_this_period = 0; // reset period counter and clock
            clock_gettime(CLOCK_REALTIME, &t1);
        }
    } while ((total_received < total_expected) && !*(done));
    std::cerr << "CQE received: " << total_received << std::endl;
}

std::vector<double>
rc_resources::client_closedloop(int client_id, int iters, int postlist, int sendbatch, int mode, int think) {
    /* send variables */
    int ops[] = {IBV_WR_RDMA_READ, IBV_WR_RDMA_WRITE, IBV_WR_ATOMIC_CMP_AND_SWP, IBV_WR_ATOMIC_FETCH_AND_ADD};
    int offset, opcode;
    struct ibv_send_wr srs[postlist];
    struct ibv_sge sges[postlist];
    struct ibv_send_wr *bad_wr = nullptr, *to_send = nullptr;
    struct timespec t1 = {}, t2 = {}, backoff = {};

    /* poll variables */
    int poll_result;
    struct ibv_wc wc = {};

    /* timing variables */
    struct timespec start = {}, end = {};

    /* statistics and misc*/
    uint64_t _elapsed_iter;
    uint64_t _elapsed_tot;
    double *_elapsed;
    double elapsed_avg;
    double elapsed_stddev;
    std::vector<double> retval;
    int r, i, j, rc = 0;

    /* prepare the scatter/gather entry for send */
    offset = (uint32_t) obj_size * client_id;
    prepare_requests(srs, sges, client_id, mode, offset, postlist);
    prepare_shmembuf(client_id, mode);

    _elapsed_tot = 0;
    _elapsed = (double *) malloc(sizeof(uint64_t) * iters);
    int divisible, todo, failures = 0, x = 0, y = 1;
    divisible = iters % postlist == 0;
    todo = (divisible ? iters / postlist : iters / postlist + 1);
    for (i = 0; i < todo; ++i) {
        backoff.tv_sec = 0;
        backoff.tv_nsec = 32;
        to_send = &srs[0];
        switch (mode) {
            case 0: // read
                opcode = 0;
                break;
            case 1: // write
                opcode = 1;
                break;
            case 2: // alternate write/read
                r = std::rand();
                if (r < RAND_MAX / 30)
                    opcode = 1;
                else
                    opcode = 0;
                if (opcode == 1)
                    memset(shmem_buf + offset, 0, obj_size);
                break;
            case 3:
                opcode = 2;
                break;
            case 4:
                opcode = 3;
                break;
            default:
                opcode = 0;
        }
        for (j = 0; j < postlist; ++j) {
            srs[j].send_flags = (((i * postlist) + j + 1) % sendbatch == 0 ? IBV_SEND_SIGNALED : 0);
            srs[j].opcode = (enum ibv_wr_opcode) ops[opcode];
        }

        int last_wr;
        if (!divisible && i == todo - 1) {
            last_wr = (iters % postlist) - 1;
            srs[last_wr].next = nullptr;
            srs[last_wr].send_flags = IBV_SEND_SIGNALED;
        } else if (i == todo - 1) {
            last_wr = postlist - 1;
            srs[last_wr].send_flags = IBV_SEND_SIGNALED;
        }
        clock_gettime(CLOCK_REALTIME, &start);
        do {
            clock_gettime(CLOCK_REALTIME, &t1);
            do {
                clock_gettime(CLOCK_REALTIME, &t2);
            } while ((BILLION * (t2.tv_sec - t1.tv_sec) + t2.tv_nsec - t1.tv_nsec) <
                     (BILLION * backoff.tv_sec) + backoff.tv_nsec);
            rc = ibv_post_send(qps[client_id], to_send, &bad_wr);
            if (rc) {
                ++failures;
                backoff.tv_nsec *= 2;
                to_send = bad_wr;
            }
        } while (rc == ENOMEM);
        // poll completion queue
        for (j = 0; i * postlist + j < iters && j < postlist; ++j) {
            do {
                poll_result = ibv_poll_cq(cqs[client_id], 1, &wc);
            } while ((poll_result == 0));
            if (poll_result < 0) {
                /* poll CQ failed */
                std::cerr << "poll CQ failed with error " << wc.status << "(" << strerror(wc.status) << ")"
                          << std::endl;
                rc = 1;
            } else if (poll_result > 0) {
                /* CQE found */
                if (wc.status != IBV_WC_SUCCESS) {
                    std::cerr << "got bad completion with status: 0x" << wc.status
                              << "(" << strerror(wc.status) << ") vendor syndrome: 0x"
                              << wc.vendor_err << std::endl;
                    rc = 1;
                }
            }
            if (rc) {
                printf("FAILED");
                exit(1);
            }
        }
        clock_gettime(CLOCK_REALTIME, &end);

        // t2 is the most recent time before calling ibv_post_send
        _elapsed_iter = (uint64_t) (BILLION * (end.tv_sec - t2.tv_sec) + end.tv_nsec - t2.tv_nsec);
        _elapsed_tot += _elapsed_iter;
        _elapsed[i] = (double) _elapsed_iter;

        for (j = 0; j < think; ++j) {
            x = y + 1;
            y = x + 1;
        }
    }
    fprintf(stderr, "%d, total failures: %d\n", x + y, failures);
    elapsed_avg = _elapsed_tot / (double) (iters);
    elapsed_stddev = 0.0;
    for (i = 0; i < iters; ++i) {
        elapsed_stddev += std::pow(_elapsed[i] - elapsed_avg, 2);
    }
    elapsed_stddev = sqrt(elapsed_stddev);
    elapsed_stddev /= iters;
    retval.push_back(elapsed_avg);
    retval.push_back(elapsed_stddev);
    return retval;
}

void rc_resources::worker(int worker_id, int mode, int think, int timeout) {
    struct timespec start{}, t1{}, t2{}, t3{}, t4{};
    volatile unsigned char* vol_shmem_buf = (volatile unsigned char *) shmem_buf;
    int r, i, j, temp = 0;

    clock_gettime(CLOCK_REALTIME, &t1);
    start = t1;
    t3 = t1;

    for (i = 0; !*(worker_flags[worker_id]); ++i) {
        switch (mode) {
            case 0:
                temp += vol_shmem_buf[(obj_size * worker_id + worker_rands[i % WRKR_RANDS] + i) % shmem_buf_size];
                break;
            case 1: // write as uint64_t for consistency with CAS
                //*reinterpret_cast<uint64_t *>(&shmem_buf[(obj_size * worker_id + i * sizeof(uint64_t)) % shmem_buf_size]) = (uint64_t) worker_id;
                vol_shmem_buf[(obj_size * worker_id + worker_rands[i % WRKR_RANDS] + i) % shmem_buf_size] = (unsigned char) worker_id;
                break;
            case 2:
                r = worker_rands[i % WRKR_RANDS];
                if (r < RAND_MAX * WORKER_WRITE_RATIO)
                    vol_shmem_buf[(obj_size * worker_id + i) % shmem_buf_size] = (unsigned char) worker_id;
                else
                    temp += vol_shmem_buf[(obj_size * worker_id + i) % shmem_buf_size];
                break;
            case 3: {
                // uncomment the following line to atomically access on the cache line boundary
                // auto * atomic_buf = reinterpret_cast<std::atomic<unsigned long> *>(&shmem_buf[(obj_size * worker_id + i * sizeof(uint64_t) + sizeof(uint64_t) / 2) % (shmem_buf_size - sizeof(uint64_t))]);
                auto * atomic_buf = reinterpret_cast<std::atomic<unsigned long> *>(&shmem_buf[(obj_size * worker_id + i * sizeof(uint64_t)) % (shmem_buf_size)]);
//                char temp_buf[sizeof(unsigned long)];
                auto expected = (uint64_t) (worker_id + 1) % num_workers;
                auto desired = (uint64_t) worker_id;
                do {
                    //  try until succeed
                } while (!std::atomic_compare_exchange_strong(atomic_buf, &expected, desired));
                break;
            }
            case 4: {
                auto * atomic_buf = reinterpret_cast<std::atomic<unsigned long> *>(&shmem_buf[(obj_size * worker_id + i * sizeof(uint64_t)) % shmem_buf_size]);
                uint64_t add = 1;
                std::atomic_fetch_add(atomic_buf, add);
                break;
            }
            default:
                break;
        }

        for (j = 0; j < think; ++j) {
            temp *= 4;
            if (temp)
                temp /= 4;
            else
                temp /= 4 * (temp * 0 + 1);
        }

#if WORKER_NUMA_CHK
        if (i % 1000000 == 0) // periodically checks that everything is in the expected NUMA zone
            checkNumaLocality();
#endif

#if WORKER_LTNCY
        if (i % 1000000 == 0) {
            clock_gettime(CLOCK_REALTIME, &t4);
            std::cout << (BILLION * (t4.tv_sec - t3.tv_sec) + t4.tv_nsec - t3.tv_nsec) / (double) 100000 << std::endl; // print worker latency
            clock_gettime(CLOCK_REALTIME, &t3);
        }
#endif


        clock_gettime(CLOCK_REALTIME, &t2);
        if (BILLION * (t2.tv_sec - start.tv_sec) + t2.tv_nsec - start.tv_nsec >= timeout * BILLION) {
            // printf("stopping...");
            //*(worker_flags[worker_id]) = true;
            // *((struct buf_t *) ctx->res->buf) = buf;
            //buf = *((struct buf_t *) ctx->res->buf);
            //_mm_clflush(shmem_buf);
        }
    }
}

int rc_resources::getNumClients() {
    return static_cast<int>(num_clients);
}

void rc_resources::signalWorkersDone() {
    for (auto &d : worker_flags)
        *d = true;
}

void rc_resources::checkNumaLocality() {
    if (numa != get_numa_zone(shmem_buf)) {
        std::cerr <<"shmem_buf failed NUMA expectation" << std::endl;
        exit(1); }
    if (numa != get_numa_zone(&num_clients)) {
        std::cerr <<"num_clients failed NUMA expectation" << std::endl;
        exit(1); }
    if (numa != get_numa_zone(&numa)) {
        std::cerr <<"numa failed NUMA expectation" << std::endl;
        exit(1); }
    if (numa != get_numa_zone(&remote_props)) {
        std::cerr <<"remote_props failed NUMA expectation" << std::endl;
        exit(1); }
    if (numa != get_numa_zone(&sock)) {
        std::cerr <<"sock failed NUMA expectation" << std::endl;
        exit(1); }
    if (numa != get_numa_zone(&dev_name)) {
        std::cerr <<"dev_name failed NUMA expectation" << std::endl;
        exit(1); }
    if (numa != get_numa_zone(&dev_attr)) {
        std::cerr <<"dev_attr failed NUMA expectation" << std::endl;
        exit(1); }
    if (numa != get_numa_zone(&ib_port)) {
        std::cerr <<"ib_port failed NUMA expectation" << std::endl;
        exit(1); }
    if (numa != get_numa_zone(&port_attr)) {
        std::cerr <<"port_attr failed NUMA expectation" << std::endl;
        exit(1); }
    if (numa != get_numa_zone(ib_ctx)) {
        std::cerr <<"ib_ctx failed NUMA expectation" << std::endl;
        exit(1); }
    if (numa != get_numa_zone(pd)) {
        std::cerr <<"pd failed NUMA expectation" << std::endl;
        exit(1); }
    if (numa !=  get_numa_zone(mr)) {
        std::cerr <<"mr failed NUMA expectation" << std::endl;
        exit(1); }
    if (numa != get_numa_zone(&shmem_buf_size)) {
        std::cerr <<"shmem_buf_size failed NUMA expectation" << std::endl;
        exit(1); }
    if (numa != get_numa_zone(&cqs)) {
        std::cerr <<"cqs failed NUMA expectation" << std::endl;
        exit(1); }
    for (auto &cq : cqs)
        if (numa != get_numa_zone(cq)) {
            std::cerr <<"cq failed NUMA expectation" << std::endl;
            exit(1); }
    if (numa != get_numa_zone(&qps)) {
        std::cerr <<"qps failed NUMA expectation" << std::endl;
        exit(1); }
    for (auto &qp : qps)
        if (numa != get_numa_zone(qp)) {
            std::cerr <<"qp failed NUMA expectation" << std::endl;
            exit(1); }
    if (numa != get_numa_zone(&client_flags)) {
        std::cerr <<"client_flags failed NUMA expectation" << std::endl;
        exit(1); }
    for (auto &f : client_flags)
        if (numa != get_numa_zone(f)) {
            std::cerr <<"cflag failed NUMA expectation" << std::endl;
            exit(1); }
    if (numa != get_numa_zone(&worker_flags)) {
        std::cerr <<"worker_flags failed NUMA expectation" << std::endl;
        exit(1); }
    for (auto &f : worker_flags)
        if (numa != get_numa_zone(f)) {
            std::cerr <<"wflag failed NUMA expectation" << std::endl;
            exit(1); }
}

void rc_resources::printShmemBuf() {
    std::cerr << "shmem_buf on NUMA " << get_numa_zone(shmem_buf) << std::endl;
    for (int i = 0; i < shmem_buf_size; ++i) {
        std::cerr << "0x" << std::hex << (int) shmem_buf[i] << "\t";
        if ((i + 1) % obj_size == 0 || i == shmem_buf_size - 1)
            std::cerr << std::endl;
    }
}

void rc_resources::prepare_requests(ibv_send_wr *srs, ibv_sge *sges, int tid, int mode, int offset, int postlist) {
    int i;
    unsigned char id_buf[8];
    for (i = 0; i < 8; ++i) {
        id_buf[i] = (unsigned char) tid;
    }
    memset(sges, 0, sizeof(struct ibv_send_wr) * postlist);
    memset(srs, 0, sizeof(struct ibv_sge) * postlist);
    for (i = 0; i < postlist; ++i) {
        sges[i].addr = (uintptr_t) shmem_buf + offset;
        sges[i].length = (uint32_t) obj_size;
        sges[i].lkey = mr->lkey;
        /* prepare the send work request */
        srs[i].wr_id = (uint64_t) tid;
        srs[i].sg_list = &sges[i];
        srs[i].num_sge = 1;
        srs[i].next = (i == postlist - 1 ? nullptr : &srs[i + 1]);

        switch (mode) {
            case 0:
            case 1:
            case 2: // RDMA read or write
                srs[i].wr.rdma.remote_addr = remote_props.addr + offset;
                srs[i].wr.rdma.rkey = remote_props.rkey;
                break;
            case 3: // compare and swap
                sges[i].length = 8;
                srs[i].wr.atomic.remote_addr = remote_props.addr + offset;
                srs[i].wr.atomic.rkey = remote_props.rkey;
                srs[i].wr.atomic.compare_add = UINT64_MAX;
                srs[i].wr.atomic.swap = *((uint64_t *)id_buf);
                break;
            case 4: // fetch and add
                sges[i].length = 8;
                srs[i].wr.atomic.remote_addr = remote_props.addr + offset;
                srs[i].wr.atomic.rkey = remote_props.rkey;
                srs[i].wr.atomic.compare_add = 1;
                break;
            default:
                break;
        }
    }
}

void rc_resources::prepare_shmembuf(int tid, int mode) {
    int i;
    switch (mode) {
        case 0:
        case 1:
        case 2:
            for (i = 0; i < obj_size; ++i) {
                *(shmem_buf + (obj_size * tid) + i) = static_cast<unsigned char>(tid);
            }
            break;
        case 3:
        case 4:
            for (i = 0; i < obj_size; ++i) {
                *(shmem_buf + (obj_size * tid) + i) = 0;
            }
            break;
        default:
            break;
    }
}

void rc_resources::flushShmemBuf() {
    // flushes all cache lines associated with shmem_buf
    // (probably a better way to accomplish this)
    for (int i = 0; i < shmem_buf_size; ++i) {
        _mm_clflush(shmem_buf + i);
    }
}

void rc_resources::client_delivery(int client_id, int iters) {
    // send
    size_t soffset;
    struct ibv_send_wr sr{};
    struct ibv_sge sge{};
    struct ibv_send_wr *sbad_wr = nullptr;

    size_t roffset;
    struct ibv_recv_wr rr{};
    struct ibv_recv_wr *rbad_wr = nullptr;
    struct timespec t1 = {}, t2 = {}, backoff = {};

    /* poll variables */
    int poll_result;
    struct ibv_wc wc = {};

    /* timing variables */
    struct timespec start = {}, end = {};

    /* statistics and misc*/
    std::vector<double> retval;
    int i, j, rc = 0;

    /* post receive for returning time */
    roffset = client_id * obj_size + obj_size / 2; // second half of entry
    sge.addr = roffset;
    sge.length = (uint32_t) obj_size / 2;
    sge.lkey = (uint32_t) mr->lkey;
    rr.wr_id = (uint32_t) client_id;
    rr.num_sge = 1;
    rr.sg_list = &sge;
    rr.next = nullptr;
    rc = ibv_post_recv(qps[client_id], &rr, &rbad_wr);
    if (rc) {
        std::cerr << "failed to post receive" << std::endl;
        exit(1);
    }

    soffset = client_id * obj_size;
    sge.addr = soffset;
    sr.wr_id = (uint32_t) client_id;
    sr.num_sge = 1;
    sr.sg_list = &sge;
    sr.opcode = IBV_WR_RDMA_WRITE;

}

void rc_resources::server_delivery(int server_id, int iters) {

}

rc_resources::~rc_resources() {
    for (auto &qp : qps) {
        ibv_destroy_qp(qp);
    }
    for (auto &cq : cqs) {
        ibv_destroy_cq(cq);
    }
    if (mr) {
        ibv_dereg_mr(mr);
    }
    if (shmem_buf) {
        numa_free(shmem_buf, shmem_buf_size);
    }
    if (pd) {
        ibv_dealloc_pd(pd);
    }
    if (ib_ctx) {
        ibv_close_device(ib_ctx);
    }
    if (sock >= 0) {
        std::cerr << "closing socket" << std::endl;
        if (close(sock)) {
            std::cerr << "could not close socket: " << sock << std::endl;
        }
    }
}

// returns true if atomic capability is supported
bool rc_resources::check_atomic_suport() {
    return dev_attr.atomic_cap != IBV_ATOMIC_NONE;
}
