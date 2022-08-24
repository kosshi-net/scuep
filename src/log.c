#include <stdlib.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdarg.h>

#include "log.h"

static bool enable_logging = false;

void scuep_log_start( char *path ){
	if(enable_logging) return;
	enable_logging = true;
}

void scuep_log_stop(){
	if(!enable_logging) return;
	enable_logging = false;
}

void scuep_logf(const char *format, ...){
	if(!enable_logging) return;

	va_list args;
	va_start (args, format);
	vfprintf(stderr, format, args);
	va_end (args);

}




