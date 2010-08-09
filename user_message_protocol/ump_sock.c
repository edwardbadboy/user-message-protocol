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
#include "ump_private.h"
#include "mevent_public.h"
#include "rtocomputer_public.h"
#include "upacket_public.h"
#include "upacket_private.h"
#include "ump_sock.h"
#include "ump_misc.h"
#include "timer_list_public.h"
#include "debug_out.h"

static UMPSmFuncs u_sm_funcs[]={
	{ump_closed_handle_ctrl,ump_closed_handle_ctrl_packet,ump_closed_handle_data_packet,ump_closed_connect,ump_closed_close,ump_closed_send,ump_closed_receive,ump_closed_enter_state,ump_closed_leave_state},
	{ump_connecting_handle_ctrl,ump_connecting_handle_ctrl_packet,ump_connecting_handle_data_packet,ump_connecting_connect,ump_connecting_close,ump_connecting_send,ump_connecting_receive,ump_connecting_enter_state,ump_connecting_leave_state},
	{ump_established_handle_ctrl,ump_established_handle_ctrl_packet,ump_established_handle_data_packet,ump_established_connect,ump_established_close,ump_established_send,ump_established_receive,ump_established_enter_state,ump_established_leave_state},
	{ump_closing_handle_ctrl,ump_closing_handle_ctrl_packet,ump_closing_handle_data_packet,ump_closing_connect,ump_closing_close,ump_closing_send,ump_closing_receive,ump_closing_enter_state,ump_closing_leave_state},
};

static UMPCtrlLockStateFuncs u_ctrl_sm_funcs[]={
	{ump_ctrlunlocked_handle_ctrl,ump_ctrlunlocked_enter_state,ump_ctrlunlocked_leave_state,ump_ctrlunlocked_end_ctrl},
	{ump_ctrlocked_handle_ctrl,ump_ctrllocked_enter_state,ump_ctrllocked_leave_state,ump_ctrllocked_end_ctrl}
};

static UMPRecLockStateFuncs u_rec_sm_funcs[]={
	{ump_recunlocked_handle_rec,ump_recunlocked_enter_state,ump_recunlocked_leave_state,ump_recunlocked_end_rec},
	{ump_reclocked_handle_rec,ump_reclocked_enter_state,ump_reclocked_leave_state,ump_reclocked_end_rec}
};

static UMPSndLockStateFuncs u_snd_sm_funcs[]={
	{ump_sndunlocked_handle_snd,ump_sndunlocked_enter_state,ump_sndunlocked_leave_state,ump_sndunlocked_end_snd,ump_sndunlocked_handle_send_data},
	{ump_sndlocked_handle_snd,ump_sndlocked_enter_state,ump_sndlocked_leave_state,ump_sndlocked_end_snd,ump_sndlocked_handle_send_data}
};

gpointer ump_sock_thread_func(gpointer data)
{
	gboolean stop_work=FALSE;
	gboolean ctrl_lock_ok=FALSE;
	glong sleep_ms=G_MAXLONG;
	UMPSocket* u_s=NULL;
	GTimeVal t_start,t_end;
	glong t_delta=0;

	if(data==NULL){
		return NULL;
	}
	u_s=(UMPSocket*)data;

	while(TRUE)
	{
		sleep_ms=UMP_MAX_SLEEP_MS;
		g_get_current_time(&t_start);

		ump_handle_ctrl(u_s,&sleep_ms);
		ump_handle_send(u_s,&sleep_ms);
		ump_handle_receive(u_s,&sleep_ms);

		if(u_s->error_occured==FALSE){
			u_s->buffer_load=ump_sock_fetch_received_packets(u_s,&sleep_ms);

			ump_handle_ctrl_packet(u_s,&sleep_ms);
			ump_handle_data_packet(u_s,&sleep_ms);
			ump_handle_data_timeout(u_s,&sleep_ms);
			ump_refresh_our_rwnd_pos(u_s);
			ump_refresh_our_ack_info(u_s,&sleep_ms);
			ump_act_req_wnd(u_s,&sleep_ms);
			ump_handle_ctrl_timeout(u_s,&sleep_ms);
			ump_handle_send_ctrl_packet(u_s,&sleep_ms);
			ump_handle_send_data(u_s,&sleep_ms);
			ump_handle_send_reset_packet(u_s,&sleep_ms);
		}

		sleep_ms=MIN((sleep_ms),tm_get_next_event(&(u_s->tm_list)));
		g_get_current_time(&t_end);
		t_delta=ump_time_sub(&t_end,&t_start);//how much time has spent.
		sleep_ms=(t_delta>=sleep_ms)?0:sleep_ms-t_delta;
#ifdef VERBOSE1
		log_out("sleep_ms=%ld\n",sleep_ms);
#endif
		m_event_timed_wait(u_s->do_work_event,sleep_ms);
		g_mutex_lock(u_s->thread_stop_work_lock);
			stop_work=u_s->thread_stop_work;
		g_mutex_unlock(u_s->thread_stop_work_lock);
		if(stop_work){
			return NULL;
		}
	}

	return NULL;
}

guint ump_sock_fetch_received_packets(UMPSocket* u_s,glong* sleep_ms)
{
	guint buffer_load=0;
	g_mutex_lock(u_s->rec_packets_lock);
		if(g_queue_is_empty(u_s->rec_control_packets)==FALSE || g_queue_is_empty(u_s->rec_data_packets)==FALSE){
			*sleep_ms=0;
		}
		while(g_queue_is_empty(u_s->rec_control_packets)==FALSE && u_s->local_ctrl_packets==NULL){//限制本地链表长度为1
			u_s->local_ctrl_packets=g_list_append(u_s->local_ctrl_packets,g_queue_pop_tail(u_s->rec_control_packets));
		}
		while(g_queue_is_empty(u_s->rec_data_packets)==FALSE && u_s->local_data_packets_count<=LOCAL_DATA_PACKET_LIMIT){//限制本地链表长度为LOCAL_DATA_PACKET_LIMIT
			u_s->local_data_packets=ump_list_append(u_s->local_data_packets,g_queue_pop_tail(u_s->rec_data_packets),&(u_s->local_data_packets_count));
			//将来从链表中删除节点时，使用ump_list_remove_link
		}
		buffer_load=g_queue_get_length(u_s->rec_data_packets);
	g_mutex_unlock(u_s->rec_packets_lock);
	return buffer_load;
}

UMPSocket* ump_sock_new(UMPCore* u_core,struct sockaddr_in *their_addr)
{
	UMPSocket* u_sock;
#ifdef DEBUG_MEMORY_LEAK
	u_sock=malloc(sizeof(UMPSocket));
	memset(u_sock,0,sizeof(UMPSocket));
#else
	u_sock=g_malloc0(sizeof(UMPSocket));
#endif
	//memset(u_sock,0,sizeof(UMPSocket));
	u_sock->u_core=u_core;
	u_sock->their_addr=(*their_addr);
	//mutex、event、queue等的初始化，线程的启动
	u_sock->pubic_state_lock=g_mutex_new();
	u_sock->state=UMP_CLOSED;
	g_mutex_lock(u_sock->pubic_state_lock);
		u_sock->public_state=UMP_CLOSED;
	g_mutex_unlock(u_sock->pubic_state_lock);

	u_sock->thread_stop_work_lock=g_mutex_new();

	u_sock->our_mss=DEFAULT_MSS;
	u_sock->our_cwnd=1;
	u_sock->our_ssthresh=MAX_CWND;
	u_sock->our_rwnd=REC_QUEUE_LIMIT/2;

	u_sock->rto_cpt=rto_computer_new();

	u_sock->rec_packets_lock=g_mutex_new();
	u_sock->rec_control_packets=g_queue_new();
	u_sock->rec_data_packets=g_queue_new();

	u_sock->do_work_event=m_event_new(FALSE,TRUE);

	u_sock->ctrl_para_lock=g_mutex_new();
	u_sock->ctrl_locked=UMP_UNLOCKED;
	u_sock->ctrl_done=m_event_new(FALSE,TRUE);

	u_sock->snd_para_lock=g_mutex_new();
	u_sock->snd_locked=UMP_UNLOCKED;
	u_sock->snd_done=m_event_new(FALSE,TRUE);

	u_sock->rec_para_lock=g_mutex_new();
	u_sock->rec_done=m_event_new(FALSE,TRUE);

	u_sock->our_ctrl_seq=(guint16)g_random_int_range(0,G_MAXUINT16);//SEQ_START;
	u_sock->our_data_start_seq=u_sock->our_ctrl_seq;
	u_sock->our_data_seq_base=u_sock->our_ctrl_seq;
	//u_sock->our_data_pos=0;
	ump_init_ctrl_rto(u_sock);
	u_sock->ctrl_packet_sent=TRUE;

	u_sock->error_occured=FALSE;
	u_sock->send_reset_packet=FALSE;

	u_sock->sock_thread=g_thread_create(ump_sock_thread_func,u_sock,TRUE,NULL);
	if(u_sock->sock_thread==NULL){
		ump_sock_free(u_sock);
		u_sock=NULL;
	}
	return u_sock;
}

