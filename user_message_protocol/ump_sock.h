#ifndef UMP_SOCK_DEF
#define UMP_SOCK_DEF
#ifdef WIN32
	#include <winsock2.h>
#endif
#include <glib.h>
#include "upacket_public.h"
#include "rtocomputer_public.h"
#include "mevent_public.h"

#define LOCAL_DATA_PACKET_LIMIT 1024
#define ACK_DELAY 100
#define REQ_WND_DELAY 500
#define RTT_MIN_MS 50
#define SEQ_START 0

struct _ump_socket;
typedef struct _ump_socket UMPSocket;

typedef enum _ump_sock_state
{
	UMP_CLOSED=0,
	UMP_CONNECTING=1,
	UMP_ESTABLISHED=2,
	UMP_CLOSING=3,
}UMPSockState;

typedef struct _ump_ack_info
{	
	//需要发送时将set置TRUE，发送完后将set置FALSE
	gboolean ctrl_ack_set;
	gboolean data_ack_set;
	guint16 ctrl_ack_data;
	guint16 data_ack_seq;
	guint16 data_ack_wnd;
	GTimeVal last_ack_t;
	gboolean push_ack;
	gboolean schedule_ack;
}UMPAckInfo;

typedef void (*ump_sm_handle_ctrl)(UMPSocket* u_sock,glong* sleep_ms);
typedef void (*ump_sm_handle_ctrl_packet)(UMPSocket* u_sock,UMPPacket* p,glong* sleep_ms);
typedef void (*ump_sm_handle_data_packet)(UMPSocket *u_sock,UMPPacket *p,glong *sleep_ms);
typedef void (*ump_sm_connect)(UMPSocket* u_sock);
typedef void (*ump_sm_close)(UMPSocket* u_sock);
typedef void (*ump_sm_send)(UMPSocket* u_sock);
typedef void (*ump_sm_receive)(UMPSocket* u_sock);
typedef void (*ump_sm_enter_state)(UMPSocket* u_sock);
typedef void (*ump_sm_leave_state)(UMPSocket* u_sock);

//每个状态机要实现的函数
typedef struct _ump_sm_funcs
{
	ump_sm_handle_ctrl sm_handle_ctrl;
	ump_sm_handle_ctrl_packet sm_handle_ctrl_packet;
	ump_sm_handle_data_packet sm_handle_data_packet;
	ump_sm_connect sm_connect;
	ump_sm_close sm_close;
	ump_sm_send sm_send;
	ump_sm_receive sm_receive;
	ump_sm_enter_state sm_enter_state;
	ump_sm_leave_state sm_leave_state;
}UMPSmFuncs;

gpointer ump_sock_thread_func(gpointer data);
guint ump_sock_fetch_received_packets(UMPSocket* u_s,glong* sleep_ms);
UMPSocket* ump_sock_new(UMPCore* u_core,struct sockaddr_in *their_addr);
void ump_sock_free(UMPSocket* u_sock);
gboolean ump_sock_connect(UMPSocket* u_sock);//,struct sockaddr_in their_addr);
gboolean ump_sock_close(UMPSocket* u_sock);
gboolean ump_sock_send(UMPSocket* u_sock,UMPPacket** data_packets,gint packets_count);
gchar* ump_sock_receive(UMPSocket* u_sock,gint *rec_len);

gboolean ump_sock_control_call(UMPSocket* u_sock,UMPCtrlParam* call_param);

void ump_handle_ctrl(UMPSocket* u_sock,glong* sleep_ms);//由sock_thread_func调用，再去调用状态机处理函数
void ump_end_ctrl(UMPSocket* u_sock,UMPCallMethod call_method, gboolean ctrl_result);
void ump_handle_ctrl_packet(UMPSocket* u_sock,glong* sleep_ms);//由sock_thread_func调用，再去调用状态机处理函数
void ump_handle_ctrl_timeout(UMPSocket* u_sock,glong* sleep_ms);
void ump_handle_send_ctrl_packet(UMPSocket* u_sock,glong* sleep_ms);//由sock_thread_func调用，发出准备好的ctrl包并捎带ctrl_ack，或单独发送ctrl_ack

void ump_handle_receive(UMPSocket* u_sock,glong* sleep_ms);
void ump_end_receive(UMPSocket* u_sock,gboolean rec_result);
void ump_end_receive_internal(UMPSocket* u_sock);

