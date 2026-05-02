#include "../common.h"


void prepr_data(char* arg1, char* arg2, char* operand, int32_t data[5]) {
    int32_t a = atoi(arg1);
    int32_t b = atoi(arg2);
    int32_t op = (int32_t)(operand[0]);
    data[0] = htonl(a);
    data[1] = htonl(b);
    data[3] = htonl(op);
}


int main(int argc, char** argv) {
    if (argc < 4) usage("Too few arguments");

    int opt;
    int protocol = 0; // 0: local, 1: tcp

    while ((opt = getopt(argc, argv, "p:")) != -1) {
        switch (opt) {
            case 'p':
                if (strcmp(optarg, "tcp") == 0) protocol = 1;
                else if (strcmp(optarg, "local") == 0) protocol = 0;
                else usage("Wrong -p argument");
                break;
            default: usage("Usage: ...");
        }
    }

    int socket_fd;
    int next_arg = optind;

    if (protocol) {
        char* addr = argv[next_arg++];
        char* port = argv[next_arg++];
        socket_fd = connect_tcp_socket(addr, port);
    } else {
        char* path = argv[next_arg++];
        socket_fd = connect_local_socket(path);
    }

    if (argc - next_arg < 3) usage("Missing math arguments");

    int32_t data[5];
    memset(data, 0, sizeof(data));

    prepr_data(argv[next_arg], argv[next_arg + 1], argv[next_arg + 2], data);

    if (bulk_write(socket_fd, (char*)data, sizeof(int32_t[5])) < 0) ERR("write");
    if (bulk_read(socket_fd, (char*)data, sizeof(int32_t[5])) < 0) ERR("read");

    if (ntohl(data[4]) == 1) {
        printf("Received result %d\n", ntohl(data[2]));
    }
    else {
        printf("Error in the mathematical expression\n");
    }
    exit(EXIT_SUCCESS);
}