void ump_sock_free(UMPSocket* u_sock)
{
	UMPPacket* p;
	GList* llist=NULL;
	if(u_sock->sock_thread!=NULL){
		g_mutex_lock(u_sock->thread_stop_work_lock);
			u_sock->thread_stop_work=TRUE;
		g_mutex_unlock(u_sock->thread_stop_work_lock);
		m_event_set(u_sock->do_work_event);
		g_thread_join(u_sock->sock_thread);
	}
	//执行清理工作，释放控制调用锁和其他锁

	//要向用户的控制线程报告控制失败
	//todo:ump_sock_free可能由主清理线程调用，导致数据损毁
	ump_end_ctrl(u_sock,METHOD_ALL_CALL,FALSE);
	ump_end_send(u_sock,FALSE);
	ump_end_receive(u_sock,FALSE);

	while(g_queue_is_empty(u_sock->rec_control_packets)==FALSE){
			p=g_queue_pop_tail(u_sock->rec_control_packets);
			u_packet_free(p);
	}
	while(g_queue_is_empty(u_sock->rec_data_packets)==FALSE){
		p=g_queue_pop_tail(u_sock->rec_data_packets);
		u_packet_free(p);
	}
	for(llist=ump_list_first(u_sock->rcv_data_packets);llist!=NULL;llist=llist->next)
	{
		u_packet_free(llist->data);
	}
	g_list_free(u_sock->rcv_data_packets);
	u_sock->local_data_packets_count=0;
	for(llist=ump_list_first(u_sock->local_data_packets);llist!=NULL;llist=llist->next)
	{
		u_packet_free(llist->data);
	}
	g_list_free(u_sock->local_data_packets);
	for(llist=ump_list_first(u_sock->local_ctrl_packets);llist!=NULL;llist=llist->next)
	{
		u_packet_free(llist->data);
	}
	g_list_free(u_sock->local_ctrl_packets);
	for(llist=ump_list_first(u_sock->rec_msg_packets);llist!=NULL;llist=llist->next)
	{
		u_packet_free(llist->data);
	}
	g_list_free(u_sock->rec_msg_packets);
	g_mutex_free(u_sock->thread_stop_work_lock);
	g_mutex_free(u_sock->pubic_state_lock);
	g_mutex_free(u_sock->rec_packets_lock);
	g_mutex_free(u_sock->ctrl_para_lock);
	g_mutex_free(u_sock->snd_para_lock);
	g_mutex_free(u_sock->rec_para_lock);
	m_event_free(u_sock->do_work_event);
	m_event_free(u_sock->ctrl_done);
	m_event_free(u_sock->snd_done);
	m_event_free(u_sock->rec_done);
	if(u_sock->ctrl_packet_to_send!=NULL){
		u_packet_free(u_sock->ctrl_packet_to_send);
		u_sock->ctrl_packet_to_send=NULL;
	}
	if(u_sock->up_req_wnd!=NULL){
		u_packet_free(u_sock->up_req_wnd);
		u_sock->up_req_wnd=NULL;
	}
	rto_computer_free(u_sock->rto_cpt);
#ifdef DEBUG_MEMORY_LEAK
	free(u_sock);
#else
	g_free(u_sock);
#endif
	return;
}

gboolean ump_sock_connect(UMPSocket* u_sock)//,struct sockaddr_in their_addr)
{
	UMPCtrlParam pa;
	gboolean r=FALSE;

	pa.ctrl_param_struct=NULL;
	pa.ctrl_method=METHOD_CONNECT;
	r=ump_sock_control_call(u_sock,&pa);
	return r;
}

gboolean ump_sock_close(UMPSocket* u_sock)
{
	UMPCtrlParam pa;
	gboolean r=FALSE;

	pa.ctrl_param_struct=NULL;
	pa.ctrl_method=METHOD_CLOSE;
	r=ump_sock_control_call(u_sock,&pa);
	return r;
}

//加锁，将参数放置到UMPSocket里，然后激发，接着等待
gboolean ump_sock_send(UMPSocket* u_sock,UMPPacket** data_packets,gint packets_count)
{
	g_mutex_lock(u_sock->snd_para_lock);
		u_sock->snd_packets=data_packets;
		u_sock->snd_packets_count=packets_count;
		m_event_reset(u_sock->snd_done);
	g_mutex_unlock(u_sock->snd_para_lock);
	m_event_set(u_sock->do_work_event);
#ifdef VERBOSE
	log_out("send issued\r\n");
#endif
	m_event_wait(u_sock->snd_done);
	return u_sock->snd_ok;
}

//加锁，将参数放置到UMPSocket里，然后激发，接着等待
gchar* ump_sock_receive(UMPSocket* u_sock,gint *rec_len)
{
	gchar* data=NULL;
	g_mutex_lock(u_sock->rec_para_lock);
		u_sock->receive_called=TRUE;
		m_event_reset(u_sock->rec_done);
	g_mutex_unlock(u_sock->rec_para_lock);
	m_event_set(u_sock->do_work_event);
	m_event_wait(u_sock->rec_done);
	if(u_sock->rec_ok==FALSE){
		*rec_len=0;
		return NULL;
	}
	*rec_len=u_sock->rec_msg_l;
	data=u_sock->rec_msg;
	u_sock->rec_msg=NULL;
	return data;
}


gboolean ump_sock_control_call(UMPSocket* u_sock,UMPCtrlParam* call_param)
{
	g_mutex_lock(u_sock->ctrl_para_lock);
		u_sock->ctrl_para=call_param;
		m_event_reset(u_sock->ctrl_done);
	g_mutex_unlock(u_sock->ctrl_para_lock);
	m_event_set(u_sock->do_work_event);
	m_event_wait(u_sock->ctrl_done);
	return u_sock->ctrl_ok;
}

void ump_handle_send_ctrl_packet(UMPSocket* u_sock,glong* sleep_ms)
{
	gint data_len;
	gpointer data=NULL;
	UMPPacket* ctrl_ack_alone=NULL;
	gint send_r=0;
	if(u_sock->ctrl_packet_sent==FALSE && u_sock->ctrl_packet_to_send!=NULL){
		//捎带ctrl_ack
		if(u_sock->ack_info.ctrl_ack_set==TRUE){
			u_packet_set_flag(u_sock->ctrl_packet_to_send,UP_CTRL_ACK);
			u_sock->ctrl_packet_to_send->ack_num=u_sock->ack_info.ctrl_ack_data;
			u_sock->ack_info.ctrl_ack_set=FALSE;
		}else{
			u_packet_clear_flag(u_sock->ctrl_packet_to_send,UP_CTRL_ACK);
		}
		data=u_packet_to_binary(u_sock->ctrl_packet_to_send,&data_len);
		g_get_current_time(&(u_sock->ctrl_resend_time));
		g_time_val_add(&(u_sock->ctrl_resend_time),u_sock->ctrl_rto*1000);
		send_r=ump_sendto(u_sock->u_core,data,data_len,&u_sock->their_addr);
		u_sock->ctrl_packet_sent=TRUE;
	}else{
		//单独发送ctrl_ack，临时构造一个包，发送出去，再释放内存
		if(u_sock->ack_info.ctrl_ack_set){
			ctrl_ack_alone=u_packet_new(P_CONTROL,P_OUTGOING);
			u_packet_set_flag(ctrl_ack_alone,UP_CTRL_ACK);
			ctrl_ack_alone->ack_num=u_sock->ack_info.ctrl_ack_data;
			u_sock->ack_info.ctrl_ack_set=FALSE;
			data=u_packet_to_binary(ctrl_ack_alone,&data_len);
			ump_sendto(u_sock->u_core,data,data_len,&u_sock->their_addr);
			u_packet_free(ctrl_ack_alone);
		}
	}
	return;
}

void ump_handle_ctrl_packet(UMPSocket* u_sock,glong* sleep_ms)
{
	GList* first=NULL;
	UMPPacket* p=NULL;
	if(u_sock->local_ctrl_packets==NULL){
		return;
	}
	while(u_sock->local_ctrl_packets!=NULL){
		first=ump_list_first(u_sock->local_ctrl_packets);
		u_sock->local_ctrl_packets=g_list_remove_link(u_sock->local_ctrl_packets,first);
		p=first->data;
#ifdef VERBOSE2
		if(u_packet_get_flag(p,UP_CTRL_FIN)==TRUE){
			log_out("got FIN\n");
		}
#endif
		if(u_packet_get_flag(p,UP_CTRL_RST)==TRUE){//处理rst报文，结束所有调用，将状态切换为closed
			u_packet_free(p);
			p=NULL;
			g_list_free(first);
			first=NULL;
			u_sock->error_occured=TRUE;
			ump_change_state(u_sock,UMP_CLOSED);
			ump_end_ctrl(u_sock,METHOD_ALL_CALL,FALSE);
			ump_end_send(u_sock,FALSE);
			ump_end_receive(u_sock,FALSE);
			break;
		}
		//调用状态机
		u_sm_funcs[u_sock->state].sm_handle_ctrl_packet(u_sock,p,sleep_ms);
		u_packet_free(p);
		p=NULL;
		g_list_free(first);
		first=NULL;
	}
	return;
}