void ump_handle_send(UMPSocket* u_sock,glong* sleep_ms);//由sock_thread_func调用，处理发送调用
void ump_end_send(UMPSocket* u_sock,gboolean snd_result);
void ump_handle_send_data(UMPSocket* u_sock,glong* sleep_ms);
void ump_handle_data_packet(UMPSocket* u_sock,glong* sleep_ms);
void ump_handle_data_timeout(UMPSocket* u_sock,glong* sleep_ms);
void ump_handle_wnd_notify(UMPSocket *u_sock,UMPPacket* u_p,glong *sleep_ms);
void ump_handle_data_ack(UMPSocket *u_sock,UMPPacket* u_p,glong *sleep_ms);
void ump_harvest_messages(UMPSocket* u_sock);
void ump_check_rcv_data_so_far(UMPSocket* u_sock);
void ump_act_req_wnd(UMPSocket* u_sock,glong *sleep_ms);
void ump_handle_send_reset_packet(UMPSocket* u_sock, glong *sleep_ms);

void ump_timeout_refresh_ctrl_rto(UMPSocket* u_sock);
void ump_init_ctrl_rto(UMPSocket* u_sock);

void ump_refresh_data_sent(UMPSocket* u_sock,guint16 sent_seq);
void ump_refresh_back_point(UMPSocket* u_sock,guint16 back_point);
void ump_refresh_snd_wnd_pos(UMPSocket* u_sock);
void ump_refresh_our_rwnd_pos(UMPSocket* u_sock);
void ump_refresh_our_ack_info(UMPSocket* u_sock,glong* sleep_ms);

void ump_set_our_ack_info(UMPSocket* u_sock,gboolean push_ack);
void ump_set_packet_ack_info(UMPSocket* u_sock,UMPPacket* u_p);
void ump_clear_packet_ack_info(UMPSocket* u_sock,UMPPacket* u_p);

void ump_change_state(UMPSocket* u_sock,UMPSockState new_state);

void ump_closed_handle_ctrl(UMPSocket* u_sock,glong* sleep_ms);
void ump_closed_handle_ctrl_packet(UMPSocket* u_sock,UMPPacket* p,glong* sleep_ms);
void ump_closed_handle_data_packet(UMPSocket *u_sock,UMPPacket *p,glong *sleep_ms);
void ump_closed_connect(UMPSocket* u_sock);
void ump_closed_close(UMPSocket* u_sock);
void ump_closed_send(UMPSocket* u_sock);
void ump_closed_receive(UMPSocket* u_sock);
void ump_closed_enter_state(UMPSocket* u_sock);
void ump_closed_leave_state(UMPSocket* u_sock);

void ump_connecting_handle_ctrl(UMPSocket* u_sock,glong* sleep_ms);
void ump_connecting_handle_ctrl_packet(UMPSocket* u_sock,UMPPacket* p,glong* sleep_ms);
void ump_connecting_handle_data_packet(UMPSocket *u_sock,UMPPacket *p,glong *sleep_ms);
void ump_connecting_connect(UMPSocket* u_sock);
void ump_connecting_close(UMPSocket* u_sock);
void ump_connecting_send(UMPSocket* u_sock);
void ump_connecting_receive(UMPSocket* u_sock);
void ump_connecting_enter_state(UMPSocket* u_sock);
void ump_connecting_leave_state(UMPSocket* u_sock);

void ump_established_handle_ctrl(UMPSocket* u_sock,glong* sleep_ms);
void ump_established_handle_ctrl_packet(UMPSocket* u_sock,UMPPacket* p,glong* sleep_ms);
void ump_established_handle_data_packet(UMPSocket *u_sock,UMPPacket *p,glong *sleep_ms);
void ump_established_connect(UMPSocket* u_sock);
void ump_established_close(UMPSocket* u_sock);
void ump_established_send(UMPSocket* u_sock);
void ump_established_receive(UMPSocket* u_sock);
void ump_established_enter_state(UMPSocket* u_sock);
void ump_established_leave_state(UMPSocket* u_sock);

void ump_closing_handle_ctrl(UMPSocket* u_sock,glong* sleep_ms);
void ump_closing_handle_ctrl_packet(UMPSocket* u_sock,UMPPacket* p,glong* sleep_ms);
void ump_closing_handle_data_packet(UMPSocket *u_sock,UMPPacket *p,glong *sleep_ms);
void ump_closing_connect(UMPSocket* u_sock);
void ump_closing_close(UMPSocket* u_sock);
void ump_closing_send(UMPSocket* u_sock);
void ump_closing_receive(UMPSocket* u_sock);
void ump_closing_enter_state(UMPSocket* u_sock);
void ump_closing_leave_state(UMPSocket* u_sock);


typedef enum _ump_lock_state
{
	UMP_UNLOCKED=0,
	UMP_LOCKED=1,
}UMPLockState;

//////////////////////////ctrl lock状态机
typedef void (*ump_ctrllock_sm_handle_ctrl)(UMPSocket* u_sock,glong* sleep_ms);
typedef void (*ump_ctrllock_sm_enter_state)(UMPSocket* u_sock);
typedef void (*ump_ctrllock_sm_leave_state)(UMPSocket* u_sock);
typedef void (*ump_ctrllock_sm_end_ctrl)(UMPSocket* u_sock,UMPCallMethod call_method, gboolean ctrl_result);

