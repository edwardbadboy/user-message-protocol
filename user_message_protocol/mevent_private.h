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
#endif
