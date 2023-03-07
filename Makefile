CFLAGS=
LIBS=

all: tserv tclient t

tserv: tserv.c sockbuf.c http.c
	gcc -o tserv tserv.c sockbuf.c http.c $(CFLAGS) $(LIBS)

tclient: tclient.c sockbuf.c
	gcc -o tclient tclient.c sockbuf.c $(CFLAGS) $(LIBS)

t: t.c sockbuf.c http.c
	gcc -o t t.c sockbuf.c http.c $(CFLAGS) $(LIBS)

clean:
	rm -rf tserv tclient

