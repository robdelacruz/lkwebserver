CFLAGS=
LIBS=

all: tserv tclient

tserv: tserv.c sockbuf.c
	gcc -o tserv tserv.c sockbuf.c $(CFLAGS) $(LIBS)

tclient: tclient.c sockbuf.c
	gcc -o tclient tclient.c sockbuf.c $(CFLAGS) $(LIBS)

clean:
	rm -rf tserv tclient

