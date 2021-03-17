#include "uri.h"
#include "util.h"

#include <stdlib.h>
#include <string.h>

#define CUE_PREFIX     "cue://"

char *path_from_uri(char*uri)
{

	size_t uri_len = strlen(uri);

	char *buffer = calloc( uri_len+1, 1 );


	if( prefix(CUE_PREFIX, uri )){
		int pre_len = strlen(CUE_PREFIX);

		int i;
		for( i=uri_len; uri[i] != '/'; i-- );

		memcpy( buffer, uri+pre_len, i-pre_len );
		
	}else{
		strcpy( buffer, uri );
	}
	
	return buffer;
}

