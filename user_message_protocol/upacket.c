#ifdef WIN32
	#ifdef DEBUG_MEMORY_LEAK
		#define _CRTDBG_MAP_ALLOC
		#include <stdlib.h>
		#include <crtdbg.h>
	#endif
#endif

#ifdef WIN32
	#include <winsock2.h>
#endif
#include <string.h>
#include "upacket_public.h"
#include "upacket_private.h"

gpointer u_packet_to_binary(UMPPacket* u_packet,gint* raw_data_l)
{
	guint16* check_sum_p=NULL;
	guint16 computed_sum=0;
	gint data_l=0;
	gpointer raw_data=NULL;
	guchar* d=NULL;
	gint pos=0;
	UMPPacketType p_type;
	UMPPacket* upacketpri=u_packet;

	if(u_packet==NULL){
		*raw_data_l=0;
		return NULL;

	}

	if(upacketpri->changed==FALSE && upacketpri->raw_data!=NULL){
		*raw_data_l=upacketpri->raw_data_l;
		return upacketpri->raw_data;
	}
	p_type=upacketpri->p_type;
	//首先计算需要分配的内存长度，分配内存，然后将数据输出到内存
	if(p_type==P_CONTROL){
		data_l+=4;
		if(u_packet_get_flag(u_packet,UP_CTRL_SEQ)==TRUE){
			data_l+=2;
		}
		if(u_packet_get_flag(u_packet,UP_CTRL_ACK)==TRUE){
			data_l+=2;
		}
		if(u_packet_get_flag(u_packet,UP_CTRL_MSS)==TRUE){
			data_l+=2;
		}
		if(u_packet_get_flag(u_packet,UP_CTRL_WND)==TRUE){
			data_l+=2;
		}
	}else{
		data_l+=3;
		if(u_packet_get_flag(u_packet,UP_DATA_SEQ)==TRUE){
			data_l+=2;
		}
		if(u_packet_get_flag(u_packet,UP_DATA_ACK)==TRUE){
			data_l+=2;
		}
		if(u_packet_get_flag(u_packet,UP_DATA_WND)==TRUE){
			data_l+=2;
		}
	}
	if(upacketpri->user_data!=NULL && upacketpri->user_data_l>0){
		data_l+=upacketpri->user_data_l;
	}

#ifdef DEBUG_MEMORY_LEAK
	raw_data=malloc(data_l);
	memset(raw_data,0,data_l);
#else
	raw_data=g_malloc0(data_l);
#endif
	d=raw_data;
	if(p_type==P_CONTROL){
		*d=upacketpri->ctrl_flags1;
		d++;
		*d=upacketpri->ctrl_flags2;
		d++;
		//为check_sum留出空间
		d+=2;
		if(u_packet_get_flag(u_packet,UP_CTRL_SEQ)==TRUE){
			(*(guint16*)d)=htons(upacketpri->seq_num);
			d+=2;
		}
		if(u_packet_get_flag(u_packet,UP_CTRL_ACK)==TRUE){
			(*(guint16*)d)=htons(upacketpri->ack_num);
			d+=2;
		}
		if(u_packet_get_flag(u_packet,UP_CTRL_MSS)==TRUE){
			(*(guint16*)d)=htons(upacketpri->mss_num);
			d+=2;
		}
		if(u_packet_get_flag(u_packet,UP_CTRL_WND)==TRUE){
			(*(guint16*)d)=htons(upacketpri->wnd_num);
			d+=2;
		}
	}else{
		*d=upacketpri->data_flags;
		d++;
		//为check_sum留出空间
		d+=2;
		if(u_packet_get_flag(u_packet,UP_DATA_SEQ)==TRUE){
			(*(guint16*)d)=htons(upacketpri->seq_num);
			d+=2;
		}
		if(u_packet_get_flag(u_packet,UP_DATA_ACK)==TRUE){
			(*(guint16*)d)=htons(upacketpri->ack_num);
			d+=2;
		}
		if(u_packet_get_flag(u_packet,UP_DATA_WND)==TRUE){
			(*(guint16*)d)=htons(upacketpri->wnd_num);
			d+=2;
		}
	}
	if(upacketpri->user_data!=NULL && upacketpri->user_data_l>0){
		memcpy(d,upacketpri->user_data,upacketpri->user_data_l);
	}

	//开始计算check_sum
	d=raw_data;
	if(p_type==P_CONTROL){
		computed_sum=computed_sum+(guint16)(*d);
		d++;pos++;
		computed_sum=computed_sum+(guint16)(*d);
	}else{
		computed_sum=computed_sum+(guint16)(*d);
	}
	d++;pos++;
	check_sum_p=(guint16*)d;
	//跳过check_sum的地方
	d+=2;pos+=2;

	while(pos<data_l-1)
	{
		computed_sum=computed_sum+(guint16)(*d);
		computed_sum=computed_sum+(guint16)((*(d+1)) << 8);
		pos+=2;
		d+=2;
	}
	if(pos==data_l-1){
		computed_sum=computed_sum+(guint16)(*d);
	}
	upacketpri->check_sum=computed_sum;
	*check_sum_p=htons(computed_sum);
	*raw_data_l=data_l;
	if(upacketpri->raw_data!=NULL){
#ifdef DEBUG_MEMORY_LEAK
		free(upacketpri->raw_data);
#else
		g_free(upacketpri->raw_data);
#endif
	}
	upacketpri->raw_data=raw_data;
	upacketpri->raw_data_l=data_l;
	upacketpri->changed=FALSE;
	return raw_data;
}

