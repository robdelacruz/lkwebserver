#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <assert.h>
#include <signal.h>
#include <sys/wait.h>
#include <time.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <pthread.h>

#include "lklib.h"
#include "lknet.h"

struct httpclientcontext {
    int sock;                         // client socket
    struct sockaddr *client_sa;       // client address
    LKString *client_ipaddr;          // client ip address string
    LKSocketReader *sr;               // input buffer for reading lines
    LKHttpRequestParser *reqparser;   // parser for httprequest
    LKHttpResponse *resp;             // http response to be sent
    struct httpclientcontext *next;   // link to next client
};

struct httpserversettings {
    LKString *homedir;
    LKString *cgidir;
    LKStringMap *aliases;
};

// local functions
LKHttpClientContext *create_clientcontext(int sock, struct sockaddr *sa);
void free_clientcontext(LKHttpClientContext *ctx);
void add_clientcontext(LKHttpClientContext **pphead, LKHttpClientContext *ctx);
void remove_clientcontext(LKHttpClientContext **pphead, int sock);

LKHttpServerSettings *create_serversettings();
void free_serversettings(LKHttpServerSettings *settings);
void finalize_settings(LKHttpServerSettings *settings);

void read_request_from_client(LKHttpServer *server, int clientfd);
int read_and_parse_line(LKHttpClientContext *ctx);
int read_and_parse_bytes(LKHttpClientContext *ctx);
void process_request(LKHttpServer *server, LKHttpClientContext *ctx);
void process_response(LKHttpServer *server, LKHttpClientContext *ctx);

void serve_files(LKHttpServer *server, LKHttpRequest *req, LKHttpResponse *resp);
int read_uri_file(char *home_dir, char *uri, LKBuffer *buf);
char *fileext(char *filepath);

void serve_cgi(LKHttpServer *server, LKHttpClientContext *ctx, LKHttpRequest *req, LKHttpResponse *resp);
void *runcgi(void *parg);

void send_response_to_client(LKHttpServer *server, int clientfd);
int send_buf_bytes(int sock, LKBuffer *buf);
void terminate_client_session(LKHttpServer *server, int clientfd);


/*** LKHttpServer functions ***/

LKHttpServer *lk_httpserver_new() {
    LKHttpServer *server = malloc(sizeof(LKHttpServer));
    server->ctxhead = NULL;
    server->settings = create_serversettings();
    return server;
}

void lk_httpserver_free(LKHttpServer *server) {
    //$$ free ctxhead clientctx linked list?

    free_serversettings(server->settings);
    memset(server, 0, sizeof(LKHttpServer));
    free(server);
}

// Fill in default values for unspecified settings.
void finalize_settings(LKHttpServerSettings *settings) {
    // If home dir not specified, use current working directory.
    if (settings->homedir->s_len == 0) {
        char *current_dir = get_current_dir_name();
        current_dir = NULL;
        if (current_dir != NULL) {
            lk_string_assign(settings->homedir, current_dir);
            free(current_dir);
        } else {
            lk_string_assign(settings->homedir, ".");
        }
    }

    // If cgi dir not specified, default to cgi-bin.
    if (settings->cgidir->s_len == 0) {
        lk_string_assign(settings->cgidir, "/cgi-bin/");
    }
}

