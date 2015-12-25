#include "ec-method.h"
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <stdio.h>
#include <assert.h>
#include <sys/time.h>
#include <sys/sysinfo.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h> 
#include <errno.h>

#include "nodes.h"

uint8_t* decoded;
uint8_t* data;
uint8_t* output[COLUMN / 4 * 5];
uint32_t row[COLUMN / 4 * 5];

void init() {
    int i;
    decoded = (uint8_t*)malloc(DATA_SIZE / NUM_CLIENTS);
    data = (uint8_t*)malloc(DATA_SIZE / NUM_CLIENTS);

    for (i = 0; i < COLUMN / 4 * 5; ++i)
        output[i] = (uint8_t*)malloc(DATA_SIZE / NUM_CLIENTS / COLUMN), row[i] = i;
    ec_method_initialize();
}

int main(int argc, char** argv) {
    int i;
    size_t size;

    int sockfd;
    struct sockaddr_in server_addr;
    struct hostent* server;
    int ret;

    // init data
    init();

    // init network settings
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    assert(sockfd >= 0);
    server = gethostbyname(server_ip);
    assert(server);
    memset(&server_addr, 0x00, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    memcpy(&server_addr.sin_addr.s_addr, server->h_addr, server->h_length);
    server_addr.sin_port = htons(server_port);
    
    ret = connect(sockfd, (struct sockaddr*)&server_addr, sizeof(server_addr));
    assert(ret >= 0);

    // receive data from server
    ret = fully_read(sockfd, data, DATA_SIZE / NUM_CLIENTS);
    assert(ret == DATA_SIZE / NUM_CLIENTS);

    // encode
    if (argc > 1 && !strcmp(argv[1], "-p"))
        size = ec_method_batch_parallel_encode(DATA_SIZE / NUM_CLIENTS, COLUMN, COLUMN / 4 * 5, data, output, get_nprocs());
    else
        size = ec_method_batch_encode(DATA_SIZE / NUM_CLIENTS, COLUMN, COLUMN / 4 * 5, data, output);

    // decode
    if (argc > 1 && !strcmp(argv[1], "-p"))
        ec_method_parallel_decode(size, COLUMN, row, output, decoded, get_nprocs());
    else
        ec_method_decode(size, COLUMN, row, output, decoded);

    if (memcmp(data, decoded, DATA_SIZE / NUM_CLIENTS)) 
        printf("wrong! \n");
    else 
        printf("right. \n");

    // close socket
    close(sockfd);

    return 0;
}
