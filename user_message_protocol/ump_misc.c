#ifdef WIN32
	#ifdef DEBUG_MEMORY_LEAK
		#define _CRTDBG_MAP_ALLOC
		#include <stdlib.h>
		#include <crtdbg.h>
	#endif
#endif

#ifdef WIN32
	#include <winsock2.h>
#endif
#include <errno.h>
#include "glib.h"

#include <string.h>

#include "ump_misc.h"
#include "ump_private.h"
#include "ump_sock.h"
#include "upacket_public.h"
#include "debug_out.h"

gint ump_sendto(UMPCore* u_core,gpointer buff,gint buff_l,struct sockaddr_in* to)
{
	int bytes_sent=0;
	g_mutex_lock(u_core->s_lock);
		bytes_sent=sendto(u_core->s,buff,buff_l,0,(SOCKADDR *)to,sizeof(struct sockaddr_in));
	g_mutex_unlock(u_core->s_lock);
	return bytes_sent;
}

gpointer ump_recvfrom(UMPCore* u_core,gint* buff_l,struct sockaddr_in* from)
{
	int bytes_rec=0;
	static gchar data[65536];
	static char reason[1024];
	gpointer d=NULL;
	gint addr_l=sizeof(struct sockaddr_in);
	bytes_rec=recvfrom(u_core->s,data,sizeof(data),0,(SOCKADDR*)from,&addr_l);
	if(bytes_rec==-1){
		strerror_s(reason,sizeof(reason),errno);
		reason[sizeof(reason)-1]=0;
		log_out("recvfrom %s \r\n",reason);
		*buff_l=0;
		return NULL;
	}
#ifdef DEBUG_MEMORY_LEAK
	d=malloc(bytes_rec);
#else
	d=g_malloc(bytes_rec);
#endif
	memcpy(d,data,bytes_rec);
	*buff_l=bytes_rec;
	return d;
}

guint ump_inaddr_hash(gconstpointer key)
{
	struct sockaddr_in *addr;
	guint r;
	if(key==NULL)return 0;
	addr=(struct sockaddr_in*)key;
	r=addr->sin_port+addr->sin_addr.s_addr;
	return r;
}

gboolean ump_inaddr_eq(gconstpointer a,gconstpointer b)
{
	struct sockaddr_in *addr1,*addr2;
	if((a==NULL && b!=NULL) || (a!=NULL && b==NULL)){
		return FALSE;
	}
	if(a==NULL && b==NULL){
		return TRUE;
	}
	addr1=(struct sockaddr_in*)a;
	addr2=(struct sockaddr_in*)b;
	return (addr1->sin_port==addr2->sin_port && addr1->sin_addr.s_addr==addr2->sin_addr.s_addr);
}

glong ump_time_sub(GTimeVal* t1,GTimeVal* t2)
{
	glong ms;
	ms=(t1->tv_sec-t2->tv_sec)*1000;
	ms+=(t1->tv_usec-t2->tv_usec)/1000;
	return ms;
}

gint32 ump_cmp_in_sndbase(UMPSocket* u_sock,guint16 x,guint16 y)
{
	gint32 a=x,b=y;
	guint16 circle_point=u_sock->our_data_seq_base+(G_MAXUINT16 >> 1);
	if(a<=circle_point){
		a+=G_MAXUINT16+1;
	}
	if(b<=circle_point){
		b+=G_MAXUINT16+1;
	}
	return a-b;
}

gint32 ump_cmp_in_rcvbase(UMPSocket* u_sock,guint16 x,guint16 y)
{
	gint32 a=x,b=y;
	guint16 circle_point=u_sock->their_data_seq_base+(G_MAXUINT16 >> 1);
	if(a<=circle_point){
		a+=G_MAXUINT16+1;
	}
	if(b<=circle_point){
		b+=G_MAXUINT16+1;
	}
	return a-b;
}

gint32 ump_seq_to_relative_via_sndstartseq(UMPSocket* u_sock,guint16 seq)
{
	gint32 a=seq,b=u_sock->our_data_start_seq;
	guint16 circle_point=u_sock->our_data_start_seq+(G_MAXUINT16 >> 1);
	if(a<=circle_point){
		a+=G_MAXUINT16+1;
	}
	if(b<=circle_point){
		b+=G_MAXUINT16+1;
	}
	return a-b;
}

guint16 ump_relative_to_seq_via_sndstartseq(UMPSocket* u_sock,guint l){
	return ((guint)u_sock->our_data_start_seq)+(guint)l;
}

guint16 ump_relative_to_seq_via_rcvseq(UMPSocket* u_sock,guint l){
	return ((guint)u_sock->their_data_seq_base)+(guint)l;
}

GList* ump_list_remove_link(GList* list,GList* llink,gint* list_len){
	if((*list_len)>0){
		(*list_len)--;
		return g_list_remove_link(list,llink);
	}else{
		return list;
	}
}

GList* ump_list_append(GList* list,gpointer data,gint* list_len){
	(*list_len)++;
	return g_list_append(list,data);
}

GList* ump_list_first(GList* l)
{
	return l;
}

void ump_send_reset_packet(UMPSocket* u_sock)
{
	UMPPacket *rst_p=NULL;
	gpointer data=NULL;
	gint data_len=0;
	
	rst_p=u_packet_new(P_CONTROL,P_OUTGOING);
	u_packet_set_flag(rst_p,UP_CTRL_RST);
	data=u_packet_to_binary(rst_p,&data_len);
	ump_sendto(u_sock->u_core,data,data_len,&u_sock->their_addr);
	u_packet_free(rst_p);

	return;
}

void ump_send_reset_packet_to(UMPCore* u_core,struct sockaddr_in *to)
{
	UMPPacket *rst_p=NULL;
	gpointer data=NULL;
	gint data_len=0;
	
	rst_p=u_packet_new(P_CONTROL,P_OUTGOING);
	u_packet_set_flag(rst_p,UP_CTRL_RST);
	data=u_packet_to_binary(rst_p,&data_len);
	ump_sendto(u_core,data,data_len,to);
	u_packet_free(rst_p);

	return;
}