void ump_handle_ctrl(UMPSocket* u_sock,glong* sleep_ms){
	u_ctrl_sm_funcs[u_sock->ctrl_locked].ctrllock_sm_handle_ctrl(u_sock,sleep_ms);
	return;
}

void ump_end_ctrl(UMPSocket* u_sock,UMPCallMethod call_method, gboolean ctrl_result){
	u_ctrl_sm_funcs[u_sock->ctrl_locked].ctrllock_sm_end_ctrl(u_sock,call_method,ctrl_result);
	return;
}

///////////////////////////control lock状态机
void ump_change_ctrllock_state(UMPSocket* u_sock,UMPLockState s)
{
	u_ctrl_sm_funcs[u_sock->ctrl_locked].ctrllock_sm_leave_state(u_sock);
	u_sock->ctrl_locked=s;
	u_ctrl_sm_funcs[u_sock->ctrl_locked].ctrllock_sm_enter_state(u_sock);
	return;
}

///////////////////////ctrl unlocked状态机
void ump_ctrlunlocked_handle_ctrl(UMPSocket* u_sock,glong* sleep_ms)
{
	gboolean locked=FALSE;
	locked=g_mutex_trylock(u_sock->ctrl_para_lock);//记得调用完成后释放锁，设置事件，并让lock_ok=FALSE
	if(locked){
		//检测用户是否发出了ctrl调用
		if(u_sock->ctrl_para!=NULL){
			ump_change_ctrllock_state(u_sock,UMP_LOCKED);
			if(u_sock->error_occured==TRUE){
				u_ctrl_sm_funcs[u_sock->ctrl_locked].ctrllock_sm_end_ctrl(u_sock,METHOD_ALL_CALL,FALSE);
			}else{
				//获取了ctrl锁，根据连接状态机执行调用
				u_sm_funcs[u_sock->state].sm_handle_ctrl(u_sock,sleep_ms);
			}
		}else{
			//没有ctrl调用
			g_mutex_unlock(u_sock->ctrl_para_lock);
		}
	}
	return;
}

void ump_ctrlunlocked_enter_state(UMPSocket* u_sock){
	return;
}

void ump_ctrlunlocked_leave_state(UMPSocket* u_sock){
	return;
}

void ump_ctrlunlocked_end_ctrl(UMPSocket* u_sock,UMPCallMethod call_method, gboolean ctrl_result)
{
	if(METHOD_CONNECT==call_method){
		m_event_set(u_sock->u_core->accept_ok);
	}
	return;
}

///////////////////////ctrl locked状态机
void ump_ctrlocked_handle_ctrl(UMPSocket* u_sock,glong* sleep_ms)
{
	//已经获取了lock并且在获取时执行过调用，现在什么也不做，不重复执行调用。
	return;
}

void ump_ctrllocked_enter_state(UMPSocket* u_sock){
	return;
}

void ump_ctrllocked_leave_state(UMPSocket* u_sock){
	return;
}

void ump_ctrllocked_end_ctrl(UMPSocket* u_sock,UMPCallMethod call_method, gboolean ctrl_result)
{
	if(call_method!=METHOD_ALL_CALL && u_sock->ctrl_para->ctrl_method!=call_method){
		return;
	}
	u_sock->ctrl_ok=ctrl_result;
	u_sock->ctrl_para=NULL;
	ump_change_ctrllock_state(u_sock,UMP_UNLOCKED);
	g_mutex_unlock(u_sock->ctrl_para_lock);
	m_event_set(u_sock->ctrl_done);
	return;
}



void ump_handle_send(UMPSocket* u_sock,glong* sleep_ms){
	u_snd_sm_funcs[u_sock->snd_locked].sndlock_sm_handle_snd(u_sock,sleep_ms);
	return;
}


void ump_end_send(UMPSocket* u_sock,gboolean snd_result){
	u_snd_sm_funcs[u_sock->snd_locked].sndlock_sm_end_snd(u_sock,snd_result);
	return;
}

void ump_handle_send_data(UMPSocket* u_sock,glong* sleep_ms){
	u_snd_sm_funcs[u_sock->snd_locked].sndlock_sm_handle_send_data(u_sock,sleep_ms);
	return;
}

///////////////////////////send lock状态机
void ump_change_sndlock_state(UMPSocket* u_sock,UMPLockState s)
{
	u_snd_sm_funcs[u_sock->snd_locked].sndlock_sm_leave_state(u_sock);
	u_sock->snd_locked=s;
	u_snd_sm_funcs[u_sock->snd_locked].sndlock_sm_enter_state(u_sock);
	return;
}

//////////////////////////snd unlocked状态机
void ump_sndunlocked_handle_snd(UMPSocket* u_sock,glong* sleep_ms)
{
	gboolean locked=FALSE;
	guint16 seq=u_sock->our_data_start_seq;
	gint i=0;

	locked=g_mutex_trylock(u_sock->snd_para_lock);

	if(locked==FALSE){
		return;
	}
	if(u_sock->snd_packets==NULL){
		//没有snd调用
		g_mutex_unlock(u_sock->snd_para_lock);
		return;
	}

#ifdef VERBOSE
	log_out("send locked\r\n");
#endif

	//切换状态机状态
	ump_change_sndlock_state(u_sock,UMP_LOCKED);

	if(u_sock->snd_packets_count<1 || u_sock->state!=UMP_ESTABLISHED || u_sock->error_occured==TRUE){
		ump_end_send(u_sock,FALSE);
		return;
	}

	//为数据包编号
	for(i=0;i<u_sock->snd_packets_count;++i,++seq){
		u_packet_set_flag(u_sock->snd_packets[i],UP_DATA_SEQ);
		u_sock->snd_packets[i]->seq_num=seq;
	}

	//一批数据报中的最后一个以BDR标志
	u_packet_set_flag(u_sock->snd_packets[u_sock->snd_packets_count-1],UP_DATA_BDR);
	
	//记录此组数据报的末seq
	u_sock->our_msg_last_seq=--seq;

	u_sock->our_data_pos=0;
	//更新发送窗口
	ump_refresh_snd_wnd_pos(u_sock);

	return;
}

void ump_sndunlocked_enter_state(UMPSocket* u_sock){
	return;
}

void ump_sndunlocked_leave_state(UMPSocket* u_sock){
	return;
}

void ump_sndunlocked_end_snd(UMPSocket* u_sock,gboolean snd_result){
	return;
}

void ump_sndunlocked_handle_send_data(UMPSocket* u_sock,glong* sleep_ms)
{
	gint data_len;
	gpointer data=NULL;
	UMPPacket* data_ack_alone=NULL;
	//单独发送ack
	if(u_sock->ack_info.data_ack_set){
#ifdef VERBOSE
		log_out("sending ack alone unlocked\r\n");
#endif
		data_ack_alone=u_packet_new(P_DATA,P_OUTGOING);
		ump_set_packet_ack_info(u_sock,data_ack_alone);
		data=u_packet_to_binary(data_ack_alone,&data_len);
		ump_sendto(u_sock->u_core,data,data_len,&u_sock->their_addr);
		u_packet_free(data_ack_alone);
	}
	return;
}

///////////////////////snd locked状态机
void ump_sndlocked_handle_snd(UMPSocket* u_sock,glong* sleep_ms){
	//Do nothing
	//log_out("send already locked\r\n");
	return;
}

void ump_sndlocked_enter_state(UMPSocket* u_sock){
	return;
}

void ump_sndlocked_leave_state(UMPSocket* u_sock){
	return;
}

void ump_sndlocked_end_snd(UMPSocket* u_sock,gboolean snd_result)
{
	ump_change_sndlock_state(u_sock,UMP_UNLOCKED);
	u_sock->snd_ok=snd_result;
	if(snd_result==TRUE){
		u_sock->our_data_start_seq+= ((unsigned int)u_sock->snd_packets_count) % (((unsigned int)G_MAXUINT16)+1);
	}
	u_sock->snd_packets=NULL;
	u_sock->snd_packets_count=0;
	g_mutex_unlock(u_sock->snd_para_lock);
	m_event_set(u_sock->snd_done);
#ifdef VERBOSE
	log_out("send completed %d\n",snd_result);
#endif
	return;
}

