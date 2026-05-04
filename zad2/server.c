#include "../common.h"

#define BACKLOG 3
#define MAX_EVENTS 10
#define MAX_MESSAGE 1024
#define ELECTORS 7

void print_epoll_events(uint32_t events) {
    printf("Zdarzenia (maska %u): ", events);

    if (events & EPOLLIN)      printf("EPOLLIN ");
    if (events & EPOLLOUT)     printf("EPOLLOUT ");
    if (events & EPOLLRDHUP)   printf("EPOLLRDHUP ");
    if (events & EPOLLHUP)     printf("EPOLLHUP ");
    if (events & EPOLLERR)     printf("EPOLLERR ");
    if (events & EPOLLET)      printf("EPOLLET ");

    printf("\n");
}

int registering(int fd, int registers[ELECTORS]) {
    for (int i=0; i<ELECTORS; i++) {
        if (registers[i]==fd)
            return 0;
    }
    return 1;
}

void disconnect_client(int fd,int epoll_fd, char* message) {
    if (message != NULL)
        bulk_write(fd,message, strlen(message));
    epoll_ctl(epoll_fd, EPOLL_CTL_DEL, fd, NULL);
    close(fd);
}

void disconnect_client_insant(int fd,int epoll_fd) {
    epoll_ctl(epoll_fd, EPOLL_CTL_DEL, fd, NULL);
    close(fd);
}

void send_welcome_message(int fd,int i, int epoll_fd) {
    char* message = NULL;
    switch (i) {
        case 1:
            message = "Welcome, elector of Mogucncja!\n";
            break;
        case 2:
            message = "Welcome, elector of Trewir!\n";
            break;
        case 3:
            message = "Welcome, elector of Kolonia!\n";
            break;
        case 4:
            message = "Welcome, elector of Czechy!\n";
            break;
        case 5:
            message = "Welcome, elector of Palatynat!\n";
            break;
        case 6:
            message = "Welcome, elector of Saksonia!\n";
            break;
        case 7:
            message = "Welcome, elector of Brandenburgia!\n";
            break;
        default:
            message = "ERROR, SHOULDNT SEE THAT MESSAGE";
            break;
    }
    if (bulk_write(fd,message, strlen(message)) < 0) {
        disconnect_client_insant(fd, epoll_fd);
    }
}
void voted_for_message(int fd,int  epoll_fd,int nr) {
    char* message = NULL;
    switch (nr) {
        case 1:
            message = "Voted for Franciszek I, król Francji\n";
            break;
        case 2:
            message = "Voted for Karol V, arcyksiążę Austrii i król Hiszpanii\n";
            break;
        case 3:
            message = "Voted for Henryk VIII, król Anglii\n";
            break;
        default:
            message = "Not supposed to be seen!!!!\n";
            break;

    }
    if (bulk_write(fd,message, strlen(message)) < 0) {
        disconnect_client_insant(fd, epoll_fd);
    }
}



int voting(int fd, int registers[ELECTORS]) {
    for (int i=0;i<ELECTORS;i++) {
        if (registers[i]==fd)
            return i;
    }
    return -1;
}