typedef struct _ump_ctrllock_sm_funcs
{
	ump_ctrllock_sm_handle_ctrl ctrllock_sm_handle_ctrl;
	ump_ctrllock_sm_enter_state ctrllock_sm_enter_state;
	ump_ctrllock_sm_leave_state ctrllock_sm_leave_state;
	ump_ctrllock_sm_end_ctrl ctrllock_sm_end_ctrl;
}UMPCtrlLockStateFuncs;

void ump_change_ctrllock_state(UMPSocket* u_sock,UMPLockState s);

void ump_ctrlunlocked_handle_ctrl(UMPSocket* u_sock,glong* sleep_ms);
void ump_ctrlunlocked_enter_state(UMPSocket* u_sock);
void ump_ctrlunlocked_leave_state(UMPSocket* u_sock);
void ump_ctrlunlocked_end_ctrl(UMPSocket* u_sock,UMPCallMethod call_method, gboolean ctrl_result);

void ump_ctrlocked_handle_ctrl(UMPSocket* u_sock,glong* sleep_ms);
void ump_ctrllocked_enter_state(UMPSocket* u_sock);
void ump_ctrllocked_leave_state(UMPSocket* u_sock);
void ump_ctrllocked_end_ctrl(UMPSocket* u_sock,UMPCallMethod call_method, gboolean ctrl_result);

///////////////////////////rec lock状态机
void ump_change_reclock_state(UMPSocket* u_sock,UMPLockState s);

typedef void (*ump_reclock_handle_rec)(UMPSocket* u_sock,glong* sleep_ms);
typedef void (*ump_reclock_enter_state)(UMPSocket* u_sock);
typedef void (*ump_reclock_leave_state)(UMPSocket* u_sock);
typedef void (*ump_reclock_end_rec)(UMPSocket* u_sock,gboolean rec_result);

typedef struct _ump_reclock_sm_funcs
{
	ump_reclock_handle_rec reclock_sm_handle_rec;
	ump_reclock_enter_state reclock_sm_enter_state;
	ump_reclock_leave_state reclock_sm_leave_state;
	ump_reclock_end_rec reclock_sm_end_rec;
}UMPRecLockStateFuncs;

void ump_recunlocked_handle_rec(UMPSocket* u_sock,glong* sleep_ms);
void ump_recunlocked_enter_state(UMPSocket* u_sock);
void ump_recunlocked_leave_state(UMPSocket* u_sock);
void ump_recunlocked_end_rec(UMPSocket* u_sock,gboolean rec_result);

void ump_reclocked_handle_rec(UMPSocket* u_sock,glong* sleep_ms);
void ump_reclocked_enter_state(UMPSocket* u_sock);
void ump_reclocked_leave_state(UMPSocket* u_sock);
void ump_reclocked_end_rec(UMPSocket* u_sock,gboolean rec_result);

//////////////////////////snd lock状态机
void ump_change_sndlock_state(UMPSocket* u_sock,UMPLockState s);

typedef void (*ump_sndlock_sm_handle_snd)(UMPSocket* u_sock,glong* sleep_ms);
typedef void (*ump_sndlock_sm_enter_state)(UMPSocket* u_sock);
typedef void (*ump_sndlock_sm_leave_state)(UMPSocket* u_sock);
typedef void (*ump_sndlock_sm_end_snd)(UMPSocket* u_sock,gboolean snd_result);
typedef void (*ump_sndlock_sm_handle_send_data)(UMPSocket* u_sock,glong* sleep_ms);

typedef struct _ump_sndlock_sm_funcs
{
	ump_sndlock_sm_handle_snd sndlock_sm_handle_snd;
	ump_sndlock_sm_enter_state sndlock_sm_enter_state;
	ump_sndlock_sm_leave_state sndlock_sm_leave_state;
	ump_sndlock_sm_end_snd sndlock_sm_end_snd;
	ump_sndlock_sm_handle_send_data sndlock_sm_handle_send_data;
}UMPSndLockStateFuncs;

void ump_sndunlocked_handle_snd(UMPSocket* u_sock,glong* sleep_ms);
void ump_sndunlocked_enter_state(UMPSocket* u_sock);
void ump_sndunlocked_leave_state(UMPSocket* u_sock);
void ump_sndunlocked_end_snd(UMPSocket* u_sock,gboolean snd_result);
void ump_sndunlocked_handle_send_data(UMPSocket* u_sock,glong* sleep_ms);

void ump_sndlocked_handle_snd(UMPSocket* u_sock,glong* sleep_ms);
void ump_sndlocked_enter_state(UMPSocket* u_sock);
void ump_sndlocked_leave_state(UMPSocket* u_sock);
void ump_sndlocked_end_snd(UMPSocket* u_sock,gboolean snd_result);
void ump_sndlocked_handle_send_data(UMPSocket* u_sock,glong* sleep_ms);

