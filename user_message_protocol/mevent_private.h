#ifndef UMP_MEVENT_PRIVATE_DEF
#define UMP_MEVENT_PRIVATE_DEF

#include <glib.h>

typedef struct _m_event
{
	guint no;
	gboolean auto_reset;
	gboolean isset;
	GMutex * mutex_isset;
	GCond * cond_isset;
}MEvent;

static gint m_time_val_cmp(GTimeVal* t1,GTimeVal* t2);
static gboolean _m_event_timed_wait(MEvent* mevent,glong milliseconds);
#endif
