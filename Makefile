CFLAGS=-g -Wall
LIBS=
LKLIB_SRC=lkstr.c lkstringmap.c lkbuf.c lknet.c lkstringlist.c

all: tserv tclient lktest

tserv: tserv.c $(LKLIB_SRC)
	gcc -o tserv tserv.c $(LKLIB_SRC) $(CFLAGS) $(LIBS)

tclient: tclient.c $(LKLIB_SRC)
	gcc -o tclient tclient.c $(LKLIB_SRC) $(CFLAGS) $(LIBS)

lktest: lktest.c $(LKLIB_SRC)
	gcc -o lktest lktest.c $(LKLIB_SRC) $(CFLAGS) $(LIBS)

clean:
	rm -rf t tserv tclient lktest

