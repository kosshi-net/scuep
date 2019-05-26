CLFAGS= -Wall `pkg-config --libs --cflags libcue`

all: bin/cue_url_parse opt/cue_path_to_urls

bin/cue_url_parse: cue_url_parse.c;
	gcc cue_url_parse.c $(CLFAGS) -o bin/cue_url_parse

opt/cue_path_to_urls: cue_path_to_urls.c;
	gcc cue_path_to_urls.c $(CLFAGS) -o opt/cue_path_to_urls

clean:
	rm bin/cue_url_parse opt/cue_path_to_urls;