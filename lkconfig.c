#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <assert.h>
#include "lklib.h"
#include "lknet.h"

#define HOSTCONFIGS_INITIAL_SIZE 10
#define CONFIG_LINE_SIZE 255

// Read config file and set config structure.
//
// Config file format:
// -------------------
//    host=127.0.0.1
//    port=5000
//
//    # Matches all other hostnames
//    hostname *
//    homedir=/var/www/testsite
//    alias latest=latest.html
//
//    # Matches http://localhost
//    hostname localhost
//    homedir=/var/www/testsite
//
//    # http://littlekitten.xyz
//    hostname littlekitten.xyz
//    homedir=/var/www/testsite
//    cgidir=cgi-bin
//    alias latest=latest.html
//    alias about=about.html
//    alias guestbook=cgi-bin/guestbook.pl
//    alias blog=cgi-bin/blog.pl
//
// Format description:
// The host and port number is defined first, followed by one or more
// host config sections. The host config section always starts with the
// 'hostname <domain>' line followed by the settings for that hostname.
// The section ends on either EOF or when a new 'hostname <domain>' line
// is read, indicating the start of the next host config section.
//
typedef enum {CFG_ROOT, CFG_HOSTSECTION} ParseCfgState;
LKConfig *lk_read_configfile(char *configfile) {
    FILE *f = fopen(configfile, "r");
    if (f == NULL) {
        lk_print_err("lk_read_configfile fopen()");
        return NULL;
    }

    LKConfig *cfg = malloc(sizeof(LKConfig));
    cfg->serverhost = lk_string_new("");
    cfg->port = lk_string_new("");
    cfg->hostconfigs = malloc(sizeof(LKHostConfig*) * HOSTCONFIGS_INITIAL_SIZE);
    cfg->hostconfigs_len = 0;
    cfg->hostconfigs_size = HOSTCONFIGS_INITIAL_SIZE;

    ParseCfgState state = CFG_ROOT;
    state = state;
    char line[CONFIG_LINE_SIZE];

    while (1) {
        char *pz = fgets(line, sizeof(line), f);
        if (pz == NULL) {
            break;
        }
    }

    fclose(f);
    return cfg;
}

void lk_config_add_hostconfig(LKConfig *cfg, LKHostConfig *hc) {
    assert(cfg->hostconfigs_len <= cfg->hostconfigs_size);

    // Increase size if no more space.
    if (cfg->hostconfigs_len == cfg->hostconfigs_size) {
        cfg->hostconfigs_size++;
        cfg->hostconfigs = realloc(cfg->hostconfigs, sizeof(LKHostConfig) * cfg->hostconfigs_size);
    }

    LKHostConfig *p = &cfg->hostconfigs[cfg->hostconfigs_len];
    p->hostname         = hc->hostname;
    p->homedir          = hc->homedir;
    p->homedir_abspath  = hc->homedir_abspath;
    p->cgidir           = hc->cgidir;
    p->aliases          = hc->aliases;
    p->proxyhost        = hc->proxyhost;

    cfg->hostconfigs_len++;
}



