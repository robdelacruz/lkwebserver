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

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <netinet/in.h>

#include "lklib.h"
#include "lknet.h"


// local functions
void FD_SET_READ(int fd, LKHttpServer *server);
void FD_SET_WRITE(int fd, LKHttpServer *server);
void FD_CLR_READ(int fd, LKHttpServer *server);
void FD_CLR_WRITE(int fd, LKHttpServer *server);

LKHttpServerContext *create_context(int fd, struct sockaddr_in *sa);
void free_context(LKHttpServerContext *ctx);
void add_context(LKHttpServerContext **pphead, LKHttpServerContext *ctx);
void remove_context(LKHttpServerContext **pphead, int fd);
LKHttpServerContext *match_ctx(LKHttpServer *server, int fd);

LKHttpServerSettings *create_serversettings();
void free_serversettings(LKHttpServerSettings *settings);
void finalize_settings(LKHttpServerSettings *settings);

void read_request_from_client(LKHttpServer *server, LKHttpServerContext *ctx);
int read_and_parse_line(LKHttpServerContext *ctx);
int read_and_parse_bytes(LKHttpServerContext *ctx);
void process_request(LKHttpServer *server, LKHttpServerContext *ctx);
void process_response(LKHttpServer *server, LKHttpServerContext *ctx);
void get_localtime_string(char *time_str, size_t time_str_len);

void read_responsefile(LKHttpServer *server, LKHttpServerContext *ctx);
void serve_files(LKHttpServer *server, LKHttpServerContext *ctx);
int open_uri_file(char *home_dir, char *uri);
int read_uri_file(char *home_dir, char *uri, LKBuffer *buf);
char *fileext(char *filepath);

void serve_cgi(LKHttpServer *server, LKHttpServerContext *ctx);
void *runcgi(void *parg);

void send_response_to_client(LKHttpServer *server, LKHttpServerContext *ctx);
int send_buf_bytes(int sock, LKBuffer *buf);
void terminate_client_session(LKHttpServer *server, int clientfd);


/*** LKHttpServer functions ***/

LKHttpServer *lk_httpserver_new() {
    LKHttpServer *server = malloc(sizeof(LKHttpServer));
    server->ctxhead = NULL;
    server->settings = create_serversettings();
    server->maxfd = 0;
    return server;
}

void lk_httpserver_free(LKHttpServer *server) {
    // Free ctx linked list
    LKHttpServerContext *ctx = server->ctxhead;
    while (ctx != NULL) {
        LKHttpServerContext *ptmp = ctx;
        ctx = ctx->next;
        free_context(ptmp);
    }

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
        lk_stringtable_set(settings->aliases, alias_k, alias_v);
        break;
    default:
        break;
    }

    va_end(args);
}

