NCURSES=	`pkg-config --libs --cflags ncursesw`
LIBCUE=		`pkg-config --libs --cflags libcue`
MPV=		`pkg-config --libs --cflags mpv`
TAGLIB=		`pkg-config --libs --cflags taglib_c`
SQLITE=		`pkg-config --libs --cflags sqlite3`

CLFAGS = -Wall 

BIN=scuep

src = src/scuep.c src/database.c src/util.c src/log.c
obj = $(src:.c=.o)
sql = sql/reset.sql
sql_h = src/sql.h

PREFIX = /usr/local
CC = cc

all: bin/scuep  bin/scuep-cue-to-urls

$(sql_h): $(sql)
	xxd -i $^ > $@;

bin/scuep-cue-to-urls: src/scuep-cue-to-urls.c src/filehelper.h;
	$(CC) src/scuep-cue-to-urls.c $(CLFAGS) $(LIBCUE)  -o bin/scuep-cue-to-urls

bin/scuep: $(sql_h) $(obj)
	$(CC) $^ -ltag $(CLFAGS) $(NCURSES) $(LIBCUE) $(MPV) $(TAGLIB) $(SQLITE) -g -o $@

.PHONY: clean install uninstall
clean:
	rm $(obj) $(sql_h) bin/scuep-cue-to-urls bin/scuep


install: all
	mkdir -p $(DESTDIR)$(PREFIX)/bin
	cp \
		"bin/scuep" \
		"bin/scuep-remote" \
		"bin/scuep-cue-scanner" \
		"bin/scuep-media-scanner" \
		"bin/scuep-dedup" \
		"bin/scuep-cue-to-urls" \
		$(DESTDIR)$(PREFIX)/bin 
	chmod 755 \
		"$(DESTDIR)$(PREFIX)/bin/scuep-cue-scanner" \
		"$(DESTDIR)$(PREFIX)/bin/scuep-media-scanner" \
		"$(DESTDIR)$(PREFIX)/bin/scuep-cue-to-urls" \
		"$(DESTDIR)$(PREFIX)/bin/scuep-remote" \
		"$(DESTDIR)$(PREFIX)/bin/scuep-dedup" \
		"$(DESTDIR)$(PREFIX)/bin/scuep"



uninstall:
	rm -f \
		"$(DESTDIR)$(PREFIX)/bin/scuep-cue-scanner" \
		"$(DESTDIR)$(PREFIX)/bin/scuep-media-scanner" \
		"$(DESTDIR)$(PREFIX)/bin/scuep-cue-to-urls" \
		"$(DESTDIR)$(PREFIX)/bin/scuep-remote" \
		"$(DESTDIR)$(PREFIX)/bin/scuep-dedup" \
		"$(DESTDIR)$(PREFIX)/bin/scuep"