void ump_sndlocked_handle_send_data(UMPSocket* u_sock,glong* sleep_ms)
{
	gint data_len;
	gpointer data=NULL;
	gint wnd_true=0;
	UMPPacket* data_ack_alone=NULL;

	if(u_sock->our_data_pos<u_sock->our_wnd_pos){
		//捎带ack
		if(u_sock->ack_info.data_ack_set==TRUE){
#ifdef VERBOSE
			log_out("sending data seq %u and ack locked\r\n",(u_sock->snd_packets[u_sock->our_data_pos])->seq_num);
#endif
			ump_set_packet_ack_info(u_sock,u_sock->snd_packets[u_sock->our_data_pos]);
		}else{
#ifdef VERBOSE
			log_out("sending data seq %u locked\r\n",(u_sock->snd_packets[u_sock->our_data_pos])->seq_num);
#endif
			ump_clear_packet_ack_info(u_sock,u_sock->snd_packets[u_sock->our_data_pos]);
		}
		data=u_packet_to_binary(u_sock->snd_packets[u_sock->our_data_pos],&data_len);
		wnd_true=MAX(1,ump_cmp_in_sndbase(u_sock,ump_relative_to_seq_via_sndstartseq(u_sock,u_sock->our_wnd_pos),u_sock->our_data_seq_base)+1);
		tm_register_packet(&(u_sock->tm_list),u_sock->snd_packets[u_sock->our_data_pos]->seq_num,rto_get_rto(u_sock->rto_cpt)*wnd_true);
		ump_sendto(u_sock->u_core,data,data_len,&u_sock->their_addr);
		ump_refresh_data_sent(u_sock,u_sock->snd_packets[u_sock->our_data_pos]->seq_num);
		++(u_sock->our_data_pos);
		*sleep_ms=0;//窗口没用完，稍后还要继续发送，所以线程就不能休息了
	}else{
#ifdef VERBOSE1
		log_out("send window exhausted\n");
#endif
		//单独发送ack
		if(u_sock->ack_info.data_ack_set==TRUE){
#ifdef VERBOSE
			log_out("sending ack alone locked\r\n");
#endif
			data_ack_alone=u_packet_new(P_DATA,P_OUTGOING);
			ump_set_packet_ack_info(u_sock,data_ack_alone);
			data=u_packet_to_binary(data_ack_alone,&data_len);
			ump_sendto(u_sock->u_core,data,data_len,&u_sock->their_addr);
			u_packet_free(data_ack_alone);
		}
	}
	return;
}





void ump_handle_receive(UMPSocket* u_sock,glong* sleep_ms){
	u_rec_sm_funcs[u_sock->rec_locked].reclock_sm_handle_rec(u_sock,sleep_ms);
	return;
}

void ump_end_receive(UMPSocket* u_sock,gboolean rec_result){
	u_rec_sm_funcs[u_sock->rec_locked].reclock_sm_end_rec(u_sock,rec_result);
	return;
}

//前件：u_sock->rec_msg_packets!=NULL且u_sock->rec_msg、u_sock->rec_msg_l中没有有效数据
void ump_end_receive_internal(UMPSocket* u_sock)
{
	GList* rec_p=NULL;
	gint data_len=0;
	gint data_pos=0;
	gchar *data=NULL;
	
	for(rec_p=ump_list_first(u_sock->rec_msg_packets);rec_p!=NULL;rec_p=rec_p->next){
		data_len+=((UMPPacket*)(rec_p->data))->user_data_l;
	}
#ifdef DEBUG_MEMORY_LEAK
	data=(gchar*)malloc(data_len);
#else
	data=(gchar*)g_malloc(data_len);
#endif
	for(rec_p=ump_list_first(u_sock->rec_msg_packets);rec_p!=NULL;rec_p=rec_p->next){
		memcpy(data+data_pos,((UMPPacket*)(rec_p->data))->user_data,((UMPPacket*)(rec_p->data))->user_data_l);
		data_pos+=((UMPPacket*)(rec_p->data))->user_data_l;
		u_packet_free((UMPPacket*)(rec_p->data));
	}
	g_list_free(u_sock->rec_msg_packets);
	u_sock->rec_msg_packets=NULL;

	u_sock->rec_msg=data;
	u_sock->rec_msg_l=data_len;
	if(u_sock->rcv_packets_msg_num>=1){
		ump_harvest_messages(u_sock);
	}
	ump_set_our_ack_info(u_sock,TRUE);
	return;
}

///////////////////////////rec lock状态机
void ump_change_reclock_state(UMPSocket* u_sock,UMPLockState s)
{
	u_rec_sm_funcs[u_sock->rec_locked].reclock_sm_leave_state(u_sock);
	u_sock->rec_locked=s;
	u_rec_sm_funcs[u_sock->rec_locked].reclock_sm_enter_state(u_sock);
	return;
}

//////////////////////////rec unlocked状态机
void ump_recunlocked_handle_rec(UMPSocket* u_sock,glong* sleep_ms)
{
	gboolean locked=FALSE;

	locked=g_mutex_trylock(u_sock->rec_para_lock);

	if(locked==FALSE){
		return;
	}
	if(u_sock->receive_called==FALSE){
		//没有receive调用
		g_mutex_unlock(u_sock->rec_para_lock);
		return;
	}
	if(u_sock->rec_msg_packets!=NULL){
		u_sock->rec_ok=TRUE;
		ump_end_receive_internal(u_sock);
		u_sock->receive_called=FALSE;
		g_mutex_unlock(u_sock->rec_para_lock);
		m_event_set(u_sock->rec_done);
		return;
	}
	if(u_sock->state!=UMP_ESTABLISHED || u_sock->error_occured==TRUE){
		u_sock->rec_ok=FALSE;
		u_sock->receive_called=FALSE;
		g_mutex_unlock(u_sock->rec_para_lock);
		m_event_set(u_sock->rec_done);
		return;
	}
	//切换状态机状态
	ump_change_reclock_state(u_sock,UMP_LOCKED);
	return;
}

void ump_recunlocked_enter_state(UMPSocket* u_sock){
	return;
}

void ump_recunlocked_leave_state(UMPSocket* u_sock){
	return;
}

void ump_recunlocked_end_rec(UMPSocket* u_sock,gboolean rec_result)
{
	return;
}

///////////////////////rec locked状态机
void ump_reclocked_handle_rec(UMPSocket* u_sock,glong* sleep_ms){
	return;
}

void ump_reclocked_enter_state(UMPSocket* u_sock){
	return;
}

void ump_reclocked_leave_state(UMPSocket* u_sock){
	return;
}

void ump_reclocked_end_rec(UMPSocket* u_sock,gboolean rec_result)
{
	ump_change_reclock_state(u_sock,UMP_UNLOCKED);
	u_sock->rec_ok=rec_result;
	if(rec_result==TRUE){
		ump_end_receive_internal(u_sock);
	}
	u_sock->receive_called=FALSE;
	g_mutex_unlock(u_sock->rec_para_lock);
	m_event_set(u_sock->rec_done);
	return;
}





void ump_handle_ctrl_timeout(UMPSocket* u_sock,glong* sleep_ms)
{
	GTimeVal curtime;
	if(u_sock->ctrl_packet_to_send==NULL){
		return;
	}
	*sleep_ms=MIN((*sleep_ms),u_sock->ctrl_rto);
	//判断是否需要处理超时
	if(u_sock->ctrl_packet_sent!=TRUE){
		return;
	}
	g_get_current_time(&curtime);
	//判断是否超时
	if(ump_time_sub(&curtime,&(u_sock->ctrl_resend_time))<0){
		return;
	}
	
	//超时了，需要重发
	if(u_sock->ctrl_rto<15000){
		//一般的超时
		u_sock->ctrl_packet_sent=FALSE;
		//重新设置超时时间
		ump_timeout_refresh_ctrl_rto(u_sock);
		//重发的包将自动由ump_handle_send_ctrl_packet发送出去
		*sleep_ms=0;
		return;
	}
	
	//如果超时太久，则放弃发送
	u_packet_free(u_sock->ctrl_packet_to_send);
	u_sock->ctrl_packet_to_send=NULL;
	u_sock->ctrl_packet_sent=TRUE;
	ump_init_ctrl_rto(u_sock);
	//要向用户的控制线程报告发送失败
	ump_end_ctrl(u_sock,METHOD_ALL_CALL,FALSE);
	//todo:要释放连接，停止工作

	return;
}

