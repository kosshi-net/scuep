CLFAGS= -Wall `pkg-config --libs --cflags libcue`


all: bin/scuep-cue-helper opt/scuep-cue-to-urls

bin/scuep-cue-helper: scuep-cue-helper.c filehelper.h;
	gcc scuep-cue-helper.c $(CLFAGS) -o bin/scuep-cue-helper

opt/scuep-cue-to-urls: scuep-cue-to-urls.c filehelper.h;
	gcc scuep-cue-to-urls.c $(CLFAGS) -o opt/scuep-cue-to-urls

.PHONY: clean
clean:
	rm bin/scuep-cue-helper opt/scuep-cue-to-urls;

PREFIX = /usr/local

.PHONY: install

install: all
	mkdir -p $(DESTDIR)$(PREFIX)/bin
	cp \
		"bin/scuep-cue-helper" \
		"bin/scuep" \
		"bin/scuep-remote" \
		"opt/scuep-cue-scanner" \
		"opt/scuep-media-scanner" \
		"opt/scuep-cue-to-urls" \
		$(DESTDIR)$(PREFIX)/bin 



.PHONY: uninstall
uninstall:
	rm -f \
		"$(DESTDIR)$(PREFIX)/bin/scuep-cue-helper" \
		"$(DESTDIR)$(PREFIX)/bin/scuep" \
		"$(DESTDIR)$(PREFIX)/bin/scuep-remote" \
		"$(DESTDIR)$(PREFIX)/bin/scuep-cue-scanner" \
		"$(DESTDIR)$(PREFIX)/bin/scuep-media-scanner" \
		"$(DESTDIR)$(PREFIX)/bin/scuep-cue-to-urls"  