void vote_again_msg(int fd, int epoll_fd) {
    char* message = "Invalid Vote!\n";
    if (bulk_write(fd,message, strlen(message)) < 0) {
        disconnect_client_insant(fd, epoll_fd);
    }
}
void server_work(int server_fd) {
    int epoll_fd = epoll_create1(0);
    if (epoll_fd < 0) ERR("epoll create");

    struct epoll_event ev, events[MAX_EVENTS];

    ev.events = EPOLLIN; // | EPOLLRDHUP | EPOLLHUP | EPOLLERR; // nie trzeba dpdawac

    //question co to dokladnie epollin?

    ev.data.fd = server_fd;

    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, server_fd, &ev) < 0) ERR("epoll ctl");

    int registers[ELECTORS] = {0};
    int votes[ELECTORS] =  {0};
    for (int i=0; i<ELECTORS; i++) {
        votes[i] = -1;
        registers[i] = -1;
    }



    int nfds;
    // char* welcome_message = "Welcome, elector!\n";
    ssize_t bytes;
    int pos;

    char buf[MAX_MESSAGE];


    while (1) { // TODO - uaktualniaj arraye jak ktos wyjdzie

        nfds = epoll_wait(epoll_fd,events,MAX_EVENTS,-1);
        if (nfds < 0)
            ERR("epoll wait");
        for (int i = 0; i < nfds; i++) {
            uint32_t e = events[i].events;
            int fd = events[i].data.fd;
            if (fd == server_fd) {
                int client_fd = add_new_client(server_fd);
                if (client_fd >= 0) {
                    fcntl(client_fd, F_SETFL, fcntl(client_fd, F_GETFL) | O_NONBLOCK);
                    ev.events = EPOLLIN;
                    ev.data.fd = client_fd;
                    epoll_ctl(epoll_fd, EPOLL_CTL_ADD, client_fd, &ev);
                    // bulk_write(client_fd, welcome_message, strlen(welcome_message));
                    // printf("[%d] Joined the call\n", client_fd);
                }
            }
            else {
                if (e & EPOLLIN) {
                    if (registering(fd, registers)) {
                        int32_t nr;
                        bytes = bulk_read(fd, buf, MAX_MESSAGE);
                        if (bytes >0) {
                            // printf("Buf is: %s\n", buf);
                            nr = atoi(buf);
                            // printf("Nr. is: '%d'\n", nr);
                            if (nr <= 7 && nr >= 1) {
                                if (registers[nr-1]==-1) {
                                    registers[nr-1] = fd;
                                    send_welcome_message(fd,nr, epoll_fd);
                                }
                                else {
                                    // printf("registers[%d] == %d\n", nr - 1, registers[nr-1]);
                                    disconnect_client(fd, epoll_fd, "Someone Already Registered As This Voter\n");
                                }
                            }
                            else {
                                disconnect_client(fd,epoll_fd,"Wrong Country Selected!\n");
                            }
                        }
                        else if (bytes==0) {
                            // printf("[%d] Rozłączono.\n", fd);
                            epoll_ctl(epoll_fd, EPOLL_CTL_DEL, fd, NULL);
                            close(fd);
                        }
                    }
                    else if ((pos = voting(fd, registers)) >= 0) {
                        int32_t nr;
                        bytes = bulk_read(fd, buf, MAX_MESSAGE);
                        if (bytes >0) {
                            nr =ntohl(atoi(buf));
                            // printf("NR == %d\n", nr);
                            nr = ntohl(nr);
                            if (nr <= 3 && nr >= 1) {
                                votes[pos] = nr;
                                voted_for_message(fd, epoll_fd, nr);
                            }
                            else {
                                vote_again_msg(fd,epoll_fd);
                            }
                        }
                        else if (bytes==0) {
                            printf("[%d] Rozłączono.\n", pos+1);
                            epoll_ctl(epoll_fd, EPOLL_CTL_DEL, fd, NULL);
                            close(fd);
                        }
                    }
                    else {
                        disconnect_client(fd,epoll_fd,"Unknown Command!\n");
                    }
                    // bytes = bulk_read(fd, buf, MAX_MESSAGE);
                    // if (bytes > 0) {
                    //     printf("[%d]: %s", fd, buf);
                    // } else if (bytes == 0) {
                    //     printf("[%d] Rozłączono.\n", fd);
                    //     epoll_ctl(epoll_fd, EPOLL_CTL_DEL, fd, NULL);
                    //     close(fd);
                    // }
                }
            }
        }
    }





}

int main(int argc, const char** argv) {
    if (argc !=2)
        usage("No. of arguments");

    int socket_fd = bind_tcp_socket(atoi(argv[1]), BACKLOG);
    if (socket_fd < 0)
        ERR("binding socket");

    sethandler(SIG_IGN, SIGPIPE);

    //QUESTION czy musze zmieniac deskryptor socketa na nieblokujacy?

    int flags = fcntl(socket_fd, F_GETFL) | O_NONBLOCK;
    fcntl(socket_fd, F_SETFL, flags);

    server_work(socket_fd);

    //
    // int client_fd = accept(socket_fd, NULL, NULL);
    // printf("Klient Polaczony\n");
    //
    //
    // if (close(client_fd) < 0)
    //     ERR("close");
    if (close(socket_fd) < 0)
        ERR("close");
}