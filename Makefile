CLFAGS= -Wall `pkg-config --libs --cflags libcue`


all: bin/cue_url_parse opt/cue_path_to_urls

bin/cue_url_parse: cue_url_parse.c filehelper.h;
	gcc cue_url_parse.c $(CLFAGS) -o bin/cue_url_parse

opt/cue_path_to_urls: cue_path_to_urls.c filehelper.h;
	gcc cue_path_to_urls.c $(CLFAGS) -o opt/cue_path_to_urls

.PHONY: clean
clean:
	rm bin/cue_url_parse opt/cue_path_to_urls;

PREFIX = /usr/local


.PHONY: install

install: all
	mkdir -p $(DESTDIR)$(PREFIX)/bin
	cp \
		"bin/cue_url_parse" \
		"bin/scuep" \
		"bin/scuep-remote" \
		"opt/cue_path_to_urls" \
		"opt/scuep-cue-scanner" \
		"opt/scuep-media-scanner" \
		$(DESTDIR)$(PREFIX)/bin 



.PHONY: uninstall
uninstall:
	rm -f \
		"$(DESTDIR)$(PREFIX)/bin/cue_url_parse" \
		"$(DESTDIR)$(PREFIX)/bin/scuep" \
		"$(DESTDIR)$(PREFIX)/bin/scuep-remote" \
		"$(DESTDIR)$(PREFIX)/bin/cue_path_to_urls" \
		"$(DESTDIR)$(PREFIX)/bin/scuep_cue_scanner" \
		"$(DESTDIR)$(PREFIX)/bin/scuep_media_scanner" 