typedef struct _ump_socket
{
	UMPCore* u_core;
	struct sockaddr_in their_addr;
	GThread* sock_thread;
	GMutex* thread_stop_work_lock;
	gboolean thread_stop_work;

	UMPSockState state;//状态机行为全部依赖于state定义的状态

	GMutex* pubic_state_lock;//保护public_state、connect_time、close_time
	UMPSockState public_state;//用锁保护的线程安全的状态，与state变量同时变化。只有state变化或者外部线程访问public_state时，才需要上锁。状态机函数不访问此变量不用上锁
	GTimeVal connect_time;
	GTimeVal close_time;

	gboolean error_occured;
	gboolean send_reset_packet;

	MEvent* do_work_event;

	GMutex* rec_packets_lock;
	GQueue* rec_control_packets;//待处理的控制数据报
	GQueue* rec_data_packets;//待处理的数据报

	RTOComputer* rto_cpt;
	GList* tm_list;

	//发送数据处理相关
	guint16 their_wnd;//对方的通告窗口
	guint16 our_mss;
	guint16 our_data_start_seq;//一次发送数据时，首个包的seq
	guint16 our_data_seq_base;//对方已收到的数据的绝对位置+1
	guint16 our_back_point;//绝对位置
	//guint16 our_data_pos;//正在发送的数据绝对位置
	gint our_data_pos;//正在发送的数据的相对our_data_start_seq的位置
	guint16 our_data_sent;//发送过数据的绝对位置
	//guint16 our_wnd;//绝对位置，最终确定的发送窗口的位置
	gint our_wnd_pos;//相对位置，最终确定的发送窗口相对our_data_start_seq的位置
	guint16 our_cwnd;//拥塞窗口尺寸
	guint16 our_ssthresh;//慢启动门限
	guint16 our_ctrl_seq;
	guint16 our_msg_last_seq;//在snd被转换到locked的时候被初始化
	GTimeVal last_refresh_cwnd;

	gboolean fast_retran;
	gint ack_rep_count;

	UMPAckInfo ack_info;//用于标记是否有ack需要发送

	//接收数据处理相关
	guint16 their_ctrl_seq;//绝对位置，等于已收到的ctrl_seq+1
	guint16 our_rwnd;//我方将要通告的窗口
	guint16 our_rwnd_pos_seq;//我方已通告窗口所确定的seq的绝对位置
	guint16 their_data_seq_base;//绝对位置，等于已收到的data_seq+1
	guint16 buffer_load;
	//guint16 their_data_seq;//绝对位置，待用

	GList* rcv_first_msg_end;
	GList* rcv_data_so_far;//指向接收到的连续数据的末尾
	GList* rcv_data_packets;//接收到的数据报在内存中重新按序号排队
	gint rcv_packets_msg_num;//data_packets中完整接收了的消息的数量
	GList* local_data_packets;//每次读取待处理的数据报，都读出当时滞留所有的数据报，存放到一个非共享的链表中，等待处理，以尽早释放共享的资源
	GList* local_ctrl_packets;//每次读取待处理的控制数据报，都读出当时滞留所有的数据报，存放到一个非共享的链表中，等待处理，以尽早释放共享的资源
	gint local_data_packets_count;

	GMutex* ctrl_para_lock;//保护ctrl_para、ctrl_done、ctrl_locked的锁
	UMPLockState ctrl_locked;//指示ump_sock_thread_func线程是否对ctrl_para_lock上了锁
	MEvent* ctrl_done;
	UMPCtrlParam* ctrl_para;
	//gpointer ctrl_result;
	gboolean ctrl_ok;

	GMutex* snd_para_lock;
	UMPLockState snd_locked;
	MEvent* snd_done;
	UMPPacket** snd_packets;//待发送的消息（被分成很多数据报放在链表里）
	gint snd_packets_count;
	//gpointer snd_result;
	gboolean snd_ok;

	GMutex* rec_para_lock;
	gboolean rec_locked;
	gboolean receive_called;
	MEvent* rec_done;
	GList* rec_msg_packets;//接收完毕的消息（被分成很多数据报放在链表里）
	gchar* rec_msg;
	gint rec_msg_l;
	//gpointer rec_result;
	gboolean rec_ok;

	glong ctrl_rto;
	GTimeVal ctrl_resend_time;
	UMPPacket* ctrl_packet_to_send;
	gboolean ctrl_packet_sent;

	gboolean syn_ack_reced;
	gboolean syn_reced;
	gboolean fin_ack_reced;
	gboolean fin_reced;

	gboolean act_req_wnd;
	GTimeVal last_req_wnd_time;
	UMPPacket *up_req_wnd;
}UMPSocket;
#endif