void FD_SET_READ(int fd, LKHttpServer *server) {
    FD_SET(fd, &server->readfds);
    if (fd > server->maxfd) {
        server->maxfd = fd;
    }
}
void FD_SET_WRITE(int fd, LKHttpServer *server) {
    FD_SET(fd, &server->writefds);
    if (fd > server->maxfd) {
        server->maxfd = fd;
    }
}
void FD_CLR_READ(int fd, LKHttpServer *server) {
    FD_CLR(fd, &server->readfds);
}
void FD_CLR_WRITE(int fd, LKHttpServer *server) {
    FD_CLR(fd, &server->writefds);
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
    FD_SET_READ(listen_sock, server);

    while (1) {
        // readfds contain the master list of read sockets
        fd_set cur_readfds = server->readfds;
        fd_set cur_writefds = server->writefds;
        z = select(server->maxfd+1, &cur_readfds, &cur_writefds, NULL, NULL);
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
        for (int i=0; i <= server->maxfd; i++) {
            if (FD_ISSET(i, &cur_readfds)) {
                // New client connection
                if (i == listen_sock) {
                    socklen_t sa_len = sizeof(struct sockaddr_in);
                    struct sockaddr_in sa;
                    int newclientsock = accept(listen_sock, (struct sockaddr*)&sa, &sa_len);
                    if (newclientsock == -1) {
                        lk_print_err("accept()");
                        continue;
                    }

                    // Add new client socket to list of read sockets.
                    FD_SET_READ(newclientsock, server);

                    LKHttpServerContext *ctx = create_context(newclientsock, &sa);
                    add_context(&server->ctxhead, ctx);
                    continue;
                } else {
                    // i contains fd with data available to be read.
                    // fd could be a client socket with http request, server file,
                    // or cgi output stream
                    int fd = i;
                    LKHttpServerContext *ctx = match_ctx(server, fd);
                    if (ctx == NULL) {
                        printf("fd %d for read not in clients list\n", fd);
                        continue;
                    }

                    // Read static server file or cgi output stream.
                    if (ctx->fd != fd) {
                        read_responsefile(server, ctx);
                        continue;
                    }

                    // Read client http request stream.
                    assert(ctx->fd == fd);
                    assert(ctx->sr->sock == fd);
                    read_request_from_client(server, ctx);
                }
            } else if (FD_ISSET(i, &cur_writefds)) {
                //printf("write fd %d\n", i);
                // i contains client socket ready to be written to.
                int fd = i;
                LKHttpServerContext *ctx = match_ctx(server, fd);
                if (ctx == NULL) {
                    printf("clientfd %d for write not in clients list\n", fd);
                    continue;
                }
                assert(ctx->fd == fd);

                if (ctx->fd != fd) {
                    // code shouldn't reach here
                    continue;
                }

                assert(ctx->fd == fd);
                assert(ctx->sr->sock == fd);
                assert(ctx->resp != NULL);
                assert(ctx->resp->head != NULL);
                send_response_to_client(server, ctx);
            }
        }
    } // while (1)

    return 0;
}

void read_request_from_client(LKHttpServer *server, LKHttpServerContext *ctx) {
    while (1) {
        //$$ todo: Check if client is eof (ctx->sr->sockclosed)
        // If client is EOF, set http request as complete and process it.
        if (!ctx->reqparser->head_complete) {
            int z = read_and_parse_line(ctx);
            if (z <= 0) {
                break;
            }
        } else {
            int z = read_and_parse_bytes(ctx);
            if (z <= 0) {
                break;
            }
        }

        if (ctx->reqparser->body_complete) {
            FD_CLR_READ(ctx->fd, server);
            int z = shutdown(ctx->fd, SHUT_RD);
            if (z == -1) {
                lk_print_err("read_request_from_client(): shutdown()");
            }
            process_request(server, ctx);
        }
    }
}

// Read and parse one request line from client socket.
// Return number of bytes read or -1 for socket error.
int read_and_parse_line(LKHttpServerContext *ctx) {
    char buf[LK_BUFSIZE_MEDIUM];
    size_t nread;
    int z = lk_socketreader_readline(ctx->sr, buf, sizeof buf, &nread);
    if (z == -1) {
        lk_print_err("lksocketreader_readline()");
    }
    if (nread == 0) {
        return z;
    }
    assert(buf[nread] == '\0');
    lk_httprequestparser_parse_line(ctx->reqparser, buf);
    return nread;
}

// Read and parse the next sequence of bytes from client socket.
// Return number of bytes read or -1 for socket error.
int read_and_parse_bytes(LKHttpServerContext *ctx) {
    char buf[LK_BUFSIZE_LARGE];
    size_t nread;
    int z = lk_socketreader_readbytes(ctx->sr, buf, sizeof buf, &nread);
    if (z == -1) {
        lk_print_err("lksocketreader_readbytes()");
    }
    if (nread == 0) {
        return z;
    }
    lk_httprequestparser_parse_bytes(ctx->reqparser, buf, nread);
    return nread;
}

void process_request(LKHttpServer *server, LKHttpServerContext *ctx) {
    LKHttpServerSettings *settings = server->settings;

    // Run cgi script if uri falls under cgidir
    if (lk_string_starts_with(ctx->req->uri, settings->cgidir->s)) {
        serve_cgi(server, ctx);
        return;
    }

    serve_files(server, ctx);
    process_response(server, ctx);
}

