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

#include "netfuncs.h"

// Print the last error message corresponding to errno.
void print_err(char *s) {
    fprintf(stderr, "%s: %s\n", s, strerror(errno));
}
void exit_err(char *s) {
    print_err(s);
    exit(1);
}

int main(int argc, char *argv[]) {
    int z;
    int sock;

    if (argc < 3) {
        printf("Usage: tclient <server domain> <port>\n");
        printf("Ex. tclient 127.0.0.1 5000\n");
        exit(1);
    }

    char *server_domain = argv[1];
    char *server_port = argv[2];
    struct addrinfo hints, *servaddr;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = 0;
    z = getaddrinfo(server_domain, server_port, &hints, &servaddr);
    if (z != 0) {
        exit_err("getaddrinfo()");
    }

    // Server socket
    sock = socket(servaddr->ai_family, servaddr->ai_socktype, servaddr->ai_protocol);
    if (sock == -1) {
        exit_err("socket()");
    }

    z = connect(sock, servaddr->ai_addr, servaddr->ai_addrlen);
    if (z != 0) {
        exit_err("connect()");
    }

    freeaddrinfo(servaddr);
    servaddr = NULL;

    printf("Connected to %s:%s\n", server_domain, server_port);

    char *chunks[] = {
        "GET2 /www/index.html HTTP/1.0\r\n", 
        "From: rob@rob", 
        "delacruz.xyz\r\n", 
        "User-Agent: tclient/1.0\r\n", 
        "\r\n",
        "message body: chunk 1 ",
        "message body: chunk 2 ",
        "message body: chunk 3\n"
    };

    char respchunk[1024];

    for (int i=0; i < sizeof(chunks) / sizeof(char *); i++) {
        z = send(sock, chunks[i], strlen(chunks[i]), 0);
        if (z == -1) {
            exit_err("send()");
        }
    }

    printf("Response:\n");
    while(1) {
        z = recv(sock, respchunk, 1024-1, 0);
        if (z <= 0) {
            break;
        }
        respchunk[z] = '\0';
        printf("%s", respchunk);
    }

    close(sock);
    return 0;
}

