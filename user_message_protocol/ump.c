#ifdef WIN32
	#ifdef DEBUG_MEMORY_LEAK
		#define _CRTDBG_MAP_ALLOC
		#include <stdlib.h>
		#include <crtdbg.h>
	#endif
	#ifdef DEBUG_MEMORY_LEAK
		#include <string.h>
	#endif
#endif

#include <stdio.h>
//#include <tchar.h>
#include <glib/gprintf.h>
#include <glib.h>

#include <limits.h>

#include "ump_public.h"
#include "ump_private.h"

#include "mevent_public.h"
#include "core_thread.h"
#include "ump_misc.h"
#include "ump_sock_public.h"
#include "upacket_public.h"
#include "debug_out.h"

UMP_DLLDES void ump_print(gint a,gint b)
{
	printf("a=%d, b=%d\n",a,b);
	return;
}

#ifdef WIN32
WSADATA wsaData;

UMP_DLLDES int ump_init()
{
	GTimeVal rnd_seed;
	gint iresult=0;
	#ifndef G_THREADS_ENABLED
		return -1;
	#endif
#ifdef RAND_DROP
	srand( ((unsigned)time( NULL ) % UINT_MAX) );
#endif
	if(!g_thread_supported()){
		g_thread_init(NULL);
	}

	g_get_current_time(&rnd_seed);
	g_random_set_seed(rnd_seed.tv_sec*1000+rnd_seed.tv_usec/1000);

	iresult = WSAStartup( MAKEWORD(2,2), &wsaData );
	return iresult;
}
#endif

UMP_DLLDES UMPCore* ump_core_bind(struct sockaddr_in *our_addr,int backlog)
{
	UMPCore* u_core=NULL;
	if(backlog<1){
		return NULL;
	}
	//malloc for u_core
#ifdef DEBUG_MEMORY_LEAK
	u_core=malloc(sizeof(UMPCore));
	memset(u_core,0,sizeof(UMPCore));
#else
	u_core=g_malloc0(sizeof(UMPCore));
#endif
	//memset(u_core,0,sizeof(UMPCore));

	//create and bind the socket
	u_core->s=socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if(u_core->s== INVALID_SOCKET){
#ifdef DEBUG_MEMORY_LEAK
		free(u_core);
#else
		g_free(u_core);
#endif
		return NULL;
	}
	if(bind(u_core->s,(SOCKADDR*)our_addr, sizeof(struct sockaddr_in)) == SOCKET_ERROR ){
		closesocket(u_core->s);
#ifdef DEBUG_MEMORY_LEAK
		free(u_core);
#else
		g_free(u_core);
#endif
		return NULL;
	}
	u_core->our_addr=(*our_addr);

	u_core->s_lock=g_mutex_new();
	u_core->umps_lock=g_mutex_new();
	//u_core->backlog_umps_lock=g_mutex_new();
	u_core->backlog_limit=backlog;
	u_core->umps=g_hash_table_new(ump_inaddr_hash,ump_inaddr_eq);
	u_core->backlog_umps=g_hash_table_new(ump_inaddr_hash,ump_inaddr_eq);
	u_core->act_connect=g_hash_table_new(ump_inaddr_hash,ump_inaddr_eq);
	u_core->closed_umps=g_hash_table_new(ump_inaddr_hash,ump_inaddr_eq);
	u_core->accept_ok=m_event_new(FALSE,TRUE);

	u_core->stop_work=m_event_new(FALSE,FALSE);

	//initiates multithreading objects
	u_core->rec_thread=g_thread_create(receive_thread_func,u_core,TRUE,NULL);
	if(u_core->rec_thread==NULL){
		ump_core_close(u_core);
		u_core=NULL;
	}
	u_core->cleaner_thread=g_thread_create(cleaner_thread_func,u_core,TRUE,NULL);
	if(u_core->cleaner_thread==NULL){
		ump_core_close(u_core);
		u_core=NULL;
	}

    return u_core;
}

