
#include <stdio.h>
#include <stdlib.h>

// This helper is used to read cue files to memory and to fix some common 
// non-syntax issues that prevent libcue from parsing them correctly.
// Tested with a collection of 6700 cue files.

// libcue will complain about UTF8 BOM.
// Check for it and return the length
int scuep_bom_length(char *string){
	if( (unsigned char)string[0] == 0xEF
	&&	(unsigned char)string[1] == 0xBB
	&&	(unsigned char)string[2] == 0xBF
	) return 3;
	return 0;
}

// libcue parses REMs and throws bunch of trash to stderr, just filter them.
// Does not handle indented REMs properly, are they in spec anyway?
void scuep_remove_rems(char *string){
	char *c = string;
	int i = 0;
	int in_rem = 0;
	while( *c ) {
		if(in_rem){
			if( *c == '\n' ) in_rem = 0;
			c++;
		} else {
			if(( *c     == 'R' )
			&& ( *(c+1) == 'E' )
			&& ( *(c+2) == 'M' )
			&& ( *(c+3) == ' ' )
			){
				in_rem = 1;
			} else {
				string[i++] = *c++;
			}
		}
	}
	string[i] = *c; // Null
}

// Rename this!! Only use for reading cue files! 
char *scuep_read_file(char *path){
	FILE *f = fopen(path, "r");

	if(f==NULL) return NULL;

	fseek(f, 0, SEEK_END);
	long fsize = ftell(f);
	fseek(f, 0, SEEK_SET); 

	char *string = calloc(fsize + 16, 1 );
	if(string==NULL) return NULL;

	fread(string, 1, fsize, f);
	fclose(f);

	// libcue errors if file does not end in a empty line
	if( string[fsize-1] != '\n' ) string[fsize] = '\n';

	// Just get rid of every backlash, libcue tries to parse them but not right
	// I've seen couple of these in end of song titles, things just breaak
	char *c = string;
	while( *c++ ) if(*c=='\\') *c='/';

	scuep_remove_rems(string);

	return string;
}