void process_response(LKHttpServer *server, LKHttpServerContext *ctx) {
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

    FD_SET_WRITE(ctx->fd, server);
    return;
}

void read_responsefile(LKHttpServer *server, LKHttpServerContext *ctx) {
    char buf[LK_BUFSIZE_LARGE];
    size_t nread;

    while (1) {
        int z = lk_read(ctx->serverfile_fd, buf, sizeof(buf), &nread);
        if (nread > 0) {
            lk_buffer_append(ctx->resp->body, buf, nread);
        }

        if (z == -1) {
            if (errno != EAGAIN && errno != EWOULDBLOCK) {
                lk_print_err("lk_read()");
            }
            break;
        }

        // EOF - finished reading this file.
        if (z == 0 && nread == 0) {
            // Remove server/cgi file from read list.
            FD_CLR_READ(ctx->serverfile_fd, server);
            z = close(ctx->serverfile_fd);
            if (z == -1) {
                lk_print_err("close()");
            }
            ctx->serverfile_fd = 0;

            process_response(server, ctx);
            break;
        }
    }
}

#define POSTTEST

// Generate an http response to an http request.
void serve_files(LKHttpServer *server, LKHttpServerContext *ctx) {
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

    LKHttpServerSettings *settings = server->settings;
    LKHttpRequest *req = ctx->req;
    LKHttpResponse *resp = ctx->resp;
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
        char *alias_uri = lk_stringtable_get(settings->aliases, uri);
        if (alias_uri != NULL) {
            uri = alias_uri;
        }

        // For root, default to index.html, ...
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
#ifdef POSTTEST
    if (!strcmp(method, "POST")) {
        static char *html_start =
           "<!DOCTYPE html>\n"
           "<html>\n"
           "<head><title>Little Kitten Sample Response</title></head>\n"
           "<body>\n";
        static char *html_end =
           "</body></html>\n";

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

// Open <home_dir>/<uri> file in nonblocking mode.
// Returns 0 for success, -1 for error.
int open_uri_file(char *home_dir, char *uri) {
    // uri_path = home_dir + uri
    // Ex. "/path/to" + "/index.html"
    LKString *uri_path = lk_string_new(home_dir);
    lk_string_append(uri_path, uri);

    int z = open(uri_path->s, O_RDONLY | O_NONBLOCK);
    lk_string_free(uri_path);
    return z;
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

void serve_cgi(LKHttpServer *server, LKHttpServerContext *ctx) {
    LKHttpServerSettings *settings = server->settings;
    LKHttpRequest *req = ctx->req;
    LKHttpResponse *resp = ctx->resp;
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

    int fd_in, fd_out, fd_err;
    int z = lk_popen3(cgifile->s, &fd_in, &fd_out, &fd_err);
    lk_string_free(cgifile);
    if (z == -1) {
        resp->status = 500;
        lk_string_assign_sprintf(resp->statustext, "Server error '%s'", strerror(errno));
        lk_httpresponse_add_header(resp, "Content-Type", "text/plain");
        lk_buffer_append_sprintf(resp->body, "Server error '%s'\n", strerror(errno));
        process_response(server, ctx);
        return;
    }

    close(fd_in);
    close(fd_err);

    //$$ how does cgi output write the Content-Type?
    lk_httpresponse_add_header(resp, "Content-Type", "text/html");

    // Set cgi output to select() readfds to be read in read_responsefile()
    ctx->serverfile_fd = fd_out;
    FD_SET_READ(fd_out, server);
}

void send_response_to_client(LKHttpServer *server, LKHttpServerContext *ctx) {
    LKHttpResponse *resp = ctx->resp;

    // Send as much response bytes as the client will receive.
    // Send response head bytes first, then response body bytes.
    if (resp->head->bytes_cur < resp->head->bytes_len) {
        int z = send_buf_bytes(ctx->fd, resp->head);
        if (z == -1 && errno == EPIPE) {
            // client socket was shutdown
            terminate_client_session(server, ctx->fd);
            return;
        }
    } else if (resp->body->bytes_cur < resp->body->bytes_len) {
        int z = send_buf_bytes(ctx->fd, resp->body);
        if (z == -1 && errno == EPIPE) {
            // client socket was shutdown
            terminate_client_session(server, ctx->fd);
            return;
        }
    } else {
        // Completed sending http response.
        terminate_client_session(server, ctx->fd);
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
    FD_CLR_READ(clientfd, server);
    FD_CLR_WRITE(clientfd, server);

    int z = shutdown(clientfd, SHUT_RDWR);
    if (z == -1) {
        lk_print_err("shutdown()");
    }
    z = close(clientfd);
    if (z == -1) {
        lk_print_err("close()");
    }
    // Remove client from ctx list.
    remove_context(&server->ctxhead, clientfd);
}


/*** LKHttpServerContext functions ***/

LKHttpServerContext *create_context(int fd, struct sockaddr_in *sa) {
    LKHttpServerContext *ctx = malloc(sizeof(LKHttpServerContext));
    ctx->fd = fd;

    ctx->client_sa = *sa;
    ctx->client_ipaddr = lk_get_ipaddr_string((struct sockaddr *) sa);
    ctx->sr = lk_socketreader_new(fd, 0);
    ctx->req = lk_httprequest_new();
    ctx->reqparser = lk_httprequestparser_new(ctx->req);
    ctx->resp = lk_httpresponse_new();

    ctx->next = NULL;
    return ctx;
}

void free_context(LKHttpServerContext *ctx) {
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

    ctx->client_ipaddr = NULL;
    ctx->sr = NULL;
    ctx->reqparser = NULL;
    ctx->req = NULL;
    ctx->resp = NULL;
    ctx->next = NULL;
    free(ctx);
}

// Add ctx to end of ctx linked list. Skip if ctx fd already in list.
void add_context(LKHttpServerContext **pphead, LKHttpServerContext *ctx) {
    assert(pphead != NULL);

    if (*pphead == NULL) {
        // first client
        *pphead = ctx;
    } else {
        // add client to end of clients list
        LKHttpServerContext *p = *pphead;
        while (p->next != NULL) {
            // ctx fd already exists
            if (p->fd == ctx->fd) {
                return;
            }
            p = p->next;
        }
        p->next = ctx;
    }
}

// Remove ctx fd from ctx linked list.
void remove_context(LKHttpServerContext **pphead, int fd) {
    assert(pphead != NULL);

    if (*pphead == NULL) {
        return;
    }
    // remove head ctx
    if ((*pphead)->fd == fd) {
        LKHttpServerContext *tmp = *pphead;
        *pphead = (*pphead)->next;
        free_context(tmp);
        return;
    }

    LKHttpServerContext *p = *pphead;
    LKHttpServerContext *prev = NULL;
    while (p != NULL) {
        if (p->fd == fd) {
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
LKHttpServerContext *match_ctx(LKHttpServer *server, int fd) {
    LKHttpServerContext *ctx = server->ctxhead;
    while (ctx != NULL) {
        if (ctx->fd == fd) {
            break;
        }
        if (ctx->serverfile_fd == fd) {
            break;
        }
        ctx = ctx->next;
    }
    return ctx;
}


/*** LKHttpServerSettings functions ***/
LKHttpServerSettings *create_serversettings() {
    LKHttpServerSettings *settings = malloc(sizeof(LKHttpServerSettings));
    settings->homedir = lk_string_new("");
    settings->cgidir = lk_string_new("");
    settings->aliases = lk_stringtable_new();
    return settings;
}

void free_serversettings(LKHttpServerSettings *settings) {
    lk_string_free(settings->homedir);
    lk_string_free(settings->cgidir);
    lk_stringtable_free(settings->aliases);

    settings->homedir = NULL;
    settings->cgidir = NULL;
    settings->aliases = NULL;
    free(settings);
}
