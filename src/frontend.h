#ifndef SCUEP_FRONTEND_H
#define SCUEP_FRONTEND_H

#include <stdint.h>

int frontend_initialize(const char *fifopath);
int frontend_tick(void);
int frontend_terminate(void);

void frontend_next(int32_t);

#endif
