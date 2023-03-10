#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <assert.h>
#include <signal.h>
#include <sys/wait.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <netinet/in.h>

#include "netfuncs.h"

#define LISTEN_PORT "5000"

void handle_client(int clientfd);

typedef struct clientctx {
    int sock;                   // client socket
    sockbuf_t *sb;              // input buffer for reading lines
    httpreq_t *req;             // http request being parsed
    char *partial_line;         // partial line from previous read
                                // (to combine with next line)
    int nlinesread;             // number of lines read so far
    int empty_line_parsed;      // flag indicating whether empty line received
    struct clientctx *next;     // linked list to next client
} clientctx_t;

clientctx_t *ctxhead = NULL;

// Print the last error message corresponding to errno.
void print_err(char *s) {
    fprintf(stderr, "%s: %s\n", s, strerror(errno));
}
void exit_err(char *s) {
    print_err(s);
    exit(1);
}

// Return whether line is empty, ignoring whitespace chars ' ', \r, \n
int is_empty_line(char *s) {
    int slen = strlen(s);
    for (int i=0; i < slen; i++) {
        // Not an empty line if non-whitespace char is present.
        if (s[i] != ' ' && s[i] != '\n' && s[i] != '\r') {
            return 0;
        }
    }
    return 1;
}

// Return whether string ends with \n char.
int ends_with_newline(char *s) {
    int slen = strlen(s);
    if (slen == 0) {
        return 0;
    }
    if (s[slen-1] == '\n') {
        return 1;
    }
    return 0;
}

// Append s to dst, returning new ptr to dst.
char *append_string(char *dst, char *s) {
    int slen = strlen(s);
    if (slen == 0) {
        return dst;
    }

    int dstlen = strlen(dst);
    dst = realloc(dst, dstlen + slen + 1);
    strncpy(dst + dstlen, s, slen+1);
    assert(dst[dstlen+slen] == '\0');

    return dst;
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

    int yes=1;
    z = setsockopt(s0, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));
    if (s0 == -1) {
        exit_err("setsockopt(SO_REUSEADDR)");
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
                int newclientsock = accept(s0, (struct sockaddr*)&a, &a_len);
                if (newclientsock == -1) {
                    print_err("accept()");
                    continue;
                }

                // Add new client socket to list of read sockets.
                FD_SET(newclientsock, &readfds);
                if (newclientsock > maxfd) {
                    maxfd = newclientsock;
                }

                printf("New client fd: %d\n", newclientsock);
                clientctx_t *ctx = malloc(sizeof(clientctx_t));
                ctx->sock = newclientsock;
                ctx->sb = sockbuf_new(newclientsock, 0);
                ctx->req = httpreq_new();
                ctx->partial_line = NULL;
                ctx->nlinesread = 0;
                ctx->empty_line_parsed = 0;
                ctx->next = NULL;

                if (ctxhead == NULL) {
                    // first client
                    ctxhead = ctx;
                } else {
                    // add client to end of clients list
                    clientctx_t* p = ctxhead;
                    while (p->next != NULL) {
                        p = p->next;
                    }
                    p->next = ctx;
                }

                continue;
            }

            // i contains client socket with data available to be read.
            int clientfd = i;
            handle_client(clientfd);

            //FD_CLR(clientfd, &readfds);
            //close(clientfd);
        }
    }

    // Code doesn't reach here.
    close(s0);
    return 0;
}

void process_line(clientctx_t *ctx, char *line) {
    if (ctx->nlinesread == 0) {
        int z = httpreq_parse_request_line(ctx->req, line);
        if (z == -1) {
            printf("Invalid request line: '%s'\n", line);
        }

        ctx->nlinesread++;
        return;
    }

    if (is_empty_line(line)) {
        ctx->empty_line_parsed = 1;
        ctx->nlinesread++;

        printf("--- HTTP REQUEST received ---\n");
        httpreq_debugprint(ctx->req);
        printf("-----------------------------\n");
        return;
    }

    if (!ctx->empty_line_parsed) {
        httpreq_parse_header_line(ctx->req, line);
        ctx->nlinesread++;
        return;
    }

    httpreq_append_body(ctx->req, line, strlen(line));
    ctx->nlinesread++;
}

void handle_client(int clientfd) {
    clientctx_t *ctx = ctxhead;
    while (ctx != NULL && ctx->sock != clientfd) {
        ctx = ctx->next;
    }
    if (ctx == NULL) {
        printf("handle_client() clientfd %d not in clients list\n", clientfd);
        return;
    }
    assert(ctx->sock == clientfd);
    assert(ctx->sb->sock == clientfd);

    char buf[1000];
    int z = sockbuf_readline(ctx->sb, buf, sizeof buf);
    if (z == 0) {
        return;
    }
    if (z == -1) {
        print_err("sockbuf_readline()");
        return;
    }
    assert(buf[z] == '\0');

    // If there's a previous partial line, combine it with current line.
    if (ctx->partial_line != NULL) {
        ctx->partial_line = append_string(ctx->partial_line, buf);
        if (ends_with_newline(ctx->partial_line)) {
            process_line(ctx, ctx->partial_line);

            free(ctx->partial_line);
            ctx->partial_line = NULL;
        }
        return;
    }

    // If current line is only partial line (not newline terminated), remember it for
    // next read.
    if (!ends_with_newline(buf)) {
        ctx->partial_line = strdup(buf);
        return;
    }

    // Current line is complete.
    process_line(ctx, buf);
    return;

}



