#ifndef UMP_PACKET_PUBLIC_DEF
#define UMP_PACKET_PUBLIC_DEF

#include <glib.h>

#define UMP_VER 64
#define UP_VER 192
#define UP_TYPE 32
#define UP_CTRL_SYN 8
#define UP_CTRL_FIN 4
#define UP_CTRL_RST 2
#define UP_CTRL_HRT 1

#define UP_CTRL_SEQ 128
#define UP_CTRL_ACK 64
#define UP_CTRL_MSS 32
#define UP_CTRL_WND 16
#define UP_CTRL_EXT 7

#define UP_DATA_REQWND 16
#define UP_DATA_BDR 8
#define UP_DATA_SEQ 4
#define UP_DATA_ACK 2
#define UP_DATA_WND 1

typedef enum _ump_packet_type
{
	P_CONTROL=0,
	P_DATA=1,
}UMPPacketType;

typedef enum _ump_packet_direction
{
	P_INCOMMING=0,
	P_OUTGOING=1,
}UMPPacketDirection;

struct _ump_packet;
typedef struct _ump_packet UMPPacket;

gpointer u_packet_to_binary(UMPPacket* u_packet,gint* raw_data_l);//ִ�й��Ժ󣬷����·�����ڴ棬�ṹ���ڵ�raw_data��ָ������raw_data����raw_data�������ݣ������ͷ�ԭ��������
UMPPacket* u_packet_from_binary(gpointer raw_data,gint raw_data_l);//ִ�й��Ժ󣬷����·����P_INCOMMING��UMPPacket���ṹ���ڵ�raw_data��ָ�������raw_data
gboolean u_packet_get_flag(UMPPacket* u_packet,guint8 flag_mask);
void u_packet_set_flag(UMPPacket* u_packet,guint8 flag_mask);
void u_packet_clear_flag(UMPPacket* u_packet,guint8 flag_mask);
void u_packet_set_data(UMPPacket* u_packet,gpointer user_data,gint data_len);
UMPPacket* u_packet_new(UMPPacketType p_type,UMPPacketDirection p_direction);
void u_packet_free(UMPPacket* u_packet);//���Զ��ͷ����Լ���raw_data
//UMPPacketType u_packet_get_type(UMPPacket* u_packet);
//void u_packet_set_acknum(UMPPacket* u_packet,guint16 acknum);
//guint16 u_packet_get_acknum(UMPPacket* u_packet);
//guint16 u_packet_get_seqnum(UMPPacket* u_packet);
//guint16 u_packet_get_mssnum(UMPPacket* u_packet);
//guint16 u_packet_get_wndnum(UMPPacket* u_packet);
#endif
