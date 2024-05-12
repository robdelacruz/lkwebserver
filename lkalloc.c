#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <assert.h>

#define ALLOCITEMS_SIZE 8192
struct allocitem {
    void *p;
    char *label;
};
static struct allocitem allocitems[ALLOCITEMS_SIZE];

void lk_alloc_init() {
    memset(allocitems, 0, sizeof(allocitems));
}

// Add p to end of allocitems[].
static void add_p(void *p, char *label) {
    int i;
    for (i=0; i < ALLOCITEMS_SIZE; i++) {
        if (allocitems[i].p == NULL) {
            allocitems[i].p = p;
            allocitems[i].label = label;
            break;
        }
    }
    assert(i < ALLOCITEMS_SIZE);
}

#if 0
// Replace matching allocitems[] p with newp.
static void replace_p(void *p, void *newp, char *label) {
    int i;
    for (i=0; i < ALLOCITEMS_SIZE; i++) {
        if (allocitems[i].p ==  p) {
            allocitems[i].p = newp;
            allocitems[i].label = label;
            break;
        }
    }
    assert(i < ALLOCITEMS_SIZE);
}
#endif

// Clear matching allocitems[] p.
static void clear_p(void *p) {
    int i;
    for (i=0; i < ALLOCITEMS_SIZE; i++) {
        if (allocitems[i].p == p) {
            allocitems[i].p = NULL;
            allocitems[i].label = NULL;
            break;
        }
    }
    if (i >= ALLOCITEMS_SIZE) {
        printf("clear_p i: %d\n", i);
    }
    assert(i < ALLOCITEMS_SIZE);
}

void *lk_malloc(size_t size, char *label) {
    void *p = malloc(size);
    add_p(p, label);
    return p;
}

void *lk_realloc(void *p, size_t size, char *label) {
    clear_p(p);
    void *newp = realloc(p, size);
    add_p(newp, label);
    return newp;
}

void lk_free(void *p) {
    clear_p(p);
    free(p);
}

char *lk_strdup(const char *s, char *label) {
    char *sdup = strdup(s);
    add_p(sdup, label);
    return sdup;
}

char *lk_strndup(const char *s, size_t n, char *label) {
    char *sdup = strndup(s, n);
    add_p(sdup, label);
    return sdup;
}

void lk_print_allocitems() {
    printf("allocitems[] labels:\n");
    for (int i=0; i < ALLOCITEMS_SIZE; i++) {
        if (allocitems[i].p != NULL) {
            printf("%s\n", allocitems[i].label);
        }
    }
}