void ump_handle_data_packet(UMPSocket* u_sock,glong* sleep_ms)
{
	GList* first=NULL;
	UMPPacket* p=NULL;
	while(u_sock->local_data_packets!=NULL){
		first=ump_list_first(u_sock->local_data_packets);
		u_sock->local_data_packets=ump_list_remove_link(u_sock->local_data_packets,first,&(u_sock->local_data_packets_count));
		p=first->data;
		if(u_packet_get_flag(p,UP_DATA_SEQ)==TRUE){
			ump_set_our_ack_info(u_sock,FALSE);
		}
		//调用状态机
		u_sm_funcs[u_sock->state].sm_handle_data_packet(u_sock,p,sleep_ms);
		p=NULL;
		g_list_free(first);
		first=NULL;
	}
	return;
}

void ump_handle_data_timeout(UMPSocket* u_sock,glong* sleep_ms)
{
	guint16 tout_seq=0;
	gboolean tout=FALSE;
	tout=get_next_timeout(&(u_sock->tm_list),&tout_seq);
	if(!tout){
		//没有超时
		return;
	}
#ifdef VERBOSE
	log_out("timeout occured\n");
#endif
	//超时太多则放弃重发，向用户报告错误
	if(rto_get_rto(u_sock->rto_cpt)>=15000){
		ump_end_send(u_sock,FALSE);
		//todo:释放连接，停止工作
		return;
	}

	//存在超时
	u_sock->our_ssthresh = MAX(u_sock->our_cwnd/2 , 2);
	u_sock->our_cwnd=1;
	ump_refresh_back_point(u_sock,ump_relative_to_seq_via_sndstartseq(u_sock,(guint)u_sock->our_data_pos));
	u_sock->our_data_pos=ump_seq_to_relative_via_sndstartseq(u_sock,u_sock->our_data_seq_base);
	u_sock->ack_rep_count=0;
	u_sock->fast_retran=FALSE;
	rto_timeout_occur(u_sock->rto_cpt);
	ump_refresh_snd_wnd_pos(u_sock);
	*sleep_ms=0;

	return;
}

void ump_handle_wnd_notify(UMPSocket *u_sock,UMPPacket* u_p,glong *sleep_ms)
{
	gint old_wnd_pos=0;
	//处理通告窗口信息
	if(u_packet_get_flag(u_p,UP_DATA_WND)==TRUE){
		u_sock->their_wnd=u_p->wnd_num;
		old_wnd_pos=u_sock->our_wnd_pos;
		ump_refresh_snd_wnd_pos(u_sock);
		if(u_sock->our_wnd_pos>old_wnd_pos){
			*sleep_ms=0;
		}
		//if(u_sock->their_wnd==u_sock->our_data_seq_base){
		if(u_sock->their_wnd==0){
			//窗口关闭，打开主动探察
			u_sock->act_req_wnd=TRUE;
			g_get_current_time(&(u_sock->last_req_wnd_time));
			*sleep_ms=MIN(*sleep_ms,REQ_WND_DELAY);
		}else{
			//窗口打开，关闭主动探察
			u_sock->act_req_wnd=FALSE;
		}
	}
	return;
}

void ump_handle_data_ack(UMPSocket *u_sock,UMPPacket* u_p,glong *sleep_ms)
{
	glong rtt=0;
	glong est_rtt=0;
	GTimeVal now;
	guint16 ack_seq=0;
	gint32 ack_diff=0;
	if(u_packet_get_flag(u_p,UP_DATA_ACK)==TRUE){
		ack_seq=u_p->ack_num;
#ifdef VERBOSE
		log_out("got ack %u\n",ack_seq);
#endif
	}
	if(ack_seq==u_sock->our_data_seq_base){
		++(u_sock->ack_rep_count);
	}
	if(u_sock->ack_rep_count>1){
		//快速重传
		ump_refresh_back_point(u_sock,ump_relative_to_seq_via_sndstartseq(u_sock,(guint)u_sock->our_data_pos));
		u_sock->our_data_pos=ump_seq_to_relative_via_sndstartseq(u_sock,u_sock->our_data_seq_base);
		u_sock->our_cwnd=MAX(u_sock->our_ssthresh+3,1);
		u_sock->ack_rep_count=0;
		u_sock->fast_retran=TRUE;
		tm_clear_list(&(u_sock->tm_list));
		rto_timeout_occur(u_sock->rto_cpt);
		*sleep_ms=0;
	}
	//检查ack是否在合法范围之内
	ack_diff=ump_cmp_in_sndbase(u_sock,ack_seq,u_sock->our_data_seq_base);
	if(ack_diff<0 || ump_cmp_in_sndbase(u_sock,ack_seq,u_sock->our_data_sent)>1){
		return;
	}
	if(ack_diff>0){
		//有新的ack到来
		u_sock->our_data_seq_base=ack_seq;
		u_sock->ack_rep_count=0;
		//为back_point设置底线，ump_refresh_back_point只增大back_point
		ump_refresh_back_point(u_sock,u_sock->our_data_seq_base);
	}
	//ack_diff=0和ack_diff>0的情况都要处理通告窗口信息
	ump_handle_wnd_notify(u_sock,u_p,sleep_ms);
	if(ack_diff==0){
		//如果没有新的ack，后面的就不用处理了
		return;
	}

	//处理快速恢复
	if(u_sock->fast_retran==TRUE){
		u_sock->fast_retran=FALSE;
		u_sock->our_cwnd=MAX(u_sock->our_ssthresh,1);
	}else{
		if(u_sock->our_cwnd<u_sock->our_ssthresh){
			//慢启动，每ack增加一次cwnd
			u_sock->our_cwnd=MAX(u_sock->our_cwnd+1,u_sock->our_cwnd);
			g_get_current_time(&(u_sock->last_refresh_cwnd));
		}else{
			g_get_current_time(&now);
			est_rtt=rto_get_estimated_rtt(u_sock->rto_cpt);
			if(ump_time_sub(&now,&(u_sock->last_refresh_cwnd))>=est_rtt){
				//拥塞避免，每rtt增加一次cwnd
				u_sock->our_cwnd=MAX(u_sock->our_cwnd+1,u_sock->our_cwnd);
				g_get_current_time(&(u_sock->last_refresh_cwnd));
			}
		}
	}
	//刷新数据发送指针
	if(u_sock->our_data_pos<ump_seq_to_relative_via_sndstartseq(u_sock,u_sock->our_data_seq_base)){
		u_sock->our_data_pos=ump_seq_to_relative_via_sndstartseq(u_sock,u_sock->our_data_seq_base);
	}
	//消解超时项目，刷新rtt
	rtt=tm_ack_packet(&(u_sock->tm_list),ack_seq,u_sock->our_data_sent);
	if(rtt>=0){//rtt<0表示出错
		if(ack_seq>=u_sock->our_back_point){//只有大于back_point的ack才计算rtt
			if(rtt==0){rtt=RTT_MIN_MS;}
			rto_refresh_rtt(u_sock->rto_cpt,rtt);
		}
	}
	if( ump_cmp_in_sndbase(u_sock,ack_seq,u_sock->our_msg_last_seq)>0 ){
		ump_end_send(u_sock,TRUE);
	}
	return;
}

void ump_harvest_messages(UMPSocket* u_sock)
{
	GList* head;
	//摘除数据
	u_sock->rec_msg_packets=u_sock->rcv_data_packets;
	u_sock->rcv_data_packets=u_sock->rcv_first_msg_end->next;
	u_sock->rcv_first_msg_end->next=NULL;
	//检查rcv_data_so_far是否被摘除
	if(u_sock->rcv_data_so_far==u_sock->rcv_first_msg_end){
		u_sock->rcv_data_so_far=NULL;
	}
	//准备更新rcv_first_msg_end
	--(u_sock->rcv_packets_msg_num);
	if(u_sock->rcv_packets_msg_num<1){
		//剩下的数据不足以拼出一个消息
		u_sock->rcv_first_msg_end=NULL;
		return;
	}
	//剩下的数据足以拼出一个或以上消息，找到第一个消息的结尾
	head=ump_list_first(u_sock->rcv_data_packets);
	while(head!=NULL && u_packet_get_flag((UMPPacket*)(head->data),UP_DATA_BDR)==FALSE){
		head=head->next;
	}
	u_sock->rcv_first_msg_end=head;//适用于head==NULL的特殊情况和head!=NULL的一般情况
	return;
}

void ump_check_rcv_data_so_far(UMPSocket* u_sock)
{
	//检查到u_sock->rcv_data_so_far为止是否包含了完整的消息
	if(u_packet_get_flag(((UMPPacket*)(u_sock->rcv_data_so_far->data)),UP_DATA_BDR)==TRUE){
		++(u_sock->rcv_packets_msg_num);
		if(u_sock->rcv_packets_msg_num==1){
			u_sock->rcv_first_msg_end=u_sock->rcv_data_so_far;
		}
		if(u_sock->rec_msg_packets==NULL){
			ump_harvest_messages(u_sock);
			ump_end_receive(u_sock,TRUE);
		}
	}
	return;
}

