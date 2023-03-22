CFLAGS=-g -Wall
LIBS=
LKLIB_SRC=lkstr.c lkstringmap.c lkbuf.c

all: tserv tclient t lktest

tserv: tserv.c netfuncs.c $(LKLIB_SRC)
	gcc -o tserv tserv.c netfuncs.c $(LKLIB_SRC) $(CFLAGS) $(LIBS)

tclient: tclient.c netfuncs.c $(LKLIB_SRC)
	gcc -o tclient tclient.c netfuncs.c $(LKLIB_SRC) $(CFLAGS) $(LIBS)

t: t.c netfuncs.c $(LKLIB_SRC)
	gcc -o t t.c netfuncs.c $(LKLIB_SRC) $(CFLAGS) $(LIBS)

lktest: lktest.c $(LKLIB_SRC)
	gcc -o lktest lktest.c $(LKLIB_SRC) $(CFLAGS) $(LIBS)

clean:
	rm -rf t tserv tclient lktest

