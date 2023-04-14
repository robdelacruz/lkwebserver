#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <pthread.h>

#include "lklib.h"
#include "lknet.h"
#include "lktables.h"

// tbl is a null-terminated array of char* key-value pairs
// Ex. tbl: {"key1", "val1", "key2", "val2", "key3", "val3", NULL}
// where key1/val1, key2/val2, key3/val3 are the key-value pairs.
// Returns matching val to testk, or NULL if no match.
void *lookup(void **tbl, char *testk) {
    void **p = tbl;
    while (*p != NULL) {
        char *k = *p;
        if (k == NULL) break;
        if (!strcmp(k, testk)) {
            return *(p+1); // val
        }
        p += 2; // next key
    }
    return NULL;
}

int main(int argc, char *argv[]) {
    char *v1 = lookup(mimetypes_tbl, "123");
    char *v2 = lookup(mimetypes_tbl, "bz");
    char *v3 = lookup(mimetypes_tbl, "htm");

    printf("v1: '%s' v2: '%s' v3: '%s'\n", v1, v2, v3);
}