void lk_httpserver_setopt(LKHttpServer *server, LKHttpServerOpt opt, ...) {
    char *homedir;
    char *cgidir;
    char *alias_k, *alias_v;
    LKHttpServerSettings *settings = server->settings;

    va_list args;
    va_start(args, opt);

    switch(opt) {
    case LKHTTPSERVEROPT_HOMEDIR:
        homedir = va_arg(args, char*);
        lk_string_assign(settings->homedir, homedir);
        lk_string_chop_end(settings->homedir, "/");
        break;
    case LKHTTPSERVEROPT_CGIDIR:
        cgidir = va_arg(args, char*);
        if (strlen(cgidir) == 0) {
            break;
        }
        lk_string_assign(settings->cgidir, cgidir);

        // Surround cgi dir with slashes: /cgi-bin/ for easy uri matching.
        if (!lk_string_starts_with(settings->cgidir, "/")) {
            lk_string_prepend(settings->cgidir, "/");
        }
        if (!lk_string_ends_with(settings->cgidir, "/")) {
            lk_string_append(settings->cgidir, "/");
        }
        break;
    case LKHTTPSERVEROPT_ALIAS:
        alias_k = va_arg(args, char*);
        alias_v = va_arg(args, char*);
        lk_stringmap_set(settings->aliases, alias_k, lk_string_new(alias_v));
        break;
    default:
        break;
    }

    va_end(args);
}

int lk_httpserver_serve(LKHttpServer *server, int listen_sock) {
    finalize_settings(server->settings);

    printf("home_dir: '%s', cgi_dir: '%s'\n", server->settings->homedir->s, server->settings->cgidir->s);

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
        if (z == -1 && errno == EINTR) {
            continue;
        }
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
                    socklen_t sa_len = sizeof(struct sockaddr_in);
                    struct sockaddr_in *sa = malloc(sa_len);
                    int newclientsock = accept(listen_sock, (struct sockaddr*)sa, &sa_len);
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
                    LKHttpClientContext *ctx = create_clientcontext(newclientsock, (struct sockaddr*) sa);
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
        if (!ctx->reqparser->head_complete) {
            int z = read_and_parse_line(ctx);
            if (z != 0) {
                break;
            }
        } else {
            int z = read_and_parse_bytes(ctx);
            if (z != 0) {
                break;
            }
        }

        if (ctx->reqparser->body_complete) {
            process_request(server, ctx);
        }
    }
}

// Read and parse one request line from client socket.
// Return 0 if line was read, 1 if no data, < 0 for socket error.
int read_and_parse_line(LKHttpClientContext *ctx) {
    char buf[1000];
    size_t nread;
    int z = lk_socketreader_readline(ctx->sr, buf, sizeof buf, &nread);
    if (z == -1) {
        lk_print_err("lksocketreader_readline()");
        return z;
    }
    if (nread == 0) {
        return 1;
    }
    assert(buf[nread] == '\0');

    lk_httprequestparser_parse_line(ctx->reqparser, buf);
    return 0;
}

// Read and parse the next sequence of bytes from client socket.
// Return 0 if bytes were read, 1 if no data, < 0 for socket error.
int read_and_parse_bytes(LKHttpClientContext *ctx) {
    char buf[2048];
    size_t nread;
    int z = lk_socketreader_readbytes(ctx->sr, buf, sizeof buf, &nread);
    if (z == -1) {
        lk_print_err("lksocketreader_readbytes()");
        return z;
    }
    if (nread == 0) {
        return 1;
    }

    lk_httprequestparser_parse_bytes(ctx->reqparser, buf, nread);
    return 0;
}

#define TIME_STRING_SIZE 50
void get_localtime_string(char *time_str, size_t time_str_len) {
    time_t t = time(NULL);
    struct tm tmtime; 
    void *pz = localtime_r(&t, &tmtime);
    if (pz != NULL) {
        int z = strftime(time_str, time_str_len, "%d/%b/%Y %H:%M:%S", &tmtime);
        if (z == 0) {
            sprintf(time_str, "???");
        }
    } else {
        sprintf(time_str, "???");
    }
}

void process_request(LKHttpServer *server, LKHttpClientContext *ctx) {
    assert(ctx->reqparser->req);

    LKHttpRequest *req = ctx->reqparser->req;
    LKHttpResponse *resp = ctx->resp;
    LKHttpServerSettings *settings = server->settings;

    // Run cgi script if uri falls under cgidir
    if (lk_string_starts_with(req->uri, settings->cgidir->s)) {
        serve_cgi(server, ctx, req, resp);
        return;
    }

    serve_files(server, ctx->reqparser->req, ctx->resp);
    process_response(server, ctx);
}

