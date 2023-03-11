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

    sockbuf_t *servsb = sockbuf_new(sock, 0);

    char *chunks[] = {
        "GET /www/index.html HTTP/1.0\r\n", 
        "From: rob@rob", 
        "delacruz.xyz\r\n", 
        "User-Agent: tclient/1.0\r\n", 
        "\r\n",
        "message body: chunk 1 ",
        "message body: chunk 2 ",
        "message body: chunk 3\n"
    };

#if 0
    char *msg =
        "1\n"
        "12345\n"
        "abc\n"
        "Lorem ipsum dolor sit amet, consectetur adipiscing elit. Proin tortor mauris, commodo quis nibh sit amet, pellentesque gravida libero. Etiam pellentesque orci vitae quam sagittis, sed facilisis quam hendrerit. Interdum et malesuada fames ac ante ipsum primis in faucibus. Etiam a dapibus mauris, ut laoreet ante. Pellentesque sit amet dui eros. Integer cursus porttitor odio non venenatis. In hac habitasse platea dictumst. Phasellus leo nibh, elementum eu ipsum eget, euismod feugiat eros. Quisque sollicitudin et tellus vel pellentesque. Integer quam felis, finibus vel dui vitae, posuere dignissim urna. Nunc a vestibulum ex. Morbi in sollicitudin ligula, quis sodales magna. Suspendisse finibus non ligula vel blandit. Morbi ornare arcu vel accumsan venenatis. Curabitur erat orci, facilisis ut auctor aliquam, placerat vel lorem.\r\n"
        "1";
#endif

    for (int i=0; i < sizeof(chunks) / sizeof(char *); i++) {
        z = send(sock, chunks[i], strlen(chunks[i]), 0);
        if (z == -1) {
            exit_err("send()");
        }
    }

    close(sock);
    return 0;
}

