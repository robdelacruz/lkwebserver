#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <assert.h>
#include "lklib.h"

lkstringmap_s *lkstringmap_new() {
    lkstringmap_s *sm = malloc(sizeof(lkstringmap_s));
    sm->items_size = 1; // start with room for n items
    sm->items_len = 0;
    sm->freev_func = NULL;

    sm->items = malloc(sm->items_size * sizeof(lkstringmap_item_s));
    memset(sm->items, 0, sm->items_size * sizeof(lkstringmap_item_s));
    return sm;
}

lkstringmap_s *lkstringmap_funcs_new(void (*freefunc)(void*)) {
    lkstringmap_s *sm = lkstringmap_new();
    sm->freev_func = freefunc;
    return sm;
}

void lkstringmap_free(lkstringmap_s *sm) {
    assert(sm->items != NULL);

    for (int i=0; i < sm->items_len; i++) {
        lkstr_free(sm->items[i].k);

        // Free v, if destructor func was set.
        if (!sm->freev_func) {
            (*sm->freev_func)(sm->items[i].v);
        }
    }
    memset(sm->items, 0, sm->items_size * sizeof(lkstringmap_item_s));

    free(sm->items);
    sm->items = NULL;
    free(sm);
}

void lkstringmap_set(lkstringmap_s *sm, char *ks, void *v) {
    assert(sm->items_size >= sm->items_len);

    // If item already exists, overwrite it.
    for (int i=0; i < sm->items_len; i++) {
        if (lkstr_sz_equal(sm->items[i].k, ks)) {
            // Free v, if destructor func was set.
            if (!sm->freev_func) {
                (*sm->freev_func)(sm->items[i].v);
            }
            sm->items[i].v = v;
            return;
        }
    }

    // If reached capacity, expand the array and add new item.
    if (sm->items_len == sm->items_size) {
        sm->items_size += 10;
        sm->items = realloc(sm->items, sm->items_size * sizeof(lkstringmap_item_s));
        memset(sm->items + sm->items_len, 0,
               (sm->items_size - sm->items_len) * sizeof(lkstringmap_item_s));
    }

    sm->items[sm->items_len].k = lkstr_new(ks);
    sm->items[sm->items_len].v = v;
    sm->items_len++;
}

void *lkstringmap_get(lkstringmap_s *sm, char *ks) {
    for (int i=0; i < sm->items_len; i++) {
        if (lkstr_sz_equal(sm->items[i].k, ks)) {
            return sm->items[i].v;
        }
    }
    return NULL;
}

void lkstringmap_remove(lkstringmap_s *sm, char *ks) {
    for (int i=0; i < sm->items_len; i++) {
        if (lkstr_sz_equal(sm->items[i].k, ks)) {
            int num_items_after = sm->items_len-i-1;
            memmove(sm->items+i, sm->items+i+1, num_items_after * sizeof(lkstringmap_item_s));
            memset(sm->items+sm->items_len-1, 0, sizeof(lkstringmap_item_s));
            sm->items_len--;
            return;
        }
    }
}
