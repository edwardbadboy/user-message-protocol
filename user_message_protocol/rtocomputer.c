#ifdef WIN32
	#ifdef DEBUG_MEMORY_LEAK
		#define _CRTDBG_MAP_ALLOC
		#include <stdlib.h>
		#include <crtdbg.h>
	#endif
#endif

#include <glib.h>
#include "rtocomputer_public.h"
#include "rtocomputer_private.h"
#include "debug_out.h"

RTOComputer* rto_computer_new(void)
{
	RTOComputer* cptpri=NULL;
#ifdef DEBUG_MEMORY_LEAK
	cptpri=malloc(sizeof(RTOComputer));
#else
	cptpri=g_malloc(sizeof(RTOComputer));
#endif
	cptpri->g=0.125;
	cptpri->h=0.25;
	cptpri->a=0;
	cptpri->d=3000;
	cptpri->rto=cptpri->a+2*cptpri->d;
	return cptpri;
}

void rto_refresh_rtt(RTOComputer* cpt,glong rtt)
{
	RTOComputer* cptpri=cpt;
	cptpri->m=rtt;
	cptpri->err=cptpri->m-cptpri->a;
	cptpri->a+=cptpri->g*cptpri->err;
	cptpri->d+=cptpri->h*(ABS(cptpri->err)-cptpri->d);
	cptpri->rto=MAX(cptpri->a+4*cptpri->d,16);
#ifdef RAND_DROP
	//log_out("rtt %ld, estimated rtt %lf\n",rtt,cpt->a);
#endif
}

void rto_timeout_occur(RTOComputer* cpt)
{
	RTOComputer* cptpri=cpt;
	cptpri->rto=MIN(cptpri->rto*2,90000);
}

glong rto_get_rto(RTOComputer* cpt)
{
#ifdef RAND_DROP
	//log_out("rto %lf\n",cpt->rto);
#endif
	return (glong)cpt->rto;
}

void rto_computer_free(RTOComputer* cpt)
{
#ifdef DEBUG_MEMORY_LEAK
	free(cpt);
#else
	g_free(cpt);
#endif
}

glong rto_get_estimated_rtt(RTOComputer* cpt){
	return (glong)(cpt->a);
}
