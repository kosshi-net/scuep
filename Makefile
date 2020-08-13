NCURSES=	`pkg-config --libs --cflags ncursesw`
LIBCUE=		`pkg-config --libs --cflags libcue`
MPV=		`pkg-config --libs --cflags mpv`
TAGLIB=		`pkg-config --libs --cflags taglib_c`

CLFAGS = -Wall 

src = scuep.c util.c log.c
obj = $(src:.c=.o)

PREFIX = /usr/local
CC = cc

all: bin/scuep  bin/scuep-cue-to-urls

bin/scuep-cue-to-urls: scuep-cue-to-urls.c filehelper.h;
	${CC} scuep-cue-to-urls.c $(CLFAGS) $(LIBCUE)  -o bin/scuep-cue-to-urls

bin/scuep: $(obj);
	${CC} $^ -ltag $(CLFAGS) $(NCURSES) $(LIBCUE) $(MPV) $(TAGLIB) -lpthread -g -o $@

.PHONY: clean install uninstall
clean:
	rm $(obj) bin/scuep-cue-to-urls bin/scuep


install: all
	mkdir -p $(DESTDIR)$(PREFIX)/bin
	cp \
		"bin/scuep" \
		"bin/scuep-remote" \
		"bin/scuep-cue-scanner" \
		"bin/scuep-media-scanner" \
		"bin/scuep-cue-to-urls" \
		$(DESTDIR)$(PREFIX)/bin 
	chmod 755 \
		"$(DESTDIR)$(PREFIX)/bin/scuep-cue-scanner" \
		"$(DESTDIR)$(PREFIX)/bin/scuep-media-scanner" \
		"$(DESTDIR)$(PREFIX)/bin/scuep-cue-to-urls" \
		"$(DESTDIR)$(PREFIX)/bin/scuep-remote" \
		"$(DESTDIR)$(PREFIX)/bin/scuep"



uninstall:
	rm -f \
		"$(DESTDIR)$(PREFIX)/bin/scuep-cue-scanner" \
		"$(DESTDIR)$(PREFIX)/bin/scuep-media-scanner" \
		"$(DESTDIR)$(PREFIX)/bin/scuep-cue-to-urls" \
		"$(DESTDIR)$(PREFIX)/bin/scuep-remote" \
		"$(DESTDIR)$(PREFIX)/bin/scuep"

