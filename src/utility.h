#ifndef __UTILITY_H__
#define __UTILITY_H__

#include <stdio.h>

void sleep_ms(int msecs);

int fprintf_init_once(void);

int fprintf_deinit_once(void);

int fprintf_locked(FILE *stream, int color, const char *format, ...);

int fix_flags(int flags);

int fix_pathname(char *pathname);

int fix_argv(char *destination, char **argv);

#endif
