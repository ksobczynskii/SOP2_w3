#include "../common.h"


void prepr_data(char* arg1, char* arg2, char* operand, int32_t data[5]) {
    int32_t a = atoi(arg1);
    int32_t b = atoi(arg2);
    int32_t op = (int32_t)(operand[0]);
    data[0] = htonl(a);
    data[1] = htonl(b);
    data[3] = htonl(op);
}


int main(int argc, const char** argv) {
    if (argc!=5)
        usage("Wrong no. of arguments");

    int socket_fd = connect_local_socket(argv[1]);
    int32_t data[5];
    memset(data,0,sizeof(data));
    prepr_data(argv[2], argv[3], argv[4], data);
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