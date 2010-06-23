#ifndef UMP_MEVENT_PUBLIC_DEF
#define UMP_MEVENT_PUBLIC_DEF
#include <glib.h>

struct _m_event;
typedef struct _m_event MEvent;

MEvent* m_event_new(gboolean isset,gboolean auto_reset);
void m_event_free(MEvent* mevent);
void m_event_set(MEvent* mevent);
void m_event_reset(MEvent* mevent);
void m_event_wait(MEvent* mevent);
gboolean m_event_timed_wait(MEvent* mevent,glong milliseconds);
gint m_time_val_cmp(GTimeVal* t1,GTimeVal* t2);
#endif
