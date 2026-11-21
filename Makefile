CFLAGS?=	-Wall -ggdb -W -O2
CC?=		gcc
LIBS?=
LDFLAGS?=	-lpthread
PREFIX?=	/usr
VERSION=	0.1
TMPDIR=/tmp/superwebbench-$(VERSION)

all:   superwebbench tags

tags:  *.c
	-ctags *.c

install: superwebbench
	install -s superwebbench $(DESTDIR)$(PREFIX)/bin	
	install -m 644 superwebbench.1 $(DESTDIR)$(PREFIX)/share/man/man1	
	install -d $(DESTDIR)$(PREFIX)/share/doc/superwebbench
	install -m 644 README $(DESTDIR)$(PREFIX)/share/doc/superwebbench
	install -m 644 Changelog $(DESTDIR)$(PREFIX)/share/doc/superwebbench

superwebbench: superwebbench.o socket.o
	$(CC) $(CFLAGS) $(LDFLAGS) -o superwebbench *.o $(LIBS) 

clean:
	-rm -f *.o superwebbench *~ core *.core tags
	
tar:   clean
	-debian/rules clean
	rm -rf $(TMPDIR)
	install -d $(TMPDIR)
	cp -p Makefile superwebbench.c socket.c superwebbench.1 $(TMPDIR)
	ln -sf  README $(TMPDIR)/README
	ln -sf Changelog $(TMPDIR)/ChangeLog
	-cd $(TMPDIR) && cd .. && tar cozf superwebbench-$(VERSION).tar.gz superwebbench-$(VERSION)

superwebbench.o:	superwebbench.c
socket.o:	socket.c

.PHONY: clean install all tar
