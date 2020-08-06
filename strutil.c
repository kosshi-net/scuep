#include <stdlib.h>
#include <stdio.h>

#include <locale.h>
#include <fontl.h>

#include <string.h>

/*
 * STRING UTILIES
 *
 * Some of these are defined in some systems, prefix with scuep!
 * */

// Strips directory from filename
char *scuep_basename(char*c){
	char *last = c;
	while(*++c){
		if( *c == '/' ) last = c;
	}
	return last+1;
}

// Case insensitive widechar substring search
// Not efficient but works
wchar_t *scuep_wcscasestr(wchar_t *haystack, wchar_t *needle){

	if(!haystack || !*haystack) return 0;

	size_t haystack_len = wcslen(haystack);
	size_t needle_len = wcslen(haystack);

	wchar_t *haystack_case = calloc( haystack_len+1, sizeof(wchar_t) );
	wchar_t *needle_case   = calloc( needle_len+1,   sizeof(wchar_t) );

	for( int i = 0; i < haystack_len; i++ )
		haystack_case[i] = towupper(haystack[i]);

	for( int i = 0; i < needle_len; i++ )
		needle_case[i] = towupper(needle[i]);

	wchar_t *p = wcsstr( haystack_case, needle_case );
	
	free(haystack_case);
	free(needle_case);

	if(!p) return p;
	
	return haystack + (p - haystack_case);

}	



// Copies characters, calculating their printed width, until max_width is 
// reached
uint32_t scuep_wcslice(wchar_t* dst, wchar_t *wc, uint32_t max_width ){
	size_t width = 0;
	while(*wc && width < max_width){
		*dst++ = *wc;
		width += wcwidth( *wc++ );
	}
	*dst++ = 0;
	return width;
}
 
// Returns true when str starts with pre
bool prefix(const char *pre, const char *str){
    return strncmp(pre, str, strlen(pre)) == 0;
}


/*
 * FILE AND IO UTILITIES
 * */



char *read_stdin(){
	size_t buffer_size = 1024*4; 
	size_t buffer_index = 0;
	char *buffer = malloc(buffer_size);
	int c = 0;
	while( (c = getchar()) != EOF ) {
		buffer[buffer_index++] = c;
		if(buffer_index == buffer_size){
			buffer_size*=2;
			buffer = realloc(buffer, buffer_size);
		}
	}
	return buffer;
}


char *read_file(char *path){
	FILE *f = fopen(path, "r");

	if(f==NULL) return NULL;

	fseek(f, 0, SEEK_END);
	long fsize = ftell(f);
	fseek(f, 0, SEEK_SET); 

	char *string = calloc(fsize + 16, 1 );
	if(string==NULL) return NULL;

	fread(string, 1, fsize, f);
	fclose(f);

	return string;
}




