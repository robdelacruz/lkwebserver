void buftest() {
    char dst[500];
    char *s1 = "abc";
    char *s2 = "defg";
    char *s3 = "hijklmnopqrstuv";
    char *s4 = "wxyz";
    char *s5 = "1234567890";
    int nread;

    inputbuf_t* ib = inputbuf_new(1);
    inputbuf_write(ib, s1, strlen(s1));
    inputbuf_debugprint(ib);

    nread = inputbuf_read(ib, dst, 2);
    printbuf(dst, nread);
    printf("\n");
    inputbuf_debugprint(ib);

    nread = inputbuf_read(ib, dst, 1);
    printbuf(dst, nread);
    printf("\n");
    inputbuf_debugprint(ib);

    nread = inputbuf_read(ib, dst, 10);
    printbuf(dst, nread);
    printf("\n");
    inputbuf_debugprint(ib);

    inputbuf_write(ib, s2, strlen(s2));
    inputbuf_debugprint(ib);

    nread = inputbuf_read(ib, dst, 4);
    printbuf(dst, nread);
    printf("\n");

    inputbuf_debugprint(ib);

    inputbuf_write(ib, s3, strlen(s3));
    inputbuf_debugprint(ib);

    inputbuf_write(ib, s4, strlen(s4));
    inputbuf_debugprint(ib);

    inputbuf_write(ib, s5, strlen(s5));
    inputbuf_debugprint(ib);

    nread = inputbuf_read(ib, dst, 8);
    printbuf(dst, nread);
    printf("\n");

    inputbuf_debugprint(ib);

    nread = inputbuf_read(ib, dst, 500);
    printbuf(dst, nread);
    printf("\n");
    inputbuf_debugprint(ib);

    nread = inputbuf_read(ib, dst, 500);
    printbuf(dst, nread);
    printf("\n");
    inputbuf_debugprint(ib);

    inputbuf_free(ib);
}


