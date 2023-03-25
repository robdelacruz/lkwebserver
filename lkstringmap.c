#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <assert.h>
#include "lklib.h"

LKStringMap *lk_stringmap_new() {
    LKStringMap *sm = malloc(sizeof(LKStringMap));
    sm->items_size = 1; // start with room for n items
    sm->items_len = 0;
    sm->freev_func = NULL;

    sm->items = malloc(sm->items_size * sizeof(LKStringMapItem));
    memset(sm->items, 0, sm->items_size * sizeof(LKStringMapItem));
    return sm;
}

LKStringMap *lk_stringmap_funcs_new(void (*freefunc)(void*)) {
    LKStringMap *sm = lk_stringmap_new();
    sm->freev_func = freefunc;
    return sm;
}

void lk_stringmap_free(LKStringMap *sm) {
    assert(sm->items != NULL);

    for (int i=0; i < sm->items_len; i++) {
        lk_string_free(sm->items[i].k);

        // Free v, if destructor func was set.
        if (!sm->freev_func) {
            (*sm->freev_func)(sm->items[i].v);
        }
    }
    memset(sm->items, 0, sm->items_size * sizeof(LKStringMapItem));

    free(sm->items);
    sm->items = NULL;
    free(sm);
}

void lk_stringmap_set(LKStringMap *sm, char *ks, void *v) {
    assert(sm->items_size >= sm->items_len);

    // If item already exists, overwrite it.
    for (int i=0; i < sm->items_len; i++) {
        if (lk_string_sz_equal(sm->items[i].k, ks)) {
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
        sm->items = realloc(sm->items, sm->items_size * sizeof(LKStringMapItem));
        memset(sm->items + sm->items_len, 0,
               (sm->items_size - sm->items_len) * sizeof(LKStringMapItem));
    }

    sm->items[sm->items_len].k = lk_string_new(ks);
    sm->items[sm->items_len].v = v;
    sm->items_len++;
}

void *lk_stringmap_get(LKStringMap *sm, char *ks) {
    for (int i=0; i < sm->items_len; i++) {
        if (lk_string_sz_equal(sm->items[i].k, ks)) {
            return sm->items[i].v;
        }
    }
    return NULL;
}

void lk_stringmap_remove(LKStringMap *sm, char *ks) {
    for (int i=0; i < sm->items_len; i++) {
        if (lk_string_sz_equal(sm->items[i].k, ks)) {
            int num_items_after = sm->items_len-i-1;
            memmove(sm->items+i, sm->items+i+1, num_items_after * sizeof(LKStringMapItem));
            memset(sm->items+sm->items_len-1, 0, sizeof(LKStringMapItem));
            sm->items_len--;
            return;
        }
    }
}
