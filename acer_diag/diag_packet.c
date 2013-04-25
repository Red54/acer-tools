#include <sys/types.h>
#include <stdlib.h>
#include <stdio.h>
#include "crc16ccit.h"
#include "diag_packet.h"

#define DIAG_HDLC_FLAG 0x7e
#define DIAG_HDLC_ESC  0x7d
#define DIAG_HDLC_MASK 0x20

diag_packet* diag_packet_new() {
    diag_packet* obj = (diag_packet*) calloc(1, sizeof(diag_packet));
    return obj;
}

void diag_packet_free(diag_packet* pkt) {
    if (pkt)
	free(pkt);
    pkt = NULL;
}

void diag_packet_add_byte(diag_packet* pkt, char value) {
    if ((DIAG_HDLC_ESC == value) || (DIAG_HDLC_FLAG == value)) {
	pkt->buffer[pkt->length++] = DIAG_HDLC_ESC;
	pkt->buffer[pkt->length++] = value ^ DIAG_HDLC_MASK;
    }
    else
	pkt->buffer[pkt->length++] = value;
}

void diag_packet_add_padding(diag_packet* pkt, char value, size_t count) {
    int i;
    for (i=0; i < count; i++) {
	diag_packet_add_byte(pkt, value);
    }
}

void diag_packet_add_crc(diag_packet* pkt) {
    unsigned short crc;
    int i, crc_start = pkt->_has_flag ? 1 : 0;

    /* FIXME: this API looks weird */
    crc16ccit_ctx crc_ctx;
    crc16ccit_new(&crc_ctx);

    for (i = crc_start; i < pkt->length; i++) {
	if (pkt->buffer[i] == DIAG_HDLC_ESC) {
	    i++;
	    crc16ccit_update(&crc_ctx, pkt->buffer[i] ^ DIAG_HDLC_MASK);
	}
	else
	    crc16ccit_update(&crc_ctx, pkt->buffer[i]);
    }

    crc = crc16ccit_digest(&crc_ctx);

    diag_packet_add_byte(pkt, crc & 0xff);
    diag_packet_add_byte(pkt, (crc >> 8) & 0xff);
}

void diag_packet_add_header(diag_packet* pkt) {
    pkt->_has_flag = 1;
    pkt->buffer[pkt->length++] = DIAG_HDLC_FLAG;
}

void diag_packet_add_trailer(diag_packet* pkt) {
    pkt->buffer[pkt->length++] = DIAG_HDLC_FLAG;
}

#ifdef DIAG_PACKET_STANDALONE_DEBUG
int main(int argc, char** argv) {
    int i;
    diag_packet* pkt = diag_packet_new();

    diag_packet_add_header(pkt);
    diag_packet_add_byte(pkt, 0x7e);
    diag_packet_add_crc(pkt);

    for (i=0; i < pkt->length; i++) {
	printf("%02d: %02x\n", i, pkt->buffer[i]);
    }

    diag_packet_free(pkt);

    return 0;
}
#endif
