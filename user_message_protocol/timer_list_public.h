#ifndef TIMER_LIST_PUBLIC_DEF
#define TIMER_LIST_PUBLIC_DEF
#include <glib.h>

void tm_register_packet(GList** tl,guint16 seq,glong rto);
glong tm_ack_packet(GList** tl,guint16 seq,guint16 upper_bound_seq);
glong tm_get_next_event(GList** tl);
gboolean get_next_timeout(GList** tl,guint16* timeout_seq);
void tm_clear_list(GList** tl);
#endif
