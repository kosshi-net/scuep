#ifndef SCUEP_LOG_H
#define SCUEP_LOG_H

#include <syslog.h>

void scuep_log_start(void);
void scuep_logf(const char *format, ...);
void scuep_log_stop(void);

#endif
