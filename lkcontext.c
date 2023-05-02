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
LKContext *lk_context_new() {
    LKContext *ctx = malloc(sizeof(LKContext));
    ctx->selectfd = 0;
    ctx->clientfd = 0;
    ctx->type = 0;
    ctx->next = NULL;

    ctx->client_ipaddr = NULL;
    ctx->client_port = 0;
    ctx->sr = NULL;
    ctx->req = NULL;
    ctx->resp = NULL;
    ctx->reqparser = NULL;

    ctx->cgifd = 0;
    ctx->cgi_outputbuf = NULL;
    ctx->cgi_inputbuf = NULL;

    ctx->proxyfd = 0;
    ctx->proxy_respbuf = NULL;

    return ctx;
}

LKContext *create_initial_context(int fd, struct sockaddr_in *sa) {
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

    ctx->cgifd = 0;
    ctx->cgi_outputbuf = NULL;
    ctx->cgi_inputbuf = NULL;

    ctx->proxyfd = 0;
    ctx->proxy_respbuf = NULL;

    return ctx;
}

void lk_context_free(LKContext *ctx) {
    if (ctx->client_ipaddr) {
        lk_string_free(ctx->client_ipaddr);
    }
    if (ctx->sr) {
        lk_socketreader_free(ctx->sr);
    }
    if (ctx->reqparser) {
        lk_httprequestparser_free(ctx->reqparser);
    }
    if (ctx->req) {
        lk_httprequest_free(ctx->req);
    }
    if (ctx->resp) {
        lk_httpresponse_free(ctx->resp);
    }
    if (ctx->cgi_outputbuf) {
        lk_buffer_free(ctx->cgi_outputbuf);
    }
    if (ctx->cgi_inputbuf) {
        lk_buffer_free(ctx->cgi_inputbuf);
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
    ctx->cgifd = 0;
    ctx->cgi_outputbuf = NULL;
    ctx->cgi_inputbuf = NULL;
    ctx->proxyfd = 0;
    ctx->proxy_respbuf = NULL;
    free(ctx);
}

// Add new client ctx to end of ctx linked list.
// Skip if ctx clientfd already in list.
void add_new_client_context(LKContext **pphead, LKContext *ctx) {
    assert(pphead != NULL);

    if (*pphead == NULL) {
        // first client
        *pphead = ctx;
    } else {
        // add client to end of clients list
        LKContext *p = *pphead;
        while (p->next != NULL) {
            // ctx fd already exists
            if (p->clientfd == ctx->clientfd) {
                return;
            }
            p = p->next;
        }
        p->next = ctx;
    }
}

// Add ctx to end of ctx linked list, allowing duplicate clientfds.
void add_context(LKContext **pphead, LKContext *ctx) {
    assert(pphead != NULL);

    if (*pphead == NULL) {
        // first client
        *pphead = ctx;
    } else {
        // add to end of clients list
        LKContext *p = *pphead;
        while (p->next != NULL) {
            p = p->next;
        }
        p->next = ctx;
    }
}


// Delete first ctx having clientfd from linked list.
// Returns 1 if context was deleted, 0 if no deletion made.
int remove_client_context(LKContext **pphead, int clientfd) {
    assert(pphead != NULL);

    if (*pphead == NULL) {
        return 0;
    }
    // remove head ctx
    if ((*pphead)->clientfd == clientfd) {
        LKContext *tmp = *pphead;
        *pphead = (*pphead)->next;
        lk_context_free(tmp);
        return 1;
    }

    LKContext *p = *pphead;
    LKContext *prev = NULL;
    while (p != NULL) {
        if (p->clientfd == clientfd) {
            assert(prev != NULL);
            prev->next = p->next;
            lk_context_free(p);
            return 1;
        }

        prev = p;
        p = p->next;
    }

    return 0;
}

// Delete all ctx's having clientfd.
//$$ todo: unused, remove this?
void remove_client_contexts(LKContext **pphead, int clientfd) {
    int z = 1;
    // Keep trying to remove matching clientfd's until none left.
    while (z != 0) {
        z = remove_client_context(pphead, clientfd);
    }
}

// Return ctx matching selectfd.
LKContext *match_select_ctx(LKContext *phead, int selectfd) {
    LKContext *ctx = phead;
    while (ctx != NULL) {
        if (ctx->selectfd == selectfd) {
            break;
        }
        ctx = ctx->next;
    }
    return ctx;
}

// Delete first ctx having selectfd from linked list.
// Returns 1 if context was deleted, 0 if no deletion made.
int remove_selectfd_context(LKContext **pphead, int selectfd) {
    assert(pphead != NULL);

    if (*pphead == NULL) {
        return 0;
    }
    // remove head ctx
    if ((*pphead)->selectfd == selectfd) {
        LKContext *tmp = *pphead;
        *pphead = (*pphead)->next;
        lk_context_free(tmp);
        return 1;
    }

    LKContext *p = *pphead;
    LKContext *prev = NULL;
    while (p != NULL) {
        if (p->selectfd == selectfd) {
            assert(prev != NULL);
            prev->next = p->next;
            lk_context_free(p);
            return 1;
        }

        prev = p;
        p = p->next;
    }

    return 0;
}