void process_response(LKHttpServer *server, LKHttpClientContext *ctx) {
    LKHttpRequest *req = ctx->reqparser->req;
    LKHttpResponse *resp = ctx->resp;

    lk_httpresponse_finalize(resp);

    char time_str[TIME_STRING_SIZE];
    get_localtime_string(time_str, sizeof(time_str));

    printf("%s [%s] \"%s %s %s\" %d\n", 
        ctx->client_ipaddr->s, time_str,
        req->method->s, req->uri->s, resp->version->s,
        resp->status);
    if (resp->status >= 500 && resp->status < 600 && resp->statustext->s_len > 0) {
        printf("%s [%s] %d - %s\n", 
            ctx->client_ipaddr->s, time_str,
            resp->status, resp->statustext->s);
    }

    FD_SET(ctx->sock, &server->writefds);
    return;
}

// Generate an http response to an http request.
void serve_files(LKHttpServer *server, LKHttpRequest *req, LKHttpResponse *resp) {
    int z;
    static char *html_error_start = 
       "<!DOCTYPE html>\n"
       "<html>\n"
       "<head><title>Error response</title></head>\n"
       "<body><h1>Error response</h1>\n";
    static char *html_error_end =
       "</body></html>\n";

    static char *html_sample =
       "<!DOCTYPE html>\n"
       "<html>\n"
       "<head><title>Little Kitten</title></head>\n"
       "<body><h1>Little Kitten webserver</h1>\n"
       "<p>Hello Little Kitten!</p>\n"
       "</body></html>\n";

#if 0
    static char *html_start =
       "<!DOCTYPE html>\n"
       "<html>\n"
       "<head><title>Little Kitten Sample Response</title></head>\n"
       "<body>\n";
    static char *html_end =
       "</body></html>\n";
#endif

    LKHttpServerSettings *settings = server->settings;
    char *method = req->method->s;
    char *uri = req->uri->s;

    if (!strcmp(method, "GET")) {
        // /littlekitten sample page
        if (!strcmp(uri, "/littlekitten")) {
            lk_httpresponse_add_header(resp, "Content-Type", "text/html");
            lk_buffer_append(resp->body, html_sample, strlen(html_sample));
            return;
        }

        LKString *content_type = lk_string_new("");
        lk_string_assign_sprintf(content_type, "text/%s;", fileext(uri));
        lk_httpresponse_add_header(resp, "Content-Type", content_type->s);
        lk_string_free(content_type);

        // Use alias if there's a match for uri.
        // Ex. uri: "/latest" => "/latest.html"
        LKString *alias_uri = lk_stringmap_get(settings->aliases, uri);
        if (alias_uri != NULL) {
            uri = alias_uri->s;
        }

        // if no page given, try opening index.html, ...
        //$$ Better way to do this?
        if (!strcmp(uri, "/")) {
            while (1) {
                z = read_uri_file(settings->homedir->s, "/index.html", resp->body);
                if (z == 0) break;
                z = read_uri_file(settings->homedir->s, "/index.htm", resp->body);
                if (z == 0) break;
                z = read_uri_file(settings->homedir->s, "/default.html", resp->body);
                if (z == 0) break;
                z = read_uri_file(settings->homedir->s, "/default.htm", resp->body);
                break;
            }
        } else {
            z = read_uri_file(settings->homedir->s, uri, resp->body);
        }
        if (z == -1) {
            // uri file not found
            resp->status = 404;
            lk_string_assign_sprintf(resp->statustext, "File not found '%s'", uri);
            lk_httpresponse_add_header(resp, "Content-Type", "text/plain");
            lk_buffer_append_sprintf(resp->body, "File not found '%s'\n", uri);
        }

        return;
    }
#if 0
    if (!strcmp(method, "POST")) {
        lk_httpresponse_add_header(resp, "Content-Type", "text/html");
        lk_buffer_append(resp->body, html_start, strlen(html_start));
        lk_buffer_append_sz(resp->body, "<pre>\n");
        lk_buffer_append(resp->body, req->body->bytes, req->body->bytes_len);
        lk_buffer_append_sz(resp->body, "\n</pre>\n");
        lk_buffer_append(resp->body, html_end, strlen(html_end));
        return;
    }
#endif

    resp->status = 501;
    lk_string_assign_sprintf(resp->statustext, "Unsupported method ('%s')", method);

    lk_buffer_append(resp->body, html_error_start, strlen(html_error_start));
    lk_buffer_append_sprintf(resp->body, "<p>Error code %d.</p>\n", resp->status);
    lk_buffer_append_sprintf(resp->body, "<p>Message: Unsupported method ('%s').</p>\n", resp->statustext->s);
    lk_buffer_append(resp->body, html_error_end, strlen(html_error_end));
}

