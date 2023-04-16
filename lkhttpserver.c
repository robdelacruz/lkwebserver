#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
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

#include "lktables.h"
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
void read_responsefile(LKHttpServer *server, LKHttpServerContext *ctx);
void process_request(LKHttpServer *server, LKHttpServerContext *ctx);
void match_aliases(LKString *path, LKStringTable *aliases);

void serve_files(LKHttpServer *server, LKHttpServerContext *ctx);
void serve_cgi(LKHttpServer *server, LKHttpServerContext *ctx);
void process_response(LKHttpServer *server, LKHttpServerContext *ctx);

void set_cgi_env1(LKHttpServer *server);
void set_cgi_env2(LKHttpServer *server, LKHttpServerContext *ctx);

void get_localtime_string(char *time_str, size_t time_str_len);
int open_path_file(char *home_dir, char *path);
int read_path_file(char *home_dir, char *path, LKBuffer *buf);
char *fileext(char *filepath);

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

    char homedir_abspath[PATH_MAX];
    char *pz = realpath(settings->homedir->s, homedir_abspath);
    if (pz == NULL) {
        lk_print_err("realpath()");
        homedir_abspath[0] = '\0';
    }
    lk_string_assign(settings->homedir_abspath, homedir_abspath);

    // cgidir defaults to cgi-bin if not specified.
    if (settings->cgidir->s_len == 0) {
        lk_string_assign(settings->cgidir, "/cgi-bin/");
    }
    // host defaults to localhost if not specified.
    if (settings->host->s_len == 0) {
        lk_string_assign(settings->host, "localhost");
    }
    // port defaults to 8000 if not specified.
    if (settings->port->s_len == 0) {
        lk_string_assign(settings->port, "8000");
    }
}

