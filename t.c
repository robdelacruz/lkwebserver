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
    int in_fd;
    int out_fd;
    int err_fd;
};

void *runcgi2(void *parg);

int main(int argc, char *argv[]) {
    if (argc < 2) {
        printf("Usage: %s <cgifile>\n", argv[0]);
        exit(1);
    }
    char *cgifile = argv[1];
    if (!lk_file_exists(cgifile)) {
        printf("%s doesn't exist.\n", cgifile);
        exit(1);
    }

    runcgi2(cgifile);
    runcgi2(cgifile);
    runcgi2(cgifile);

    int z;
    char *filename = "spec.txt";
    char buf[100];
    int fd1 = open(filename, 0);
    z = read(fd1, buf, 10);

    int fd2 = open(filename, 0);
    z = read(fd2, buf, 25);

    z = read(fd1, buf, 7);

    int fd3 = open(filename, 0);
    z = read(fd3, buf, 5);

    printf("main() fd1: %d fd2: %d fd3: %d\n", fd1, fd2, fd3);

}

void *runcgi2(void *parg) {
    printf("*** Start runcgi() function...\n");
    char *cgifile = parg;

    int in, out, err;
    int z = lk_popen3(cgifile, &in, &out, &err);
    if (z == -1) {
        perror("popen3()");
        exit(1);
    }

    printf("runcgi() in: %d out: %d err: %d\n", in, out, err);

    printf("buf_out:\n");
    LKBuffer *buf_out = lk_buffer_new(0);
    lk_readfd(out, buf_out);
    for (int i=0; i < buf_out->bytes_len; i++) {
//        putchar(buf_out->bytes[i]);
    }
    lk_buffer_free(buf_out);

    printf("buf_err:\n");
    LKBuffer *buf_err = lk_buffer_new(0);
    lk_readfd(err, buf_err);
    for (int i=0; i < buf_err->bytes_len; i++) {
//        putchar(buf_err->bytes[i]);
    }
    lk_buffer_free(buf_err);

//    close(in);
//    close(out);
//    close(err);
    printf("*** End runcgi() function\n");
    return NULL;
}

