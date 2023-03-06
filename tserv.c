#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>
#include <sys/wait.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <fcntl.h>

#include "sockbuf.h"

#define LISTEN_PORT "5000"

void handle_client(int clientfd, int clienttype);

// Print the last error message corresponding to errno.
void print_err(char *s) {
    fprintf(stderr, "%s: %s\n", s, strerror(errno));
}
void exit_err(char *s) {
    print_err(s);
    exit(1);
}

// Return sin_addr or sin6_addr depending on address family.
void *addrinfo_sin_addr(struct addrinfo *addr) {
    // addr->ai_addr is either struct sockaddr_in* or sockaddr_in6* depending on ai_family
    if (addr->ai_family == AF_INET) {
        struct sockaddr_in *p = (struct sockaddr_in*) addr->ai_addr;
        return &(p->sin_addr);
    } else {
        struct sockaddr_in6 *p = (struct sockaddr_in6*) addr->ai_addr;
        return &(p->sin6_addr);
    }
}

void handle_sigint(int sig) {
    printf("SIGINT received\n");
    fflush(stdout);
    exit(0);
}

void handle_sigchld(int sig) {
    int tmp_errno = errno;
    while (waitpid(-1, NULL, WNOHANG) > 0) {
    }
    errno = tmp_errno;
}

int main(int argc, char *argv[]) {
    int z;

    signal(SIGINT, handle_sigint);
    signal(SIGCHLD, handle_sigchld);

    // Get this server's address.
    struct addrinfo hints, *servaddr;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;
    z = getaddrinfo("localhost", LISTEN_PORT, &hints, &servaddr);
    if (z != 0) {
        exit_err("getaddrinfo()");
    }

    // Get human readable IP address string in servipstr.
    char servipstr[INET6_ADDRSTRLEN];
    const char *pz = inet_ntop(servaddr->ai_family, addrinfo_sin_addr(servaddr), servipstr, sizeof(servipstr));
    if (pz ==NULL) {
        exit_err("inet_ntop()");
    }

    int s0 = socket(servaddr->ai_family, servaddr->ai_socktype, servaddr->ai_protocol);
    if (s0 == -1) {
        exit_err("socket()");
    }

    z = bind(s0, servaddr->ai_addr, servaddr->ai_addrlen);
    if (z != 0) {
        exit_err("bind()");
    }

    freeaddrinfo(servaddr);
    servaddr = NULL;

    z = listen(s0, 5);
    if (z != 0) {
        exit_err("listen()");
    }
    printf("Listening on %s:%s...\n", servipstr, LISTEN_PORT);

    fd_set readfds;
    FD_ZERO(&readfds);
    FD_SET(s0, &readfds);
    int maxfd = s0;

    while (1) {
        // readfds contain the master list of read sockets
        fd_set fds = readfds;
        z = select(maxfd+1, &fds, NULL, NULL, NULL);
        if (z == -1) {
            exit_err("select()");
        }
        if (z == 0) {
            // timeout returned
            continue;
        }

        // fds now contain list of clients with data available to be read.
        for (int i=0; i <= maxfd; i++) {
            if (!FD_ISSET(i, &fds)) {
                continue;
            }

            // New client connection
            if (i == s0) {
                struct sockaddr_in a;
                socklen_t a_len = sizeof(a);
                int newclient = accept(s0, (struct sockaddr*)&a, &a_len);
                if (newclient == -1) {
                    print_err("accept()");
                    continue;
                }

                // Add new client socket to list of read sockets.
                FD_SET(newclient, &readfds);
                if (newclient > maxfd) {
                    maxfd = newclient;
                }
                printf("New client fd: %d\n", newclient);
                continue;
            }

            // i contains client socket with data available to be read.
            int clientfd = i;
            handle_client(clientfd, 0);

            FD_CLR(clientfd, &readfds);
            close(clientfd);
        }
    }

    // Code doesn't reach here.
    close(s0);
    return 0;
}

void handle_client(int clientfd, int clienttype) {
    char line[1000];
    char buf[50];

//    fcntl(clientfd, F_SETFL, O_NONBLOCK);
    sockbuf_t *sb = sockbuf_new(clientfd, 4);

    while (1) {
        memset(line, '-', sizeof line);
        memset(buf, '-', sizeof buf);

        if (clienttype == 0) {
            int nchars = sockbuf_readline(sb, line, sizeof line);
            if (nchars == 0) break;
            printf("readline(): %s\n", line);
        } else {
            int nchars = sockbuf_read(sb, buf, sizeof(buf)-1);
            if (nchars == 0) break;
            buf[nchars] = '\0';
            printf("read(): %s\n", buf);
        }
    }

    printf("EOF\n");
    sockbuf_free(sb);
}



