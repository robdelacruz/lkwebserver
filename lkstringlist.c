#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <assert.h>
#include "lklib.h"

#define N_GROW_STRINGLIST 10

LKStringList *lk_stringlist_new() {
    LKStringList *sl = malloc(sizeof(LKStringList));
    sl->items_size = 1; // start with room for n items
    sl->items_len = 0;

    sl->items = malloc(sl->items_size * sizeof(LKString*));
    memset(sl->items, 0, sl->items_size * sizeof(LKString*));
    return sl;
}

void lk_stringlist_free(LKStringList *sl) {
    assert(sl->items != NULL);

    for (int i=0; i < sl->items_len; i++) {
        lk_string_free(sl->items[i]);
    }
    memset(sl->items, 0, sl->items_size * sizeof(LKString*));

    free(sl->items);
    sl->items = NULL;
    free(sl);
}

void lk_stringlist_append_lkstring(LKStringList *sl, LKString *lks) {
    assert(sl->items_len <= sl->items_size);

    if (sl->items_len == sl->items_size) {
        LKString **pitems = realloc(sl->items, (sl->items_size+N_GROW_STRINGLIST) * sizeof(LKString*));
        if (pitems == NULL) {
            return;
        }
        sl->items = pitems;
        sl->items_size += N_GROW_STRINGLIST;
    }
    sl->items[sl->items_len] = lks;
    sl->items_len++;

    assert(sl->items_len <= sl->items_size);
}

void lk_stringlist_append(LKStringList *sl, char *s) {
    lk_stringlist_append_lkstring(sl, lk_string_new(s));
}

void lk_stringlist_append_sprintf(LKStringList *sl, const char *fmt, ...) {
    va_list args;
    char *ps;
    va_start(args, fmt);
    int z = vasprintf(&ps, fmt, args);
    va_end(args);
    if (z == -1) return;

    lk_stringlist_append(sl, ps);
    free(ps);
}

LKString *lk_stringlist_get(LKStringList *sl, unsigned int i) {
    if (sl->items_len == 0) return NULL;
    if (i >= sl->items_len) return NULL;

    return sl->items[i];
}

void lk_stringlist_remove(LKStringList *sl, unsigned int i) {
    if (sl->items_len == 0) return;
    if (i >= sl->items_len) return;

    lk_string_free(sl->items[i]);

    int num_items_after = sl->items_len-i-1;
    memmove(sl->items+i, sl->items+i+1, num_items_after * sizeof(LKString*));
    memset(sl->items+sl->items_len-1, 0, sizeof(LKString*));
    sl->items_len--;
}


