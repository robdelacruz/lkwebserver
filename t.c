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

struct procfile {
    FILE *f;
    int fd;
};

void *runcgi(void *parg);

int file_exists(char *filename) {
    struct stat statbuf;
    int z = stat(filename, &statbuf);
    return !z;
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        printf("Usage: %s <cgifile>\n", argv[0]);
        exit(1);
    }
    char *cgifile = argv[1];
    if (!file_exists(cgifile)) {
        printf("%s doesn't exist.\n", cgifile);
        exit(1);
    }
    FILE *f1 = popen(cgifile, "r");
    if (f1 == NULL) {
        perror("popen()");
        exit(1);
    }

    int fd = fileno(f1);
    if (fd == -1) {
        perror("fileno()");
        exit(1);
    }

    struct procfile pfile;
    pfile.f = f1;
    pfile.fd = fd;

    printf("pthread_create runcgi()...\n");
    pthread_t t1;
    int z = pthread_create(&t1, NULL, runcgi, &pfile);
    if (z != 0) {
        errno = z;
        perror("pthread_create()");
        exit(1);
    }
/*
    z = pthread_detach(t1);
    if (z != 0) {
        errno = z;
        perror("pthread_detach()");
        exit(1);
    }
*/
/*
    z = pthread_join(t1, NULL);
    if (z != 0) {
        errno = z;
        perror("pthread_join()");
        exit(1);
    }
*/

    sleep(1);
    printf("Exit main.\n");
    return 0;
}

void *runcgi(void *parg) {
    printf("*** Start runcgi() function...\n");
    struct procfile *pfile = (struct procfile *) parg;
    FILE *f = pfile->f;
    int fd = pfile->fd;

    LKBuffer *buf = lk_buffer_new(0);
    lk_readfd(fd, buf);
    for (int i=0; i < buf->bytes_len; i++) {
        putchar(buf->bytes[i]);
    }
    lk_buffer_free(buf);

    pclose(f);
    printf("*** End runcgi() function\n");
    return NULL;
}

