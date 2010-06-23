#ifndef UMP_PACKET_PRIVATE_DEF
#define UMP_PACKET_PRIVATE_DEF

typedef struct _ump_packet
{
	gboolean changed;
	UMPPacketType p_type;
	UMPPacketDirection direction;
	guchar ctrl_flags1;
	guchar ctrl_flags2;
	guchar data_flags;//��8λ���ã������ʱ��ֻռ8λ
	guint16 check_sum;
	guint16 seq_num;
	guint16 ack_num;
	guint16 mss_num;
	guint16 wnd_num;
	gpointer user_data;//����Ϊoutgoingʱ��ָ���û����ݣ�����ʱ��ָ��raw_data���û�������ʼ��
	gint user_data_l;
	gpointer raw_data;//Ϊ����/����˽ṹ���ԭʼ2��������
	gint raw_data_l;
}UMPPacket;
#endif
