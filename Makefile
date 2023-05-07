CFLAGS=-g -Wall
LIBS=
LKLIB_SRC=lklib.c lkstring.c lkstringtable.c lkbuffer.c lknet.c lkstringlist.c lkreflist.c lkhttpserver.c lkcontext.c lkhttprequestparser.c lkhttpcgiparser.c lkconfig.c

all: lkws tclient lktest

lkws: lkws.c $(LKLIB_SRC)
	gcc -o lkws lkws.c $(LKLIB_SRC) $(CFLAGS) $(LIBS)

tclient: tclient.c $(LKLIB_SRC)
	gcc -o tclient tclient.c $(LKLIB_SRC) $(CFLAGS) $(LIBS)

lktest: lktest.c $(LKLIB_SRC)
	gcc -o lktest lktest.c $(LKLIB_SRC) $(CFLAGS) $(LIBS)

t: t.c $(LKLIB_SRC)
	gcc -o t t.c $(LKLIB_SRC) $(CFLAGS) $(LIBS)

clean:
	rm -rf t lkws tclient lktest

