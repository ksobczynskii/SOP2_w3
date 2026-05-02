#include "../common.h"
#define MAX_EVENTS 10
#define BACKLOG_SIZE 5



volatile sig_atomic_t doWork = 1;

void calculate(int32_t data[5]) {
    int32_t a = ntohl(data[0]);
    int32_t b = ntohl(data[1]);
    int32_t result, status = 1;
    switch ((char)ntohl(data[3])) {
        case '+':
            result = a+b;
            break;
        case '-':
            result = a-b;
            break;
        case '*':
            result = a*b;
            break;
        case '/':
            if (b!=0)
                result=a/b;
            else {
                result = 0;
                status=0;
            }
            break;
        default:
            result = 0;
            status = 0;
            break;
    }
    data[2] = htonl(result);
    data[4] = htonl(status);
}

void stop_work(int sig) {
    doWork = 0;
}
void server_work(int tcp_listener, int local_listener) {
    struct epoll_event ev, events[MAX_EVENTS];
    int nfds, epoll_fd;

    if ((epoll_fd=epoll_create1(0)) < 0) ERR("epoll create");

    ev.events = EPOLLIN;
    ev.data.fd = tcp_listener;

    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, tcp_listener, &ev) < 0) ERR("epoll_ctl");

    ev.data.fd = local_listener;
    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, local_listener, &ev) < 0) ERR("epoll_ctl");

    int32_t data[5];
    ssize_t size;
    sigset_t mask, oldmask;
    sigemptyset(&mask);
    sigaddset(&mask, SIGINT);
    sigprocmask(SIG_BLOCK, &mask, &oldmask);
    while (1) {
        nfds = epoll_pwait(epoll_fd, events, MAX_EVENTS, -1, &oldmask);
        if (nfds < 0) ERR("epoll wait");

        for (int i=0; i<nfds; i++) {
            int client_fd = add_new_client(events[i].data.fd);

            if ((size = bulk_read(client_fd, (char*)data, sizeof(data))) < 0) ERR("read");
            if (size == (int)sizeof(int32_t[5])) {
                calculate(data);
                if (bulk_write(client_fd, (char*)data, sizeof(data)) < 0) ERR("bulk write");
            }

            if (TEMP_FAILURE_RETRY(close(client_fd)) < 0)
                ERR("close");
        }
    }

}
int main(int argc, const char** argv) {

    if (argc != 3)
        usage("Wrong number of parameters");

    const char* unix_port = argv[1];
    uint16_t tcp_port = atoi(argv[2]);

    sethandler(SIG_IGN, SIGPIPE);
    sethandler(stop_work, SIGINT);

    int tcp_listener = bind_tcp_socket(tcp_port, BACKLOG_SIZE);
    int local_listener = bind_local_socket(unix_port, BACKLOG_SIZE);

    int tcp_flags = fcntl(tcp_listener, F_GETFL)| O_NONBLOCK;
    fcntl(tcp_listener, F_SETFL, tcp_flags | O_NONBLOCK);

    int local_flags = fcntl(local_listener, F_GETFL) | O_NONBLOCK;
    fcntl(local_listener, F_SETFL, local_flags);

    server_work(tcp_listener, local_listener);
    if (TEMP_FAILURE_RETRY(close(local_listener)) < 0)
        ERR("close");
    if (unlink(argv[1]) < 0)
        ERR("unlink");
    if (TEMP_FAILURE_RETRY(close(tcp_listener)) < 0)
        ERR("close");
    return 0;

}