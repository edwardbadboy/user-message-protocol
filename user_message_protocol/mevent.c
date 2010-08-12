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

#include "mevent_public.h"
#include "mevent_private.h"

//static GList* mlist=NULL;
//static guint seq=0;

MEvent* m_event_new(gboolean isset,gboolean auto_reset)
{
	//MEventPri* mepri=NULL;
	MEvent* mepri=NULL;
	if(g_thread_supported()==FALSE){
		return NULL;
	}
#ifdef DEBUG_MEMORY_LEAK
	mepri=(MEvent*)malloc(sizeof(MEvent));
	memset(mepri,0,sizeof(MEvent));
#else
	mepri=(MEvent*)g_malloc0(sizeof(MEvent));
#endif

	mepri->mutex_isset=g_mutex_new();
	if(mepri->mutex_isset==NULL){
#ifdef DEBUG_MEMORY_LEAK
		free(mepri);
#else
		g_free(mepri);
#endif
		return NULL;
	}

	mepri->cond_isset=g_cond_new();
	if(mepri->cond_isset==NULL){
		g_mutex_free(mepri->mutex_isset);
#ifdef DEBUG_MEMORY_LEAK
		free(mepri);
#else
		g_free(mepri);
#endif
		return NULL;
	}

	mepri->isset=isset;
	mepri->auto_reset=auto_reset;
	return mepri;
}

void m_event_set(MEvent* mevent)
{
	MEvent* mepri=mevent;
	g_mutex_lock(mepri->mutex_isset);
	mepri->isset=TRUE;
	if(mepri->auto_reset){
		g_cond_signal(mepri->cond_isset);
	}else{
		g_cond_broadcast(mepri->cond_isset);
	}
	g_mutex_unlock(mepri->mutex_isset);
	return;
}

void m_event_broadcast(MEvent* mevent)
{
	MEvent* mepri=mevent;
	g_mutex_lock(mepri->mutex_isset);
	mepri->isset=TRUE;
	g_cond_broadcast(mepri->cond_isset);
	g_mutex_unlock(mepri->mutex_isset);
	return;
}

void m_event_reset(MEvent* mevent)
{
	MEvent* mepri=mevent;
	g_mutex_lock(mepri->mutex_isset);
	mepri->isset=FALSE;
	g_mutex_unlock(mepri->mutex_isset);
	return;
}

void m_event_wait(MEvent* mevent)
{
	MEvent* mepri=mevent;
	g_mutex_lock(mepri->mutex_isset);
	while(mepri->isset==FALSE){
		g_cond_wait(mepri->cond_isset,mepri->mutex_isset);
	}
	if(mepri->auto_reset){
		mepri->isset=FALSE;
	}
	g_mutex_unlock(mepri->mutex_isset);
	return;
}

void m_event_free(MEvent* mevent)
{
	MEvent* mepri=mevent;
	g_cond_free(mepri->cond_isset);
	g_mutex_free(mepri->mutex_isset);
#ifdef DEBUG_MEMORY_LEAK
	free(mepri);
#else
	g_free(mepri);
#endif
	return;
}

static gboolean _m_event_timed_wait(MEvent* mevent,glong milliseconds)
{
	MEvent* mepri=mevent;
	gboolean r=TRUE;
	GTimeVal time_end;
	g_get_current_time(&time_end);
	g_time_val_add(&time_end,milliseconds*1000);
	g_mutex_lock(mepri->mutex_isset);
	while(mepri->isset==FALSE && r){
		r=g_cond_timed_wait(mepri->cond_isset,mepri->mutex_isset,&time_end);
	}
	if(r && mepri->auto_reset){
		mepri->isset=FALSE;
	}
	g_mutex_unlock(mepri->mutex_isset);
	return r;
}

gboolean m_event_timed_wait(MEvent* mevent,glong milliseconds)
{
	if(milliseconds<0){
		m_event_wait(mevent);
		return TRUE;
	}else{
		return _m_event_timed_wait(mevent,milliseconds);
	}
}

static gint m_time_val_cmp(GTimeVal* t1,GTimeVal* t2)
{
	if(t1->tv_sec > t2->tv_sec){
		return 1;
	}
	
	if(t1->tv_sec < t2->tv_sec){
		return -1;
	}

	if(t1->tv_usec > t2->tv_usec){
		return 1;
	}
	
	if(t1->tv_usec < t2->tv_usec){
		return -1;
	}
	
	return 0;
}
