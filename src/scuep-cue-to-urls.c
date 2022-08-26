#include "config.h"

#include "filehelper.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <libcue/libcue.h>

/* Gets the count of tracks in a .cue file and prints urls */

int main( int argc, char **argv ){

	if (argc != 2) {
		printf("Usage: %s path.cue '\n", argv[0]);
		return 1;
	}

	char *string = scuep_read_file(argv[1]);
	if (string==NULL) goto error;

	Cd *cd = cue_parse_string( string + scuep_bom_length(string) );
	if (cd==NULL) goto error;

	int num_tracks = cd_get_ntrack(cd);
	if (num_tracks==0) goto error;

	for( int i = 0; i < num_tracks; i++ )
		printf("cue://%s/%i\n", argv[1], i+1);

	cd_delete(cd);
	free(string);

	return 0;

	error:;

	fprintf(stderr, "libcue error %s\n", argv[1]);
	return 1;
}