// Read <home_dir>/<uri> file into buffer.
// Returns 0 for success, -1 for error.
int read_uri_file(char *home_dir, char *uri, LKBuffer *buf) {
    // uri_path = home_dir + uri
    // Ex. "/path/to" + "/index.html"
    LKString *uri_path = lk_string_new(home_dir);
    lk_string_append(uri_path, uri);
    int z = lk_readfile(uri_path->s, buf);
    lk_string_free(uri_path);
    return z;
}

int is_valid_http_method(char *method) {
    if (method == NULL) {
        return 0;
    }

    if (!strcasecmp(method, "GET")      ||
        !strcasecmp(method, "POST")     || 
        !strcasecmp(method, "PUT")      || 
        !strcasecmp(method, "DELETE")   ||
        !strcasecmp(method, "HEAD"))  {
        return 1;
    }

    return 0;
}

// Return ptr to start of file extension within filepath.
// Ex. "path/to/index.html" returns "index.html"
char *fileext(char *filepath) {
    int filepath_len = strlen(filepath);
    // filepath of "" returns ext of "".
    if (filepath_len == 0) {
        return filepath;
    }

    char *p = filepath + strlen(filepath) - 1;
    while (p >= filepath) {
        if (*p == '.') {
            return p+1;
        }
        p--;
    }
    return filepath;
}

struct cgiparams_s {
    LKString *cgifile;
    LKHttpServer *server;
    LKHttpClientContext *ctx;
};

void serve_cgi(LKHttpServer *server, LKHttpClientContext *ctx, LKHttpRequest *req, LKHttpResponse *resp) {
    LKHttpServerSettings *settings = server->settings;
    char *uri = req->uri->s;

    LKString *cgifile = lk_string_new(settings->homedir->s);
    lk_string_append(cgifile, req->uri->s);

    // cgi file not found
    if (!lk_file_exists(cgifile->s)) {
        lk_string_free(cgifile);

        resp->status = 404;
        lk_string_assign_sprintf(resp->statustext, "File not found '%s'", uri);
        lk_httpresponse_add_header(resp, "Content-Type", "text/plain");
        lk_buffer_append_sprintf(resp->body, "File not found '%s'\n", uri);

        process_response(server, ctx);
        return;
    }

    struct cgiparams_s cgiparams;
    cgiparams.cgifile = cgifile;
    cgiparams.server = server;
    cgiparams.ctx = ctx;

    pthread_t t;
    int z = pthread_create(&t, NULL, runcgi, &cgiparams);
    if (z != 0) {
        lk_string_free(cgifile);

        resp->status = 500;
        errno = z;
        lk_string_assign_sprintf(resp->statustext, "pthread_create() '%s'", strerror(errno));
        lk_httpresponse_add_header(resp, "Content-Type", "text/plain");
        lk_buffer_append_sprintf(resp->body, "pthread_create() '%s'\n", strerror(errno));
        process_response(server, ctx);
        return;
    }

    //z = pthread_detach(t);
    z = pthread_join(t, NULL);
    if (z != 0) {
        resp->status = 500;
        errno = z;
        lk_string_assign_sprintf(resp->statustext, "pthread_detach() '%s'", strerror(errno));
        lk_httpresponse_add_header(resp, "Content-Type", "text/plain");
        lk_buffer_append_sprintf(resp->body, "pthread_detach() '%s'\n", strerror(errno));
        process_response(server, ctx);
        return;
    }
}

