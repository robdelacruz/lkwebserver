#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <assert.h>
#include <ctype.h>
#include "lklib.h"

// Zero out entire size
void zero_s(LKString *lks) {
    memset(lks->s, 0, lks->s_size+1);
}
// Zero out the free unused space
void zero_unused_s(LKString *lks) {
    memset(lks->s + lks->s_len, '\0', lks->s_size+1 - lks->s_len);
}

LKString *lk_string_new(char *s) {
    if (s == NULL) {
        s = "";
    }
    size_t s_len = strlen(s);

    LKString *lks = lk_malloc(sizeof(LKString), "lk_string_new");
    lks->s_len = s_len;
    lks->s_size = s_len;
    lks->s = lk_malloc(lks->s_size+1, "lk_string_new_s");
    zero_s(lks);
    strncpy(lks->s, s, s_len);

    return lks;
}
LKString *lk_string_size_new(size_t size) {
    LKString *lks = lk_malloc(sizeof(LKString), "lk_string_size_new");

    lks->s_len = 0;
    lks->s_size = size;
    lks->s = lk_malloc(lks->s_size+1, "lk_string_size_new_s");
    zero_s(lks);

    return lks;
}
void lk_string_free(LKString *lks) {
    assert(lks->s != NULL);

    zero_s(lks);
    lk_free(lks->s);
    lks->s = NULL;
    lk_free(lks);
}
void lk_string_voidp_free(void *plkstr) {
    lk_string_free((LKString *) plkstr);
}

void lk_string_assign(LKString *lks, char *s) {
    size_t s_len = strlen(s);
    if (s_len > lks->s_size) {
        lks->s_size = s_len;
        lks->s = lk_realloc(lks->s, lks->s_size+1, "lk_string_assign");
    }
    zero_s(lks);
    strncpy(lks->s, s, s_len);
    lks->s_len = s_len;
}

void lk_string_assign_sprintf(LKString *lks, char *fmt, ...) {
    char sbuf[LK_BUFSIZE_MEDIUM];

    // Try to use static buffer first, to avoid allocation.
    va_list args;
    va_start(args, fmt);
    int z = vsnprintf(sbuf, sizeof(sbuf), fmt, args);
    va_end(args);
    if (z < 0) return;

    // If snprintf() truncated output to sbuf due to space,
    // use asprintf() instead.
    if (z >= sizeof(sbuf)) {
        va_list args;
        char *ps;
        va_start(args, fmt);
        int z = vasprintf(&ps, fmt, args);
        va_end(args);
        if (z == -1) return;

        lk_string_assign(lks, ps);
        free(ps);
        return;
    }

    lk_string_assign(lks, sbuf);
}

//$$ Duplicate code from lk_string_sprintf().
void lk_string_append_sprintf(LKString *lks, char *fmt, ...) {
    char sbuf[LK_BUFSIZE_MEDIUM];

    // Try to use static buffer first, to avoid allocation.
    va_list args;
    va_start(args, fmt);
    int z = vsnprintf(sbuf, sizeof(sbuf), fmt, args);
    va_end(args);
    if (z < 0) return;

    // If snprintf() truncated output to sbuf due to space,
    // use asprintf() instead.
    if (z >= sizeof(sbuf)) {
        va_list args;
        char *ps;
        va_start(args, fmt);
        int z = vasprintf(&ps, fmt, args);
        va_end(args);
        if (z == -1) return;

        lk_string_assign(lks, ps);
        free(ps);
        return;
    }

    lk_string_append(lks, sbuf);
}

void lk_string_append(LKString *lks, char *s) {
    size_t s_len = strlen(s);
    if (lks->s_len + s_len > lks->s_size) {
        lks->s_size = lks->s_len + s_len;
        lks->s = lk_realloc(lks->s, lks->s_size+1, "lk_string_append");
        zero_unused_s(lks);
    }

    strncpy(lks->s + lks->s_len, s, s_len);
    lks->s_len = lks->s_len + s_len;
}

void lk_string_append_char(LKString *lks, char c) {
    if (lks->s_len + 1 > lks->s_size) {
        // Grow string by ^2
        lks->s_size = lks->s_len + ((lks->s_len+1) * 2);
        lks->s = lk_realloc(lks->s, lks->s_size+1, "lk_string_append_char");
        zero_unused_s(lks);
    }

    lks->s[lks->s_len] = c;
    lks->s[lks->s_len+1] = '\0';
    lks->s_len++;
}

