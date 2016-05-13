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

#include "nodes.h"
#include "timer.h"

uint8_t* decoded;
uint8_t* data;
uint8_t* output[ROW];
uint32_t row[ROW];

void init() {
    int i;
    decoded = (uint8_t*)malloc(DATA_SIZE);
    data = (uint8_t*)malloc(DATA_SIZE);

    for (i = 0; i < ROW; ++i)
        output[i] = (uint8_t*)malloc(DATA_SIZE / COLUMN), row[i] = i;

    ec_method_initialize();
    

    for (i = 0; i < DATA_SIZE; ++i)
        data[i] = rand() % 256;
}

int main(int argc, char** argv) {
    int i, j;
    size_t size;

    int server_sock_fd;
    int client_sock_fd[NUM_CLIENTS];
    struct sockaddr_in server_addr, client_addr[NUM_CLIENTS];
    int client_addr_length;
    int ret;

    double start_time, total_time;

    assert(DATA_SIZE % NUM_CLIENTS == 0);
    assert(DATA_SIZE / NUM_CLIENTS % COLUMN == 0);

    // init data
    init();
    printf("Finish init. \n");

    // init network settings
    server_sock_fd = socket(AF_INET, SOCK_STREAM, 0);
    assert(server_sock_fd >= 0);
    
    int enable = 1;
    ret = setsockopt(server_sock_fd, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int));
    assert(ret >= 0);
    
    memset((void*)&server_addr, 0x00, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(server_port);

    ret = bind(server_sock_fd, (struct sockaddr*)&server_addr, sizeof(server_addr));
    assert(ret >= 0);
    listen(server_sock_fd, NUM_CLIENTS);
    
    // build connections
    client_addr_length = sizeof(client_addr[i]);
    for (i = 0; i < NUM_CLIENTS; ++i) {
        client_sock_fd[i] = accept(server_sock_fd, (struct sockaddr*)(client_addr + i), &client_addr_length);
        assert(client_sock_fd[i] >= 0);
        printf("%d client connected. \n", i);
    }
    
    total_time = start_time = timer_start();

    // send data
    for (i = 0; i < NUM_CLIENTS; ++i) {
        ret = send(client_sock_fd[i], data + i * DATA_SIZE / NUM_CLIENTS, DATA_SIZE / NUM_CLIENTS, 0);
        assert(ret == DATA_SIZE / NUM_CLIENTS);
    }

    timer_end(start_time, "Send initial data to clients: %lfs \n");

    // receive encoded data from clients
    for (i = 0; i < NUM_CLIENTS; ++i) 
        for (j = 0; j < ROW; ++j) {
            ret = recv(client_sock_fd[i], 
                       output[j] + DATA_SIZE / NUM_CLIENTS / COLUMN * i, 
                       DATA_SIZE / NUM_CLIENTS / COLUMN,
                       MSG_WAITALL);
            assert(ret == DATA_SIZE / NUM_CLIENTS / COLUMN);

            if (!i && !j) start_time = timer_start();
        }

    start_time = timer_end(start_time, "Receive encoded data from clients: %lfs \n");
    total_time = timer_end(total_time, "Encode total time: %lfs \n");

    // send encoded data to clients
    for (i = 0; i < NUM_CLIENTS; ++i)
        for (j = 0; j < ROW; ++j) {
            ret = send(client_sock_fd[i],
                       output[j] + DATA_SIZE / NUM_CLIENTS / COLUMN * i,
                       DATA_SIZE / NUM_CLIENTS / COLUMN,
                       0);
            assert(ret == DATA_SIZE / NUM_CLIENTS / COLUMN);
        }

    timer_end(start_time, "Send encoded data to clients: %lfs \n");

    // receive decoded data from clients
    for (i = 0; i < NUM_CLIENTS; ++i) {
        ret = recv(client_sock_fd[i], decoded + i * DATA_SIZE / NUM_CLIENTS, DATA_SIZE / NUM_CLIENTS, MSG_WAITALL);
        assert(ret == DATA_SIZE / NUM_CLIENTS);
        if (!i) start_time = timer_start();
    }

    timer_end(start_time, "Receive decoded data from clients: %lfs \n");
    timer_end(total_time, "Decde total time: %lfs \n");

    if (memcmp(data, decoded, DATA_SIZE)) 
        printf("wrong! \n");
    else 
        printf("right. \n");

    // close sockets
    close(server_sock_fd);
    for (i = 0; i < NUM_CLIENTS; ++i)
        close(client_sock_fd[i]);

    return 0;
}
