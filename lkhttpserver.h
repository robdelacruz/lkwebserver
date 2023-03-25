#ifndef LKHTTPSERVER_H
#define LKHTTPSERVER_H

#include "lklib.h"
#include "lknet.h"

typedef struct httpclientcontext LKHttpClientContext;
typedef void (*LKHttpHandlerFunc)(LKHttpRequest *req, LKHttpResponse *resp);

typedef struct {
    LKHttpClientContext *ctxhead;
    fd_set readfds;
    fd_set writefds;
    LKHttpHandlerFunc http_handler_func;
} LKHttpServer;

LKHttpServer *lk_httpserver_new(LKHttpHandlerFunc handlerfunc);
void lk_httpserver_free(LKHttpServer *server);
void lk_httpserver_serve(LKHttpServer *server, int listen_sock);



#endif