void ump_act_req_wnd(UMPSocket* u_sock,glong *sleep_ms)
{
	GTimeVal now;
	glong delay;
	gpointer data;
	gint data_len;
	if(u_sock->act_req_wnd==FALSE){
		return;
	}
	g_get_current_time(&now);
	delay=ump_time_sub(&now,&u_sock->last_req_wnd_time);
	if(delay>=REQ_WND_DELAY){
		if(u_sock->up_req_wnd==NULL){
			u_sock->up_req_wnd=u_packet_new(P_DATA,P_OUTGOING);
			u_packet_set_flag(u_sock->up_req_wnd,UP_DATA_REQWND);
		}
		data=u_packet_to_binary(u_sock->up_req_wnd,&data_len);
		ump_sendto(u_sock->u_core,data,data_len,&u_sock->their_addr);
		*sleep_ms=MIN(*sleep_ms,REQ_WND_DELAY);
	}else{
		*sleep_ms=MIN(*sleep_ms,REQ_WND_DELAY-delay);
	}
	return;
}


void ump_timeout_refresh_ctrl_rto(UMPSocket* u_sock)
{
	u_sock->ctrl_rto=2*u_sock->ctrl_rto;
	return;
}

void ump_init_ctrl_rto(UMPSocket* u_sock)
{
	u_sock->ctrl_rto=UMP_CTRL_TIMEOUT;
	return;
}

void ump_refresh_snd_wnd_pos(UMPSocket* u_sock)
{
	u_sock->our_wnd_pos= MIN(
			u_sock->snd_packets_count,
			ump_seq_to_relative_via_sndstartseq( u_sock, u_sock->our_data_seq_base + MIN(u_sock->their_wnd,u_sock->our_cwnd) )
		);
#ifdef VERBOSE1
	log_out("snd_packets_count %d, their_wnd %u, cwnd %u\n",u_sock->snd_packets_count,u_sock->their_wnd,u_sock->our_cwnd);
#endif
	return;
}

void ump_refresh_data_sent(UMPSocket* u_sock,guint16 sent_seq){
	if(ump_cmp_in_sndbase(u_sock,u_sock->our_data_sent,sent_seq)<0){
		u_sock->our_data_sent=sent_seq;
	}
	return;
}

void ump_refresh_back_point(UMPSocket* u_sock,guint16 back_point){
	if(ump_cmp_in_sndbase(u_sock,u_sock->our_back_point,back_point)<0){
		u_sock->our_back_point=back_point;
	}
	return;
}

void ump_refresh_our_rwnd_pos(UMPSocket* u_sock){
	if(u_sock->rcv_packets_msg_num>0 || u_sock->buffer_load> (REC_QUEUE_LIMIT/2)){
		//刷新our_rwnd使our_rwnd_pos_seq不增
		u_sock->our_rwnd=MIN(u_sock->our_rwnd,ump_cmp_in_rcvbase(u_sock,u_sock->our_rwnd_pos_seq,u_sock->their_data_seq_base))+1;
	}else{
		u_sock->our_rwnd=REC_QUEUE_LIMIT/2;
		//刷新our_rwnd_pos_seq
		u_sock->our_rwnd_pos_seq=ump_relative_to_seq_via_rcvseq(u_sock,u_sock->our_rwnd-1);
	}
}

void ump_refresh_our_ack_info(UMPSocket* u_sock,glong* sleep_ms)
{
	if(u_sock->ack_info.push_ack==TRUE){
		u_sock->ack_info.data_ack_set=TRUE;
		u_sock->ack_info.data_ack_seq=u_sock->their_data_seq_base;
		u_sock->ack_info.data_ack_wnd=u_sock->our_rwnd;
		u_sock->ack_info.push_ack=FALSE;
		u_sock->ack_info.schedule_ack=FALSE;
		g_get_current_time(&(u_sock->ack_info.last_ack_t));
		*sleep_ms=0;
		return;
	}
	if(u_sock->ack_info.schedule_ack==TRUE){
		GTimeVal now;
		glong delay=0;
		g_get_current_time(&now);
		delay=ump_time_sub(&now,&(u_sock->ack_info.last_ack_t));
		if(delay<ACK_DELAY){
			*sleep_ms=MIN(ACK_DELAY-delay,*sleep_ms);
			return;
		}
		u_sock->ack_info.data_ack_set=TRUE;
		u_sock->ack_info.data_ack_seq=u_sock->their_data_seq_base;
		u_sock->ack_info.data_ack_wnd=u_sock->our_rwnd;
		u_sock->ack_info.push_ack=FALSE;
		u_sock->ack_info.schedule_ack=FALSE;
		g_get_current_time(&(u_sock->ack_info.last_ack_t));
		*sleep_ms=0;
	}
	return;
}

void ump_set_our_ack_info(UMPSocket* u_sock,gboolean push_ack)
{
	if(push_ack==TRUE){
		u_sock->ack_info.push_ack=TRUE;
	}else{
		u_sock->ack_info.schedule_ack=TRUE;
	}
	/*if(u_sock->buffer_load <= (REC_QUEUE_LIMIT/2)){
		u_sock->ack_info.push_ack=TRUE;
	}else{
		u_sock->ack_info.schedule_ack=TRUE;
	}*/
}

void ump_set_packet_ack_info(UMPSocket* u_sock,UMPPacket* u_p)
{
	u_packet_set_flag(u_p,UP_DATA_ACK);
	u_p->ack_num=u_sock->ack_info.data_ack_seq;
	u_packet_set_flag(u_p,UP_DATA_WND);
	u_p->wnd_num=u_sock->ack_info.data_ack_wnd;
	u_sock->ack_info.data_ack_set=FALSE;
	return;
}

void ump_clear_packet_ack_info(UMPSocket* u_sock,UMPPacket* u_p)
{
	u_packet_clear_flag(u_p,UP_DATA_ACK);
	u_packet_clear_flag(u_p,UP_DATA_WND);
	return;
}

void ump_change_state(UMPSocket* u_sock,UMPSockState new_state)
{
	g_mutex_lock(u_sock->pubic_state_lock);
		u_sm_funcs[u_sock->state].sm_leave_state(u_sock);
		u_sock->state=new_state;
		u_sock->public_state=new_state;
		u_sm_funcs[u_sock->state].sm_enter_state(u_sock);
	g_mutex_unlock(u_sock->pubic_state_lock);
}




//todo: closed处理数据报文的时候采取忽略的策略
/////////////closed状态机相关函数
void ump_closed_handle_ctrl(UMPSocket* u_sock,glong* sleep_ms)
{
	//if(u_sock->ctrl_para==NULL){
	//	u_sock->ctrl_locked=FALSE;
	//	g_mutex_unlock(u_sock->ctrl_para_lock);
	//	m_event_set(u_sock->ctrl_done);
	//	return;
	//}

	//do the work
	if(u_sock->ctrl_para->ctrl_method==METHOD_CONNECT){
		ump_closed_connect(u_sock);
		ump_change_state(u_sock,UMP_CONNECTING);
	}else if(u_sock->ctrl_para->ctrl_method==METHOD_CLOSE){
		//要向用户的控制线程报告控制成功
		ump_end_ctrl(u_sock,METHOD_CLOSE,TRUE);
	}else{
		//要向用户的控制线程报告控制失败
		ump_end_ctrl(u_sock,METHOD_ALL_CALL,FALSE);
	}
	return;
}

void ump_closed_handle_ctrl_packet(UMPSocket* u_sock,UMPPacket* p,glong* sleep_ms)
{
	UMPPacket* uppri=p;
	//如果收到了对flag的ack，那么调用完毕，取消锁，设置事件，释放控制包，设置指针为NULL
	//如果收到了所有对data的ack，那么调用完毕，取消锁，设置事件，释放报文
	if(u_sock->syn_reced==FALSE && u_packet_get_flag(p,UP_CTRL_SYN)==TRUE){
		//获取SEQ、MSS、WND信息
		if(u_packet_get_flag(p,UP_CTRL_SEQ)==TRUE){
			u_sock->their_ctrl_seq=uppri->seq_num+1;
			u_sock->their_data_seq_base=uppri->seq_num;//+1;
		}else{
			return;
		}
		if(u_packet_get_flag(p,UP_CTRL_MSS)==TRUE){
			u_sock->our_mss=MIN(u_sock->our_mss,uppri->mss_num);
		}
		if(u_packet_get_flag(p,UP_CTRL_WND)==TRUE){
			u_sock->their_wnd=uppri->wnd_num;
		}
		//发送我们的syn包
		ump_closed_connect(u_sock);
		//设置回复给对方的ack
		u_sock->ack_info.ctrl_ack_set=TRUE;
		u_sock->ack_info.ctrl_ack_data=u_sock->their_ctrl_seq;
		u_sock->syn_reced=TRUE;
		ump_change_state(u_sock,UMP_CONNECTING);
	}else{
		if(u_packet_get_flag(p,UP_CTRL_SEQ)==TRUE){
			if(uppri->seq_num==u_sock->their_ctrl_seq){
				u_sock->their_ctrl_seq++;
			}
			u_sock->ack_info.ctrl_ack_set=TRUE;
			u_sock->ack_info.ctrl_ack_data=u_sock->their_ctrl_seq;
		}
	}
	return;
}

