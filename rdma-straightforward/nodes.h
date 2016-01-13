#ifndef NODES_H_
#define NODES_H_

#define NUM_CLIENTS (4)

const int server_port = 5000;
const char server_port_string[] = "5000";
const char server_ip[] = "10.0.0.4";

#define RESOLVE_TIMEOUT_MS (5000)
#define DATA_SIZE (1 << 27)
#define COLUMN (4)

#define NUM_BRICKS (COLUMN / 4 * 5)

const int brick_port[COLUMN / 4 * 5] = { 5011, 5012, 5013, 5014, 5015 };
const char* brick_port_string[COLUMN / 4 * 5] = { "5011", "5012", "5013", "5014", "5015" };
const char* brick_ip[COLUMN / 4 * 5] = { "10.0.0.4", "10.0.0.4", "10.0.0.4", "10.0.0.4", "10.0.0.4" };

struct rdma_private_data {
    int index;
    uint64_t data_va;
    uint64_t output_va;
    uint32_t data_rkey;
    uint32_t output_rkey;
};

#endif /* NODES_H_ */

