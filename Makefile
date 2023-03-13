CFLAGS=-g -Wall
LIBS=

all: tserv tclient t

tserv: tserv.c netfuncs.c
	gcc -o tserv tserv.c netfuncs.c $(CFLAGS) $(LIBS)

tclient: tclient.c netfuncs.c
	gcc -o tclient tclient.c netfuncs.c $(CFLAGS) $(LIBS)

t: t.c netfuncs.c
	gcc -o t t.c netfuncs.c $(CFLAGS) $(LIBS)

clean:
	rm -rf t tserv tclient

