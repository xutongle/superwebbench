CFLAGS?=	-Wall -ggdb -W -O2
CC?=		gcc
LIBS?=
LDFLAGS?=	-lpthread
PREFIX?=	/usr
VERSION=	1.0.0
TMPDIR=/tmp/superwebbench-$(VERSION)

all:   superwebbench tags

tags:  *.c
	@if command -v ctags >/dev/null 2>&1; then \
		ctags *.c; \
	else \
		echo "ctags not found, skipping tags generation"; \
	fi

install: superwebbench
	install -s superwebbench $(DESTDIR)$(PREFIX)/bin	
	install -m 644 superwebbench.1 $(DESTDIR)$(PREFIX)/share/man/man1	
	install -d $(DESTDIR)$(PREFIX)/share/doc/superwebbench
	install -m 644 README $(DESTDIR)$(PREFIX)/share/doc/superwebbench
	install -m 644 README.md $(DESTDIR)$(PREFIX)/share/doc/superwebbench
	install -m 644 Changelog $(DESTDIR)$(PREFIX)/share/doc/superwebbench
	install -m 644 LICENSE $(DESTDIR)$(PREFIX)/share/doc/superwebbench

superwebbench: superwebbench.o socket.o
	$(CC) $(CFLAGS) $(LDFLAGS) -o superwebbench *.o $(LIBS) 

clean:
	-rm -f *.o superwebbench *~ core *.core tags
	
tar:   clean
	rm -rf $(TMPDIR)
	install -d $(TMPDIR)
	cp -p Makefile superwebbench.c socket.c socket.h superwebbench.1 README README.md Changelog LICENSE $(TMPDIR)
	tar -czf superwebbench-$(VERSION).tar.gz -C /tmp superwebbench-$(VERSION)
	rm -rf $(TMPDIR)

superwebbench.o:	superwebbench.c
socket.o:	socket.c

.PHONY: clean install all tar
