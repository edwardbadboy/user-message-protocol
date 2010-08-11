#ifndef UMP_INNER_DEF
#define UMP_INNER_DEF

#include <glib.h>
#include "ump_public.h"

gint ump_sendto(UMPCore* u_core,gpointer buff,gint buff_l,struct sockaddr_in* to);
gpointer ump_recvfrom(UMPCore* u_core,gint* buff_l,struct sockaddr_in* from);
guint ump_inaddr_hash(gconstpointer key);
gboolean ump_inaddr_eq(gconstpointer a,gconstpointer b);
glong ump_time_sub(GTimeVal* t1,GTimeVal* t2);
GList* ump_list_append(GList* list,gpointer data,gint* list_len);
GList* ump_list_remove_link(GList* list,GList* llink,gint* list_len);
GList* ump_list_first(GList* l);
void ump_send_reset_packet(UMPSocket* u_sock);
void ump_send_reset_packet_to(UMPCore* u_core,struct sockaddr_in *to);
#endif
