NCURSES=`pkg-config --libs --cflags ncursesw`
LIBCUE=`pkg-config --libs --cflags libcue`
MPV=`pkg-config --libs --cflags mpv`
TAGLIB=`pkg-config --libs --cflags taglib_c`

CLFAGS=-Wall 

src = scuep.c util.c log.c
obj = $(src:.c=.o)

all: bin/scuep  opt/scuep-cue-to-urls

opt/scuep-cue-to-urls: scuep-cue-to-urls.c filehelper.h;
	gcc scuep-cue-to-urls.c $(CLFAGS) $(LIBCUE)  -o opt/scuep-cue-to-urls

bin/scuep: $(obj);
	gcc $^ -ltag $(CLFAGS) $(NCURSES) $(LIBCUE) $(MPV) $(TAGLIB) -lpthread -g -o $@

.PHONY: clean
clean:
	rm $(obj) opt/scuep-cue-to-urls bin/scuep

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

