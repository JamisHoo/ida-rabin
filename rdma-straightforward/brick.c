#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <stdint.h>
#include <string.h>
#include <arpa/inet.h>
#include <infiniband/arch.h>
#include <rdma/rdma_cma.h>

#include "nodes.h"
#include "timer.h"

uint8_t* output[NUM_CLIENTS];

void init() {
    int i;

    output[0] = (uint8_t*)malloc(DATA_SIZE / COLUMN);

    for (i = 0; i < NUM_CLIENTS; ++i)
        output[i] = output[0] + DATA_SIZE / NUM_CLIENTS / COLUMN * i;
}

void on_get_output(int my_index) {
    size_t size;

    char filename[128];
    sprintf(filename, "output%d", my_index);
    FILE* pfile = fopen(filename, "wb");
    size = fwrite(output[0], DATA_SIZE / COLUMN, 1, pfile);
    assert(size == 1);
    fclose(pfile);
}

void release() {
    free(output[0]);
}

int main(int argc, char** argv) {
    
    assert(argc == 2);
    int my_index = atoi(argv[1]);
    assert(my_index >= 0 && my_index < NUM_BRICKS);

    struct rdma_private_data brick_pdata[NUM_CLIENTS];
    struct rdma_private_data client_pdata;

    struct rdma_event_channel* cm_channel;
    struct rdma_cm_id* listen_id;
    struct rdma_cm_id* cm_id[NUM_CLIENTS];
    struct rdma_cm_event* event[NUM_CLIENTS];
    struct rdma_conn_param conn_param[NUM_CLIENTS] = { };
    struct ibv_pd* pd[NUM_CLIENTS];
    struct ibv_comp_channel* comp_chan[NUM_CLIENTS];
    struct ibv_cq* cq[NUM_CLIENTS];
    struct ibv_cq* evt_cq[NUM_CLIENTS];
    struct ibv_mr* mr_output[NUM_CLIENTS];
    struct ibv_qp_init_attr qp_attr[NUM_CLIENTS] = { };
    struct ibv_sge sge_output[NUM_CLIENTS];
    struct ibv_recv_wr recv_wr[NUM_CLIENTS] = { };
    struct ibv_recv_wr* bad_recv_wr[NUM_CLIENTS];
    struct ibv_wc wc;
    void* cq_context;
    struct sockaddr_in sin;
    int i, n, err;
    
    init();

    /* Set up RDMA CM structures */

    cm_channel = rdma_create_event_channel();
    assert(cm_channel);

    err = rdma_create_id(cm_channel, &listen_id, 0, RDMA_PS_TCP);
    assert(err == 0);

    memset(&sin, 0x00, sizeof(sin));
    sin.sin_family = AF_INET;
    printf("brick %d port %d  \n", my_index, brick_port[my_index]);
    sin.sin_port = htons(brick_port[my_index]); 
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
        memcpy(&client_pdata, event[i]->param.conn.private_data, sizeof(client_pdata));
        n = client_pdata.index;
        printf("Brick %d and Client %d connected. \n", my_index, n);
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

        mr_output[i] = ibv_reg_mr(pd[i], output[n], DATA_SIZE / NUM_CLIENTS / COLUMN,
                                  IBV_ACCESS_LOCAL_WRITE | IBV_ACCESS_REMOTE_WRITE);
        assert(mr_output[i]);

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
        
        sge_output[i].addr = (uintptr_t)output[n];
        sge_output[i].length = DATA_SIZE / NUM_CLIENTS / COLUMN;
        sge_output[i].lkey = mr_output[i]->lkey;

        recv_wr[i].sg_list = &sge_output[i];
        recv_wr[i].num_sge = 1;

        err = ibv_post_recv(cm_id[i]->qp, &recv_wr[i], &bad_recv_wr[i]);
        assert(err == 0);

        /* Construct connection params */

        brick_pdata[i].index = my_index;
        brick_pdata[i].output_va = htonll((uintptr_t)output[n]);
        brick_pdata[i].output_rkey = htonl(mr_output[i]->rkey);

        conn_param[i].responder_resources = 1;
        conn_param[i].private_data = &brick_pdata[i];
        conn_param[i].private_data_len = sizeof(brick_pdata[i]);
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
        /* Wait for receive completon */
        err = ibv_get_cq_event(comp_chan[i], &evt_cq[i], &cq_context);
        assert(err == 0);

        ibv_ack_cq_events(evt_cq[i], 1);

        err = ibv_req_notify_cq(cq[i], 0);
        assert(err == 0);

        n = ibv_poll_cq(cq[i], 1, &wc);
        assert(n >= 1);
        assert(wc.status == IBV_WC_SUCCESS);
    }

    on_get_output(my_index);

    for (i = 0; i < NUM_CLIENTS; ++i) {
        ibv_dereg_mr(mr_output[i]);
        rdma_destroy_id(cm_id[i]);
    }
    rdma_destroy_id(listen_id);
    rdma_destroy_event_channel(cm_channel);
    release();
}

