// test_ump.cpp : 定义控制台应用程序的入口点。
//

#ifdef DEBUG_MEMORY_LEAK
	#define _CRTDBG_MAP_ALLOC
	#include <stdlib.h>
	#include <crtdbg.h>
#endif

#include <stdio.h>

#ifdef WIN32
#define WIN32_LEAN_AND_MEAN		// 从 Windows 头中排除极少使用的资料
	#include <winsock2.h>
#endif

#include <glib.h>
#include "test_ump.h"
#include "../user_message_protocol/ump_public.h"
void send_data(UMPSocket *u_sock);
void receive_data(UMPSocket *u_sock);
void send_file(UMPSocket *u_sock);
void receive_file(UMPSocket *u_sock);

//typedef void (*test_func1)(void);
//
//void print_test(void)
//{
//	printf("aaa\n");
//}
//
//void test_pointer_to_func()
//{
//	gpointer p=NULL;
//	test_func1 fu;
//	p=(gpointer)print_test;
//	fu=(test_func1)p;
//	fu();
//}

gpointer my_malloc(gsize n_bytes)
{
	return malloc(n_bytes);
}

gpointer my_realloc(gpointer mem,gsize n_bytes)
{
	return realloc(mem,n_bytes);
}

void my_free(gpointer mem)
{
	free(mem);
}

GMemVTable memvt={my_malloc,my_realloc,my_free,NULL,NULL,NULL};

int main(int argc, char* argv[])
{
	//guint8 x=255;
	//test_pointer_to_func();
	//ump_print(3,4);
	//x=x +2;
	//printf("%u\n",x);
	//printf("short %d, int %d, long %d\n",sizeof(short),sizeof(int),sizeof(long));
	//ump_test_packet_funcs();
	gboolean u_listen=FALSE;
	gint our_port=0,their_port=0;
	char* our_address=NULL,*their_address=NULL;
	GKeyFile *etc;
	UMPCore* u_core;
	UMPSocket* u_sock;
	struct sockaddr_in our,their;

	//g_mem_set_vtable(&memvt);
	g_mem_set_vtable(glib_mem_profiler_table);

	#ifdef DEBUG_MEMORY_LEAK
		_CrtSetDbgFlag ( _CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF );
		_CrtSetReportMode(_CRT_WARN, _CRTDBG_MODE_FILE | _CRTDBG_MODE_DEBUG);
		_CrtSetReportFile(_CRT_WARN, _CRTDBG_FILE_STDOUT);
		_CrtSetReportMode(_CRT_ERROR, _CRTDBG_MODE_FILE | _CRTDBG_MODE_DEBUG);
		_CrtSetReportFile(_CRT_ERROR, _CRTDBG_FILE_STDOUT);
		//_CrtSetReportMode( _CRT_ERROR, _CRTDBG_MODE_DEBUG );
	#endif

	etc=g_key_file_new();
	if(g_key_file_load_from_file(etc,"test_ump.conf",G_KEY_FILE_NONE,NULL)==FALSE){
		g_key_file_free(etc);
		g_print("error open test_ump.conf\n");
		getchar();
		return 0;
	}
	our_port=g_key_file_get_integer(etc,"general","ourport",NULL);
	if(our_port==0){
		g_key_file_free(etc);
		g_print("error reading ourport\n");
		getchar();
	}
	our_address=g_key_file_get_string(etc,"general","ouraddress",NULL);
	if(our_address==NULL){
		g_key_file_free(etc);
		g_print("error reading ouraddress\n");
		getchar();
	}
	their_port=g_key_file_get_integer(etc,"general","theirport",NULL);
	if(their_port==0){
		g_free(our_address);
		g_key_file_free(etc);
		g_print("error reading theirport\n");
		getchar();
	}
	their_address=g_key_file_get_string(etc,"general","theiraddress",NULL);
	if(their_address==NULL){
		g_free(our_address);
		g_key_file_free(etc);
		g_print("error reading theiraddress\n");
		getchar();
	}
	u_listen=g_key_file_get_boolean(etc,"general","listen",NULL);
	g_key_file_free(etc);

	our.sin_family = AF_INET;
	our.sin_addr.s_addr=inet_addr(our_address);
	our.sin_port=htons(our_port);
	their.sin_family = AF_INET;
	their.sin_addr.s_addr=inet_addr(their_address);
	their.sin_port=htons(their_port);

	ump_init();
	ump_set_log_stream(stderr);
	u_core=ump_core_bind(&our,10);
	if(u_core==NULL){
		g_free(our_address);
		g_free(their_address);
		g_print("bind error\n");
		getchar();
		return 0;
	}
	g_print("bind at %s:%d ok\n",our_address,our_port);
	if(u_listen==TRUE){
		g_print("start accept\n");
		u_sock=ump_accept(u_core);
		if(u_sock!=NULL){
			g_print("accept ok\n");
			//send_data(u_sock);
			send_file(u_sock);
		}else{
			g_print("accept failed\n");
		}
	}else{
		g_print("start connect\n");
		u_sock=ump_connect(u_core,&their);
		if(u_sock!=NULL){
			g_print("connect ok\n");
			//receive_data(u_sock);
			receive_file(u_sock);
		}else{
			g_print("connect failed\n");
		}
	}
	//getchar();
	//g_usleep(3000000);
	g_print("start close\n");
	if(u_sock!=NULL){
		ump_close(u_sock);
	}
	g_print("close ok\n");
	#ifdef DEBUG_MEMORY_LEAK
		//_CrtDumpMemoryLeaks();
	#endif
	g_mem_profile();
	getchar();
	return 0;
}

