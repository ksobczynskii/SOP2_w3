#include "../common.h"

int main(int argc, char** argv) {
    if (argc != 3)
        usage("Usage: ./client <address> <port>");

    int16_t my_pid = (int16_t)getpid();
    printf("My pid is %d\n", my_pid);

    int socket_fd = connect_tcp_socket(argv[1], argv[2]);
    if (socket_fd < 0) ERR("connect");

    int16_t net_pid = htons(my_pid);

    if (bulk_write(socket_fd, (char*)&net_pid, sizeof(int16_t)) < 0)
        ERR("write");

    int16_t net_sum = 0;
    if (bulk_read(socket_fd, (char*)&net_sum, sizeof(int16_t)) < 0)
        ERR("read");

    // 4. Konwersja wyniku (Network to Host Short)
    printf("Sum of pid digits is: %d\n", ntohs(net_sum));

    close(socket_fd);
    exit(EXIT_SUCCESS);
}