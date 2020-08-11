#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <syslog.h>

#include "log.h"

// This whole file is a bit redundant

int enable_logging = 0;

void scuep_log_start(){
	if(enable_logging) return;
	enable_logging = 1;
	
	openlog( "scuep", 0, LOG_USER );
	syslog(LOG_INFO, "Logger started");
}

void scuep_log_stop(){
	if(!enable_logging) return;
	enable_logging = 0;

	syslog(LOG_INFO, "Logger stopped");
	closelog();
}

void scuep_logf(char *format, ...){
	if(!enable_logging) return;

	va_list args;
	va_start (args, format);
	vsyslog(LOG_INFO, format, args);
	va_end (args);
}




