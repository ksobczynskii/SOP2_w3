#include "../common.h"
#define BACKLOG 5
#define BUF_SIZE 100
#define MAX_EVENTS 100
#define MAX_CLIENTS 4
#define CITY_COUNT 20
#define MSG_SIZE 4

volatile sig_atomic_t do_work = 1;


int try_add_client(int clients[MAX_CLIENTS],int client_fd) {
    for (int i=0; i<MAX_CLIENTS; i++) {
        if (clients[i] == client_fd)
            return 0;
    }
    for (int i=0; i<MAX_CLIENTS; i++) {
        if (clients[i] == -1) {
            clients[i] = client_fd;
            return 1;
        }
    }
    return 0;
}
void disconnect_client_insant(int fd,int epoll_fd, int clients[MAX_CLIENTS]) {
    for (int i=0; i<MAX_CLIENTS; i++) {
        if (clients[i] == fd)
            clients[i] = -1;
    }
    printf("Disconnecting %d\n", fd);
    epoll_ctl(epoll_fd, EPOLL_CTL_DEL, fd, NULL);
    close(fd);
}
void broadcast_change(int clients[MAX_CLIENTS], int from, char message[MSG_SIZE], int epoll_fd) {
    printf("STARTING TO ITERATE...\n");
    for (int i=0; i<MAX_CLIENTS; i++) {
        printf("clients[%d] = %d\n", i, clients[i]);
        if (clients[i] == -1 || clients[i] == from)
            continue;
        printf("SENDING: %s to everyone!\n", message);
        ssize_t bytes = bulk_write(clients[i], message, strlen(message));
        if (bytes < 0) {
            if (errno == EPIPE) {
                disconnect_client_insant(from,epoll_fd, clients);
            }
            ERR("write");
        }

    }
}
void manage_message(char buf[MSG_SIZE], int cities[CITY_COUNT], int clients[MAX_CLIENTS], int from, int epoll_fd) {
    // printf("MANAGING MESSAGE...\n");
    switch (buf[0]) {
        case 'g': {
            int city = atoi((buf+1));
            if (city <1 || city > 20) {
                disconnect_client_insant(from,epoll_fd,clients);
                return;

            }
            if (cities[city-1] == 1) {
                cities[city-1] = 0;
                broadcast_change(clients, from, buf,epoll_fd);
            }
            break;
        }
        case 'p': {
            // printf("P! ...\n");
            int city = atoi((buf+1));
            // printf("CITY = %d\n", city);
            if (city < 1 || city > 20) {
                disconnect_client_insant(from,epoll_fd,clients);
            }
            // printf("Checking state...\n");
            if (cities[city-1] == 0) {
                // printf("CHANGES COMING...\n");
                cities[city-1] = 1;
                broadcast_change(clients, from, buf,epoll_fd);
            }
            break;
        }
        default:
            break;
    }
}

void print_cities(int cities[CITY_COUNT]) {
    for (int i=0; i<CITY_COUNT; i++) {
        if (cities[i]==0)
            printf("City %d belongs to the greeks!\n",i+1);
        else if (cities[i] ==1)
            printf("City %d belongs to the persians!\n",i+1);
    }
}



void server_work(int server_fd) {

    int epoll_fd = epoll_create1(0);
    if (epoll_fd < 0) ERR("epoll create");

    struct epoll_event ev, events[MAX_EVENTS];

    ev.data.fd = server_fd;
    ev.events = EPOLLIN | EPOLLET;

    if (epoll_ctl(epoll_fd,EPOLL_CTL_ADD, server_fd, &ev) < 0) ERR("epoll ctl");

    int nfds;
    char buf[MSG_SIZE];
    ssize_t bytes;
    int clients[MAX_CLIENTS] = {0};

    for (int i=0; i<MAX_CLIENTS; i++) {
        clients[i] = -1;
    }

    int cities[CITY_COUNT] = {0};
    for (int i=0; i<CITY_COUNT; i++) {
        cities[i] = 0;
    }


    sigset_t mask, oldmask;
    sigemptyset(&mask);
    sigaddset(&mask, SIGINT);
    sigprocmask(SIG_BLOCK, &mask, &oldmask);

    while (do_work) {

        nfds = epoll_pwait(epoll_fd, events, MAX_EVENTS, -1, &oldmask);
        if (nfds < 0) {
            if (errno == EINTR)
                continue;
            ERR("epoll_pwait");
        }
        for (int n=0; n<nfds; n++) {

            if (events[n].data.fd == server_fd) {
                int client_fd = add_new_client(server_fd);


                if (!try_add_client(clients,client_fd) ){
                    if (close(client_fd) < 0)
                        ERR("close");
                    continue;
                }
                //
                int flags = fcntl(client_fd, F_GETFL) | O_NONBLOCK;
                fcntl(client_fd, F_SETFL, flags);

                ev.data.fd = client_fd;
                ev.events = EPOLLIN | EPOLLOUT | EPOLLET;
                if (epoll_ctl(epoll_fd,EPOLL_CTL_ADD, client_fd, &ev) < 0) ERR("epoll ctl");
                printf("[%d]: Client connected\n", client_fd);
            }
            else {
                if (events[n].events & EPOLLIN) {
                    bytes = bulk_read(events[n].data.fd,buf,sizeof(buf));

                    if (bytes > 0) {
                        // printf("[%d] Client Sent: %s\n", events[n].data.fd, buf);
                        if (bytes != MSG_SIZE)
                            continue;
                        manage_message(buf, cities, clients, events[n].data.fd,epoll_fd);
                    }
                    else {
                        disconnect_client_insant(events[n].data.fd, epoll_fd, clients);
                    }
                    // else if (bytes==0){
                    //     disconnect_client_insant(events[n].data.fd, epoll_fd);
                    // }
                }
            }
        }
    }
    if (TEMP_FAILURE_RETRY(close(epoll_fd)) < 0)
        ERR("close");
    for (int i=0; i<MAX_CLIENTS; i++) {
        if (clients[i] != -1) {
            if (TEMP_FAILURE_RETRY(close(clients[i])) < 0)
                ERR("close");
        }
    }
    print_cities(cities);

    sigprocmask(SIG_UNBLOCK, &mask, NULL);

}

void set_flag(int sig) {
    do_work = 0;
}

int main(int argc, const char** argv) {

    if (argc != 2)
        usage("Wrong no. of arguments");

    int server_fd = bind_tcp_socket(atoi(argv[1]), BACKLOG);
    if (server_fd < 0)
        ERR("bind");

    int flags = fcntl(server_fd, F_GETFL) | O_NONBLOCK;
    fcntl(server_fd,F_SETFL, flags);

    sethandler(SIG_IGN, SIGPIPE);
    sethandler(set_flag, SIGINT);

    server_work(server_fd);

    // struct sockaddr addr;

    //ETAP 1
    // int client_fd = -1;
    // while (client_fd==-1)
    //     client_fd= add_new_client(server_fd);
    //
    // // if (client_fd < 0)
    // //     ERR("accept");
    // printf("Client Connected!\n");
    //
    // ssize_t bytes;
    // char buf[4];
    //
    // bytes = bulk_read(client_fd, buf, 4);
    //
    // if (bytes < 0) ERR("read");
    //
    // printf("%s\n", buf);
    // // fflush(stdout);
    //
    // if (close(client_fd) < 0) ERR("close");
    //
    if (close(server_fd) < 0) ERR("close");
    //
    exit(EXIT_SUCCESS);


}