void send_data(UMPSocket *u_sock)
{
	int r=0,i=0;
	int l=20480;
	char *data=NULL;

	data=malloc(l);
	if(data==NULL){
		printf("not enough memory\n");
		return;
	}

	for(i=0;i<l;i++){
		data[i]=i % 128;
	}

	for(i=0;i<10;i++){
		r=ump_send_message(u_sock,data,l);
		printf("%dth send result %d\n",i,r);
	}
	return;
}

void receive_data(UMPSocket *u_sock)
{
	int r=0,i=0,j=0,get_ok=1;
	char* h=NULL;
	int hl=0;
	g_usleep(1000000);
	for(i=0;i<10;i++){
		r=ump_receive_message(u_sock,&h,&hl);
		printf("%dth receive result %d length %d\n",i,r,hl);
		for(j=0;j<hl;j++){
			if( (j % 128)!=h[j] ){
				get_ok=0;
				printf("bad data at %d\n",j);
				break;
			}
		}
		ump_free_message(h);
		h=NULL;
	}
	if(get_ok==0){
		printf("received bad data\n");
	}else{
		printf("all received data is good\n");
	}
	return;
}

void send_file(UMPSocket *u_sock)
{
	FILE* f=NULL;
	char* buffer=NULL;
	int l=4096;
	int got=0;
	int r=0;
	buffer=malloc(l);

	f=fopen("plant vs zombie.rar","rb");
	if(f==NULL){
		perror("open file");
		free(buffer);
		return;
	}
	got=fread(buffer,1,l,f);
	while(got>0){
		r=ump_send_message(u_sock,buffer,got);
		if(r<0){
			printf("send error\n");
			break;
		}
		got=fread(buffer,1,l,f);
	}
	if(r==0 && !feof(f)){
		perror("read file error");
	}
	fclose(f);
	free(buffer);
	printf("send over\n");

	return;
}

void receive_file(UMPSocket *u_sock)
{
	FILE* f=NULL;
	char *buffer=NULL;
	int got=0;
	int written=0;
	int r=0;

	f=fopen("plant vs zombie1.rar","wb");
	if(f==NULL){
		perror("open file");
		return;
	}
	r=ump_receive_message(u_sock,&buffer,&got);
	while(r==0){
		written=fwrite(buffer,1,got,f);
		ump_free_message(buffer);
		if(written<got){
			perror("write file error");
			break;
		}
		r=ump_receive_message(u_sock,&buffer,&got);
	}
	fclose(f);
	printf("receive over\n");

	return;
}

