#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef USE_LIBCUE
#include <libcue/libcue.h>
#endif

// A quick and dirty helper for cue sheet parsing to avoid regex and shell mess

int main( int argc, char **argv ){
	if (argc != 3) {
		printf("Usage: cue_url_parse [OPTION] 'cue:///path/0'\n");
		printf("Options:\n");
		printf("  p[ath]\n");
		printf("  c[hapter]\n");
		printf("  t[itle]\n");
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

	if( argv[1][0] == 'p' ) printf("%s\n" , path_buf);  
	if( argv[1][0] == 'c' ) printf("%s\n", url+chapter_index+1);
	if( argv[1][0] == 't' ) {

		#ifdef USE_LIBCUE

		FILE 	*cue 		= fopen(path_buf, "r");
		if(cue==NULL) 		goto error;

		Cd 		*cd 		= cue_parse_file(cue);
		if(cd==NULL) 		goto error;

		Track 	*track 		= cd_get_track( cd, atoi( url+chapter_index+1 ) );
		if(track==NULL) 	goto error;

		Cdtext 	*text 		= track_get_cdtext(track);
		if(text==NULL)		goto error;

		printf("%s\n", cdtext_get( PTI_TITLE, text ));

		#else   // USE_LIBCUE

		printf("(libcue disabled)\n");

		#endif  // USE_LIBCUE
	}
	
	return 0;

	error:;
	printf("(libcue error)\n");
	return 1;
}
