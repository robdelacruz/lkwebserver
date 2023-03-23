#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <assert.h>
#include "lklib.h"

lkstringlist_s *lkstringlist_new() {
    lkstringlist_s *sl = malloc(sizeof(lkstringlist_s));
    sl->items_size = 1; // start with room for n items
    sl->items_len = 0;

    sl->items = malloc(sl->items_size * sizeof(lkstr_s*));
    memset(sl->items, 0, sl->items_size * sizeof(lkstr_s*));
    return sl;
}

void lkstringlist_free(lkstringlist_s *sl) {
    assert(sl->items != NULL);

    for (int i=0; i < sl->items_len; i++) {
        lkstr_free(sl->items[i]);
    }
    memset(sl->items, 0, sl->items_size * sizeof(lkstr_s*));

    free(sl->items);
    sl->items = NULL;
    free(sl);
}

#define N_GROW_ITEMS 1
void lkstringlist_append(lkstringlist_s *sl, char *s) {
    assert(sl->items_len <= sl->items_size);

    if (sl->items_len == sl->items_size) {
        lkstr_s **pitems = realloc(sl->items, (sl->items_size+N_GROW_ITEMS) * sizeof(lkstr_s*));
        if (pitems == NULL) {
            return;
        }
        sl->items = pitems;
        sl->items_size += N_GROW_ITEMS;
    }
    sl->items[sl->items_len] = lkstr_new(s);
    sl->items_len++;

    assert(sl->items_len <= sl->items_size);
}

void lkstringlist_append_sprintf(lkstringlist_s *sl, const char *fmt, ...) {
    int z;
    char *pstr = NULL;

    va_list args;
    va_start(args, fmt);
    z = vasprintf(&pstr, fmt, args);
    va_end(args);

    if (z == -1) return;

    lkstringlist_append(sl, pstr);

    free(pstr);
    pstr = NULL;
}

lkstr_s *lkstringlist_get(lkstringlist_s *sl, unsigned int i) {
    if (sl->items_len == 0) return NULL;
    if (i >= sl->items_len) return NULL;

    return sl->items[i];
}

void lkstringlist_remove(lkstringlist_s *sl, unsigned int i) {
    if (sl->items_len == 0) return;
    if (i >= sl->items_len) return;

    lkstr_free(sl->items[i]);

    int num_items_after = sl->items_len-i-1;
    memmove(sl->items+i, sl->items+i+1, num_items_after * sizeof(lkstr_s*));
    memset(sl->items+sl->items_len-1, 0, sizeof(lkstr_s*));
    sl->items_len--;
}


