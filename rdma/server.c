#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <stdint.h>
#include <string.h>
#include <arpa/inet.h>
#include <infiniband/arch.h>
#include <rdma/rdma_cma.h>

// #include "ec/ec-method.h"
#include "nodes.h"
#include "timer.h"

uint8_t* data;
uint8_t* decoded;
uint8_t* output[NUM_CLIENTS];
uint32_t row[ROW];

void init() {
    int i;

    assert(DATA_SIZE % NUM_CLIENTS == 0);
    assert(DATA_SIZE / NUM_CLIENTS % COLUMN == 0);

    data = (uint8_t*)malloc(DATA_SIZE);
    decoded = (uint8_t*)malloc(DATA_SIZE);

    output[0] = (uint8_t*)malloc(DATA_SIZE / COLUMN * ROW);
    for (i = 0; i < NUM_CLIENTS; ++i) 
        output[i] = output[0] + DATA_SIZE / COLUMN * ROW / NUM_CLIENTS * i;

    for (i = 0; i < ROW; ++i) 
        row[i] = i;

    // ec_method_initialize();

    for (i = 0; i < DATA_SIZE; ++i)
        data[i] = rand();

    printf("Init finished\n");
}

void release() {
    free(data);
    free(output[0]);
    free(decoded);
}

int main(int argc, char** argv) {
    struct rdma_private_data rep_pdata[NUM_CLIENTS];
    struct rdma_event_channel* cm_channel;
    struct rdma_cm_id* listen_id;
    struct rdma_cm_id* cm_id[NUM_CLIENTS];
    struct rdma_cm_event* event[NUM_CLIENTS];
    struct rdma_conn_param conn_param[NUM_CLIENTS] = { };
    struct ibv_pd* pd[NUM_CLIENTS];
    struct ibv_comp_channel* comp_chan[NUM_CLIENTS];
    struct ibv_cq* cq[NUM_CLIENTS];
    struct ibv_cq* evt_cq[NUM_CLIENTS];
    struct ibv_mr* mr[NUM_CLIENTS]; // for data
    struct ibv_mr* mr2[NUM_CLIENTS]; // for output
    struct ibv_mr* mr3[NUM_CLIENTS]; // for decoded
    struct ibv_qp_init_attr qp_attr[NUM_CLIENTS] = { };
    struct ibv_sge sge[NUM_CLIENTS];
    struct ibv_sge sge2[NUM_CLIENTS];
    struct ibv_send_wr send_wr[NUM_CLIENTS] = { };
    struct ibv_send_wr* bad_send_wr[NUM_CLIENTS];
    struct ibv_recv_wr recv_wr[NUM_CLIENTS] = { };
    struct ibv_recv_wr recv_wr2[NUM_CLIENTS] = { };
    struct ibv_recv_wr* bad_recv_wr[NUM_CLIENTS];
    struct ibv_wc wc;
    void* cq_context;
    struct sockaddr_in sin;
    int err;
    int i;

    init();

    /* Set up RDMA CM structures */

    cm_channel = rdma_create_event_channel();
    assert(cm_channel);

    err = rdma_create_id(cm_channel, &listen_id, 0, RDMA_PS_TCP);
    assert(err == 0);

    memset(&sin, 0x00, sizeof(sin));
    sin.sin_family = AF_INET;
    sin.sin_port = htons(server_port); 
    sin.sin_addr.s_addr = INADDR_ANY;

    /* Bind to local port and listen for connection request */

    err = rdma_bind_addr(listen_id, (struct sockaddr*)&sin);
    assert(err == 0);

    err = rdma_listen(listen_id, NUM_CLIENTS);
    assert(err == 0);

    for (i = 0; i < NUM_CLIENTS; ++i) {
        err = rdma_get_cm_event(cm_channel, &event[i]);
        assert(err == 0);
        assert(event[i]->event == RDMA_CM_EVENT_CONNECT_REQUEST);
        cm_id[i] = event[i]->id;
        rdma_ack_cm_event(event[i]);
        
        /* Create verbs objects now that we know which device to use */

        pd[i] = ibv_alloc_pd(cm_id[i]->verbs);
        assert(pd[i]);

        comp_chan[i] = ibv_create_comp_channel(cm_id[i]->verbs);
        assert(comp_chan[i]);
        
        cq[i] = ibv_create_cq(cm_id[i]->verbs, 10, 0, comp_chan[i], 0);
        assert(cq[i]);

        err = ibv_req_notify_cq(cq[i], 0);
        assert(err == 0);

        mr[i] = ibv_reg_mr(pd[i], data + DATA_SIZE / NUM_CLIENTS * i, 
                           DATA_SIZE / NUM_CLIENTS, 
                           IBV_ACCESS_LOCAL_WRITE | IBV_ACCESS_REMOTE_READ); 
        assert(mr[i]);

        mr2[i] = ibv_reg_mr(pd[i], output[i], DATA_SIZE / NUM_CLIENTS / COLUMN * ROW,
                            IBV_ACCESS_LOCAL_WRITE | IBV_ACCESS_REMOTE_READ | IBV_ACCESS_REMOTE_WRITE);
        assert(mr2[i]);

        mr3[i] = ibv_reg_mr(pd[i], decoded + DATA_SIZE / NUM_CLIENTS * i,
                            DATA_SIZE / NUM_CLIENTS,
                            IBV_ACCESS_LOCAL_WRITE | IBV_ACCESS_REMOTE_WRITE);
        assert(mr3[i]);

        qp_attr[i].cap.max_send_wr = 10;
        qp_attr[i].cap.max_send_sge = 10;
        qp_attr[i].cap.max_recv_wr = 10;
        qp_attr[i].cap.max_recv_sge = 10;
        qp_attr[i].send_cq = cq[i];
        qp_attr[i].recv_cq = cq[i];
        qp_attr[i].qp_type = IBV_QPT_RC;
        
        err = rdma_create_qp(cm_id[i], pd[i], &qp_attr[i]);
        assert(err == 0);

        /* Post receive for output before accepting connection */
        
        sge[i].addr = (uintptr_t)output[i];
        sge[i].length = DATA_SIZE / NUM_CLIENTS / COLUMN * ROW;
        sge[i].lkey = mr2[i]->lkey;

        recv_wr[i].sg_list = &sge[i];
        recv_wr[i].num_sge = 1;

        err = ibv_post_recv(cm_id[i]->qp, &recv_wr[i], &bad_recv_wr[i]);
        assert(err == 0);

        /* Post receive for decoded */
        sge2[i].addr = (uintptr_t)(decoded + DATA_SIZE / NUM_CLIENTS * i); 
        sge2[i].length = DATA_SIZE / NUM_CLIENTS;
        sge2[i].lkey = mr3[i]->lkey;

        recv_wr2[i].sg_list = &sge2[i];
        recv_wr2[i].num_sge = 1;

        err = ibv_post_recv(cm_id[i]->qp, &recv_wr2[i], &bad_recv_wr[i]);
        assert(err == 0);

        // TODO: maybe move to next loop?
        rep_pdata[i].index = i;
        rep_pdata[i].data_va = htonll((uintptr_t)(data + DATA_SIZE / NUM_CLIENTS * i));
        rep_pdata[i].data_rkey = htonl(mr[i]->rkey);
        rep_pdata[i].output_va = htonll((uintptr_t)output[i]);
        rep_pdata[i].output_rkey = htonl(mr2[i]->rkey);
        rep_pdata[i].decoded_va = htonll((uintptr_t)(decoded + DATA_SIZE / NUM_CLIENTS * i));
        rep_pdata[i].decoded_rkey = htonl(mr3[i]->rkey);

        conn_param[i].responder_resources = 1;
        conn_param[i].private_data = &rep_pdata[i];
        conn_param[i].private_data_len = sizeof(rep_pdata[i]);
    }

    for (i = 0; i < NUM_CLIENTS; ++i) {
        /* Accept connection */

        err = rdma_accept(cm_id[i], &conn_param[i]);
        assert(err == 0);

        err = rdma_get_cm_event(cm_channel, &event[i]);
        assert(err == 0);
        assert(event[i]->event == RDMA_CM_EVENT_ESTABLISHED);

        rdma_ack_cm_event(event[i]);
    }

    for (i = 0; i < NUM_CLIENTS; ++i) {
        /* Wait for receive completion */

        err = ibv_get_cq_event(comp_chan[i], &evt_cq[i], &cq_context);
        assert(err == 0);

        ibv_ack_cq_events(evt_cq[i], 1);

        err = ibv_req_notify_cq(cq[i], 0);
        assert(err == 0);

        err = ibv_poll_cq(cq[i], 1, &wc);
        assert(err >= 1);
        assert(wc.status == IBV_WC_SUCCESS);
    }


    for (i = 0; i < NUM_CLIENTS; ++i) {
        /* Wait for receive completion */

        err = ibv_get_cq_event(comp_chan[i], &evt_cq[i], &cq_context);
        assert(err == 0);

        ibv_ack_cq_events(evt_cq[i], 1);

        err = ibv_req_notify_cq(cq[i], 0);
        assert(err == 0);

        err = ibv_poll_cq(cq[i], 1, &wc);
        assert(err >= 1);
        assert(wc.status == IBV_WC_SUCCESS);
    }


    /* Release resources */

    for (i = 0; i < NUM_CLIENTS; ++i) {
        ibv_dereg_mr(mr[i]);
        ibv_dereg_mr(mr2[i]);
        ibv_dereg_mr(mr3[i]);
    }
    for (i = 0; i < NUM_CLIENTS; ++i)
        rdma_destroy_id(cm_id[i]);

    rdma_destroy_id(listen_id);
    rdma_destroy_event_channel(cm_channel);
    release();
}
