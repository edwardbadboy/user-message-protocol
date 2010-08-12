#ifndef USER_MESSAGE_PROTOCOL_PRIVATE_DEF
#define USER_MESSAGE_PROTOCOL_PRIVATE_DEF
#include <glib.h>
#ifdef WIN32
	//#define WIN32_LEAN_AND_MEAN		// 从 Windows 头中排除极少使用的资料
	#include <winsock2.h>
#endif
#include "mevent_public.h"

#define REC_QUEUE_LIMIT 100
#define DEFAULT_MSS 1400
#define MAX_CWND 512

#define UMP_MAX_SLEEP_MS 200
#define UMP_CTRL_TIMEOUT 3000

//可选择的预定义宏：LOG_TIMEOUT、DEBUG_OUT、VERBOSE、VERBOSE1、VERBOSE2、DEBUG_MEMORY_LEAK、RAND_DROP
//其中DEBUG_MEMORY_LEAK要在dll和exe工程中都启用才行

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
	MEvent *stop_work;
}UMPCore;

static void ump_stop_background_threads_and_socket(UMPCore *u_core);
static gboolean ump_free_ump_sock(gpointer key,gpointer value,gpointer user_data);
#endif