void *runcgi(void *parg) {
    printf("*** Start runcgi() function...\n");

    struct cgiparams_s *cgiparams = parg;
    LKString *cgifile = cgiparams->cgifile;
    LKHttpServer *server = cgiparams->server;
    LKHttpClientContext *ctx = cgiparams->ctx;

    LKHttpResponse *resp = ctx->resp;

    int fd_in, fd_out, fd_err;
    int z = lk_popen3(cgifile->s, &fd_in, &fd_out, &fd_err);
    lk_string_free(cgifile);
    if (z == -1) {
        resp->status = 500;
        lk_string_assign_sprintf(resp->statustext, "Server error '%s'", strerror(errno));
        lk_httpresponse_add_header(resp, "Content-Type", "text/plain");
        lk_buffer_append_sprintf(resp->body, "Server error '%s'\n", strerror(errno));
        process_response(server, ctx);
        return NULL;
    }

    //$$ how does cgi output write the Content-Type?
    lk_httpresponse_add_header(resp, "Content-Type", "text/html");

    lk_readfd(fd_err, resp->body);
    lk_readfd(fd_out, resp->body);
    process_response(server, ctx);

    close(fd_in);
    close(fd_out);
    close(fd_err);

    printf("*** End runcgi() function\n");
    return NULL;
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


/*** LKHttpClientContext functions ***/

LKHttpClientContext *create_clientcontext(int sock, struct sockaddr *sa) {
    LKHttpClientContext *ctx = malloc(sizeof(LKHttpClientContext));
    ctx->sock = sock;
    ctx->client_sa = sa;
    ctx->client_ipaddr = lk_get_ipaddr_string(sa);
    ctx->sr = lk_socketreader_new(sock, 0);
    ctx->reqparser = lk_httprequestparser_new();
    ctx->resp = lk_httpresponse_new();
    ctx->next = NULL;
    return ctx;
}

void free_clientcontext(LKHttpClientContext *ctx) {
    if (ctx->client_sa) {
        free(ctx->client_sa);
    }
    lk_string_free(ctx->client_ipaddr);
    lk_socketreader_free(ctx->sr);
    lk_httprequestparser_free(ctx->reqparser);
    if (ctx->resp) {
        lk_httpresponse_free(ctx->resp);
    }

    ctx->client_sa = NULL;
    ctx->client_ipaddr = NULL;
    ctx->sr = NULL;
    ctx->reqparser = NULL;
    ctx->resp = NULL;
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
        free_clientcontext(tmp);
        return;
    }

    LKHttpClientContext *p = *pphead;
    LKHttpClientContext *prev = NULL;
    while (p != NULL) {
        if (p->sock == sock) {
            assert(prev != NULL);
            prev->next = p->next;
            free_clientcontext(p);
            return;
        }

        prev = p;
        p = p->next;
    }
}

/*** LKHttpServerSettings functions ***/
LKHttpServerSettings *create_serversettings() {
    LKHttpServerSettings *settings = malloc(sizeof(LKHttpServerSettings));
    settings->homedir = lk_string_new("");
    settings->cgidir = lk_string_new("");
    settings->aliases = lk_stringmap_funcs_new(lk_string_voidp_free);
    return settings;
}

void free_serversettings(LKHttpServerSettings *settings) {
    lk_string_free(settings->homedir);
    lk_string_free(settings->cgidir);
    lk_stringmap_free(settings->aliases);

    settings->homedir = NULL;
    settings->cgidir = NULL;
    settings->aliases = NULL;
    free(settings);
}
