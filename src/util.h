#ifndef SCUEP_UTILS_H
#define SCUEP_UTILS_H

#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#include <locale.h>
#include <fcntl.h>

#ifndef MIN
#define MIN(a,b) (((a)<(b))?(a):(b))
#endif
#ifndef MAX
#define MAX(a,b) (((a)>(b))?(a):(b))
#endif

/*
 * Points to the start of the filename in the path provided.
 * Does not edit or copy.
 */
char *scuep_basename(const char*);
/*
 * Returns dirname of a path.
 * Does not edit the original, instead duplicates it. free()-able
 */
char *scuep_dirname(const char*);


char *scuep_strdup(const char*);

// Case insensitive widechar substring search
wchar_t *scuep_wcscasestr(wchar_t *haystack, wchar_t *needle);

/*
 * The purpose of this function is to truncate wide char strings by visual
 * width, for printing.
 * Copies characters, calculating their print width, until max_width is reached
 * or until n-1 items have been written. Always null terminates the
 * buffer (writes 0 into dst[n] if it reaches that point).
 * Writes visual width of dst into out_width.
 * Returns 1 if src did not fit into dst, 0 otherwise.
 */
int scuep_wcsnvslice(
	wchar_t *dst,
	wchar_t *src,
	uint32_t max_width,
	uint32_t n,
	uint32_t *out_width);

/*
 * Reallocates dest and adds src to it.
 */
char *scuep_strcat(char *dest, char *src);

void sleep_ms(uint32_t);

time_t time_ms(void);

/* Returns true when str starts with pre */
bool scuep_prefix(const char *pre, const char *str);

char *read_stdin(void);

char *read_file(char *path);

#endif
