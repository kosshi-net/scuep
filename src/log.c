#include <stdbool.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdint.h>
#include <pthread.h>

#include "log.h"
#include "frontend.h"

static bool enable_logging = false;

static char buffer[1024];
static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

void log_start(void)
{
	if(enable_logging) return;
	enable_logging = true;
}

void log_stop(void)
{
	if(!enable_logging) return;
	enable_logging = false;
}


void scuep_logf(uint32_t type, const char *format, ...)
{
	pthread_mutex_lock(&mutex);

	va_list args;
	va_start (args, format);
	vsnprintf(buffer, sizeof(buffer)-1, format, args);
	va_end (args);


	if (enable_logging) {
		fprintf(stderr, "%s\n", buffer);
	}

	switch (type) {
	case SCUEP_WARN:
	case SCUEP_ERROR:
		frontend_print(type, buffer);
		break;
	case SCUEP_DEBUG:
		if (enable_logging) frontend_print(type, buffer);
		break;
	default:
		break;
	}

	pthread_mutex_unlock(&mutex);
}