void lk_httpserver_setopt(LKHttpServer *server, LKHttpServerOpt opt, ...) {
    char *homedir;
    char *port;
    char *host;
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
    case LKHTTPSERVEROPT_PORT:
        port = va_arg(args, char*);
        lk_string_assign(settings->port, port);
        break;
    case LKHTTPSERVEROPT_HOST:
        host = va_arg(args, char*);
        lk_string_assign(settings->host, host);
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

int lk_httpserver_serve(LKHttpServer *server) {
    int z;
    LKHttpServerSettings *settings = server->settings;
    finalize_settings(settings);

    // Get this server's address.
    struct addrinfo hints, *servaddr;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;
    z = getaddrinfo(settings->host->s, settings->port->s, &hints, &servaddr);
    if (z != 0) {
        lk_print_err("getaddrinfo()");
        return z;
    }

    int s0 = socket(servaddr->ai_family, servaddr->ai_socktype, servaddr->ai_protocol);
    if (s0 == -1) {
        lk_print_err("socket()");
        return -1;
    }

    int yes=1;
    z = setsockopt(s0, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));
    if (z == -1) {
        lk_print_err("setsockopt(SO_REUSEADDR)");
        return z;
    }

    z = bind(s0, servaddr->ai_addr, servaddr->ai_addrlen);
    if (z == -1) {
        lk_print_err("bind()");
        return z;
    }

    z = listen(s0, 5);
    if (z != 0) {
        lk_print_err("listen()");
        return z;
    }

    LKString *server_ipaddr_str = lk_get_ipaddr_string(servaddr->ai_addr);
    printf("Serving HTTP on %s port %s...\n", server_ipaddr_str->s, settings->port->s);
    lk_string_free(server_ipaddr_str);
    freeaddrinfo(servaddr);

    printf("home_dir: '%s', cgi_dir: '%s'\n", settings->homedir->s, settings->cgidir->s);

    clearenv();
    set_cgi_env1(server);

    FD_ZERO(&server->readfds);
    FD_ZERO(&server->writefds);
    FD_SET_READ(s0, server);

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
                if (i == s0) {
                    socklen_t sa_len = sizeof(struct sockaddr_in);
                    struct sockaddr_in sa;
                    int newclientsock = accept(s0, (struct sockaddr*)&sa, &sa_len);
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

// Sets the cgi environment variables that stay the same across http requests.
void set_cgi_env1(LKHttpServer *server) {
    int z;
    LKHttpServerSettings *settings = server->settings;

    char hostname[LK_BUFSIZE_SMALL];
    z = gethostname(hostname, sizeof(hostname)-1);
    if (z == -1) {
        lk_print_err("gethostname()");
        hostname[0] = '\0';
    }
    hostname[sizeof(hostname)-1] = '\0';
    
    setenv("SERVER_NAME", hostname, 1);
    setenv("SERVER_SOFTWARE", "littlekitten/0.1", 1);
    setenv("DOCUMENT_ROOT", settings->homedir_abspath->s, 1);
    setenv("SERVER_PROTOCOL", "HTTP/1.0", 1);

    char *server_port = "todo";
    setenv("SERVER_PORT", server_port, 1);

}

// Sets the cgi environment variables that vary for each http request.
void set_cgi_env2(LKHttpServer *server, LKHttpServerContext *ctx) {
    LKHttpRequest *req = ctx->req;
    LKHttpServerSettings *settings = server->settings;

    char *http_user_agent = lk_stringtable_get(req->headers, "User-Agent");
    if (!http_user_agent) http_user_agent = "";
    setenv("HTTP_USER_AGENT", http_user_agent, 1);

    char *http_host = lk_stringtable_get(req->headers, "Host");
    if (!http_host) http_host = "";
    setenv("HTTP_HOST", http_host, 1);

    LKString *lkscript_filename = lk_string_new(settings->homedir_abspath->s);
    lk_string_append(lkscript_filename, req->path->s);
    setenv("SCRIPT_FILENAME", lkscript_filename->s, 1);
    lk_string_free(lkscript_filename);

    setenv("REQUEST_METHOD", req->method->s, 1);
    setenv("SCRIPT_NAME", req->path->s, 1);
    setenv("REQUEST_URI", req->uri->s, 1);
    setenv("QUERY_STRING", req->querystring->s, 1);

    char *content_type = "";
    char *content_length = "";
    setenv("CONTENT_TYPE", content_type, 1);
    setenv("CONTENT_LENGTH", content_length, 1);

    setenv("REMOTE_ADDR", ctx->client_ipaddr->s, 1);
    char portstr[10];
    snprintf(portstr, sizeof(portstr), "%d", ctx->client_port);
    setenv("REMOTE_PORT", portstr, 1);
}

void read_request_from_client(LKHttpServer *server, LKHttpServerContext *ctx) {
    char buf[LK_BUFSIZE_LARGE];
    size_t nread;
    int z = 0;

    while (1) {
        if (!ctx->reqparser->head_complete) {
            z = lk_socketreader_readline(ctx->sr, buf, sizeof(buf), &nread);
            if (z == -1) {
                lk_print_err("lksocketreader_readline()");
            }
            if (nread > 0) {
                assert(buf[nread] == '\0');
                lk_httprequestparser_parse_line(ctx->reqparser, buf);
            }
        } else {
            z = lk_socketreader_readbytes(ctx->sr, buf, sizeof(buf), &nread);
            if (z == -1) {
                lk_print_err("lksocketreader_readbytes()");
            }
            if (nread > 0) {
                lk_httprequestparser_parse_bytes(ctx->reqparser, buf, nread);
            }
        }
        // No more data coming in.
        if (ctx->sr->sockclosed) {
            ctx->reqparser->body_complete = 1;
        }
        if (ctx->reqparser->body_complete) {
            FD_CLR_READ(ctx->fd, server);
            if (shutdown(ctx->fd, SHUT_RD) == -1) {
                lk_print_err("read_request_from_client(): shutdown()");
            }
            process_request(server, ctx);
            break;
        }
        if (z == -1 || nread == 0) {
            break;
        }
    }
}

void read_responsefile(LKHttpServer *server, LKHttpServerContext *ctx) {
    char readbuf[LK_BUFSIZE_LARGE];
    char cgiline[LK_BUFSIZE_MEDIUM];

    while (1) {
        // Incrementally read cgi output into ctx->cgibuf
        size_t nread;
        int z = lk_read(ctx->serverfile_fd, readbuf, sizeof(readbuf), &nread);
        if (nread > 0) {
            lk_buffer_append(ctx->cgibuf, readbuf, nread);
        }
        if (z == -1) {
            if (errno != EAGAIN && errno != EWOULDBLOCK) {
                lk_print_err("lk_read()");
            }
            break;
        }

        // EOF - finished reading cgi output.
        if (z == 0 && nread == 0) {
            // Remove server/cgi file from read list.
            FD_CLR_READ(ctx->serverfile_fd, server);
            z = close(ctx->serverfile_fd);
            if (z == -1) {
                lk_print_err("close()");
            }
            ctx->serverfile_fd = 0;

            // Parse cgibuf line by line into http response.
            while (1) {
                if (!ctx->cgiparser->head_complete) {
                    lk_buffer_readline(ctx->cgibuf, cgiline, sizeof(cgiline));
                    lk_chomp(cgiline);
                    lk_httpcgiparser_parse_line(ctx->cgiparser, cgiline);
                    continue;
                }
                lk_httpcgiparser_parse_bytes(
                    ctx->cgiparser,
                    ctx->cgibuf->bytes + ctx->cgibuf->bytes_cur,
                    ctx->cgibuf->bytes_len - ctx->cgibuf->bytes_cur
                );
                break;
            }
            process_response(server, ctx);
            break;
        }
    }
}

void process_request(LKHttpServer *server, LKHttpServerContext *ctx) {
    // Replace path with any matching alias.
    match_aliases(ctx->req->path, server->settings->aliases);

    // Run cgi script if uri falls under cgidir
    if (lk_string_starts_with(ctx->req->uri, server->settings->cgidir->s)) {
        serve_cgi(server, ctx);
        return;
    }

    serve_files(server, ctx);
    process_response(server, ctx);
}

// Replace path with any matching alias.
// Ex. path: "/latest" => "/latest.html"
void match_aliases(LKString *path, LKStringTable *aliases) {
    char *match = lk_stringtable_get(aliases, path->s);
    if (match != NULL) {
        lk_string_assign(path, match);
    }
}

// Generate an http response to an http request.
#define POSTTEST
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
    char *path = req->path->s;

    if (!strcmp(method, "GET")) {
        // /littlekitten sample page
        if (!strcmp(path, "/littlekitten")) {
            lk_httpresponse_add_header(resp, "Content-Type", "text/html");
            lk_buffer_append(resp->body, html_sample, strlen(html_sample));
            return;
        }

        // For root, default to index.html, ...
        if (!strcmp(path, "")) {
            char *default_files[] = {"/index.html", "/index.htm", "/default.html", "/default.htm"};
            for (int i=0; i < sizeof(default_files) / sizeof(char *); i++) {
                z = read_path_file(settings->homedir->s, default_files[i], resp->body);
                if (z >= 0) {
                    lk_httpresponse_add_header(resp, "Content-Type", "text/html");
                    break;
                }
            }
        } else {
            z = read_path_file(settings->homedir->s, path, resp->body);
            char *content_type = (char *) lk_lookup(mimetypes_tbl, fileext(path));
            if (content_type == NULL) {
                content_type = "text/plain";
            }
            lk_httpresponse_add_header(resp, "Content-Type", content_type);
        }
        if (z == -1) {
            // path not found
            resp->status = 404;
            lk_string_assign_sprintf(resp->statustext, "File not found '%s'", path);
            lk_httpresponse_add_header(resp, "Content-Type", "text/plain");
            lk_buffer_append_sprintf(resp->body, "File not found '%s'\n", path);
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

    lk_httpresponse_add_header(resp, "Content-Type", "text/html");
    lk_buffer_append(resp->body, html_error_start, strlen(html_error_start));
    lk_buffer_append_sprintf(resp->body, "<p>Error code %d.</p>\n", resp->status);
    lk_buffer_append_sprintf(resp->body, "<p>Message: Unsupported method ('%s').</p>\n", resp->statustext->s);
    lk_buffer_append(resp->body, html_error_end, strlen(html_error_end));
}

void serve_cgi(LKHttpServer *server, LKHttpServerContext *ctx) {
    LKHttpServerSettings *settings = server->settings;
    LKHttpRequest *req = ctx->req;
    LKHttpResponse *resp = ctx->resp;
    char *path = req->path->s;

    LKString *cgifile = lk_string_new(settings->homedir->s);
    lk_string_append(cgifile, req->path->s);

    // cgi file not found
    if (!lk_file_exists(cgifile->s)) {
        lk_string_free(cgifile);

        resp->status = 404;
        lk_string_assign_sprintf(resp->statustext, "File not found '%s'", path);
        lk_httpresponse_add_header(resp, "Content-Type", "text/plain");
        lk_buffer_append_sprintf(resp->body, "File not found '%s'\n", path);

        process_response(server, ctx);
        return;
    }

    set_cgi_env2(server, ctx);

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

    // Set cgi output to select() readfds to be read in read_responsefile()
    ctx->serverfile_fd = fd_out;
    FD_SET_READ(fd_out, server);
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

// Open <home_dir>/<uri> file in nonblocking mode.
// Returns 0 for success, -1 for error.
int open_path_file(char *home_dir, char *path) {
    // full_path = home_dir + path
    // Ex. "/path/to" + "/index.html"
    LKString *full_path = lk_string_new(home_dir);
    lk_string_append(full_path, path);

    int z = open(full_path->s, O_RDONLY | O_NONBLOCK);
    lk_string_free(full_path);
    return z;
}

// Read <home_dir>/<uri> file into buffer.
// Return number of bytes read or -1 for error.
int read_path_file(char *home_dir, char *path, LKBuffer *buf) {
    // full_path = home_dir + path
    // Ex. "/path/to" + "/index.html"
    LKString *full_path = lk_string_new(home_dir);
    lk_string_append(full_path, path);
    int z = lk_readfile(full_path->s, buf);
    lk_string_free(full_path);
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
        lk_print_err("terminate_client_session(): shutdown()");
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
    ctx->client_port = lk_get_sockaddr_port((struct sockaddr *) sa);
    ctx->sr = lk_socketreader_new(fd, 0);
    ctx->req = lk_httprequest_new();
    ctx->resp = lk_httpresponse_new();
    ctx->reqparser = lk_httprequestparser_new(ctx->req);
    ctx->cgiparser = lk_httpcgiparser_new(ctx->resp);
    ctx->cgibuf = lk_buffer_new(0);

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

    ctx->client_ipaddr = NULL;
    ctx->sr = NULL;
    ctx->reqparser = NULL;
    ctx->cgiparser = NULL;
    ctx->req = NULL;
    ctx->resp = NULL;
    ctx->cgibuf = NULL;
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
    settings->homedir_abspath = lk_string_new("");
    settings->host = lk_string_new("");
    settings->port = lk_string_new("");
    settings->cgidir = lk_string_new("");
    settings->aliases = lk_stringtable_new();
    return settings;
}

void free_serversettings(LKHttpServerSettings *settings) {
    lk_string_free(settings->homedir);
    lk_string_free(settings->homedir_abspath);
    lk_string_free(settings->host);
    lk_string_free(settings->port);
    lk_string_free(settings->cgidir);
    lk_stringtable_free(settings->aliases);

    settings->homedir = NULL;
    settings->homedir_abspath = NULL;
    settings->host = NULL;
    settings->port = NULL;
    settings->cgidir = NULL;
    settings->aliases = NULL;
    free(settings);
}
