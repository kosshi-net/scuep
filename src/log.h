#ifndef SCUEP_LOG_H
#define SCUEP_LOG_H

#include <syslog.h>
#include <stdint.h>

#define SCUEP_INFO  0
#define SCUEP_DEBUG 1
#define SCUEP_WARN  2
#define SCUEP_ERROR 3

/*
 * Logging is thread-safe.
 * Do not add newlines (\n) to the logging format text.
 */

void log_stop (void);
void log_start(void);

/* Don't use directly */
void scuep_logf(uint32_t type, const char *format, ...);


/* Printed to stderr if --debug is enabled. Never displayed on frontend. */
#define log_info(...) \
	scuep_logf(SCUEP_INFO, __VA_ARGS__)


/* Printed to stderr if --debug is enabled. Always displayed on the frontend */
#define log_warn(...) \
	scuep_logf(SCUEP_WARN, __VA_ARGS__)


/* Printed to stderr if --debug is enabled. Always displayed on the frontend */
#define log_error(...) \
	scuep_logf(SCUEP_ERROR, __VA_ARGS__)


/* Printed to stderr AND the frontend if --debug is enabled.
 * Use as high importance info to eg describe hidden program state useful for
 * debugging. */
#define log_debug(...) \
	scuep_logf(SCUEP_DEBUG, __VA_ARGS__)

#endif
