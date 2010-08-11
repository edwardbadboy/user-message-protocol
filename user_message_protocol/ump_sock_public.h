#ifndef UMP_SOCK_PUBLIC_DEF
#define UMP_SOCK_PUBLIC_DEF
#ifdef WIN32
	#include <winsock2.h>
#endif
#include <glib.h>
#include "upacket_public.h"
#include "rtocomputer_public.h"
#include "mevent_public.h"

struct _ump_core;
typedef struct _ump_core UMPCore;
struct _ump_socket;
typedef struct _ump_socket UMPSocket;

typedef enum _ump_sock_state
{
	UMP_CLOSED=0,
	UMP_CONNECTING=1,
	UMP_ESTABLISHED=2,
	UMP_CLOSING=3,
}UMPSockState;


UMPSocket* ump_sock_new(UMPCore* u_core,struct sockaddr_in *their_addr);
void ump_sock_free(UMPSocket* u_sock);
gboolean ump_sock_connect(UMPSocket* u_sock);//,struct sockaddr_in their_addr);
gboolean ump_sock_close(UMPSocket* u_sock);
gboolean ump_sock_send(UMPSocket* u_sock,UMPPacket** data_packets,gint packets_count);
gchar* ump_sock_receive(UMPSocket* u_sock,gint *rec_len);

struct sockaddr_in* ump_sock_remote_peer(UMPSocket* u_sock);
void ump_sock_lock_rec_packets(UMPSocket *u_sock);
void ump_sock_unlock_rec_packets(UMPSocket *u_sock);
gboolean ump_sock_rec_packets_space_available(UMPSocket *u_sock);
void ump_sock_rec_packets_append(UMPSocket *u_sock,UMPPacket *u_p);
gboolean ump_sock_rec_ctrl_packets_space_available(UMPSocket *u_sock);
void ump_sock_rec_ctrl_packets_append(UMPSocket *u_sock,UMPPacket *u_p);
void ump_sock_notify_do_work(UMPSocket *u_sock);
void ump_sock_lock_public_state(UMPSocket *u_sock);
void ump_sock_unlock_public_state(UMPSocket *u_sock);
UMPSockState ump_sock_public_state(UMPSocket *u_sock);
GTimeVal* ump_sock_close_time(UMPSocket *u_sock);
GTimeVal* ump_sock_connect_time(UMPSocket *u_sock);
UMPCore* ump_sock_umpcore(UMPSocket *u_sock);
guint16 ump_sock_our_mss(UMPSocket *u_sock);
#endif