void lk_string_prepend(LKString *lks, char *s) {
    size_t s_len = strlen(s);
    if (lks->s_len + s_len > lks->s_size) {
        lks->s_size = lks->s_len + s_len;
        lks->s = lk_realloc(lks->s, lks->s_size+1, "lk_string_prepend");
        zero_unused_s(lks);
    }

    memmove(lks->s + s_len, lks->s, lks->s_len); // shift string to right
    strncpy(lks->s, s, s_len);                   // prepend s to string
    lks->s_len = lks->s_len + s_len;
}


int lk_string_sz_equal(LKString *lks1, char *s2) {
    if (!strcmp(lks1->s, s2)) {
        return 1;
    }
    return 0;
}

int lk_string_equal(LKString *lks1, LKString *lks2) {
    return lk_string_sz_equal(lks1, lks2->s);
}

// Return if string starts with s.
int lk_string_starts_with(LKString *lks, char *s) {
    size_t s_len = strlen(s);
    if (s_len > lks->s_len) {
        return 0;
    }
    if (!strncmp(lks->s, s, s_len)) {
        return 1;
    }
    return 0;
}

// Return if string ends with s.
int lk_string_ends_with(LKString *lks, char *s) {
    size_t s_len = strlen(s);
    if (s_len > lks->s_len) {
        return 0;
    }
    size_t i = lks->s_len - s_len;
    if (!strncmp(lks->s + i, s, s_len)) {
        return 1;
    }
    return 0;
}

// Remove leading and trailing white from string.
void lk_string_trim(LKString *lks) {
    if (lks->s_len == 0) {
        return;
    }

    // starti: index to first non-whitespace char
    // endi: index to last non-whitespace char
    int starti = 0;
    int endi = lks->s_len-1;
    assert(endi >= starti);

    for (int i=0; i < lks->s_len; i++) {
        if (!isspace(lks->s[i])) {
            break;
        }
        starti++;
    }
    // All chars are whitespace.
    if (starti >= lks->s_len) {
        lk_string_assign(lks, "");
        return;
    }
    for (int i=lks->s_len-1; i >= 0; i--) {
        if (!isspace(lks->s[i])) {
            break;
        }
        endi--;
    }
    assert(endi >= starti);

    size_t new_len = endi-starti+1;
    memmove(lks->s, lks->s + starti, new_len);
    memset(lks->s + new_len, 0, lks->s_len - new_len);
    lks->s_len = new_len;
}

void lk_string_chop_start(LKString *lks, char *s) {
    size_t s_len = strlen(s);
    if (s_len > lks->s_len) {
        return;
    }
    if (strncmp(lks->s, s, s_len)) {
        return;
    }

    size_t new_len = lks->s_len - s_len;
    memmove(lks->s, lks->s + s_len, lks->s_len - s_len);
    memset(lks->s + new_len, 0, lks->s_len - new_len);
    lks->s_len = new_len;
}

void lk_string_chop_end(LKString *lks, char *s) {
    size_t s_len = strlen(s);
    if (s_len > lks->s_len) {
        return;
    }
    if (strncmp(lks->s + lks->s_len - s_len, s, s_len)) {
        return;
    }

    size_t new_len = lks->s_len - s_len;
    memset(lks->s + new_len, 0, s_len);
    lks->s_len = new_len;
}

// Return whether delim matches the first delim_len chars of s.
int match_delim(char *s, char *delim, size_t delim_len) {
    return !strncmp(s, delim, delim_len);
}

LKStringList *lk_string_split(LKString *lks, char *delim) {
    LKStringList *sl = lk_stringlist_new();
    size_t delim_len = strlen(delim);

    LKString *segment = lk_string_new("");
    for (int i=0; i < lks->s_len; i++) {
        if (match_delim(lks->s + i, delim, delim_len)) {
            lk_stringlist_append_lkstring(sl, segment);
            segment = lk_string_new("");
            i += delim_len-1; // need to -1 to account for for loop incrementor i++
            continue;
        }

        lk_string_append_char(segment, lks->s[i]);
    }
    lk_stringlist_append_lkstring(sl, segment);

    return sl;
}

// Given a "k<delim>v" string, assign k and v.
void lk_string_split_assign(LKString *s, char *delim, LKString *k, LKString *v) {
    LKStringList *ss = lk_string_split(s, delim);
    if (k != NULL) {
        lk_string_assign(k, ss->items[0]->s);
    }
    if (v != NULL) {
        if (ss->items_len >= 2) {
            lk_string_assign(v, ss->items[1]->s);
        } else {
            lk_string_assign(v, "");
        }
    }
    lk_stringlist_free(ss);
}

void sz_string_split_assign(char *s, char *delim, LKString *k, LKString *v) {
    LKString *lks = lk_string_new(s);
    lk_string_split_assign(lks, delim, k, v);
    lk_string_free(lks);
}

