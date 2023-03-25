#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <assert.h>
#include "lklib.h"

void zero_s(LKString *lks) {
    memset(lks->s, '\0', lks->s_size+1);
}

LKString *lk_string_new(char *s) {
    if (s == NULL) {
        s = "";
    }
    size_t s_len = strlen(s);

    LKString *lks = malloc(sizeof(LKString));
    lks->s_len = s_len;
    lks->s_size = s_len;
    lks->s = malloc(lks->s_size+1);
    zero_s(lks);
    strncpy(lks->s, s, s_len);

    return lks;
}
LKString *lk_string_size_new(size_t size) {
    LKString *lks = malloc(sizeof(LKString));

    lks->s_len = 0;
    lks->s_size = size;
    lks->s = malloc(lks->s_size+1);
    zero_s(lks);

    return lks;
}
void lk_string_free(LKString *lks) {
    assert(lks->s != NULL);

    zero_s(lks);
    free(lks->s);
    lks->s = NULL;
    free(lks);
}
void lk_string_voidp_free(void *plkstr) {
    lk_string_free((LKString *) plkstr);
}

void lk_string_assign(LKString *lks, char *s) {
    size_t s_len = strlen(s);
    if (s_len > lks->s_size) {
        lks->s_size = s_len;
        lks->s = realloc(lks->s, lks->s_size+1);
    }
    zero_s(lks);
    strncpy(lks->s, s, s_len);
    lks->s_len = s_len;
}

void lk_string_sprintf(LKString *lks, char *fmt, ...) {
    va_list args;
    char *ps;
    va_start(args, fmt);
    int z = vasprintf(&ps, fmt, args);
    if (z == -1) return;
    va_end(args);

    lk_string_assign(lks, ps);
    free(ps);
}

void lk_string_append(LKString *lks, char *s) {
    size_t s_len = strlen(s);
    if (lks->s_len + s_len > lks->s_size) {
        lks->s_size = lks->s_len + s_len;
        lks->s = realloc(lks->s, lks->s_size+1);
    }

    memset(lks->s + lks->s_len, '\0', s_len+1);
    strncpy(lks->s + lks->s_len, s, s_len);
    lks->s_len = lks->s_len + s_len;
}

int lk_string_sz_equal(LKString *lks1, char *s2) {
    if (strcmp(lks1->s, s2) == 0) {
        return 1;
    }
    return 0;
}

int lk_string_equal(LKString *lks1, LKString *lks2) {
    return lk_string_sz_equal(lks1, lks2->s);
}

