#ifndef SCUEP_FRONTEND_H
#define SCUEP_FRONTEND_H

#include <stdint.h>
#include <stddef.h>

int frontend_initialize(const char *fifopath);
int frontend_tick(void);
int frontend_terminate(void);

void frontend_next(int32_t);
void frontend_set_search(wchar_t *str);

void frontend_mark_by_search(const char *needle);

uint32_t frontend_get_cursor(void);

void frontend_print(uint32_t level, const char*);
void frontend_printf(uint32_t color, const char *format, ...);

#endif
