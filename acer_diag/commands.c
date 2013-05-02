#include <stdio.h>
#include <string.h>

#include "diag_packet.h"

#define LOCK_PASSWORD   "acer.dfzse,eizdfXD3#($%)@dxiexAA"
#define UNLOCK_PASSWORD "acer.llxdiafkZidf#$i1234(@01xdiP"

#define RESPONSE_ACK 0x02
#define RESPONSE_NACK 0x03

int cmd_os_unlock() {
    diag_write(UNLOCK_PASSWORD, strlen(UNLOCK_PASSWORD));
    return 0;
}

int cmd_os_lock() {
    diag_write(LOCK_PASSWORD, strlen(LOCK_PASSWORD));
    return 0;
}

int cmd_os_get_os_version() {
    unsigned char response[256];
    int result_length = 0;
    diag_discard_content();

    diag_packet *pkt = diag_packet_new();

    diag_packet_add_byte(pkt, 0xd1);
    diag_packet_add_byte(pkt, 0x29);
    diag_packet_add_byte(pkt, 0x26);

    diag_packet_add_padding(pkt, 0, 81);

    diag_packet_add_trailer(pkt);
    diag_write_packet(pkt);

    diag_packet_free(pkt);

    if (diag_read(response, sizeof(response), 20) > 0) {
	// OS response is NULL-terminated
	printf("%s\n", response + 4);
	return 0;
    };

    return 1;
}

// 0xC from  http://forum.xda-developers.com/showpost.php?p=36371804&postcount=2
int cmd_amss_get_hw_version() {
    unsigned char response[256];
    unsigned char result[21];

    diag_packet *pkt = diag_packet_new();

    diag_packet_add_header(pkt);
    diag_packet_add_byte(pkt, 0xc);
    diag_packet_add_trailer(pkt);

    diag_write_packet(pkt);

    diag_packet_free(pkt);

    if (diag_read(response, sizeof(response), 5) > 0) {
	if (response[1] > 20) {
	    fprintf(stderr, "Response too long\n");
	    return 1;
	}
	memcpy(result, response+2, response[1]);
	result[21] = 0;

	printf("%s\n", result);
	return 0;
    }

    return 1;
}

int cmd_os_test() {
    unsigned char response[256];
    diag_packet *pkt = diag_packet_new();

    // diag_packet_add_header(pkt);
    diag_packet_add_byte(pkt, 0x06);
    diag_packet_add_trailer(pkt);

    diag_write_packet(pkt);

    if (diag_read(response, sizeof(response), 1) > 0) {
	return 0;
    }

    fprintf(stderr, "[I] No response from NOP command. Device may be locked.\n");

    return 1;
}

int cmd_os_get_amss_version() {
    unsigned char response[256];

    diag_packet *pkt = diag_packet_new();

    diag_packet_add_byte(pkt, 0xd1);
    diag_packet_add_byte(pkt, 0x0b);
    diag_packet_add_byte(pkt, 0x26);

    diag_packet_add_padding(pkt, 0, 13);

    diag_packet_add_trailer(pkt);
    diag_write_packet(pkt);

    diag_packet_free(pkt);

    if (diag_read(response, sizeof(response), 5) > 0) {
	printf("%s\n", response+4);
	return 0;
    }

    return 1;
}

int cmd_amss_get_os_version() {
    unsigned char response[256];

    diag_packet *pkt = diag_packet_new();

    diag_packet_add_byte(pkt, 0x51);

    diag_packet_add_trailer(pkt);
    diag_write_packet(pkt);

    usleep(200);

    diag_write_packet(pkt);

    usleep(200);

    if (diag_read(response, sizeof(response), 5) > 0) {
	return 0;
    }

    return 1;
}

int cmd_os_switch_to_amss() {
    fprintf(stderr, "> Switch from OS to AMSS\n");
    diag_packet *pkt = diag_packet_new();

    diag_packet_add_byte(pkt, 0xd1);
    diag_packet_add_byte(pkt, 0x49);
    diag_packet_add_byte(pkt, 0x27);
    diag_packet_add_byte(pkt, 0x00);
    diag_packet_add_byte(pkt, 0x02);

    diag_packet_add_padding(pkt, 0, 11);

    /* for some reason we need this twice according to Acer */
    diag_packet_add_trailer(pkt);

    diag_write_packet(pkt);
    diag_write_packet(pkt);

    diag_packet_free(pkt);

    cmd_os_reset();
}

int cmd_os_reset() {
    fprintf(stderr, "> Reset (OS Mode)\n");

    diag_packet *pkt = diag_packet_new();

    diag_packet_add_byte(pkt, 0xd1);
    diag_packet_add_byte(pkt, 100);

    diag_packet_add_padding(pkt, 0, 14);

    diag_packet_add_trailer(pkt);

    /* we need this twice again */
    diag_write_packet(pkt);
    diag_write_packet(pkt);
    diag_packet_free(pkt);

    diag_discard_content();
}

int cmd_amss_switch_to_recovery() {
    diag_packet *pkt = diag_packet_new();

    diag_packet_add_header(pkt);
    diag_packet_add_byte(pkt, 0x1d);
    diag_packet_add_trailer(pkt);
    diag_write_packet(pkt);

    usleep(100);

    diag_write_packet(pkt);

    usleep(100);

    diag_packet_free(pkt);

    usleep(100);

    diag_discard_content();

    cmd_amss_reset(); 
}

int cmd_amss_reset() {
    fprintf(stderr, "> Reset (AMSS Mode)\n");
    diag_packet *pkt = diag_packet_new();

    diag_packet_add_header(pkt);

    diag_packet_add_byte(pkt, 0xa);

    diag_packet_add_trailer(pkt);
    diag_write_packet(pkt);
    usleep(200);
    diag_write_packet(pkt);

    diag_packet_free(pkt);

    usleep(200);
    diag_discard_content();
}

int cmd_amss_test() {
    unsigned char response[256];

    diag_packet *pkt = diag_packet_new();

    diag_packet_add_header(pkt);
    diag_packet_add_byte(pkt, 31);
    diag_packet_add_byte(pkt, 0);

    diag_packet_add_trailer(pkt);
    diag_write_packet(pkt);

    usleep(200);

    if (diag_read(response, sizeof(response), 5) > 0) {
	return 0;
    }

    return 1;
}