//释放资源，关闭所有的线程
UMP_DLLDES void ump_core_close(UMPCore* u_core)
{
	ump_stop_background_threads_and_socket(u_core);
	g_mutex_lock(u_core->umps_lock);
		g_hash_table_foreach(u_core->umps,ump_free_ump_sock,NULL);
		g_hash_table_foreach(u_core->closed_umps,ump_free_ump_sock,NULL);
		g_hash_table_foreach(u_core->act_connect,ump_free_ump_sock,NULL);
		g_hash_table_foreach(u_core->backlog_umps,ump_free_ump_sock,NULL);
	g_mutex_unlock(u_core->umps_lock);
	g_mutex_free(u_core->s_lock);
	g_mutex_free(u_core->umps_lock);
	m_event_free(u_core->accept_ok);
	m_event_free(u_core->stop_work);
	g_hash_table_destroy(u_core->umps);
	g_hash_table_destroy(u_core->backlog_umps);
	g_hash_table_destroy(u_core->closed_umps);
	g_hash_table_destroy(u_core->act_connect);
#ifdef DEBUG_MEMORY_LEAK
		free(u_core);
#else
		g_free(u_core);
#endif
	return;
}

static void ump_free_ump_sock(gpointer key,gpointer value,gpointer user_data)
{
	UMPSocket* u_sock;

	u_sock=value;
	ump_sock_free(u_sock);
	return;
}

UMP_DLLDES UMPSocket* ump_connect(UMPCore* u_core,struct sockaddr_in *their_addr)
{
	UMPSocket* u_sock=NULL;
	gboolean connect_r=FALSE;

	g_mutex_lock(u_core->umps_lock);
	//如果主动连接、活动连接、已关闭表中已存在their_addr，直接返回错误
	if(g_hash_table_lookup(u_core->act_connect,their_addr)!=NULL){
		g_mutex_unlock(u_core->umps_lock);
		return NULL;
	}
	if(g_hash_table_lookup(u_core->umps,their_addr)!=NULL){
		g_mutex_unlock(u_core->umps_lock);
		return NULL;
	}
	if(g_hash_table_lookup(u_core->closed_umps,their_addr)!=NULL){
		g_mutex_unlock(u_core->umps_lock);
		return NULL;
	}
	//如果后备队列中已存在their_addr，将其移动到主动连接表中
	u_sock=g_hash_table_lookup(u_core->backlog_umps,their_addr);
	if(u_sock!=NULL){
		g_hash_table_remove(u_core->backlog_umps,their_addr);
		ump_sock_lock_public_state(u_sock);
			if(ump_sock_public_state(u_sock)==UMP_CLOSED || ump_sock_public_state(u_sock)==UMP_CLOSING){
				//处理一种特殊情况，后备队列中的连接被主动关闭，而我们却开始主动连接
				ump_sock_unlock_public_state(u_sock);
				g_mutex_unlock(u_core->umps_lock);
				return NULL;
			}
		ump_sock_unlock_public_state(u_sock);
	}else{
		u_sock=ump_sock_new(u_core,their_addr);
	}
	g_hash_table_insert(u_core->act_connect,ump_sock_remote_peer(u_sock),u_sock);
	g_mutex_unlock(u_core->umps_lock);

	connect_r=ump_sock_connect(u_sock);

	g_mutex_lock(u_core->umps_lock);
		g_hash_table_remove(u_core->act_connect,their_addr);
		if(connect_r==FALSE){
			ump_sock_free(u_sock);
			u_sock=NULL;
		}else{
			g_hash_table_insert(u_core->umps,ump_sock_remote_peer(u_sock),u_sock);
		}
	g_mutex_unlock(u_core->umps_lock);

	return u_sock;
}

UMP_DLLDES UMPSocket* ump_accept(UMPCore* u_core)
{
	GHashTableIter iter;
	gpointer key, value;
	UMPSocket* u_s=NULL;
	UMPSocket* s=NULL;
	while(u_s==NULL){
		//锁后备队列，检查是否有可用的连接，取出连接，放到连接哈希表中。如果没有等待被唤醒。
		m_event_wait(u_core->accept_ok);
		g_mutex_lock(u_core->umps_lock);
			g_hash_table_iter_init (&iter,u_core->backlog_umps);
			while (g_hash_table_iter_next (&iter, &key, &value)) 
			{
				s=value;
				ump_sock_lock_public_state(s);
					if(ump_sock_public_state(s)==UMP_ESTABLISHED){
						if(u_s==NULL){
							u_s=s;
						}else{
							//搜索后备队列中最早建立好的连接
							if(ump_time_sub(ump_sock_connect_time(u_s),ump_sock_connect_time(s))>0){
								u_s=s;
							}
						}
					}
				ump_sock_unlock_public_state(s);
			}
			if(u_s!=NULL){
				g_hash_table_remove(u_core->backlog_umps,ump_sock_remote_peer(u_s));
				g_hash_table_insert(u_core->umps,ump_sock_remote_peer(u_s),u_s);
			}
		g_mutex_unlock(u_core->umps_lock);
	}
	return u_s;
}

