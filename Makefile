CFLAGS=
LIBS=

all: t

t: t.c inputbuf.c
	gcc -o t t.c inputbuf.c $(CFLAGS) $(LIBS)

clean:
	rm -rf t

