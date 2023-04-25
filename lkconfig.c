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
//    serverhost=127.0.0.1
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
//    # http://newsboard.littlekitten.xyz
//    hostname newsboard.littlekitten.xyz
//    proxyhost=localhost:8001
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

    char line[CONFIG_LINE_SIZE];
    ParseCfgState state = CFG_ROOT;
    LKString *l = lk_string_new("");
    LKString *k = lk_string_new("");
    LKString *v = lk_string_new("");
    LKString *aliask = lk_string_new("");
    LKString *aliasv = lk_string_new("");
    LKHostConfig *hc = NULL;

    while (1) {
        char *pz = fgets(line, sizeof(line), f);
        if (pz == NULL) {
            break;
        }
        lk_string_assign(l, line);
        lk_string_trim(l);

        // Skip # comment line.
        if (lk_string_starts_with(l, "#")) {
            continue;
        }

        // hostname littlekitten.xyz
        lk_string_split_assign(l, " ", k, v); // l:"k v", assign k and v
        if (lk_string_sz_equal(k, "hostname")) {
            // Add previous hostconfig section
            if (hc != NULL) {
                lk_config_add_hostconfig(cfg, hc);
                hc = NULL;
            }

            hc = lk_hostconfig_new(v->s);
            state = CFG_HOSTSECTION;
            continue;
        }

        if (state == CFG_ROOT) {
            assert(hc == NULL);

            // serverhost=127.0.0.1
            // port=8000
            lk_string_split_assign(l, "=", k, v); // l:"k=v", assign k and v
            if (lk_string_sz_equal(k, "serverhost")) {
                lk_string_assign(cfg->serverhost, v->s);
                continue;
            } else if (lk_string_sz_equal(k, "port")) {
                lk_string_assign(cfg->port, v->s);
                continue;
            }
            continue;
        }
        if (state == CFG_HOSTSECTION) {
            assert(hc != NULL);

            // homedir=testsite
            // cgidir=cgi-bin
            // proxyhost=localhost:8001
            lk_string_split_assign(l, "=", k, v);
            if (lk_string_sz_equal(k, "homedir")) {
                lk_string_assign(hc->homedir, v->s);
                continue;
            } else if (lk_string_sz_equal(k, "cgidir")) {
                lk_string_assign(hc->cgidir, v->s);
                continue;
            } else if (lk_string_sz_equal(k, "proxyhost")) {
                lk_string_assign(hc->proxyhost, v->s);
                continue;
            }
            // alias latest=latest.html
            lk_string_split_assign(l, " ", k, v);
            if (lk_string_sz_equal(k, "alias")) {
                lk_string_split_assign(v, "=", aliask, aliasv);
                lk_stringtable_set(hc->aliases, aliask->s, aliasv->s);
                continue;
            }
            continue;
        }
    }

    // Add last hostconfig section
    if (hc != NULL) {
        lk_config_add_hostconfig(cfg, hc);
        hc = NULL;
    }

    lk_string_free(l);
    lk_string_free(k);
    lk_string_free(v);
    lk_string_free(aliask);
    lk_string_free(aliasv);

    fclose(f);
    return cfg;
}

void lk_config_free(LKConfig *cfg) {
    lk_string_free(cfg->serverhost);
    lk_string_free(cfg->port);
    for (int i=0; i < cfg->hostconfigs_len; i++) {
        LKHostConfig *hc = cfg->hostconfigs[i];
        lk_hostconfig_free(hc);
    }
    memset(cfg->hostconfigs, 0, sizeof(LKHostConfig*) * cfg->hostconfigs_size);
    free(cfg->hostconfigs);

    cfg->serverhost = NULL;
    cfg->port = NULL;
    cfg->hostconfigs = NULL;
    
    free(cfg);
}

void lk_config_add_hostconfig(LKConfig *cfg, LKHostConfig *hc) {
    assert(cfg->hostconfigs_len <= cfg->hostconfigs_size);

    // Increase size if no more space.
    if (cfg->hostconfigs_len == cfg->hostconfigs_size) {
        cfg->hostconfigs_size++;
        cfg->hostconfigs = realloc(cfg->hostconfigs, sizeof(LKHostConfig*) * cfg->hostconfigs_size);
    }
    cfg->hostconfigs[cfg->hostconfigs_len] = hc;
    cfg->hostconfigs_len++;
}

void lk_config_print(LKConfig *cfg) {
    printf("serverhost: %s\n", cfg->serverhost->s);
    printf("port: %s\n", cfg->port->s);

    for (int i=0; i < cfg->hostconfigs_len; i++) {
        LKHostConfig *hc = cfg->hostconfigs[i];
        printf("%2d. hostname %s\n", i+1, hc->hostname->s);
        if (hc->homedir->s_len > 0) {
            printf("    homedir: %s\n", hc->homedir->s);
        }
        if (hc->cgidir->s_len > 0) {
            printf("    cgidir: %s\n", hc->cgidir->s);
        }
        if (hc->proxyhost->s_len > 0) {
            printf("    proxyhost: %s\n", hc->proxyhost->s);
        }
        for (int j=0; j < hc->aliases->items_len; j++) {
            printf("    alias %s=%s\n", hc->aliases->items[j].k->s, hc->aliases->items[j].v->s);
        }
    }
    printf("\n");
}

LKHostConfig *lk_hostconfig_new(char *hostname) {
    LKHostConfig *hc = malloc(sizeof(LKHostConfig));

    hc->hostname = lk_string_new(hostname);
    hc->homedir = lk_string_new("");
    hc->homedir_abspath = lk_string_new("");
    hc->cgidir = lk_string_new("");
    hc->aliases = lk_stringtable_new();
    hc->proxyhost = lk_string_new("");

    return hc;
}

void lk_hostconfig_free(LKHostConfig *hc) {
    lk_string_free(hc->hostname);
    lk_string_free(hc->homedir);
    lk_string_free(hc->homedir_abspath);
    lk_string_free(hc->cgidir);
    lk_stringtable_free(hc->aliases);
    lk_string_free(hc->proxyhost);

    hc->hostname = NULL;
    hc->homedir = NULL;
    hc->homedir_abspath = NULL;
    hc->cgidir = NULL;
    hc->aliases = NULL;
    hc->proxyhost = NULL;

    free(hc);
}

