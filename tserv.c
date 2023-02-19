#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <netinet/in.h>

#include "sockbuf.h"

#define LISTEN_PORT "5000"

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

int main(int argc, char *argv[]) {
    int z;
    int sock;

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

    sock = socket(servaddr->ai_family, servaddr->ai_socktype, servaddr->ai_protocol);
    if (sock == -1) {
        exit_err("socket()");
    }

    z = bind(sock, servaddr->ai_addr, servaddr->ai_addrlen);
    if (z != 0) {
        exit_err("bind()");
    }

    freeaddrinfo(servaddr);
    servaddr = NULL;

    z = listen(sock, 5);
    if (z != 0) {
        exit_err("listen()");
    }
    printf("Listening on %s:%s...\n", servipstr, LISTEN_PORT);

    char line[1000];
    char buf[10];

    while (1) {
        struct sockaddr_in a;
        socklen_t a_len = sizeof(a);
        int client = accept(sock, (struct sockaddr*)&a, &a_len);
        if (client == -1) {
            print_err("accept()");
            continue;
        }

        printf("--- New client ---\n");
        sockbuf_t *sb = sockbuf_new(client, 4);

        while (1) {
            memset(line, '-', sizeof line);
            memset(buf, '-', sizeof buf);
#if 0
            int nchars = sockbuf_readline(sb, line, sizeof line);
            if (nchars == 0) break;
            printf("readline(): %s\n", line);
#endif

//#if 0
            int nchars = sockbuf_read(sb, buf, sizeof(buf)-1);
            if (nchars == 0) break;
            buf[nchars] = '\0';
            printf("read(): %s\n", buf);
//#endif
        }
        printf("EOF\n");

        sockbuf_free(sb);
        close(client);
        break;
    }

    close(sock);
    return 0;
}


