#ifndef RTO_COMPUTER_PUBLIC_DEF
#define RTO_COMPUTER_PUBLIC_DEF
#include <glib.h>

struct _rto_cmpt;
typedef struct _rto_cmpt RTOComputer;

RTOComputer* rto_computer_new(void);
void rto_refresh_rtt(RTOComputer* cpt,glong rtt);
void rto_timeout_occur(RTOComputer* cpt);
glong rto_get_rto(RTOComputer* cpt);
void rto_computer_free(RTOComputer* cpt);
glong rto_get_estimated_rtt(RTOComputer* cpt);
#endif
