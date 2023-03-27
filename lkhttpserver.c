#define _GNU_SOURCE
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

#include "lklib.h"
#include "lknet.h"

struct httpclientcontext {
    int sock;                         // client socket
    LKSocketReader *sr;               // input buffer for reading lines
    LKHttpRequestParser *reqparser;   // parser for httprequest
    LKString *partial_line;
    LKHttpResponse *resp;             // http response to be sent
    struct httpclientcontext *next;   // link to next client
};

// local functions
void lk_print_err(char *s);
LKHttpClientContext *lk_clientcontext_new(int sock);
void lk_clientcontext_free(LKHttpClientContext *ctx);
void add_clientcontext(LKHttpClientContext **pphead, LKHttpClientContext *ctx);
void remove_clientcontext(LKHttpClientContext **pphead, int sock);

void read_request_from_client(LKHttpServer *server, int clientfd);
int ends_with_newline(char *s);
void process_line(LKHttpServer *server, LKHttpClientContext *ctx, char *line);

void send_response_to_client(LKHttpServer *server, int clientfd);
int send_buf_bytes(int sock, LKBuffer *buf);
void terminate_client_session(LKHttpServer *server, int clientfd);


/*** LKHttpServer functions ***/

// Print the last error message corresponding to errno.
void lk_print_err(char *s) {
    fprintf(stderr, "%s: %s\n", s, strerror(errno));
}

LKHttpServer *lk_httpserver_new(LKHttpHandlerFunc handlerfunc, void *handler_ctx) {
    LKHttpServer *server = malloc(sizeof(LKHttpServer));
    server->ctxhead = NULL;
    server->handler_ctx = handler_ctx;
    server->http_handler_func = handlerfunc;
    return server;
}

void lk_httpserver_free(LKHttpServer *server) {
    memset(server, 0, sizeof(LKHttpServer));
    free(server);
}

int lk_httpserver_serve(LKHttpServer *server, int listen_sock) {
    int z = listen(listen_sock, 5);
    if (z != 0) {
        lk_print_err("listen()");
        return z;
    }

    FD_ZERO(&server->readfds);
    FD_ZERO(&server->writefds);

    FD_SET(listen_sock, &server->readfds);
    int maxfd = listen_sock;

    while (1) {
        // readfds contain the master list of read sockets
        fd_set cur_readfds = server->readfds;
        fd_set cur_writefds = server->writefds;
        z = select(maxfd+1, &cur_readfds, &cur_writefds, NULL, NULL);
        if (z == -1) {
            lk_print_err("select()");
            return z;
        }
        if (z == 0) {
            // timeout returned
            continue;
        }

        // fds now contain list of clients with data available to be read.
        for (int i=0; i <= maxfd; i++) {
            if (FD_ISSET(i, &cur_readfds)) {
                // New client connection
                if (i == listen_sock) {
                    struct sockaddr_in a;
                    socklen_t a_len = sizeof(a);
                    int newclientsock = accept(listen_sock, (struct sockaddr*)&a, &a_len);
                    if (newclientsock == -1) {
                        lk_print_err("accept()");
                        continue;
                    }

                    // Add new client socket to list of read sockets.
                    FD_SET(newclientsock, &server->readfds);
                    if (newclientsock > maxfd) {
                        maxfd = newclientsock;
                    }

                    //printf("read fd: %d\n", newclientsock);
                    LKHttpClientContext *ctx = lk_clientcontext_new(newclientsock);
                    add_clientcontext(&server->ctxhead, ctx);
                    continue;
                } else {
                    // i contains client socket with data available to be read.
                    int clientfd = i;
                    read_request_from_client(server, clientfd);
                }
            } else if (FD_ISSET(i, &cur_writefds)) {
                //printf("write fd %d\n", i);
                // i contains client socket ready to be written to.
                int clientfd = i;
                send_response_to_client(server, clientfd);
            }
        }
    } // while (1)

    return 0;
}


/*** LKHttpClientContext functions ***/

LKHttpClientContext *lk_clientcontext_new(int sock) {
    LKHttpClientContext *ctx = malloc(sizeof(LKHttpClientContext));
    ctx->sock = sock;
    ctx->sr = lk_socketreader_new(sock, 0);
    ctx->reqparser = lk_httprequestparser_new();
    ctx->partial_line = lk_string_new("");
    ctx->resp = NULL;
    ctx->next = NULL;
    return ctx;
}

void lk_clientcontext_free(LKHttpClientContext *ctx) {
    lk_socketreader_free(ctx->sr);
    lk_httprequestparser_free(ctx->reqparser);
    lk_string_free(ctx->partial_line);
    if (ctx->resp) {
        lk_httpresponse_free(ctx->resp);
    }

    ctx->sr = NULL;
    ctx->reqparser = NULL;
    ctx->resp = NULL;
    ctx->partial_line = NULL;
    ctx->next = NULL;
    free(ctx);
}

// Add ctx to end of clientctx linked list. Skip if ctx sock already in list.
void add_clientcontext(LKHttpClientContext **pphead, LKHttpClientContext *ctx) {
    assert(pphead != NULL);

    if (*pphead == NULL) {
        // first client
        *pphead = ctx;
    } else {
        // add client to end of clients list
        LKHttpClientContext *p = *pphead;
        while (p->next != NULL) {
            // ctx sock already exists
            if (p->sock == ctx->sock) {
                return;
            }
            p = p->next;
        }
        p->next = ctx;
    }
}

