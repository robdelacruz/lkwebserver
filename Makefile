CFLAGS=-g -Wall
LIBS=
LKLIB_SRC=lkstr.c lkstringmap.c

all: tserv tclient t lktest

tserv: tserv.c netfuncs.c
	gcc -o tserv tserv.c netfuncs.c $(CFLAGS) $(LIBS)

tclient: tclient.c netfuncs.c
	gcc -o tclient tclient.c netfuncs.c $(CFLAGS) $(LIBS)

t: t.c netfuncs.c
	gcc -o t t.c netfuncs.c $(CFLAGS) $(LIBS)

lktest: lktest.c $(LKLIB_SRC)
	gcc -o lktest lktest.c $(LKLIB_SRC) $(CFLAGS) $(LIBS)

clean:
	rm -rf t tserv tclient lktest

