CFLAGS=-g -Wall
LIBS=
LKLIB_SRC=lklib.c lkstring.c lkstringtable.c lkbuffer.c lknet.c lkstringlist.c lkreflist.c lkalloc.c
LKNET_SRC=lkhttpserver.c lkcontext.c lkhttprequestparser.c lkhttpcgiparser.c lkconfig.c
#DEFINES=-DDEBUGALLOC
DEFINES=

all: lkws tclient lktest

lkws: lkws.c $(LKLIB_SRC) $(LKNET_SRC)
	gcc -o lkws lkws.c $(LKLIB_SRC) $(LKNET_SRC) $(DEFINES) $(CFLAGS) $(LIBS)

tclient: tclient.c $(LKLIB_SRC) $(LKNET_SRC)
	gcc -o tclient tclient.c $(LKLIB_SRC) $(LKNET_SRC) $(DEFINES) $(CFLAGS) $(LIBS)

lktest: lktest.c $(LKLIB_SRC) $(LKNET_SRC)
	gcc -o lktest lktest.c $(LKLIB_SRC) $(LKNET_SRC) $(DEFINES) $(CFLAGS) $(LIBS)

t: t.c $(LKLIB_SRC) $(LKNET_SRC)
	gcc -o t t.c $(LKLIB_SRC) $(LKNET_SRC) $(DEFINES) $(CFLAGS) $(LIBS)

clean:
	rm -rf t lkws tclient lktest

