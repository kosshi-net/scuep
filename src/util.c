#include "config.h"

#include <stdlib.h>
#include <stdio.h>

#include <locale.h>
#include <fcntl.h>

#include <string.h>
#include <unistd.h>
#include <wctype.h>
#include <stdbool.h>
#include <stdint.h>

#include <wchar.h>

#include <unistd.h>
#include <time.h>

#include "util.h"

/*
 * STRING UTILIES
 *
 * Some stdlib/posix string functions like basename and dirname have issues 
 * like lack of thread safety. Such rewritten functions here can have 
 * different interface and behavior.
 */


char *scuep_basename(const char*c){
	const char *last = c;
	while (*++c){
		if (*c == '/') last = c;
	}
	return (char*)(last+1);
}

char *scuep_dirname(const char*path){
	char *dir = scuep_strdup( path );
	*scuep_basename( dir ) = 0;
	return dir;
}

char *scuep_strdup(const char*str){
	char *dup = malloc( strlen(str)+1 );
	strcpy(dup, str);
	return dup;
}

char *scuep_strcat( char *dest, char *src ){
	
	size_t dest_len = strlen(dest);
	size_t src_len  = strlen(src);

	char *ret = realloc(dest, dest_len+src_len+1);

	memcpy( ret+dest_len, src, src_len+1 );

	return ret;
}

/* Case insensitive widechar substring search
 * Not efficient but works */
wchar_t *scuep_wcscasestr(wchar_t *haystack, wchar_t *needle){

	if (!haystack || !*haystack) return 0;

	size_t haystack_len = wcslen(haystack);
	size_t needle_len = wcslen(haystack);

	wchar_t *haystack_case = calloc(haystack_len+1, sizeof(wchar_t));
	wchar_t *needle_case   = calloc(needle_len+1,   sizeof(wchar_t));

	for (int i = 0; i < haystack_len; i++)
		haystack_case[i] = towupper(haystack[i]);

	for (int i = 0; i < needle_len; i++)
		needle_case[i] = towupper(needle[i]);

	wchar_t *p = wcsstr(haystack_case, needle_case);
	
	free(haystack_case);
	free(needle_case);

	if(!p) return p;
	
	return haystack + (p - haystack_case);

}	



/* 
 * Copies characters until total glyph width exceeds max_width. If source text
 * was not cut, returns 0, otherwise returns total glyph width. 
 */
int scuep_wcslice(wchar_t* dst, wchar_t *wc, uint32_t max_width, uint32_t *width ){

	*width = 0;
	while(*wc){
		int gw = wcwidth(*wc);

		if(*width+gw > max_width ){
			*dst++ = 0;
			return 1;
		}

		*dst++ = *wc++;
		*width += gw;
	}
	
	*dst++ = 0;
	return 0;
}
 
/* Returns true when str starts with pre */
bool prefix(const char *pre, const char *str){
    return strncmp(pre, str, strlen(pre)) == 0;
}

/*
 * FILE AND IO UTILITIES
 */

char *read_stdin(){
	size_t buffer_size = 1024*4; 
	size_t buffer_index = 0;
	char *buffer = malloc(buffer_size);
	int c = 0;
	while((c = getchar()) != EOF) {
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


void sleep_ms( uint32_t ms ){
	// Why is usleep depricated????????!!!!!
	
	struct timespec tm;

	tm.tv_sec  =  ms / 1000;
	tm.tv_nsec = (ms % 1000) * 1000000;

	nanosleep( &tm, &tm );

}

time_t time_ms(){
    struct timespec t;
	clock_gettime(CLOCK_MONOTONIC_RAW, &t);
	
    return (t.tv_sec*1000L) + (t.tv_nsec/1000000L);
}

