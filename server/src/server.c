#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <memory.h>
#include <netdb.h>
#include <unistd.h>
#include <fcntl.h>
#include <arpa/inet.h>
#include <sys/epoll.h>
#include <errno.h>
#include "connector.h"

#define MAXEVENTS 64

char defaultDir[50], defaultPort[10];

static int unblockSocket(int sockfd)
{
    int flags = 0;
    flags = fcntl(sockfd, F_GETFL, 0);
    if (flags == -1) {
        perror ("fcntl get");
        return -1;
    }
    flags |= O_NONBLOCK;
    flags = fcntl(sockfd, F_SETFL, flags);
    if (flags == -1) {
        perror("fcntl set");
        return -1;
    }
    return 0;
}

static int createAndBind(char *port)
{
    struct sockaddr_in addr;
    int sockfd = 0;
    if ((sockfd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) == -1) {
        printf("Error socket(): %s(%d)\n", strerror(errno), errno);
        return 1;
    }
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(c2i(defaultPort));
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    if (bind(sockfd, (struct sockaddr*)&addr, sizeof(addr)) == -1) {
		printf("Error bind(): %s(%d)\n", strerror(errno), errno);
		return 1;
    }
    return sockfd;
}

int main (int argc, char *argv[])
{
    int sockfd = 0, flag = 0;
    int efd = 0, i = 0;
    struct epoll_event event;
    struct epoll_event *events;
    Connector* connList = NULL;
    
    memset(defaultDir, '\0', sizeof(defaultDir));
    memset(defaultPort, '\0', sizeof(defaultPort));
    
    snprintf(defaultPort, sizeof(defaultPort) - 1, "%s", "21");
    snprintf(defaultDir, sizeof(defaultDir) - 1, "%s", "/tmp");

    for (i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "-port")) snprintf(defaultPort, sizeof(defaultPort) - 1, "%s", argv[++i]);
        else if (!strcmp(argv[i], "-root")) snprintf(defaultDir, sizeof(defaultDir) - 1, "%s", argv[++i]);
        else {
            printf("wrong arguments![%s]", argv[i]);
            return 1;
        }
    }

    printf("server on port[%s] at root[%s]\n", defaultPort, defaultDir);

    connList = createConnectorList();
    sockfd = createAndBind(argv[1]);
    if (sockfd == -1)
        abort();
    flag = unblockSocket(sockfd);
    if (flag == -1)
        abort();
    flag = listen(sockfd, SOMAXCONN);
    if (flag == -1) {
        perror("listen");
        abort();
    }
    efd = epoll_create1(0);
    if (efd == -1) {
        perror("epoll_create");
        abort();
    }
    
    event.data.fd = sockfd;
    event.events = EPOLLIN | EPOLLET;
    flag = epoll_ctl(efd, EPOLL_CTL_ADD, sockfd, &event);
    if (flag == -1) {
        perror("epoll_ctl");
        abort();
    }

    events = calloc(MAXEVENTS, sizeof event);

    while (1) {
        int n = 0, i = 0;
        n = epoll_wait(efd, events, MAXEVENTS, -1);
        for (i = 0; i < n; i++) {
            if ((events[i].events & EPOLLERR) ||
                (events[i].events & EPOLLHUP) ||
                (!(events[i].events & EPOLLIN))) {
                deleteConnector(connList, events[i].data.fd);
                continue;
            }
            else if (sockfd == events[i].data.fd) {
                /* New connections */
                while (1) {
                    struct sockaddr addr;
                    socklen_t len;
                    int connfd = 0;
                    char hbuf[NI_MAXHOST], sbuf[NI_MAXSERV];
                    
                    memset(hbuf, '\0', sizeof(hbuf));
                    memset(sbuf, '\0', sizeof(sbuf));
                    
                    len = sizeof addr;
                    connfd = accept(sockfd, &addr, &len);
                    if (connfd == -1) {
                        if ((errno == EAGAIN) ||
                            (errno == EWOULDBLOCK)) {
                            break;
                        }
                        else {
                            perror ("accept");
                            break;
                        }
                    }
                    flag = getnameinfo(&addr, len,
                                       hbuf, sizeof hbuf,
                                       sbuf, sizeof sbuf,
                                       NI_NUMERICHOST | NI_NUMERICSERV);
                    if (flag == 0) {
                        printf("Accepted connection on descriptor %d "
                               "(host=%s, port=%s)\n", connfd, hbuf, sbuf);
                    }
                    flag = unblockSocket(connfd);
                    if (flag == -1)
                        abort();
                    event.data.fd = connfd;
                    event.events = EPOLLIN | EPOLLET;
                    appendConnector(connList, connfd, defaultDir);
                    flag = write(connfd, S220, strlen(S220));
                    flag = epoll_ctl(efd, EPOLL_CTL_ADD, connfd, &event);
                    if (flag == -1) {
                        perror("epoll_ctl");
                        abort();
                    }
                }
                continue;
            }
            else {
                int done = 0;
                ssize_t count = 0;
                char buf[512];
                
                memset(buf, '\0', sizeof(buf));
                
                while (1) {
                    count = read(events[i].data.fd, buf, sizeof buf);
                    if (count == -1) {
                        if (errno != EAGAIN) {
                            perror("read");
                            done = 1;
                        }
                        break;
                    }
                    else if (count == 0) {
                        done = 1;
                        break;
                    }
                    buf[count] = '\0';
                    printf("%s", buf);
                }
                if (!done) {
                    responseClient(connList, events[i].data.fd, buf);
                }
                else {
                    deleteConnector(connList, events[i].data.fd);
                }
            }
        }
    }
    
    if(events != NULL)
    	free(events);
    close(sockfd);
    return EXIT_SUCCESS;
}
