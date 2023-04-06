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

#define LISTEN_PORT "5000"

void handle_sigint(int sig);
void handle_sigchld(int sig);
void parse_args(int argc, char *argv[], LKHttpServerSettings *settings);
void parse_args_alias(char *arg, LKStringMap *aliases);

int main(int argc, char *argv[]) {
    int z;

    signal(SIGPIPE, SIG_IGN);           // Don't abort on SIGPIPE
    signal(SIGINT, handle_sigint);      // exit on CTRL-C
    signal(SIGCHLD, handle_sigchld);

    // Get this server's address.
    struct addrinfo hints, *servaddr;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;
    z = getaddrinfo("localhost", LISTEN_PORT, &hints, &servaddr);
    if (z != 0) {
        lk_exit_err("getaddrinfo()");
    }

    int s0 = socket(servaddr->ai_family, servaddr->ai_socktype, servaddr->ai_protocol);
    if (s0 == -1) {
        lk_exit_err("socket()");
    }

    int yes=1;
    z = setsockopt(s0, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));
    if (s0 == -1) {
        lk_exit_err("setsockopt(SO_REUSEADDR)");
    }

    z = bind(s0, servaddr->ai_addr, servaddr->ai_addrlen);
    if (z != 0) {
        lk_exit_err("bind()");
    }

    // $ lkws /testsite -a latest=/latest.html -a about=/about.html
    LKHttpServerSettings *settings = lk_httpserver_settings_new();
    parse_args(argc, argv, settings);

    // If home_dir not specified, use current working directory.
    if (settings->home_dir->s_len == 0) {
        char *current_dir = get_current_dir_name();
        current_dir = NULL;
        if (current_dir != NULL) {
            lk_string_assign(settings->home_dir, current_dir);
            free(current_dir);
        } else {
            lk_string_assign(settings->home_dir, ".");
        }
    }
    if (settings->cgi_dir->s_len == 0) {
        lk_string_append(settings->cgi_dir, "/cgi-bin");
    }
    lk_string_prepend(settings->cgi_dir, settings->home_dir->s);
    printf("home_dir: '%s', cgi_dir: '%s'\n", settings->home_dir->s, settings->cgi_dir->s);

    LKHttpServer *httpserver = lk_httpserver_new(settings);

    LKString *server_ipaddr_str = lk_get_ipaddr_string(servaddr->ai_addr);
    printf("Serving HTTP on %s port %s...\n", server_ipaddr_str->s, LISTEN_PORT);
    lk_string_free(server_ipaddr_str);
    freeaddrinfo(servaddr);

    z = lk_httpserver_serve(httpserver, s0);
    if (z == -1) {
        lk_exit_err("lk_httpserver_serve()");
    }

    // Code doesn't reach here.
    lk_httpserver_settings_free(settings);
    lk_httpserver_free(httpserver);
    close(s0);
    return 0;
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

typedef enum {PA_NONE, PA_ALIAS} ParseArgsState;

// $ lkws testsite/ -a /latest=/latest.html -a about=/about.html
// $ lkws testsite/ -a latest=foo/latest.html -a about=about.html
// $ lkws testsite/ --cgidir=cgifolder
void parse_args(int argc, char *argv[], LKHttpServerSettings *settings) {
    ParseArgsState state = PA_NONE;

    for (int i=1; i < argc; i++) {
        char *arg = argv[i];
        if (state == PA_NONE && !strcmp(arg, "-a")) {
            state = PA_ALIAS;
            continue;
        }
        // --cgidir=cgifolder
        if (state == PA_NONE && !strncmp(arg, "--cgidir=", 9)) {
            LKString *lksarg = lk_string_new(arg);
            LKStringList *parts = lk_string_split(lksarg, "=");
            if (parts->items_len == 2) {
                LKString *v = lk_stringlist_get(parts, 1);
                if (!lk_string_starts_with(v, "/")) {
                    lk_string_prepend(v, "/");
                }
                lk_string_assign(settings->cgi_dir, v->s);
            }
            lk_stringlist_free(parts);
            lk_string_free(lksarg);
            continue;
        }
        if (state == PA_ALIAS) {
            parse_args_alias(arg, settings->aliases);
            state = PA_NONE;
            continue;
        }
        assert(state == PA_NONE);
        lk_string_assign(settings->home_dir, arg);
        continue;
    }

    lk_string_chop_end(settings->home_dir, "/");
    lk_string_chop_end(settings->cgi_dir, "/");
}

// Parse and add new alias definition into aliases
// alias definition "/latest=/latest.html" ==> "/latest" : "/latest.html"
void parse_args_alias(char *arg, LKStringMap *aliases) {
    LKString *lksarg = lk_string_new(arg);
    LKStringList *parts = lk_string_split(lksarg, "=");
    if (parts->items_len == 2) {
        LKString *k = lk_stringlist_get(parts, 0);
        LKString *v = lk_stringlist_get(parts, 1);

        if (!lk_string_starts_with(k, "/")) {
            lk_string_prepend(k, "/");
        }
        if (!lk_string_starts_with(v, "/")) {
            lk_string_prepend(v, "/");
        }
        lk_stringmap_set(aliases, k->s, lk_string_new(v->s));
    }

    lk_stringlist_free(parts);
    lk_string_free(lksarg);
}

LKHttpServerSettings *lk_httpserver_settings_new() {
    LKHttpServerSettings *settings = malloc(sizeof(LKHttpServerSettings));
    memset(settings, 0, sizeof(LKHttpServerSettings));
    settings->home_dir = lk_string_new("");
    settings->cgi_dir = lk_string_new("");
    settings->aliases = lk_stringmap_funcs_new(lk_string_voidp_free);
    return settings;
}

void lk_httpserver_settings_free(LKHttpServerSettings *settings) {
    lk_string_free(settings->home_dir);
    lk_string_free(settings->cgi_dir);
    lk_stringmap_free(settings->aliases);
    free(settings);
}


