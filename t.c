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

void *runcgi(void *parg);

int file_exists(char *filename) {
    struct stat statbuf;
    int z = stat(filename, &statbuf);
    return !z;
}

void close_pipes(int pair1[2], int pair2[2], int pair3[2]) {
    int tmp_errno = errno;
    if (pair1[0] != 0) {
        close(pair1[0]);
    }
    if (pair1[1] != 0) {
        close(pair1[1]);
    }
    if (pair2[0] != 0) {
        close(pair2[0]);
    }
    if (pair2[1] != 0) {
        close(pair2[1]);
    }
    if (pair3[0] != 0) {
        close(pair3[0]);
    }
    if (pair3[1] != 0) {
        close(pair3[1]);
    }
    errno = tmp_errno;
    return;
}

int popen3(char *cmd, int *fd_in, int *fd_out, int *fd_err) {
    int z;
    int in[2] = {0, 0};
    int out[2] = {0, 0};
    int err[2] = {0, 0};

    z = pipe(in);
    if (z == -1) {
        return z;
    }
    z = pipe(out);
    if (z == -1) {
        close_pipes(in, out, err);
        return z;
    }
    z = pipe(err);
    if (z == -1) {
        close_pipes(in, out, err);
        return z;
    }

    int pid = fork();
    if (pid == 0) {
        // child proc
        z = dup2(in[0], STDIN_FILENO);
        if (z == -1) {
            close_pipes(in, out, err);
            return z;
        }
        z = dup2(out[1], STDOUT_FILENO);
        if (z == -1) {
            close_pipes(in, out, err);
            return z;
        }
        z = dup2(err[1], STDERR_FILENO);
        if (z == -1) {
            close_pipes(in, out, err);
            return z;
        }

        close_pipes(in, out, err);
        z = execl("/bin/sh", "sh",  "-c", cmd, NULL);
        return z;
    }

    // parent proc
    close(in[0]);
    *fd_in = in[1]; // return the other end of the dup2() pipe

    close(out[1]);
    *fd_out = out[0];

    close(err[1]);
    *fd_err = err[0];

    return 0;
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

//    runcgi(cgifile);

    printf("pthread_create runcgi()...\n");
    pthread_t t1;
    int z = pthread_create(&t1, NULL, runcgi, cgifile);
    if (z != 0) {
        errno = z;
        perror("pthread_create()");
        exit(1);
    }
    z = pthread_join(t1, NULL);
    if (z != 0) {
        errno = z;
        perror("pthread_join()");
        exit(1);
    }

    printf("Exit main.\n");
    return 0;
}

void *runcgi(void *parg) {
    printf("*** Start runcgi() function...\n");
    char *cgifile = parg;

    int in, out, err;
    int z = popen3(cgifile, &in, &out, &err);
    if (z == -1) {
        perror("popen3()");
        exit(1);
    }

    printf("buf_out:\n");
    LKBuffer *buf_out = lk_buffer_new(0);
    lk_readfd(out, buf_out);
    for (int i=0; i < buf_out->bytes_len; i++) {
        putchar(buf_out->bytes[i]);
    }
    lk_buffer_free(buf_out);

    printf("buf_err:\n");
    LKBuffer *buf_err = lk_buffer_new(0);
    lk_readfd(err, buf_err);
    for (int i=0; i < buf_err->bytes_len; i++) {
        putchar(buf_err->bytes[i]);
    }
    lk_buffer_free(buf_err);

    close(in);
    close(out);
    close(err);
    printf("*** End runcgi() function\n");
    return NULL;
}

