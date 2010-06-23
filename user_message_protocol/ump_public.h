#ifndef USER_MESSAGE_PROTOCOL_PUBLIC_DEF
#define USER_MESSAGE_PROTOCOL_PUBLIC_DEF

#include <stdio.h>

#ifdef WIN32
	//#define WIN32_LEAN_AND_MEAN		// 从 Windows 头中排除极少使用的资料
	#ifdef OUTPORT
		#define UMP_DLLDES __declspec(dllexport)  
	#else
		#define UMP_DLLDES __declspec(dllimport) 
	#endif
#else
	#define UMP_DLLDES	
#endif

struct _ump_core;
typedef struct _ump_core UMPCore;
struct _ump_socket;
typedef struct _ump_socket UMPSocket;

UMP_DLLDES void ump_print(int a,int b);

#ifdef WIN32
UMP_DLLDES int ump_init();
#endif

UMP_DLLDES UMPCore* ump_core_bind(struct sockaddr_in* our_addr,int backlog);
UMP_DLLDES void ump_core_close(UMPCore* u_core);

UMP_DLLDES UMPSocket* ump_connect(UMPCore* u_core,struct sockaddr_in *their_addr);
UMP_DLLDES UMPSocket* ump_accept(UMPCore* u_core);
UMP_DLLDES void ump_close(UMPSocket* u_sock);

UMP_DLLDES int ump_send_message(UMPSocket* u_connection,void *data,int data_len);
UMP_DLLDES int ump_receive_message(UMPSocket* u_connection,void **data,int *data_len);
UMP_DLLDES int ump_free_message(void * data);

UMP_DLLDES void ump_set_log_stream(FILE* f);
//UMP_DLLDES gboolean ump_test_packet_funcs(void);

#endif
