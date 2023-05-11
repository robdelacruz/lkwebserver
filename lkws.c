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

void handle_sigint(int sig);
void handle_sigchld(int sig);
int parse_args(int argc, char *argv[], LKConfig *cfg);
void print_help();
void print_sample_config();

LKHttpServer *httpserver;

// lkws [homedir] [port] [host] [-f configfile] [--cgidir=cgifolder]
//
// configfile = configuration file containing site settings
//              see sample configuration file below
// homedir    = absolute or relative path to a home directory
//              defaults to current working directory if not specified
// port       = port number to bind to server
//              defaults to 8000
// host       = IP address to bind to server
//              defaults to localhost
// Examples:
// lkws ./testsite/ 8080
// lkws /var/www/testsite/ 8080 127.0.0.1
// lkws /var/www/testsite/ --cgidir=cgi-bin
// lkws -f sites.conf
int main(int argc, char *argv[]) {
    int z;
    signal(SIGPIPE, SIG_IGN);           // Don't abort on SIGPIPE
    signal(SIGINT, handle_sigint);      // exit on CTRL-C
    signal(SIGCHLD, handle_sigchld);


    lk_alloc_init();
    LKConfig *cfg = lk_config_new();
    z = parse_args(argc, argv, cfg);
    if (z == 1) {
        lk_config_free(cfg);
        print_help();
        exit(0);
    }

    printf("Little Kitten Web Server version 0.9 (%s -h for instructions)\n", argv[0]);
    httpserver = lk_httpserver_new(cfg);

    z = lk_httpserver_serve(httpserver);
    if (z == -1) {
        return z;
    }

    // Code doesn't reach here.
    lk_httpserver_free(httpserver);
    return 0;
}

void handle_sigint(int sig) {
    printf("SIGINT received\n");
    fflush(stdout);
    lk_httpserver_free(httpserver);

    lk_print_allocitems();
    exit(0);
}

void handle_sigchld(int sig) {
    int tmp_errno = errno;
    while (waitpid(-1, NULL, WNOHANG) > 0) {
    }
    errno = tmp_errno;
}

void print_help() {
    printf(
"Usage:\n"
"lkws [homedir] [port] [host] [-f configfile] [--cgidir=cgifolder]\n"
"\n"
"configfile = configuration file containing site settings\n"
"             see sample configuration file below\n"
"homedir    = absolute or relative path to a home directory\n"
"             defaults to current working directory if not specified\n"
"port       = port number to bind to server\n"
"             defaults to 8000\n"
"host       = IP address to bind to server\n"
"             defaults to localhost\n"
"Examples:\n"
"lkws ./testsite/ 8080\n"
"lkws /var/www/testsite/ 8080 127.0.0.1\n"
"lkws /var/www/testsite/ --cgidir=cgi-bin\n"
"lkws -f sites.conf\n"
"\n"
"Source code and docs at https://github.com/robdelacruz/lkwebserver\n"
"\n"
    );
}

void print_sample_config() {
    printf(
"Sample config file:\n"
"\n"
"serverhost=127.0.0.1\n"
"port=5000\n"
"\n"
"# Matches all other hostnames\n"
"hostname *\n"
"homedir=/var/www/testsite\n"
"alias latest=latest.html\n"
"\n"
"# Matches http://localhost\n"
"hostname localhost\n"
"homedir=/var/www/testsite\n"
"\n"
"# http://littlekitten.xyz\n"
"hostname littlekitten.xyz\n"
"homedir=/var/www/testsite\n"
"cgidir=cgi-bin\n"
"alias latest=latest.html\n"
"alias about=about.html\n"
"alias guestbook=cgi-bin/guestbook.pl\n"
"alias blog=cgi-bin/blog.pl\n"
"\n"
"# http://newsboard.littlekitten.xyz\n"
"hostname newsboard.littlekitten.xyz\n"
"proxyhost=localhost:8001\n"
"\n"
"# Format description:\n"
"# The host and port number is defined first, followed by one or more\n"
"# host config sections. The host config section always starts with the\n"
"# 'hostname <domain>' line followed by the settings for that hostname.\n"
"# The section ends on either EOF or when a new 'hostname <domain>' line\n"
"# is read, indicating the start of the next host config section.\n"
"\n"
    );
}

typedef enum {PA_NONE, PA_FILE} ParseArgsState;

// lkws [homedir] [port] [host] [--cgidir=<cgidir>] [-f <configfile>]
// homedir    = absolute or relative path to a home directory
//              defaults to current working directory if not specified
// port       = port number to bind to server
//              defaults to 8000
// host       = IP address to bind to server
//              defaults to localhost
// cgidir     = root directory for cgi files, relative path to homedir
//              defaults to cgi-bin
// configfile = plaintext file containing configuration options
//
// Examples:
// lkws ./testsite/ 8080
// lkws /var/www/testsite/ 8080 127.0.0.1
// lkws /var/www/testsite/ --cgidir=guestbook
// lkws -f littlekitten.conf
int parse_args(int argc, char *argv[], LKConfig *cfg) {
    ParseArgsState state = PA_NONE;
    int is_homedir_set = 0;
    int is_port_set = 0;
    int is_host_set = 0;

    for (int i=1; i < argc; i++) {
        char *arg = argv[i];
        if (state == PA_NONE && !strcmp(arg, "-f")) {
            state = PA_FILE;
            continue;
        }
        if (state == PA_NONE &&
            (!strcmp(arg, "-h") || !strcmp(arg, "--help"))) {
            return 1;
        }
        // --cgidir=cgifolder
        if (state == PA_NONE && !strncmp(arg, "--cgidir=", 9)) {
            LKHostConfig *default_hc = lk_config_create_get_hostconfig(cfg, "*");
            LKString *v = lk_string_new("");
            sz_string_split_assign(arg, "=", NULL, v);
            lk_string_assign(default_hc->cgidir, v->s);
            lk_string_free(v);
            continue;
        }
        if (state == PA_FILE) {
            lk_config_read_configfile(cfg, arg);
            state = PA_NONE;
            continue;
        }
        assert(state == PA_NONE);
        if (!is_homedir_set) {
            LKHostConfig *hc0 = lk_config_create_get_hostconfig(cfg, "*");
            lk_string_assign(hc0->homedir, arg);
            is_homedir_set = 1;

            // cgidir default to cgi-bin
            if (hc0->cgidir->s_len == 0) {
                lk_string_assign(hc0->cgidir, "/cgi-bin/");
            }
        } else if (!is_port_set) {
            lk_string_assign(cfg->port, arg);
            is_port_set = 1;
        } else if (!is_host_set) {
            lk_string_assign(cfg->serverhost, arg);
            is_host_set = 1;
        }
    }
    return 0;
}

