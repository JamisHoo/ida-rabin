#ifndef NODES_H_
#define NODES_H_

#define NUM_CLIENTS (2)

int server_port = 5000;
const char* server_port_string = "5000";
const char* server_ip = "127.0.0.1";

#define RESOLVE_TIMEOUT_MS (5000)
#define DATA_SIZE (1<<24)
#define COLUMN (64)

struct rdma_private_data {
    int index;
    uint64_t data_va;
    uint64_t output_va;
    uint64_t decode_va;
    uint32_t data_rkey;
    uint32_t output_rkey;
    uint32_t decode_rkey;
};

#endif /* NODES_H_ */

