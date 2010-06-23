#ifndef TIMER_LIST_PRIVATE_DEF
#define TIMER_LIST_PRIVATE_DEF
#include <glib.h>

typedef struct _tm_entry
{
	guint16 seq;
	GTimeVal fire_time;
	GTimeVal reg_time;
}TmEntry;
#endif
