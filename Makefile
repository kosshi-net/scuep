CLFAGS= -Wall `pkg-config --libs --cflags libcue`

cue_url_parse: cue_url_parse.c;
	gcc cue_url_parse.c $(CLFAGS) -o bin/cue_url_parse