UMP_DLLDES void ump_close(UMPSocket* u_sock)
{
	UMPCore* u_core=NULL;
	UMPSocket* u_s=NULL;
	u_core=ump_sock_umpcore(u_sock);

	ump_sock_close(u_sock);

	g_mutex_lock(u_core->umps_lock);
		u_s=g_hash_table_lookup(u_core->act_connect,ump_sock_remote_peer(u_sock));
		if(u_s!=NULL){
			g_hash_table_remove(u_core->act_connect,ump_sock_remote_peer(u_sock));
		}
		u_s=g_hash_table_lookup(u_core->backlog_umps,ump_sock_remote_peer(u_sock));
		if(u_s!=NULL){
			g_hash_table_remove(u_core->backlog_umps,ump_sock_remote_peer(u_sock));
		}
		u_s=g_hash_table_lookup(u_core->umps,ump_sock_remote_peer(u_sock));
		if(u_s!=NULL){
			g_hash_table_remove(u_core->umps,ump_sock_remote_peer(u_sock));
		}
		if(g_hash_table_lookup(u_core->umps,ump_sock_remote_peer(u_sock))==NULL){
			g_hash_table_insert(u_core->closed_umps,ump_sock_remote_peer(u_sock),u_sock);
		}
	g_mutex_unlock(u_core->umps_lock);
	return;
}

UMP_DLLDES int ump_send_message(UMPSocket* u_connection,void * data,int data_len)
{
	gboolean result;
	//GList* packets=NULL,*head=NULL;
	UMPPacket *p=NULL,**pkts=NULL;
	char* user_data=data;
	gint p_size,total_len=0,pkts_count=0,pkts_index=0;

	if(data==NULL||data_len<=0){
		return -1;
	}

	pkts_count=(data_len / ((signed int)ump_sock_our_mss(u_connection)));
	if(pkts_count* ((signed int)ump_sock_our_mss(u_connection)) < data_len){
		++pkts_count;
	}

#ifdef DEBUG_MEMORY_LEAK
	pkts=malloc(pkts_count*sizeof(UMPPacket *));
	memset(pkts,0,pkts_count*sizeof(UMPPacket *));
#else
	pkts=g_malloc0(pkts_count*sizeof(UMPPacket *));
#endif

	while(total_len<data_len){
		p_size=MIN(ump_sock_our_mss(u_connection),data_len-total_len);
		p=u_packet_new(P_DATA,P_OUTGOING);
		u_packet_set_data(p,user_data,p_size);
		user_data+=p_size;
		total_len+=p_size;
		//packets=g_list_append(packets,p);
		pkts[pkts_index]=p;
		++pkts_index;
	}

	result=ump_sock_send(u_connection,pkts,pkts_count);

	/*head=ump_list_first(packets);
	while(head!=NULL){
		u_packet_free(head->data);
		head=head->next;
	}
	g_list_free(packets);*/
	for(pkts_index=0;pkts_index<pkts_count;++pkts_index){
		u_packet_free(pkts[pkts_index]);
	}
#ifdef DEBUG_MEMORY_LEAK
	free(pkts);
#else
	g_free(pkts);
#endif
	//返回-1表示失败，0表示成功
	if(result!=TRUE){
		return -1;
	}
	return 0;
}

UMP_DLLDES int ump_receive_message(UMPSocket* u_connection,void **data, int* data_len)
{
	gchar *r=NULL;
	gint len=0;
	r=ump_sock_receive(u_connection,&len);
	if(r==NULL){
		*data_len=0;
		*data=NULL;
		return -1;
	}
	*data_len=len;
	*data=r;
	return 0;
}

