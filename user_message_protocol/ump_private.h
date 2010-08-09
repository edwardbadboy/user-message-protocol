#ifndef USER_MESSAGE_PROTOCOL_PRIVATE_DEF
#define USER_MESSAGE_PROTOCOL_PRIVATE_DEF
#include <glib.h>
#ifdef WIN32
	//#define WIN32_LEAN_AND_MEAN		// �� Windows ͷ���ų�����ʹ�õ�����
	#include <winsock2.h>
#endif
#include "mevent_public.h"

#define REC_QUEUE_LIMIT 100
#define DEFAULT_MSS 1400
#define MAX_CWND 512

#define UMP_MAX_SLEEP_MS 200
#define UMP_CTRL_TIMEOUT 3000

//��ѡ���Ԥ����꣺LOG_TIMEOUT��DEBUG_OUT��VERBOSE��VERBOSE1��VERBOSE2��DEBUG_MEMORY_LEAK��RAND_DROP
//����DEBUG_MEMORY_LEAKҪ��dll��exe�����ж����ò���

//typedef struct _ump_connect_param
//{
//	struct sockaddr_in their_addr;
//}ump_connect_param;

typedef enum _ump_call_method
{
	METHOD_CONNECT,
	METHOD_CLOSE,
	METHOD_SEND,
	METHOD_RECEIVE,
	METHOD_NO_CALL,
	METHOD_ALL_CALL,
}UMPCallMethod;

typedef struct _ump_ctrl_param
{
	UMPCallMethod ctrl_method;
	gpointer ctrl_param_struct;
}UMPCtrlParam;

typedef struct _ump_core
{
	GMutex* s_lock;
	SOCKET s;
	struct sockaddr_in our_addr;
	GThread* rec_thread;
	GThread* cleaner_thread;
	GMutex* umps_lock;
	GHashTable* umps;
	//GMutex* backlog_umps_lock;
	guint backlog_limit;
	GHashTable* backlog_umps;
	GHashTable* act_connect;
	GHashTable* closed_umps;
	MEvent* accept_ok;
}UMPCore;

#endif