#ifndef _ACER_DIAG_DIAG_PACKET_H
#define _ACER_DIAG_DIAG_PACKET_H

#include <sys/types.h>

struct _diag_packet {
    unsigned char buffer[2048];
    size_t length;
};

typedef struct _diag_packet diag_packet;

diag_packet* diag_packet_new();
void diag_packet_free(diag_packet *pkt);
void diag_packet_add_byte(diag_packet* pkt, char value);
void diag_packet_add_padding(diag_packet* pkt, char value, size_t count);
void diag_packet_add_crc(diag_packet* pkt);
void diag_packet_add_header(diag_packet* pkt);
void diag_packet_add_trailer(diag_packet* pkt);

#endif // _ACER_DIAG_DIAG_PACKET_H
