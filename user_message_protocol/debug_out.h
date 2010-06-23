#ifndef DEBUG_OUT_H
#define DEBUG_OUT_H

#include <stdio.h>

void log_out(const char* format,...);
void log_set_stream(FILE* f);

#endif