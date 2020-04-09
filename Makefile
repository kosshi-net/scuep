NCURSES=`pkg-config --libs --cflags ncursesw`
LIBCUE=`pkg-config --libs --cflags libcue`
MPV=`pkg-config --libs --cflags mpv`
TAGLIB=`pkg-config --libs --cflags taglib_c`

CLFAGS=-Wall 

all: bin/scuep  opt/scuep-cue-to-urls

opt/scuep-cue-to-urls: scuep-cue-to-urls.c filehelper.h;
	gcc scuep-cue-to-urls.c $(CLFAGS) $(LIBCUE)  -o opt/scuep-cue-to-urls

bin/scuep: scuep.c;
	gcc scuep.c -ltag $(CLFAGS) $(NCURSES) $(LIBCUE) $(MPV) $(TAGLIB) -lpthread -g -o bin/scuep

.PHONY: clean
clean:
	rm opt/scuep-cue-to-urls bin/scuep

PREFIX = /usr/local

.PHONY: install

install: all
	mkdir -p $(DESTDIR)$(PREFIX)/bin
	cp \
		"bin/scuep" \
		"bin/scuep-remote" \
		"opt/scuep-cue-scanner" \
		"opt/scuep-media-scanner" \
		"opt/scuep-cue-to-urls" \
		$(DESTDIR)$(PREFIX)/bin 



.PHONY: uninstall
uninstall:
	rm -f \
		"$(DESTDIR)$(PREFIX)/opt/scuep-cue-scanner" \
		"$(DESTDIR)$(PREFIX)/opt/scuep-media-scanner" \
		"$(DESTDIR)$(PREFIX)/opt/scuep-cue-to-urls" \
		"$(DESTDIR)$(PREFIX)/bin/scuep-remote" \
		"$(DESTDIR)$(PREFIX)/bin/scuep"

