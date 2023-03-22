#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <assert.h>
#include "lklib.h"

void zero_s(lkstr_s *lks) {
    memset(lks->s, '\0', lks->s_size+1);
}

lkstr_s *lkstr_new(char *s) {
    lkstr_s *lks = malloc(sizeof(lkstr_s));

    size_t s_len = strlen(s);
    lks->s_len = s_len;
    lks->s_size = s_len;
    lks->s = malloc(lks->s_size+1);
    zero_s(lks);
    strncpy(lks->s, s, s_len);

    return lks;
}
lkstr_s *lkstr_size_new(size_t size) {
    lkstr_s *lks = malloc(sizeof(lkstr_s));

    lks->s_len = 0;
    lks->s_size = size;
    lks->s = malloc(lks->s_size+1);
    zero_s(lks);

    return lks;
}
void lkstr_free(lkstr_s *lks) {
    assert(lks->s != NULL);

    zero_s(lks);
    free(lks->s);
    lks->s = NULL;
    free(lks);
}

void lkstr_assign(lkstr_s *lks, char *s) {
    size_t s_len = strlen(s);
    if (s_len > lks->s_size) {
        lks->s_size = s_len;
        lks->s = realloc(lks->s, lks->s_size+1);
    }
    zero_s(lks);
    strncpy(lks->s, s, s_len);
    lks->s_len = s_len;
}

void lkstr_sprintf(lkstr_s *lks, char *fmt, ...) {
    va_list args;
    char *ps;
    va_start(args, fmt);
    int z = vasprintf(&ps, fmt, args);
    if (z == -1) return;
    va_end(args);

    lkstr_assign(lks, ps);
    free(ps);
}

void lkstr_append(lkstr_s *lks, char *s) {
    size_t s_len = strlen(s);
    if (lks->s_len + s_len > lks->s_size) {
        lks->s_size = lks->s_len + s_len;
        lks->s = realloc(lks->s, lks->s_size+1);
    }

    memset(lks->s + lks->s_len, '\0', s_len+1);
    strncpy(lks->s + lks->s_len, s, s_len);
    lks->s_len = lks->s_len + s_len;
}

int lkstr_sz_equal(lkstr_s *lks1, char *s2) {
    if (strcmp(lks1->s, s2) == 0) {
        return 1;
    }
    return 0;
}

int lkstr_equal(lkstr_s *lks1, lkstr_s *lks2) {
    return lkstr_sz_equal(lks1, lks2->s);
}

