#include "config.h"

#include "filehelper.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <libcue/libcue.h>

// A quick and dirty helper for cue sheet parsing to avoid regex and shell mess

int main( int argc, char **argv ){
	if (argc != 3) {
		printf("Usage: cue_url_parse [OPTION] 'cue:///path/0'\n");
		printf("Options:\n");
		printf("  path\n");
		printf("  chapter\n");
		printf("  title\n");
		return 1;
	}
	char *url = argv[2];

	int url_length = strlen(url);
	int chapter_index = url_length;

	while(--chapter_index) if( url[chapter_index]=='/' ) break;
	
	int chapter_length = url_length-chapter_index;
	char path_buf[1024] = {0};
	int	path_bufi = 0;

	for( 
		int i = 6; // length of "cue://"
		i < url_length-chapter_length; 
		path_buf[path_bufi++] = url[i++] 
	);

	if( strcmp(argv[1], "path") == 0 ) 
		printf("%s\n" , path_buf);  

	if( strcmp(argv[1], "chapter") == 0 ) 
		printf("%s\n", url+chapter_index+1);

	if( strcmp(argv[1], "title") == 0 )  {

		char  *string		= scuep_read_file(path_buf);
		if(string==NULL)	goto error;

		Cd 		*cd 		= cue_parse_string( string + scuep_bom_length(string) );
		if(cd==NULL) 		goto error;

		Track 	*track 		= cd_get_track( cd, atoi( url+chapter_index+1 ) );
		if(track==NULL) 	goto error;

		Cdtext 	*cdtext 	= cd_get_cdtext(cd);
		if(cdtext==NULL)	goto error;

		Cdtext 	*tracktext 	= track_get_cdtext(track);
		if(tracktext==NULL)	goto error;

		printf("ALBUM: %s ", cdtext_get( PTI_TITLE, cdtext ));
		printf("TRACK: %s ", cdtext_get( PTI_PERFORMER, tracktext ));
		printf("-- %s\n", cdtext_get( PTI_TITLE, tracktext ));

		cd_delete(cd);
		free(string);

	}
	
	return 0;

	error:;
	printf("(error)\n");
	return 1;
}