UMPPacket* u_packet_from_binary(gpointer raw_data,gint raw_data_l)
{
	UMPPacketType p_type;
	guchar *d=NULL;
	UMPPacket* u_p=NULL;
	guint16 computed_sum=0;
	guint16 data_check_sum=0;
	gint pos=0;

	if(raw_data_l<1){
		return NULL;
	}

	d=(guchar*)raw_data;
	if(((*d)&UP_VER)!=UMP_VER){
		return NULL;
	}

	//取flags，计算校验和，与传输的校验和不同的就丢弃

	if(((*d) & UP_TYPE)==P_CONTROL){
		if(raw_data_l<4){
			return NULL;
		}
		u_p=u_packet_new(P_CONTROL,P_INCOMMING);
		p_type=P_CONTROL;
		u_p->ctrl_flags1=(*d);
		computed_sum=computed_sum+(guint16)(*d);
		d++;pos++;
		u_p->ctrl_flags2=(*d);
		computed_sum=computed_sum+(guint16)(*d);
	}else{
		if(raw_data_l<3){
			return NULL;
		}
		u_p=u_packet_new(P_DATA,P_INCOMMING);
		p_type=P_DATA;
		u_p->data_flags=(*d);
		computed_sum=computed_sum+(guint16)(*d);
	}
	d++;pos++;
	u_p->raw_data=raw_data;
	u_p->raw_data_l=raw_data_l;

	data_check_sum=ntohs((*(guint16*)d));
	d+=2;pos+=2;

	while(pos<raw_data_l-1)
	{
		computed_sum=computed_sum+(guint16)(*d);
		computed_sum=computed_sum+(guint16)((*(d+1)) << 8);
		pos+=2;
		d+=2;
	}
	if(pos==raw_data_l-1){
		computed_sum=computed_sum+(guint16)(*d);
	}

	if(computed_sum!=data_check_sum){
		//会把raw_data也释放
		u_packet_free(u_p);
		return NULL;
	}
	u_p->check_sum=computed_sum;
	//校验完毕

	//取出SEQ、ACK、MSS等和用户数据
	d=(guchar*)raw_data;
	pos=0;

	//这样保证d不会越界，这样可以识别畸形数据报
	if(p_type==P_CONTROL){
		d+=4;pos+=4;
		if(u_packet_get_flag(u_p,UP_CTRL_SEQ)==TRUE){
			if(pos>=raw_data_l){
				u_packet_free(u_p);
				return NULL;
			}
			u_p->seq_num=ntohs((*(guint16*)d));
			d+=2;pos+=2;
		}
		if(u_packet_get_flag(u_p,UP_CTRL_ACK)==TRUE){
			if(pos>=raw_data_l){
				u_packet_free(u_p);
				return NULL;
			}
			u_p->ack_num=ntohs((*(guint16*)d));
			d+=2;pos+=2;
		}
		if(u_packet_get_flag(u_p,UP_CTRL_MSS)==TRUE){
			if(pos>=raw_data_l){
				u_packet_free(u_p);
				return NULL;
			}
			u_p->mss_num=ntohs((*(guint16*)d));
			d+=2;pos+=2;
		}
		if(u_packet_get_flag(u_p,UP_CTRL_WND)==TRUE){
			if(pos>=raw_data_l){
				u_packet_free(u_p);
				return NULL;
			}
			u_p->wnd_num=ntohs((*(guint16*)d));
			d+=2;pos+=2;
		}
	}else{
		d+=3;pos+=3;
		if(u_packet_get_flag(u_p,UP_DATA_SEQ)==TRUE){
			if(pos>=raw_data_l){
				u_packet_free(u_p);
				return NULL;
			}
			u_p->seq_num=ntohs((*(guint16*)d));
			d+=2;pos+=2;
		}
		if(u_packet_get_flag(u_p,UP_DATA_ACK)==TRUE){
			if(pos>=raw_data_l){
				u_packet_free(u_p);
				return NULL;
			}
			u_p->ack_num=ntohs((*(guint16*)d));
			d+=2;pos+=2;
		}
		if(u_packet_get_flag(u_p,UP_DATA_WND)==TRUE){
			if(pos>=raw_data_l){
				u_packet_free(u_p);
				return NULL;
			}
			u_p->wnd_num=ntohs((*(guint16*)d));
			d+=2;pos+=2;
		}
	}

	if(raw_data_l-pos>0){
		u_p->user_data=(gpointer)d;
		u_p->user_data_l=raw_data_l-pos;
	}

	return u_p;
}

