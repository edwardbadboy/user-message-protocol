#ifndef UMP_PACKET_PRIVATE_DEF
#define UMP_PACKET_PRIVATE_DEF

typedef struct _ump_packet
{
	gboolean changed;
	UMPPacketType p_type;
	UMPPacketDirection direction;
	guchar ctrl_flags1;
	guchar ctrl_flags2;
	guchar data_flags;//低8位不用，输出的时候只占8位
	guint16 check_sum;
	guint16 seq_num;
	guint16 ack_num;
	guint16 mss_num;
	guint16 wnd_num;
	gpointer user_data;//方向为outgoing时，指向用户数据；其他时，指向raw_data的用户数据起始处
	gint user_data_l;
	gpointer raw_data;//为输入/输出此结构体的原始2进制数据
	gint raw_data_l;
}UMPPacket;
#endif
