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
	//��Ҫ����ʱ��set��TRUE���������set��FALSE
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

//ÿ��״̬��Ҫʵ�ֵĺ���
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

void ump_handle_ctrl(UMPSocket* u_sock,glong* sleep_ms);//��sock_thread_func���ã���ȥ����״̬��������
void ump_end_ctrl(UMPSocket* u_sock,UMPCallMethod call_method, gboolean ctrl_result);
void ump_handle_ctrl_packet(UMPSocket* u_sock,glong* sleep_ms);//��sock_thread_func���ã���ȥ����״̬��������
void ump_handle_ctrl_timeout(UMPSocket* u_sock,glong* sleep_ms);
void ump_handle_send_ctrl_packet(UMPSocket* u_sock,glong* sleep_ms);//��sock_thread_func���ã�����׼���õ�ctrl�����Ӵ�ctrl_ack���򵥶�����ctrl_ack

void ump_handle_receive(UMPSocket* u_sock,glong* sleep_ms);
void ump_end_receive(UMPSocket* u_sock,gboolean rec_result);
void ump_end_receive_internal(UMPSocket* u_sock);

void ump_handle_send(UMPSocket* u_sock,glong* sleep_ms);//��sock_thread_func���ã������͵���
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

//////////////////////////ctrl lock״̬��
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

///////////////////////////rec lock״̬��
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

//////////////////////////snd lock״̬��
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

	UMPSockState state;//״̬����Ϊȫ��������state�����״̬

	GMutex* pubic_state_lock;//����public_state��connect_time��close_time
	UMPSockState public_state;//�����������̰߳�ȫ��״̬����state����ͬʱ�仯��ֻ��state�仯�����ⲿ�̷߳���public_stateʱ������Ҫ������״̬�����������ʴ˱�����������
	GTimeVal connect_time;
	GTimeVal close_time;

	gboolean error_occured;
	gboolean send_reset_packet;

	MEvent* do_work_event;

	GMutex* rec_packets_lock;
	GQueue* rec_control_packets;//������Ŀ������ݱ�
	GQueue* rec_data_packets;//����������ݱ�

	RTOComputer* rto_cpt;
	GList* tm_list;

	//�������ݴ������
	guint16 their_wnd;//�Է���ͨ�洰��
	guint16 our_mss;
	guint16 our_data_start_seq;//һ�η�������ʱ���׸�����seq
	guint16 our_data_seq_base;//�Է����յ������ݵľ���λ��+1
	guint16 our_back_point;//����λ��
	//guint16 our_data_pos;//���ڷ��͵����ݾ���λ��
	gint our_data_pos;//���ڷ��͵����ݵ����our_data_start_seq��λ��
	guint16 our_data_sent;//���͹����ݵľ���λ��
	//guint16 our_wnd;//����λ�ã�����ȷ���ķ��ʹ��ڵ�λ��
	gint our_wnd_pos;//���λ�ã�����ȷ���ķ��ʹ������our_data_start_seq��λ��
	guint16 our_cwnd;//ӵ�����ڳߴ�
	guint16 our_ssthresh;//����������
	guint16 our_ctrl_seq;
	guint16 our_msg_last_seq;//��snd��ת����locked��ʱ�򱻳�ʼ��
	GTimeVal last_refresh_cwnd;

	gboolean fast_retran;
	gint ack_rep_count;

	UMPAckInfo ack_info;//���ڱ���Ƿ���ack��Ҫ����

	//�������ݴ������
	guint16 their_ctrl_seq;//����λ�ã��������յ���ctrl_seq+1
	guint16 our_rwnd;//�ҷ���Ҫͨ��Ĵ���
	guint16 our_rwnd_pos_seq;//�ҷ���ͨ�洰����ȷ����seq�ľ���λ��
	guint16 their_data_seq_base;//����λ�ã��������յ���data_seq+1
	guint16 buffer_load;
	//guint16 their_data_seq;//����λ�ã�����

	GList* rcv_first_msg_end;
	GList* rcv_data_so_far;//ָ����յ����������ݵ�ĩβ
	GList* rcv_data_packets;//���յ������ݱ����ڴ������°�����Ŷ�
	gint rcv_packets_msg_num;//data_packets�����������˵���Ϣ������
	GList* local_data_packets;//ÿ�ζ�ȡ����������ݱ�����������ʱ�������е����ݱ�����ŵ�һ���ǹ���������У��ȴ������Ծ����ͷŹ������Դ
	GList* local_ctrl_packets;//ÿ�ζ�ȡ������Ŀ������ݱ�����������ʱ�������е����ݱ�����ŵ�һ���ǹ���������У��ȴ������Ծ����ͷŹ������Դ
	gint local_data_packets_count;

	GMutex* ctrl_para_lock;//����ctrl_para��ctrl_done��ctrl_locked����
	UMPLockState ctrl_locked;//ָʾump_sock_thread_func�߳��Ƿ��ctrl_para_lock������
	MEvent* ctrl_done;
	UMPCtrlParam* ctrl_para;
	//gpointer ctrl_result;
	gboolean ctrl_ok;

	GMutex* snd_para_lock;
	UMPLockState snd_locked;
	MEvent* snd_done;
	UMPPacket** snd_packets;//�����͵���Ϣ�����ֳɺܶ����ݱ����������
	gint snd_packets_count;
	//gpointer snd_result;
	gboolean snd_ok;

	GMutex* rec_para_lock;
	gboolean rec_locked;
	gboolean receive_called;
	MEvent* rec_done;
	GList* rec_msg_packets;//������ϵ���Ϣ�����ֳɺܶ����ݱ����������
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
