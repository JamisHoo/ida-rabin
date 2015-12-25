#ifndef NODES_H_
#define NODES_H_


#define NUM_CLIENTS (2)

int server_port = 5000;
char* server_ip = "127.0.0.1";


#define DATA_SIZE (1<<24)
#define COLUMN (64)

// read certain bytes from a fd
int fully_read(int socket, char* data, size_t total_size) {
    int bytes_read;
    int current_read;
    bytes_read = 0;

    while (bytes_read < total_size) {
        current_read = read(socket, data + bytes_read, total_size - bytes_read);
        if (current_read < 0) return current_read;
        bytes_read += current_read;
    }

    return bytes_read;
}


#endif /* NODES_H_ */
