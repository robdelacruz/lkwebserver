#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <assert.h>
#include "lklib.h"

LKStringTable *lk_stringtable_new() {
    LKStringTable *st = malloc(sizeof(LKStringTable));
    st->items_size = 1; // start with room for n items
    st->items_len = 0;

    st->items = malloc(st->items_size * sizeof(LKStringTableItem));
    memset(st->items, 0, st->items_size * sizeof(LKStringTableItem));
    return st;
}

void lk_stringtable_free(LKStringTable *st) {
    assert(st->items != NULL);

    for (int i=0; i < st->items_len; i++) {
        lk_string_free(st->items[i].k);
        lk_string_free(st->items[i].v);
    }
    memset(st->items, 0, st->items_size * sizeof(LKStringTableItem));

    free(st->items);
    st->items = NULL;
    free(st);
}

void lk_stringtable_set(LKStringTable *st, char *ks, char *v) {
    assert(st->items_size >= st->items_len);

    // If item already exists, overwrite it.
    for (int i=0; i < st->items_len; i++) {
        if (lk_string_sz_equal(st->items[i].k, ks)) {
            lk_string_assign(st->items[i].v, v);
            return;
        }
    }

    // If reached capacity, expand the array and add new item.
    if (st->items_len == st->items_size) {
        st->items_size += 10;
        st->items = realloc(st->items, st->items_size * sizeof(LKStringTableItem));
        memset(st->items + st->items_len, 0,
               (st->items_size - st->items_len) * sizeof(LKStringTableItem));
    }

    st->items[st->items_len].k = lk_string_new(ks);
    st->items[st->items_len].v = lk_string_new(v);
    st->items_len++;
}

char *lk_stringtable_get(LKStringTable *st, char *ks) {
    for (int i=0; i < st->items_len; i++) {
        if (lk_string_sz_equal(st->items[i].k, ks)) {
            return st->items[i].v->s;
        }
    }
    return NULL;
}

void lk_stringtable_remove(LKStringTable *st, char *ks) {
    for (int i=0; i < st->items_len; i++) {
        if (lk_string_sz_equal(st->items[i].k, ks)) {
            int num_items_after = st->items_len-i-1;
            memmove(st->items+i, st->items+i+1, num_items_after * sizeof(LKStringTableItem));
            memset(st->items+st->items_len-1, 0, sizeof(LKStringTableItem));
            st->items_len--;
            return;
        }
    }
}
