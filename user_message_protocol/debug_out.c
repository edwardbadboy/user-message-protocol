#ifdef WIN32
	#ifdef DEBUG_MEMORY_LEAK
		#define _CRTDBG_MAP_ALLOC
		#include <stdlib.h>
		#include <crtdbg.h>
	#endif
#endif

#include <stdarg.h>
#include <stdio.h>

#define DEBUG_OUT

static FILE* f_debug=NULL;

void log_out(const char* format,...)
{
#ifdef DEBUG_OUT
	va_list ap;
	if(f_debug==NULL){
		return;
	}
	va_start(ap, format);
	vfprintf(f_debug,format,ap);
	va_end(ap);
#endif
	return;
}

void log_set_stream(FILE* f)
{
	f_debug=f;
	return;
}