// Remove ctx sock from clientctx linked list.
void remove_clientcontext(LKHttpClientContext **pphead, int sock) {
    assert(pphead != NULL);

    if (*pphead == NULL) {
        return;
    }
    // remove head ctx
    if ((*pphead)->sock == sock) {
        LKHttpClientContext *tmp = *pphead;
        *pphead = (*pphead)->next;
        lk_clientcontext_free(tmp);
        return;
    }

    LKHttpClientContext *p = *pphead;
    LKHttpClientContext *prev = NULL;
    while (p != NULL) {
        if (p->sock == sock) {
            assert(prev != NULL);
            prev->next = p->next;
            lk_clientcontext_free(p);
            return;
        }

        prev = p;
        p = p->next;
    }
}

void read_request_from_client(LKHttpServer *server, int clientfd) {
    LKHttpClientContext *ctx = server->ctxhead;
    while (ctx != NULL && ctx->sock != clientfd) {
        ctx = ctx->next;
    }
    if (ctx == NULL) {
        printf("read_request_from_client() clientfd %d not in clients list\n", clientfd);
        return;
    }
    assert(ctx->sock == clientfd);
    assert(ctx->sr->sock == clientfd);

    while (1) {
        char buf[1000];
        int z = lk_socketreader_readline(ctx->sr, buf, sizeof buf);
        if (z == 0) {
            break;
        }
        if (z == -1) {
            lk_print_err("lksocketreader_readline()");
            break;
        }
        assert(buf[z] == '\0');

        // If there's a previous partial line, combine it with current line.
        if (ctx->partial_line->s_len > 0) {
            lk_string_append(ctx->partial_line, buf);
            if (ends_with_newline(ctx->partial_line->s)) {
                process_line(server, ctx, ctx->partial_line->s);
                lk_string_assign(ctx->partial_line, "");
            }
            continue;
        }

        // If current line is only partial line (not newline terminated), remember it for
        // next read.
        if (!ends_with_newline(buf)) {
            lk_string_assign(ctx->partial_line, buf);
            continue;
        }

        // Current line is complete.
        process_line(server, ctx, buf);
    }
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

void process_line(LKHttpServer *server, LKHttpClientContext *ctx, char *line) {
    lk_httprequestparser_parse_line(ctx->reqparser, line);
    if (ctx->reqparser->body_complete) {
        LKHttpRequest *req = ctx->reqparser->req;
        printf("127.0.0.1 [11/Mar/2023 14:05:46] \"%s %s HTTP/1.1\" %d\n", 
            req->method->s, req->uri->s, 200);

        ctx->resp = lk_httpresponse_new();
        
        LKHttpHandlerFunc handler_func = server->http_handler_func;
        if (handler_func) {
            (*handler_func)(server->handler_ctx, req, ctx->resp);
            lk_httpresponse_finalize(ctx->resp);
        } else {
            // If no handler, return default 200 OK response.
            ctx->resp->status = 200;
            lk_string_assign(ctx->resp->statustext, "Default OK");
            lk_string_assign(ctx->resp->version, "HTTP/1.0");
            lk_httpresponse_finalize(ctx->resp);
        }
        FD_SET(ctx->sock, &server->writefds);
        return;
    }
}

void send_response_to_client(LKHttpServer *server, int clientfd) {
    LKHttpClientContext *ctx = server->ctxhead;
    while (ctx != NULL && ctx->sock != clientfd) {
        ctx = ctx->next;
    }
    if (ctx == NULL) {
        printf("send_response_to_client() clientfd %d not in clients list\n", clientfd);
        return;
    }
    assert(ctx->sock == clientfd);
    assert(ctx->sr->sock == clientfd);
    assert(ctx->resp != NULL);
    assert(ctx->resp->head != NULL);

    LKHttpResponse *resp = ctx->resp;

    // Send as much response bytes as the client will receive.
    // Send response head bytes first, then response body bytes.
    if (resp->head->bytes_cur < resp->head->bytes_len) {
        int z = send_buf_bytes(ctx->sock, resp->head);
        if (z == -1 && errno == EPIPE) {
            // client socket was shutdown
            terminate_client_session(server, ctx->sock);
            return;
        }
    } else if (resp->body->bytes_cur < resp->body->bytes_len) {
        int z = send_buf_bytes(ctx->sock, resp->body);
        if (z == -1 && errno == EPIPE) {
            // client socket was shutdown
            terminate_client_session(server, ctx->sock);
            return;
        }
    } else {
        // Completed sending http response.
        terminate_client_session(server, ctx->sock);
    }
}

// Send buf data into sock, keeping track of last buffer position.
// Used to cumulatively send buffer data with multiple sends.
int send_buf_bytes(int sock, LKBuffer *buf) {
    size_t nsent = 0;
    int z = lk_sock_send(sock,
        buf->bytes + buf->bytes_cur,
        buf->bytes_len - buf->bytes_cur,
        &nsent);
    buf->bytes_cur += nsent;
    return z;
}

// Disconnect from client.
void terminate_client_session(LKHttpServer *server, int clientfd) {
    //printf("terminate_client fd %d\n", clientfd);

    // Remove select() read and write I/O
    FD_CLR(clientfd, &server->readfds);
    FD_CLR(clientfd, &server->writefds);

    // Close read and writes.
    int z= shutdown(clientfd, SHUT_RDWR);
    if (z == -1) {
        lk_print_err("shutdown()");
    }
    z = close(clientfd);
    if (z == -1) {
        lk_print_err("close()");
    }
    // Remove client from clientctx list.
    remove_clientcontext(&server->ctxhead, clientfd);
}

