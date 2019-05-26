#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef USE_LIBCUE
#include <libcue/libcue.h>
#endif

// A quick and dirty helper for cue sheet parsing to avoid regex and shell mess

int main( int argc, char **argv ){
	#ifdef USE_LIBCUE

	if (argc != 2) {
		printf("Usage: %s path.cue '\n", argv[0]);
		return 1;
	}

	FILE *f = fopen(argv[1], "rb");
	fseek(f, 0, SEEK_END);
	long fsize = ftell(f);
	fseek(f, 0, SEEK_SET); 
	char *string = malloc(fsize + 1);
	fread(string, 1, fsize, f);
	fclose(f);

	//FILE 	*cue 		= fopen(argv[1], "r");
	//if(cue==NULL) 		goto error;

	Cd 		*cd 		= cue_parse_string(string);
	if(cd==NULL) 		goto error;

	int 	num_tracks	= cd_get_ntrack(cd);

	for( int i = 0; i < num_tracks; i++ )
		printf("cue://%s/%i\n", argv[1], i+1);

	return 0;

	error:;

	#endif // USE_LIBCUE

	fprintf(stderr, "libcue error %s\n", argv[1]);

	return 1;
}