UMP_DLLDES int ump_free_message(void * data)
{
#ifdef DEBUG_MEMORY_LEAK
	free(data);
#else
	g_free(data);
#endif
	return 0;
}

UMP_DLLDES void ump_set_log_stream(FILE* f)
{
	log_set_stream(f);
	return;
}

static void ump_stop_background_threads_and_socket(UMPCore *u_core)
{
	//打断后台线程
	m_event_broadcast(u_core->stop_work);//打断清理线程

	closesocket(u_core->s);//打断接收数据报的线程
	u_core->s=INVALID_SOCKET;

	if(u_core->rec_thread!=NULL){
		g_thread_join(u_core->rec_thread);
		u_core->rec_thread=NULL;
	}
	if(u_core->cleaner_thread!=NULL){
		g_thread_join(u_core->cleaner_thread);
		u_core->cleaner_thread=NULL;
	}
	return;
}

/*#include "upacket_public.h"
UMP_DLLDES gboolean ump_test_packet_funcs(void)
{
	guchar data[2000]={0};
	guchar *out_data1=NULL;
	gint d1_l=0;
	guchar *out_data2=NULL;
	gint d2_l=0;
	UMPPacket *u_p1=NULL;
	UMPPacket *u_p2=NULL;
	gint i=0;
	gpointer data_dup=NULL;
	gboolean eq=TRUE;

	for(i=0;i<sizeof(data);i++){
		data[i]=i / 256;
	}

	u_p1=u_packet_new(P_CONTROL,P_OUTGOING);
	u_packet_set_flag(u_p1,UP_CTRL_SYN);
	u_packet_set_flag(u_p1,UP_CTRL_SEQ);
	//u_packet_set_flag(u_p1,UP_CTRL_ACK);
	u_packet_set_flag(u_p1,UP_CTRL_MSS);
	u_packet_set_flag(u_p1,UP_CTRL_WND);
	u_p1->seq_num=123;
	u_p1->ack_num=456;
	u_p1->mss_num=789;
	u_p1->wnd_num=123;
	u_p1->user_data=data;
	u_p1->user_data_l=sizeof(data);

	out_data1=u_packet_to_binary(u_p1,&d1_l);

	data_dup=g_memdup(out_data1,d1_l);

	u_p2=u_packet_from_binary(data_dup,d1_l);

	out_data2=u_packet_to_binary(u_p2,&d2_l);

	g_printf("data1_l=%lu, data2_l=%lu\n",d1_l,d2_l);

	if(d1_l==d2_l){
		for(i=0;i<d1_l;i++){
			if(out_data1[i]!=out_data2[i]){
				eq=FALSE;
				break;
			}
		}
	}else{
		eq=FALSE;
	}

	u_packet_free(u_p1);
	u_packet_free(u_p2);
	//g_free(out_data2);

	u_p1=NULL;
	out_data1=NULL;
	u_p2=NULL;
	out_data2=NULL;
	data_dup=NULL;

	g_printf("eq=%d\n",eq);

	u_p1=u_packet_new(P_DATA,P_OUTGOING);
	u_packet_set_flag(u_p1,UP_DATA_SEQ);
	u_packet_set_flag(u_p1,UP_DATA_WND);
	u_p1->seq_num=123;
	u_p1->mss_num=789;
	u_p1->wnd_num=123;
	u_p1->user_data=data;
	u_p1->user_data_l=sizeof(data);

	out_data1=u_packet_to_binary(u_p1,&d1_l);
	data_dup=g_memdup(out_data1,d1_l);
	u_p2=u_packet_from_binary(data_dup,d1_l);
	out_data2=u_packet_to_binary(u_p2,&d2_l);

	g_printf("data1_l=%lu, data2_l=%lu\n",d1_l,d2_l);

	if(d1_l==d2_l){
		for(i=0;i<d1_l;i++){
			if(out_data1[i]!=out_data2[i]){
				eq=FALSE;
				break;
			}
		}
	}else{
		eq=FALSE;
	}

	u_packet_free(u_p1);
	u_packet_free(u_p2);
	//g_free(out_data2);

	g_printf("eq=%d\n",eq);

	return eq;
}*/