void ump_closed_handle_data_packet(UMPSocket *u_sock,UMPPacket *p,glong *sleep_ms){
	//处理ack信息
	ump_handle_data_ack(u_sock,p,sleep_ms);
	u_packet_free(p);
	return;
}

void ump_closed_connect(UMPSocket* u_sock)
{
	UMPPacket* p=u_packet_new(P_CONTROL,P_OUTGOING);

	u_packet_set_flag(p,UP_CTRL_SYN);
	u_packet_set_flag(p,UP_CTRL_SEQ);
	u_packet_set_flag(p,UP_CTRL_MSS);
	u_packet_set_flag(p,UP_CTRL_WND);

	p->seq_num=u_sock->our_ctrl_seq;
	p->mss_num=u_sock->our_mss;
	p->wnd_num=REC_QUEUE_LIMIT;

	if(u_sock->ctrl_packet_to_send!=NULL){
		u_packet_free(u_sock->ctrl_packet_to_send);
		u_sock->ctrl_packet_to_send=NULL;
	}
	u_sock->ctrl_packet_to_send=p;
	u_sock->ctrl_packet_sent=FALSE;
	ump_init_ctrl_rto(u_sock);
	u_sock->our_ctrl_seq++;
	return;
}

void ump_closed_close(UMPSocket* u_sock)
{
	return;
}

void ump_closed_send(UMPSocket* u_sock)
{
	return;
}

void ump_closed_receive(UMPSocket* u_sock)
{
	return ;
}

void ump_closed_enter_state(UMPSocket* u_sock)
{
	g_mutex_lock(u_sock->pubic_state_lock);
		g_get_current_time(&u_sock->close_time);
	g_mutex_unlock(u_sock->pubic_state_lock);

	//缓冲中的数据不足一个消息，而连接已关闭，丢弃这些数据，结束未接收完毕的调用
	if(u_sock->rcv_packets_msg_num<1){
		ump_end_receive(u_sock,FALSE);
	}
	
	//如果存在close调用，表明我们正在执行主动关闭，则释放锁
	u_ctrl_sm_funcs[u_sock->ctrl_locked].ctrllock_sm_end_ctrl(u_sock,METHOD_CLOSE,TRUE);
	//否则正在执行被动关闭，ctrl_locked的状态应是unlocked，而unlocked的end_ctrl什么也不做直接返回。

	return;
}

void ump_closed_leave_state(UMPSocket* u_sock)
{
	return;
}



//todo: connecting处理数据报文的时候采取忽略的策略
////////////connecting状态机相关函数
void ump_connecting_handle_ctrl(UMPSocket* u_sock,glong* sleep_ms)
{
	/*if(u_sock->ctrl_para==NULL){
		u_sock->ctrl_locked=FALSE;
		g_mutex_unlock(u_sock->ctrl_para_lock);
		m_event_set(u_sock->ctrl_done);
		return;
	}*/

	//do the work
	if(u_sock->ctrl_para->ctrl_method!=METHOD_CONNECT){
		//要向用户的控制线程报告控制失败
		ump_end_ctrl(u_sock,METHOD_ALL_CALL,FALSE);
	}
	return;
}

void ump_connecting_handle_ctrl_packet(UMPSocket* u_sock,UMPPacket* p,glong* sleep_ms)
{
	UMPPacket* uppri=p;
	//如果收到了对flag的ack，那么调用完毕，取消锁，设置事件，释放控制包，设置指针为NULL
	//如果收到了所有对data的ack，那么调用完毕，取消锁，设置事件，释放报文
	//解析收到的ack信息
	if(u_packet_get_flag(p,UP_CTRL_ACK)==TRUE){
		if(u_sock->our_ctrl_seq==uppri->ack_num){
			//首先消解超时
			if(u_sock->ctrl_packet_to_send!=NULL){
				u_packet_free(u_sock->ctrl_packet_to_send);
				u_sock->ctrl_packet_to_send=NULL;
			}
			u_sock->ctrl_packet_sent=TRUE;
			ump_init_ctrl_rto(u_sock);
		}
		u_sock->syn_ack_reced=TRUE;
	}
	if(u_packet_get_flag(p,UP_CTRL_SYN)==TRUE){
		if(u_sock->syn_reced==TRUE){
			u_sock->ack_info.ctrl_ack_set=TRUE;
			u_sock->ack_info.ctrl_ack_data=u_sock->their_ctrl_seq;
			return;
		}
		//获取SEQ、MSS、WND信息
		if(u_packet_get_flag(p,UP_CTRL_SEQ)==TRUE){
			u_sock->their_ctrl_seq=uppri->seq_num+1;
			u_sock->their_data_seq_base=uppri->seq_num;//+1;
		}else{
			return;
		}
		if(u_packet_get_flag(p,UP_CTRL_MSS)==TRUE){
			u_sock->our_mss=MIN(u_sock->our_mss,uppri->mss_num);
		}
		if(u_packet_get_flag(p,UP_CTRL_WND)==TRUE){
			u_sock->their_wnd=uppri->wnd_num;
		}
		//设置发出的ack
		u_sock->ack_info.ctrl_ack_set=TRUE;
		u_sock->ack_info.ctrl_ack_data=u_sock->their_ctrl_seq;
		u_sock->syn_reced=TRUE;
	}else{
		if(u_packet_get_flag(p,UP_CTRL_SEQ)==TRUE){
			if(uppri->seq_num==u_sock->their_ctrl_seq){
				u_sock->their_ctrl_seq++;
			}
			u_sock->ack_info.ctrl_ack_set=TRUE;
			u_sock->ack_info.ctrl_ack_data=u_sock->their_ctrl_seq;
		}
	}
	//如果ack和syn都收到了，就切换状态
	if(u_sock->syn_reced==TRUE && u_sock->syn_ack_reced==TRUE){
		ump_change_state(u_sock,UMP_ESTABLISHED);
	}
	return;
}

void ump_connecting_handle_data_packet(UMPSocket *u_sock,UMPPacket *p,glong *sleep_ms){
	u_packet_free(p);
	return;
}

void ump_connecting_connect(UMPSocket* u_sock)
{
	return;
}

void ump_connecting_close(UMPSocket* u_sock)
{
	return;
}

void ump_connecting_send(UMPSocket* u_sock)
{
	return;
}

void ump_connecting_receive(UMPSocket* u_sock)
{
	return;
}

void ump_connecting_enter_state(UMPSocket* u_sock)
{
	return;
}

void ump_connecting_leave_state(UMPSocket* u_sock)
{
	return;
}




////////////////established状态机相关函数
void ump_established_handle_ctrl(UMPSocket* u_sock,glong* sleep_ms)
{
	/*if(u_sock->ctrl_para==NULL){
		u_sock->ctrl_locked=FALSE;
		g_mutex_unlock(u_sock->ctrl_para_lock);
		m_event_set(u_sock->ctrl_done);
		return;
	}*/

	//do the work
	if(u_sock->ctrl_para->ctrl_method==METHOD_CLOSE){
		ump_established_close(u_sock);
		ump_change_state(u_sock,UMP_CLOSING);
		//如果存在send调用，则结束调用
		ump_end_send(u_sock,FALSE);
		//如果存在receive调用，则结束调用
		ump_end_receive(u_sock,FALSE);
	}else if(u_sock->ctrl_para->ctrl_method==METHOD_CONNECT){
		//要向用户的控制线程报告控制成功
		ump_end_ctrl(u_sock,METHOD_CONNECT,TRUE);
	}else{
		//要向用户的控制线程报告控制失败
		ump_end_ctrl(u_sock,METHOD_ALL_CALL,FALSE);
	}
	return;
}

