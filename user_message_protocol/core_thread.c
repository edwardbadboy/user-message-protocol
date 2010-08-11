#ifdef WIN32
	#ifdef DEBUG_MEMORY_LEAK
		#define _CRTDBG_MAP_ALLOC
		#include <stdlib.h>
		#include <crtdbg.h>
	#endif
#endif

#include <glib.h>
#ifdef WIN32
	#include <winsock2.h>
#endif
#include "core_thread.h"
#include "ump_public.h"
#include "ump_private.h"
#include "upacket_public.h"
#include "upacket_private.h"
#include "ump_misc.h"
#include "mevent_public.h"
#include "ump_sock_public.h"
#include "debug_out.h"
#define UMP_CALM_MICROS 20000000

static gboolean ump_clean_closed_sock(gpointer key,gpointer value,gpointer user_data);

gpointer receive_thread_func(gpointer data)
{
	UMPCore* u_core=NULL;
	gpointer rec_data;
	gint rec_len;
	struct sockaddr_in from;
	UMPPacket* u_p=NULL;
	UMPSocket* u_sock;
	gboolean stop_work=FALSE;

	if(data==NULL){
		log_out("cannot start receive thread: para is null\r\n");
		return NULL;
	}
	u_core=(UMPCore*)data;
	
	log_out("receiver thread started\r\n");

	while(TRUE)
	{
		rec_data=ump_recvfrom(u_core,&rec_len,&from);
		if(rec_data==NULL){
			stop_work=m_event_timed_wait(u_core->stop_work,20);
			if(stop_work==TRUE){
				break;
			}
			log_out("receive ump packet failed\r\n");
			continue;
		}
		//log_out("received ump packet\r\n");
		u_p=NULL;
		u_p=u_packet_from_binary(rec_data,rec_len);
		if(u_p==NULL){
#ifdef DEBUG_MEMORY_LEAK
			free(rec_data);
#else
			g_free(rec_data);
#endif
			continue;
		}

		u_sock=NULL;
		g_mutex_lock(u_core->umps_lock);
#ifdef VERBOSE
			log_out("finding receiver in connected usocks\r\n");
#endif
			u_sock=g_hash_table_lookup(u_core->umps,&from);
			if(u_sock==NULL){
#ifdef VERBOSE
				log_out("finding receiver in backlog\r\n");
#endif
				u_sock=g_hash_table_lookup(u_core->backlog_umps,&from);
			}
			if(u_sock==NULL){
#ifdef VERBOSE
				log_out("finding receiver in act_connect\r\n");
#endif
				u_sock=g_hash_table_lookup(u_core->act_connect,&from);
			}
			if(u_sock==NULL){
#ifdef VERBOSE
				log_out("finding receiver in closed usocks\r\n");
#endif
				u_sock=g_hash_table_lookup(u_core->closed_umps,&from);
			}
			if(u_sock==NULL){
				//检测是否对方在尝试连接我们并处理
				if(u_p->p_type==P_CONTROL && u_packet_get_flag(u_p,UP_CTRL_SYN)){
					u_sock=ump_sock_new(u_core,&from);
					if(g_hash_table_size(u_core->backlog_umps)<=u_core->backlog_limit){
						g_hash_table_insert(u_core->backlog_umps,ump_sock_remote_peer(u_sock),u_sock);
						log_out("active connect request, added to backlog\r\n");
					}else{
						ump_sock_free(u_sock);
						u_sock=NULL;
						log_out("active connect request, backlog full\r\n");
					}
				}
			}
			if(u_sock==NULL){
				u_packet_free(u_p);
				u_p=NULL;
				g_mutex_unlock(u_core->umps_lock);
				log_out("found no receiver\r\n");
				ump_send_reset_packet_to(u_core,&from);
				continue;
			}

			ump_sock_lock_rec_packets(u_sock);
				if(u_p->p_type==P_CONTROL){
					if(ump_sock_rec_ctrl_packets_space_available(u_sock)==TRUE){
						ump_sock_rec_ctrl_packets_append(u_sock,u_p);
#ifdef VERBOSE
						log_out("received ctrl packet\r\n");
#endif
					}else{
						//丢弃包
						u_packet_free(u_p);
						u_p=NULL;
#ifdef VERBOSE
						log_out("receiver ctrl packets full\r\n");
#endif
					}
				}else{
					if(ump_sock_rec_packets_space_available(u_sock)==TRUE){
						ump_sock_rec_packets_append(u_sock,u_p);
#ifdef VERBOSE
						log_out("received data packet\r\n");
#endif
					}else{
						//丢弃包
						u_packet_free(u_p);
						u_p=NULL;
#ifdef VERBOSE
						log_out("received data packets full\r\n");
#endif
					}
				}
			ump_sock_unlock_rec_packets(u_sock);

			ump_sock_notify_do_work(u_sock);
		g_mutex_unlock(u_core->umps_lock);
		//处理期间umps等表是被锁定的，不允许被清理线程关闭
	}
	return NULL;
}

gpointer cleaner_thread_func(gpointer data)
{
	gboolean stop_work=FALSE;
	UMPCore* u_core=NULL;

	if(data==NULL){
		return NULL;
	}
	u_core=(UMPCore*)data;

	while(TRUE){
		stop_work=m_event_timed_wait(u_core->stop_work,20000);
		if(stop_work==TRUE){
			break;
		}
		g_mutex_lock(u_core->umps_lock);
			g_hash_table_foreach_remove(u_core->closed_umps,ump_clean_closed_sock,NULL);
			g_hash_table_foreach_remove(u_core->act_connect,ump_clean_closed_sock,NULL);
			g_hash_table_foreach_remove(u_core->backlog_umps,ump_clean_closed_sock,NULL);
		g_mutex_unlock(u_core->umps_lock);
	}
	return NULL;
}

static gboolean ump_clean_closed_sock(gpointer key,gpointer value,gpointer user_data)
{
	UMPSocket* u_sock;
	gboolean to_remove=FALSE;
	GTimeVal cur;

	u_sock=value;
	g_get_current_time(&cur);
	g_time_val_add(&cur,-UMP_CALM_MICROS);
	ump_sock_lock_public_state(u_sock);
		to_remove=(ump_sock_public_state(u_sock)==UMP_CLOSED) && (ump_time_sub(&cur,ump_sock_close_time(u_sock))>=0);
	ump_sock_unlock_public_state(u_sock);
	if(to_remove){
		ump_sock_free(u_sock);
	}
	return to_remove;
}
