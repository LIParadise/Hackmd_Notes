//! https://web.archive.org/web/20120504033548/https://banu.com/blog/2/how-to-use-epoll-a-complete-example-in-c/

#include <errno.h>
#include <fcntl.h>
#include <memory.h>
#include <netdb.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#define MAXEVENTS 64

static int create_and_bind(char *port) {
    struct addrinfo hints;
    struct addrinfo *result, *rp;
    int status, socket_fd;

    memset(&hints, 0, sizeof(struct addrinfo));
    hints.ai_family = AF_UNSPEC;     /* Return IPv4 and IPv6 choices */
    hints.ai_socktype = SOCK_STREAM; /* We want a TCP socket */
    hints.ai_flags = AI_PASSIVE;     /* All interfaces */

    status = getaddrinfo(NULL, port, &hints, &result);
    if (status != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(status));
        return -1;
    }

    for (rp = result; rp != NULL; rp = rp->ai_next) {
        socket_fd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
        if (socket_fd == -1)
            continue;

        status = bind(socket_fd, rp->ai_addr, rp->ai_addrlen);
        if (status == 0) {
            /* We managed to bind successfully! */
            break;
        }

        close(socket_fd);
    }

    if (rp == NULL) {
        fprintf(stderr, "Could not bind\n");
        return -1;
    }

    freeaddrinfo(result);

    return socket_fd;
}

static int make_socket_non_blocking(int sfd) {
    int flags, status;

    flags = fcntl(sfd, F_GETFL, 0);
    if (flags == -1) {
        perror("fcntl");
        return -1;
    }

    flags |= O_NONBLOCK;
    status = fcntl(sfd, F_SETFL, flags);
    if (status == -1) {
        perror("fcntl");
        return -1;
    }

    return 0;
}

int main(int argc, char *argv[]) {
    int socket_fd, status;
    int epoll_fd;
    struct epoll_event event;
    struct epoll_event *events;

    if (argc != 2) {
        fprintf(stderr, "Usage: %s [port]\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    socket_fd = create_and_bind(argv[1]);
    if (socket_fd == -1) {
        abort();
    }

    status = make_socket_non_blocking(socket_fd);
    if (status == -1) {
        abort();
    }

    status = listen(socket_fd, SOMAXCONN);
    if (status == -1) {
        perror("listen");
        abort();
    }

    epoll_fd = epoll_create1(0);
    if (epoll_fd == -1) {
        perror("epoll_create");
        abort();
    }

    event.data.fd = socket_fd;
    event.events = EPOLLIN | EPOLLET;
    status = epoll_ctl(epoll_fd, EPOLL_CTL_ADD, socket_fd, &event);
    if (status == -1) {
        perror("epoll_ctl");
        abort();
    }

    /* Buffer where events are returned */
    events = calloc(MAXEVENTS, sizeof event);

    /* The event loop */
    while (1) {
        int num_events;

        num_events = epoll_wait(epoll_fd, events, MAXEVENTS, -1);
        for (int i = 0; i < num_events; i++) {
            if ((events[i].events & EPOLLERR) ||
                (events[i].events & EPOLLHUP) ||
                (!(events[i].events & EPOLLIN))) {
                /* An error has occured on this fd, or the socket is not
                   ready for reading (why were we notified then?) */
                fprintf(stderr, "epoll error\n");
                close(events[i].data.fd);
                continue;
            } else if (socket_fd == events[i].data.fd) {
                /* We have a notification on the listening socket, which
                   means one or more incoming connections. */
                while (1) {
                    struct sockaddr in_addr;
                    socklen_t in_addr_len;
                    int in_fd;
                    char host_buf[NI_MAXHOST], service_buf[NI_MAXSERV];

                    in_addr_len = sizeof in_addr;
                    in_fd = accept(socket_fd, &in_addr, &in_addr_len);
                    if (in_fd == -1) {
                        if ((errno == EAGAIN) || (errno == EWOULDBLOCK)) {
                            /* We have processed all incoming
                               connections. */
                        } else {
                            perror("accept");
                        }
                        break;
                    }

                    status = getnameinfo(&in_addr, in_addr_len, host_buf,
                                         sizeof host_buf, service_buf,
                                         sizeof service_buf,
                                         NI_NUMERICHOST | NI_NUMERICSERV);
                    if (status == 0) {
                        printf("Accepted connection on descriptor %d "
                               "(host=%s, port=%s)\n",
                               in_fd, host_buf, service_buf);
                    }

                    /* Make the incoming socket non-blocking and add it to the
                       list of fds to monitor. */
                    status = make_socket_non_blocking(in_fd);
                    if (status == -1) {
                        abort();
                    }

                    event.data.fd = in_fd;
                    event.events = EPOLLIN | EPOLLET;
                    status = epoll_ctl(epoll_fd, EPOLL_CTL_ADD, in_fd, &event);
                    if (status == -1) {
                        perror("epoll_ctl");
                        abort();
                    }
                }
                continue;
            } else {
                /*
                 * We have data on the fd waiting to be read.
                 * Read and display it.
                 *
                 * We must read whatever data is available completely,
                 * as we are running in edge-triggered mode
                 * thus won't get a notification again for the same data.
                 */
                bool done = false;

                while (1) {
                    ssize_t count;
                    char buf[512];

                    count = read(events[i].data.fd, buf, sizeof buf);
                    if (count == -1) {
                        /*
                         * If EAGAIN, we read all data.
                         * So go back to the main loop.
                         */
                        if (errno != EAGAIN) {
                            perror("read");
                            done = true;
                        }
                        break;
                    } else if (count == 0) {
                        /* End of file. The remote has closed the
                           connection. */
                        done = true;
                        break;
                    }

                    /* Write the buffer to standard output */
                    status = write(1, buf, count);
                    if (status == -1) {
                        perror("write");
                        abort();
                    }
                }

                if (done) {
                    printf("Closed connection on descriptor %d\n",
                           events[i].data.fd);

                    /* Closing the descriptor will make epoll remove it
                       from the set of descriptors which are monitored. */
                    close(events[i].data.fd);
                }
            }
        }
    }

    free(events);

    close(socket_fd);

    return EXIT_SUCCESS;
}
