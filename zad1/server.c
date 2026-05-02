#include "../common.h"
#define BACKLOG 3
#define MAX_EVENTS 10
#define MAX_BUF 20
volatile sig_atomic_t do_work = 1;


void stop_working(int sig) {
    do_work = 0;
}

int16_t calculate_sum(int16_t pid) {
    char buf[MAX_BUF] = {0};
    snprintf(buf,MAX_BUF,"%d", pid);
    int16_t sum = 0;

    for (int i=0; i<strlen(buf); i++) {
        sum += (buf[i] - '0');
    }
    return sum;
}
void server_work(int server_fd) {
    int epoll_fd = epoll_create1(0);
    if (epoll_fd<0)
        ERR("epoll_create");

    struct epoll_event ev, events[MAX_EVENTS];

    ev.events = EPOLLIN;

    ev.data.fd = server_fd;

    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, server_fd,&ev) < 0) ERR("epoll ctl");
    int nfds;
    int16_t pid=0, sum=0;
    ssize_t size;

    sigset_t mask, oldmask;
    sigemptyset(&mask);
    sigaddset(&mask, SIGINT);
    sigprocmask(SIG_BLOCK, &mask, &oldmask);

    while (do_work) {
        nfds = epoll_pwait(epoll_fd,events,MAX_EVENTS,-1,&oldmask);
        if (nfds < 0)
            ERR("epoll_pwait");
        for (int n=0; n<nfds; n++) {
            int client_fd = add_new_client(server_fd);

            size = bulk_read(client_fd,(char*)&pid, sizeof(int16_t));
            if (size<0)
                ERR("read");
            if (size==sizeof(int16_t)) {
                sum = htons(calculate_sum(ntohs(pid)));
                if (bulk_write(client_fd, (char*)&sum, sizeof(int16_t)) < 0) ERR("write");
            }
            if (TEMP_FAILURE_RETRY(close(client_fd)) < 0)
                ERR("close");
        }
    }
}
int main(int argc, const char** argv) {
    if (argc!=2)
        usage("no. of args");

    // sethandler(SIG_IGN, SIGPIPE);
    sethandler(stop_working, SIGINT);
    int listener_fd = bind_tcp_socket(atoi(argv[1]), BACKLOG);
    int tcp_flags = fcntl(listener_fd, F_GETFL)| O_NONBLOCK;
    fcntl(listener_fd, F_SETFL, tcp_flags | O_NONBLOCK);
    // czy nieblokujacy??
    server_work(listener_fd);

    if (close(listener_fd) < 0) ERR("close");

    exit(EXIT_SUCCESS);

}