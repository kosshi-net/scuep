#ifndef SCUEP_UTILS_H
#define SCUEP_UTILS_H

#include <stdlib.h>
#include <stdio.h>

#include <locale.h>
#include <fcntl.h>

#include <string.h>


// Strips directory from filename
char *scuep_basename(const char*c);

// Case insensitive widechar substring search
wchar_t *scuep_wcscasestr(wchar_t *haystack, wchar_t *needle);

// Copies characters, calculating their printed width, until max_width is 
// reached
uint32_t scuep_wcslice(wchar_t* dst, wchar_t *wc, uint32_t max_width );
 
// Returns true when str starts with pre
bool prefix(const char *pre, const char *str);

char *read_stdin();

char *read_file(char *path);


#endif
