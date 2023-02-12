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

#define LISTEN_PORT 5000

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
    char buf[1024];

    sock = socket(PF_INET, SOCK_STREAM, 0);
    if (sock == -1) {
        exit_err("socket()");
    }

    struct sockaddr_in a;
    a.sin_family = AF_INET;
    a.sin_port = htons(LISTEN_PORT);
    a.sin_addr.s_addr = INADDR_ANY;
    memset(a.sin_zero, 0, sizeof(a.sin_zero));
    z = bind(sock, (struct sockaddr*)&a, sizeof(a));
    if (z != 0) {
        exit_err("bind()");
    }

    z = listen(sock, 5);
    if (z != 0) {
        exit_err("listen()");
    }
    printf("Listening for connections...\n");

    while (1) {
        struct sockaddr_in a;
        socklen_t a_len = sizeof(a);
        int client = accept(sock, (struct sockaddr*)&a, &a_len);
        if (client == -1) {
            print_err("accept()");
            continue;
        }

        printf("--- New client ---\n");

        // Block and receive messages from client until client socket is closed.
        while (recv(client, buf, sizeof buf, 0) != 0) {
            printf("recv(): %s\n", buf);
        }
        printf("recv(): EOF\n");
    }

    close(sock);
    return 0;
}