UMPPacket* u_packet_new(UMPPacketType p_type,UMPPacketDirection p_direction)
{
	UMPPacket* u_p=NULL;

#ifdef DEBUG_MEMORY_LEAK
	u_p=malloc(sizeof(UMPPacket));
	memset(u_p,0,sizeof(UMPPacket));
#else
	u_p=g_malloc0(sizeof(UMPPacket));
#endif
	u_p->p_type=p_type;
	u_p->direction=p_direction;	
	if(p_type==P_CONTROL){
		u_p->ctrl_flags1 |= UMP_VER;
		//不置位第6位，则为control packet
	}
	else{
		u_p->data_flags |= UMP_VER;
		u_p->data_flags |= UP_TYPE;//置位第6位，则为data packet
	}
	return u_p;
}

void u_packet_free(UMPPacket* u_packet)
{
	UMPPacket* upacketpri=u_packet;
	//if(upacketpri->direction==P_OUTGOING){
	//	//g_free(upacketpri->user_data);
	//}
	//if(upacketpri->direction==P_INCOMMING){
	//	g_free(upacketpri->raw_data);
	//}
	if(upacketpri->raw_data!=NULL){
#ifdef DEBUG_MEMORY_LEAK
		free(upacketpri->raw_data);
#else
		g_free(upacketpri->raw_data);
#endif
	}
#ifdef DEBUG_MEMORY_LEAK
	free(upacketpri);
#else
	g_free(upacketpri);
#endif
}

gboolean u_packet_get_flag(UMPPacket* u_packet,guint8 flag_mask)
{
	UMPPacket* upacketpri=u_packet;

	if(upacketpri->p_type==P_DATA){
		return (upacketpri->data_flags & flag_mask)>0;
	}else if(upacketpri->p_type==P_CONTROL){
		if(flag_mask<=UP_CTRL_SYN && flag_mask!=UP_CTRL_EXT){
			return (upacketpri->ctrl_flags1 & flag_mask)>0;
		}else{
			return (upacketpri->ctrl_flags2 & flag_mask)>0;
		}
	}
	return FALSE;
}

void u_packet_set_flag(UMPPacket* u_packet,guint8 flag_mask)
{
	UMPPacket* upacketpri=u_packet;

	if(upacketpri->p_type==P_DATA){
		upacketpri->data_flags |= flag_mask;
	}else if(upacketpri->p_type==P_CONTROL){
		if(flag_mask<=UP_CTRL_SYN && flag_mask!=UP_CTRL_EXT){
			upacketpri->ctrl_flags1 |= flag_mask;
		}else{
			upacketpri->ctrl_flags2 |= flag_mask;
		}
	}
	upacketpri->changed=TRUE;
	return;
}

void u_packet_clear_flag(UMPPacket* u_packet,guint8 flag_mask)
{
	UMPPacket* upacketpri=u_packet;

	if(upacketpri->p_type==P_DATA){
		upacketpri->data_flags &= flag_mask^255;
	}else if(upacketpri->p_type==P_CONTROL){
		if(flag_mask<=UP_CTRL_SYN && flag_mask!=UP_CTRL_EXT){
			upacketpri->ctrl_flags1 &= flag_mask^255;
		}else{
			upacketpri->ctrl_flags2 &= flag_mask^255;
		}
	}
	upacketpri->changed=TRUE;
	return;
}

void u_packet_set_data(UMPPacket* u_packet,gpointer user_data,gint data_len)
{
	UMPPacket* upacketpri=u_packet;

	if(upacketpri->direction==P_OUTGOING){
		upacketpri->user_data=user_data;
		upacketpri->user_data_l=data_len;
		upacketpri->changed=TRUE;
	}
}

//UMPPacketType u_packet_get_type(UMPPacket* u_packet)
//{
//	return u_packet->p_type;
//}
//
//void u_packet_set_acknum(UMPPacket* u_packet,guint16 acknum)
//{
//	u_packet->ack_num=acknum;
//	return;
//}
//
//guint16 u_packet_get_acknum(UMPPacket* u_packet)
//{
//	return u_packet->ack_num;
//}
//
//guint16 u_packet_get_seqnum(UMPPacket* u_packet)
//{
//	return u_packet->seq_num;
//}
//
//guint16 u_packet_get_mssnum(UMPPacket* u_packet)
//{
//	return u_packet->mss_num;
//}
//
//guint16 u_packet_get_wndnum(UMPPacket* u_packet)
//{
//	return u_packet->wnd_num;
//}
