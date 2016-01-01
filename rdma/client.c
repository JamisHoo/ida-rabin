#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <stdint.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <infiniband/arch.h>
#include <rdma/rdma_cma.h>

// #include "ec-method.h"
#include "nodes.h"
// #include "timer.h"

uint8_t* data;
uint8_t* output[COLUMN / 4 * 5];
uint8_t* decoded;
uint32_t row[COLUMN / 4 * 5];

void init() {
    int i;
    data = (uint8_t*)malloc(DATA_SIZE / NUM_CLIENTS);
    decoded = (uint8_t*)malloc(DATA_SIZE / NUM_CLIENTS);

    output[0] = (uint8_t*)malloc(DATA_SIZE / NUM_CLIENTS / 4 * 5);
    for (i = 0; i < COLUMN / 4 * 5; ++i)
        output[i] = output[0] + DATA_SIZE / NUM_CLIENTS / COLUMN * i;

    // ec-method_initialize();
}

void release() {
    free(data);
    free(decoded);
    free(output[0]);
}



#define RESOLVE_TIMEOUT_MS (5000)

int main(int argc, char** argv) {
    struct rdma_private_data server_pdata;
    struct rdma_event_channel* cm_channel;
    struct rdma_cm_id* cm_id;
    struct rdma_cm_event* event;
    struct rdma_conn_param conn_param = { };
    struct ibv_pd* pd;
    struct ibv_comp_channel* comp_chan;
    struct ibv_cq* cq;
    struct ibv_cq* evt_cq;
    struct ibv_mr* mr;
    struct ibv_qp_init_attr qp_attr = { };
    struct ibv_sge sge;
    struct ibv_send_wr send_wr = { };
    struct ibv_send_wr* bad_send_wr;
    struct ibv_recv_wr recv_wr = { };
    struct ibv_recv_wr* bad_recv_wr;
    struct ibv_wc wc;
    void* cq_context;
    struct addrinfo* res, *t;
    struct addrinfo hints = {
        .ai_family = AF_INET,
        .ai_socktype = SOCK_STREAM
    };
    int n;
    int err;

    init();

    /* Set up RDMA CM structures */

    cm_channel = rdma_create_event_channel();
    assert(cm_channel);

    err = rdma_create_id(cm_channel, &cm_id, 0, RDMA_PS_TCP);
    assert(err == 0);

    /* Resolve server address and route */

    n = getaddrinfo(server_ip, server_port_string, &hints, &res); 
    assert(n >= 0);

    for (t = res; t; t = t->ai_next) {
        err = rdma_resolve_addr(cm_id, 0, t->ai_addr, RESOLVE_TIMEOUT_MS);
        if (!err) break;
    }
    assert(err == 0);

    err = rdma_get_cm_event(cm_channel, &event);
    assert(err == 0);
    assert(event->event == RDMA_CM_EVENT_ADDR_RESOLVED);

    rdma_ack_cm_event(event);

    err = rdma_resolve_route(cm_id, RESOLVE_TIMEOUT_MS);
    assert(err == 0);

    err = rdma_get_cm_event(cm_channel, &event);
    assert(err == 0);
    assert(event->event == RDMA_CM_EVENT_ROUTE_RESOLVED);

    rdma_ack_cm_event(event);

    /* Create verbs objects now that we know which device to use */

    pd = ibv_alloc_pd(cm_id->verbs);
    assert(pd);

    comp_chan = ibv_create_comp_channel(cm_id->verbs);
    assert(comp_chan);

    cq = ibv_create_cq(cm_id->verbs, 10, 0, comp_chan, 0);
    assert(cq);

    err = ibv_req_notify_cq(cq, 0);
    assert(err == 0);

    mr = ibv_reg_mr(pd, data, DATA_SIZE / NUM_CLIENTS, IBV_ACCESS_LOCAL_WRITE);
    assert(mr);

    qp_attr.cap.max_send_wr = 10;
    qp_attr.cap.max_send_sge = 10;
    qp_attr.cap.max_recv_wr = 10;
    qp_attr.cap.max_recv_sge = 10;
    qp_attr.send_cq = cq;
    qp_attr.recv_cq = cq;
    qp_attr.qp_type = IBV_QPT_RC;

    err = rdma_create_qp(cm_id, pd, &qp_attr);
    assert(err == 0);

    conn_param.initiator_depth = 1;
    conn_param.retry_count = 7;

    /* Connect to server */

    err = rdma_connect(cm_id, &conn_param);
    assert(err == 0);

    err = rdma_get_cm_event(cm_channel, &event);
    assert(err == 0);
    assert(event->event == RDMA_CM_EVENT_ESTABLISHED);

    memcpy(&server_pdata, event->param.conn.private_data, sizeof(server_pdata));
    rdma_ack_cm_event(event);

    printf("my index == %d\n", server_pdata.index);

    /* Prepost receive */

    /*
    sge.addr = (uintptr_t)buf;
    sge.length = BUFFER_SIZE;
    sge.lkey = mr->lkey;

    recv_wr.wr_id = 0;
    recv_wr.sg_list = &sge;
    recv_wr.num_sge = 1;

    err = ibv_post_recv(cm_id->qp, &recv_wr, &bad_recv_wr);
    assert(err == 0);
    */

    /*
    // Send data to server 
    sge.addr = (uintptr_t)buf;
    sge.length = BUFFER_SIZE;
    sge.lkey = mr->lkey;

    send_wr.wr_id = 1;
    send_wr.opcode = IBV_WR_SEND;
    send_wr.send_flags = IBV_SEND_SIGNALED;
    send_wr.sg_list = &sge;
    send_wr.num_sge = 1;
    */

    /* Read data from server */

    sge.addr = (uintptr_t)data;
    sge.length = DATA_SIZE / NUM_CLIENTS;
    sge.lkey = mr->lkey;

    send_wr.wr_id = 1;
    send_wr.opcode = IBV_WR_RDMA_READ;
    send_wr.send_flags = IBV_SEND_SIGNALED;
    send_wr.sg_list = &sge;
    send_wr.num_sge = 1;
    send_wr.wr.rdma.rkey = ntohl(server_pdata.data_rkey);
    send_wr.wr.rdma.remote_addr = ntohll(server_pdata.data_va);

    err = ibv_post_send(cm_id->qp, &send_wr, &bad_send_wr);
    assert(err == 0);

    /* Wait for send completion */

    err = ibv_get_cq_event(comp_chan, &evt_cq, &cq_context);
    assert(err == 0);

    ibv_ack_cq_events(evt_cq, 1);

    err = ibv_req_notify_cq(cq, 0);
    assert(err == 0);

    n = ibv_poll_cq(cq, 1, &wc);
    assert(n >= 0);
    assert(wc.status == IBV_WC_SUCCESS);

    memset(data, server_pdata.index, DATA_SIZE / NUM_CLIENTS);

    /* Send data to server */
    sge.addr = (uintptr_t)data;
    sge.length = DATA_SIZE / NUM_CLIENTS;
    sge.lkey = mr->lkey;

    send_wr.wr_id = 1;
    send_wr.opcode = IBV_WR_RDMA_WRITE_WITH_IMM;
    send_wr.send_flags = IBV_SEND_SIGNALED;
    send_wr.sg_list = &sge;
    send_wr.num_sge = 1;
    send_wr.wr.rdma.rkey = ntohl(server_pdata.data_rkey);
    send_wr.wr.rdma.remote_addr = ntohll(server_pdata.data_va);

    err = ibv_post_send(cm_id->qp, &send_wr, &bad_send_wr);
    assert(err == 0);

    /* Wait for send completion */

    err = ibv_get_cq_event(comp_chan, &evt_cq, &cq_context);
    assert(err == 0);

    ibv_ack_cq_events(evt_cq, 1);

    err = ibv_req_notify_cq(cq, 0);
    assert(err == 0);

    n = ibv_poll_cq(cq, 1, &wc);
    assert(n >= 0);
    assert(wc.status == IBV_WC_SUCCESS);



    // TODO

    






    /* Release resources */

    ibv_dereg_mr(mr);
    rdma_destroy_id(cm_id);
    rdma_destroy_event_channel(cm_channel);
    release();
}
    
