#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <assert.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <netinet/in.h>

#include "lklib.h"
#include "lknet.h"

/*** LKContext functions ***/
LKContext *create_context(int fd, struct sockaddr_in *sa) {
    LKContext *ctx = malloc(sizeof(LKContext));
    ctx->selectfd = fd;
    ctx->clientfd = fd;
    ctx->type = CTX_READ_REQ;
    ctx->next = NULL;

    ctx->client_sa = *sa;
    ctx->client_ipaddr = lk_get_ipaddr_string((struct sockaddr *) sa);
    ctx->client_port = lk_get_sockaddr_port((struct sockaddr *) sa);
    ctx->sr = lk_socketreader_new(fd, 0);
    ctx->req = lk_httprequest_new();
    ctx->resp = lk_httpresponse_new();
    ctx->reqparser = lk_httprequestparser_new(ctx->req);

    ctx->cgiparser = NULL;
    ctx->cgibuf = NULL;

    ctx->proxyfd = 0;
    ctx->proxy_respbuf = NULL;

    return ctx;
}

void free_context(LKContext *ctx) {
    if (ctx->client_ipaddr) {
        lk_string_free(ctx->client_ipaddr);
    }
    if (ctx->sr) {
        lk_socketreader_free(ctx->sr);
    }
    if (ctx->reqparser) {
        lk_httprequestparser_free(ctx->reqparser);
    }
    if (ctx->cgiparser) {
        lk_httpcgiparser_free(ctx->cgiparser);
    }
    if (ctx->req) {
        lk_httprequest_free(ctx->req);
    }
    if (ctx->resp) {
        lk_httpresponse_free(ctx->resp);
    }
    if (ctx->cgibuf) {
        lk_buffer_free(ctx->cgibuf);
    }
    if (ctx->proxy_respbuf) {
        lk_buffer_free(ctx->proxy_respbuf);
    }

    ctx->selectfd = 0;
    ctx->clientfd = 0;
    ctx->next = NULL;
    memset(&ctx->client_sa, 0, sizeof(struct sockaddr_in));
    ctx->client_ipaddr = NULL;
    ctx->sr = NULL;
    ctx->reqparser = NULL;
    ctx->req = NULL;
    ctx->resp = NULL;
    ctx->cgiparser = NULL;
    ctx->cgibuf = NULL;
    ctx->proxyfd = 0;
    ctx->proxy_respbuf = NULL;
    free(ctx);
}

// Add ctx to end of ctx linked list. Skip if ctx fd already in list.
void add_context(LKContext **pphead, LKContext *ctx) {
    assert(pphead != NULL);

    if (*pphead == NULL) {
        // first client
        *pphead = ctx;
    } else {
        // add client to end of clients list
        LKContext *p = *pphead;
        while (p->next != NULL) {
            // ctx fd already exists
            if (p->selectfd == ctx->selectfd) {
                return;
            }
            p = p->next;
        }
        p->next = ctx;
    }
}

// Remove ctx fd from ctx linked list.
void remove_context(LKContext **pphead, int fd) {
    assert(pphead != NULL);

    if (*pphead == NULL) {
        return;
    }
    // remove head ctx
    if ((*pphead)->selectfd == fd) {
        LKContext *tmp = *pphead;
        *pphead = (*pphead)->next;
        free_context(tmp);
        return;
    }

    LKContext *p = *pphead;
    LKContext *prev = NULL;
    while (p != NULL) {
        if (p->selectfd == fd) {
            assert(prev != NULL);
            prev->next = p->next;
            free_context(p);
            return;
        }

        prev = p;
        p = p->next;
    }
}

// Return ctx matching either client fd or server file / cgi fd.
LKContext *match_ctx(LKContext *phead, int fd) {
    LKContext *ctx = phead;
    while (ctx != NULL) {
        if (ctx->selectfd == fd) {
            break;
        }
        ctx = ctx->next;
    }
    return ctx;
}