void ump_established_handle_ctrl_packet(UMPSocket* u_sock,UMPPacket* p,glong* sleep_ms)
{
	UMPPacket* uppri=p;
	//如果收到了对flag的ack，那么调用完毕，取消锁，设置事件，释放控制包，设置指针为NULL
	//如果收到了所有对data的ack，那么调用完毕，取消锁，设置事件，释放报文
	//解析收到的ack信息
	if(u_packet_get_flag(p,UP_CTRL_ACK)==TRUE){
		if(u_sock->our_ctrl_seq==uppri->ack_num){
			//首先消解超时
			if(u_sock->ctrl_packet_to_send!=NULL){
				u_packet_free(u_sock->ctrl_packet_to_send);
				u_sock->ctrl_packet_to_send=NULL;
			}
			u_sock->ctrl_packet_sent=TRUE;
			ump_init_ctrl_rto(u_sock);
			//要向用户的控制线程报告控制成功
			ump_end_ctrl(u_sock,METHOD_ALL_CALL,TRUE);
		}
	}
	if(u_packet_get_flag(p,UP_CTRL_SEQ)==TRUE){
		if(uppri->seq_num==u_sock->their_ctrl_seq){
			u_sock->their_ctrl_seq++;
			if(u_packet_get_flag(p,UP_CTRL_FIN)==TRUE){
				u_sock->fin_reced=TRUE;
				ump_established_close(u_sock);
				ump_change_state(u_sock,UMP_CLOSING);
			}
		}
		u_sock->ack_info.ctrl_ack_set=TRUE;
		u_sock->ack_info.ctrl_ack_data=u_sock->their_ctrl_seq;
	}
	return;
}

void ump_established_handle_data_packet(UMPSocket *u_sock,UMPPacket *p,glong *sleep_ms)
{
	GList *tail=NULL,*next=NULL,*start=NULL;
	int check_if_get_msg=0;
#ifdef RAND_DROP
	int rnd_num=0;
	rnd_num=rand();
	if( rnd_num < (RAND_MAX/500) ){
		u_packet_free(p);
		log_out("drop data on purposes %d\n",rnd_num);
		return;
	}else{
		//log_out("regular data %p\n",p);
	}
#endif
	//处理ack信息
	ump_handle_data_ack(u_sock,p,sleep_ms);
	if(u_packet_get_flag(p,UP_DATA_SEQ)==FALSE){//没有SEQ编号
		u_packet_free(p);
		return;
	}
#ifdef VERBOSE
	log_out("got data seq %u and our rcvbase_seq %u\n",p->seq_num,u_sock->their_data_seq_base);
#endif
	if(ump_cmp_in_rcvbase(u_sock,p->seq_num,u_sock->their_data_seq_base)<0){//不在接收范围之内
		u_packet_free(p);
		return;
	}
	//找到要插入数据的位置
	tail=g_list_last(u_sock->rcv_data_packets);
	while(tail!=NULL && ump_cmp_in_rcvbase(u_sock,p->seq_num,((UMPPacket*)(tail->data))->seq_num)<0){
		next=tail;
		tail=tail->prev;
	}
	if(tail && p->seq_num==((UMPPacket*)(tail->data))->seq_num){//收到重复数据
		u_packet_free(p);
		return;
	}
	//插入数据
	u_sock->rcv_data_packets=g_list_insert_before(u_sock->rcv_data_packets,next,p);
	//准备更新their_data_seq_base和rcv_data_so_far
	if(u_sock->rcv_data_so_far==NULL){
		start=ump_list_first(u_sock->rcv_data_packets);
		if(((UMPPacket*)(start->data))->seq_num!=u_sock->their_data_seq_base){
			//收到不连续数据
			return;
		}
		//收到新数据
		u_sock->rcv_data_so_far=start;
		check_if_get_msg=1;
	}
	while(u_sock->rcv_data_so_far->next!=NULL && (guint16)(((UMPPacket*)(u_sock->rcv_data_so_far->data))->seq_num+1) == (guint16)((UMPPacket*)(u_sock->rcv_data_so_far->next->data))->seq_num){
		//收到新数据
		u_sock->rcv_data_so_far=u_sock->rcv_data_so_far->next;
		check_if_get_msg=1;
	}
	u_sock->their_data_seq_base=((UMPPacket*)(u_sock->rcv_data_so_far->data))->seq_num+1;
	if(check_if_get_msg==1){
		//收到新数据（即rcv_data_so_far改变）后才要检查是否构成了新的消息
		//其他情况下调用ump_check_rcv_data_so_far则会出错
		ump_check_rcv_data_so_far(u_sock);
	}
	return;
}

void ump_established_connect(UMPSocket* u_sock)
{
	return;
}

void ump_established_close(UMPSocket* u_sock)
{
	UMPPacket* p=u_packet_new(P_CONTROL,P_OUTGOING);

	u_packet_set_flag(p,UP_CTRL_FIN);
	u_packet_set_flag(p,UP_CTRL_SEQ);

	p->seq_num=u_sock->our_ctrl_seq;

	if(u_sock->ctrl_packet_to_send!=NULL){
		u_packet_free(u_sock->ctrl_packet_to_send);
		u_sock->ctrl_packet_to_send=NULL;
	}
	u_sock->ctrl_packet_to_send=p;
	u_sock->ctrl_packet_sent=FALSE;
	ump_init_ctrl_rto(u_sock);
	u_sock->our_ctrl_seq++;
	return;
}

void ump_established_send(UMPSocket* u_sock)
{
	return;
}

void ump_established_receive(UMPSocket* u_sock)
{
	return;
}

void ump_established_enter_state(UMPSocket* u_sock)
{
	g_mutex_lock(u_sock->pubic_state_lock);
		g_get_current_time(&u_sock->connect_time);
	g_mutex_unlock(u_sock->pubic_state_lock);
	//如果存在connect调用，表明我们正在执行主动打开，则释放锁
	u_ctrl_sm_funcs[u_sock->ctrl_locked].ctrllock_sm_end_ctrl(u_sock,METHOD_CONNECT,TRUE);
	//若正在执行被动打开，则ctrl_locked值为unlocked，unlocked状态机的sm_end_ctrl操作会去通知accept线程，表明连接已建立
	return;
}

void ump_established_leave_state(UMPSocket* u_sock)
{
	return;
}




/////////////////////////closing状态机相关函数
void ump_closing_handle_ctrl(UMPSocket* u_sock,glong* sleep_ms)
{
	/*if(u_sock->ctrl_para==NULL){
		u_sock->ctrl_locked=FALSE;
		g_mutex_unlock(u_sock->ctrl_para_lock);
		m_event_set(u_sock->ctrl_done);
		return;
	}*/

	//do the work
	if(u_sock->ctrl_para->ctrl_method!=METHOD_CLOSE){
		//要向用户的控制线程报告控制失败
		ump_end_ctrl(u_sock,METHOD_ALL_CALL,FALSE);
	}
	return;
}

void ump_closing_handle_ctrl_packet(UMPSocket* u_sock,UMPPacket* p,glong* sleep_ms)
{
	UMPPacket* uppri=p;
	////如果收到了对flag的ack，那么调用完毕，取消锁，设置事件，释放控制包，设置指针为NULL
	////如果收到了所有对data的ack，那么调用完毕，取消锁，设置事件，释放报文
	//解析收到的ack信息
	if(u_packet_get_flag(p,UP_CTRL_ACK)==TRUE){
		if(u_sock->our_ctrl_seq==uppri->ack_num){
			//首先消解超时
			if(u_sock->ctrl_packet_to_send!=NULL){
				u_packet_free(u_sock->ctrl_packet_to_send);
				u_sock->ctrl_packet_to_send=NULL;
			}
			u_sock->ctrl_packet_sent=TRUE;
			ump_init_ctrl_rto(u_sock);
		}
		u_sock->fin_ack_reced=TRUE;
	}
	if(u_packet_get_flag(p,UP_CTRL_SEQ)==TRUE){
		if(uppri->seq_num==u_sock->their_ctrl_seq){
			u_sock->their_ctrl_seq++;
			if(u_packet_get_flag(p,UP_CTRL_FIN)==TRUE){
				u_sock->fin_reced=TRUE;
			}
		}
		u_sock->ack_info.ctrl_ack_set=TRUE;
		u_sock->ack_info.ctrl_ack_data=u_sock->their_ctrl_seq;
	}
	//如果ack和fin都收到了，就切换状态
	if(u_sock->fin_reced==TRUE && u_sock->fin_ack_reced==TRUE){
		ump_change_state(u_sock,UMP_CLOSED);
	}
	return;
}

void ump_closing_handle_data_packet(UMPSocket *u_sock,UMPPacket *p,glong *sleep_ms){
	//处理ack信息
	ump_handle_data_ack(u_sock,p,sleep_ms);
	u_packet_free(p);
	return;
}

void ump_closing_connect(UMPSocket* u_sock)
{
	return;
}

void ump_closing_close(UMPSocket* u_sock)
{
	return;
}

void ump_closing_send(UMPSocket* u_sock)
{
	return;
}

void ump_closing_receive(UMPSocket* u_sock)
{
	return;
}

void ump_closing_enter_state(UMPSocket* u_sock)
{
	return;
}

void ump_closing_leave_state(UMPSocket* u_sock)
{
	return;
}

void ump_handle_send_reset_packet(UMPSocket* u_sock, glong *sleep_ms)
{
	if(u_sock->send_reset_packet==TRUE){
		ump_send_reset_packet(u_sock);
		u_sock->send_reset_packet=FALSE;
	}
	return;
}
