#ifdef WIN32
	#ifdef DEBUG_MEMORY_LEAK
		#define _CRTDBG_MAP_ALLOC
		#include <stdlib.h>
		#include <crtdbg.h>
	#endif
#endif

#include <glib.h>
#include "ump_private.h"
#include "ump_misc.h"
#include "timer_list_public.h"
#include "timer_list_private.h"
#include "debug_out.h"

void tm_register_packet(GList** tl,guint16 seq,glong rto)
{
	TmEntry* te;
#ifdef DEBUG_MEMORY_LEAK
	te=malloc(sizeof(TmEntry));
#else
	te=g_malloc(sizeof(TmEntry));
#endif
	te->seq=seq;
	g_get_current_time(&te->fire_time);
	te->reg_time=te->fire_time;
	g_time_val_add(&te->fire_time,1000*rto);
	*tl=g_list_append(*tl,te);
}

//返回值<0，ack失败，其余情况，返回rtt
glong tm_ack_packet(GList** tl,guint16 seq,guint16 upper_bound_seq)
{
	GList* head;
	guint16 seq1,seq2,entry_seq;
	glong rtt=-1;
	GTimeVal cur_t;
	if(*tl==NULL){
		return rtt;
	}
	seq1=(((TmEntry *)ump_list_first(*tl)->data)->seq)+1;
	seq2=upper_bound_seq+1;
	//seq2=(((TmEntry *)g_list_last(*tl)->data)->seq)+1;
	if(seq1>seq2){
		if(seq<seq1&&seq>seq2){
			return rtt;
		}
	}else{
		if(seq<seq1||seq>seq2){
			return rtt;
		}
	}
	head=ump_list_first(*tl);
	while(head!=NULL){
		entry_seq=((TmEntry *)head->data)->seq;
		if((seq)==(guint16)(entry_seq+1)){
			g_get_current_time(&cur_t);
			rtt=ump_time_sub(&cur_t,&((TmEntry*)head->data)->reg_time);
#ifdef DEBUG_MEMORY_LEAK
			free(head->data);
#else
			g_free(head->data);
#endif
			head->data=NULL;
			*tl=g_list_delete_link(*tl,head);
			/*head=ump_list_first(*tl);
			while(head != NULL){
				entry_seq=((TmEntry*)(head->data))->seq;
				if(seq != (guint16)(entry_seq+1)){
					break;
				}
#ifdef DEBUG_MEMORY_LEAK
				free(head->data);
#else
				g_free(head->data);
#endif
				head->data=NULL;
				*tl=g_list_delete_link(*tl,head);
				head=ump_list_first(*tl);
			}*/
			break;
		}else{
#ifdef DEBUG_MEMORY_LEAK
			free(head->data);
#else
			g_free(head->data);
#endif
			head->data=NULL;
			*tl=g_list_delete_link(*tl,head);
		}
		head=ump_list_first(*tl);
	}
	return rtt;
}

//返回值>=0
glong tm_get_next_event(GList** tl)
{
	TmEntry* t_entry;
	GTimeVal cur_t;
	if(*tl==NULL){
		return UMP_MAX_SLEEP_MS;
	}
	t_entry=ump_list_first(*tl)->data;
	g_get_current_time(&cur_t);
	return MAX(ump_time_sub(&t_entry->fire_time,&cur_t),0);
}

//返回FALSE，无超时，返回TRUE，有超时并设置timeout_seq
gboolean get_next_timeout(GList** tl,guint16* timeout_seq)
{
	GTimeVal cur_t;
	GList* head;
	if(*tl==NULL){
#ifdef LOG_TIMEOUT
		//log_out("no timeout\n");
#endif
		return FALSE;
	}
	g_get_current_time(&cur_t);
	if(ump_time_sub(&((TmEntry*)ump_list_first(*tl)->data)->fire_time,&cur_t)>0){
#ifdef LOG_TIMEOUT
		//log_out("no timeout\n");
#endif
		return FALSE;
	}
	*timeout_seq=((TmEntry*)ump_list_first(*tl)->data)->seq;
#ifdef LOG_TIMEOUT
	log_out("timeout occured %u\n",*timeout_seq);
#endif
	head=ump_list_first(*tl);
	while(head!=NULL){
#ifdef DEBUG_MEMORY_LEAK
		free(head->data);
#else
		g_free(head->data);
#endif
		head=head->next;
	}
	g_list_free(*tl);
	*tl=NULL;
	return TRUE;
}

void tm_clear_list(GList** tl)
{
	GList* head;
	head=ump_list_first(*tl);
	while(head!=NULL){
#ifdef DEBUG_MEMORY_LEAK
		free(head->data);
#else
		g_free(head->data);
#endif
		head=head->next;
	}
	g_list_free(*tl);
	*tl=NULL;
	return;
}
