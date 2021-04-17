
NCURSES=	`pkg-config --libs --cflags ncursesw`
LIBCUE=		`pkg-config --libs --cflags libcue`
MPV=		`pkg-config --libs --cflags mpv`
TAGLIB=		`pkg-config --libs --cflags taglib_c`
SQLITE=		`pkg-config --libs --cflags sqlite3`
AVCODEC=    `pkg-config --libs --cflags libavcodec`
AVFORMAT=   `pkg-config --libs --cflags libavformat`
AVUTIL=     `pkg-config --libs --cflags libavutil`
FFMPEG=     $(AVCODEC) $(AVFORMAT) $(AVUTIL)

CFLAGS = -Wall -ggdb

BIN=scuep

SRCDIR=src
OBJDIR=obj
BINDIR=bin

src_pre = main.c database.c util.c log.c player.c uri.c frontend.c
obj_pre = $(src_pre:.c=.o)

src = $(addprefix $(SRCDIR)/, $(src_pre) )
obj = $(addprefix $(OBJDIR)/, $(obj_pre) )

sql = sql/schema.sql
sql_h = src/sql.h

PREFIX = /usr/local
CC = cc

all: bin/scuep  bin/scuep-cue-to-urls

$(OBJDIR)/%.o:$(SRCDIR)/%.c
	mkdir -p $(OBJDIR)
	$(CC) $(CFLAGS) -c $< -o $@

bin/scuep-cue-to-urls: src/scuep-cue-to-urls.c src/filehelper.h;
	$(CC) src/scuep-cue-to-urls.c $(CFLAGS) $(LIBCUE)  -o $(BINDIR)/scuep-cue-to-urls

$(BINDIR)/scuep: $(sql_h) $(obj)
	$(CC) $^ -ltag -lm $(CFLAGS) $(AVCODEC) $(AVFORMAT) $(AVUTIL) $(NCURSES) $(LIBCUE) $(MPV) $(TAGLIB) $(SQLITE) -g -o $@


$(sql_h): $(sql)
	xxd -i sql/schema.sql >  $@;
	xxd -i sql/insert_track.sql >> $@;

.PHONY: clean install uninstall
clean:
	rm $(obj) $(sql_h) bin/scuep-cue-to-urls bin/scuep

