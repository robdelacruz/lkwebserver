CFLAGS=-g -Wall
LIBS=

all: tserv tclient t lktest

tserv: tserv.c netfuncs.c lklib.c
	gcc -o tserv tserv.c netfuncs.c lklib.c $(CFLAGS) $(LIBS)

tclient: tclient.c netfuncs.c lklib.c
	gcc -o tclient tclient.c netfuncs.c lklib.c $(CFLAGS) $(LIBS)

t: t.c netfuncs.c lklib.c
	gcc -o t t.c netfuncs.c lklib.c $(CFLAGS) $(LIBS)

lktest: lktest.c lklib.c
	gcc -o lktest lktest.c lklib.c $(CFLAGS) $(LIBS)

clean:
	rm -rf t tserv tclient lktest

