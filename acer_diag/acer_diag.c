#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>

#include <termios.h>

#include <sys/select.h>

#include "crc16ccit.h"
#include "diag_packet.h"

#define LOCK_PASSWORD   "acer.dfzse,eizdfXD3#($%)@dxiexAA"
#define UNLOCK_PASSWORD "acer.llxdiafkZidf#$i1234(@01xdiP"

#define DIAG_DEVICE "/dev/ttyUSB0"

// device file descriptor
static int dev_fd;

int diag_write(unsigned char* buf, size_t count) {
    int res = 0;
#ifdef DEBUG
    int i;
    for (i=0; i < count; i++) {
	if (i % 16 == 0)
	    printf("\n");
	printf("%02x ", buf[i]);
    }
    printf("\n");
#endif
    res = write(dev_fd, buf, count);
    return res;
}

int diag_read(unsigned char* buf, size_t count, int timeout) {
    int res = 0;
    fd_set rfds;
    struct timeval tv;

    FD_ZERO(&rfds);
    FD_SET(dev_fd, &rfds);

    tv.tv_sec = timeout;

    res = select(dev_fd + 1, &rfds, NULL, NULL, &tv);
    if (res == -1) {
	perror("select()");
    }
    else {
	res = read(dev_fd, buf, count);
    }

    return res;
}

/*
 * Get AMSS version:
 *  0xd1, 11, 0x26, 0 * 13, CRC, END
 * Get OS version:
 *  0xd1, 0x29, 0x26, 0 * 81, CRC, END
 */

void unlock_os() {
    fprintf(stderr, "> Unlock OS\n");
    diag_write(UNLOCK_PASSWORD, strlen(UNLOCK_PASSWORD));
}

void lock_os() {
    fprintf(stderr, "> Lock OS\n");
    diag_write(LOCK_PASSWORD, strlen(LOCK_PASSWORD));
}

void diag_discard_content() {
    unsigned char response[1024];
    diag_read(response, sizeof(response), 0);
}

int get_os_version(char* buffer, size_t length) {
    unsigned char response[256];
    int result_length = 0;
    memset(response, 0, sizeof(response));
    diag_discard_content();

    fprintf(stderr, "> Get OS Version\n");

    diag_packet *pkt = diag_packet_new();

    diag_packet_add_byte(pkt, 0xd1);
    diag_packet_add_byte(pkt, 0x29);
    diag_packet_add_byte(pkt, 0x26);

    diag_packet_add_padding(pkt, 0, 81);

    diag_packet_add_crc(pkt);
    diag_packet_add_trailer(pkt);

    diag_write(pkt->buffer, pkt->length);

    diag_packet_free(pkt);

    if (diag_read(response, 0x100, 20)) {
	// OS response is NULL-terminated
	result_length = strlen(response+4);
	strncpy(buffer, response + 4, result_length);
	buffer[result_length] = 0;
	return 0;
    };

    return 1;
}

int get_amss_version(char* buffer, size_t length) {
    unsigned char response[256];

    memset(response, 0, sizeof(response));

    diag_discard_content();

    fprintf(stderr, "> Get AMSS Version\n");

    diag_packet *pkt = diag_packet_new();

    diag_packet_add_byte(pkt, 0xd1);
    diag_packet_add_byte(pkt, 0x0b);
    diag_packet_add_byte(pkt, 0x26);

    diag_packet_add_padding(pkt, 0, 13);

    diag_packet_add_crc(pkt);
    diag_packet_add_trailer(pkt);

    diag_write(pkt->buffer, pkt->length);

    diag_packet_free(pkt);

    if (diag_read(response, 0x100, 5)) {
	memcpy(buffer, response+4, 11);
	buffer[11] = 0;
	return 0;
    }

    return 1;
}

int main(int argc, char** argv) {
    char* device_path;
    char  buffer[1024];
    int i = 0;
    int res = EXIT_SUCCESS;

    if (argc > 1)
	device_path = argv[1];
    else
	device_path = DIAG_DEVICE;

    dev_fd = open(device_path, O_RDWR | O_EXCL | O_NONBLOCK | O_NOCTTY);

    if (dev_fd < 0) {
	perror(device_path);
	return EXIT_FAILURE;
    }

    unlock_os();

    if (get_amss_version(buffer, sizeof(buffer))) {
	fprintf(stderr, "%s: Could not get AMSS version", argv[0]);
	res = EXIT_FAILURE;
	goto err_out;
    }

    printf("AMSS Version: %s\n", buffer);

    if (get_os_version(buffer, sizeof(buffer))) {
	fprintf(stderr, "%s: Could not get OS version", argv[0]);
	res = EXIT_FAILURE;
	goto err_out;
    }

    printf("OS Version: %s\n", buffer);

err_out:
    lock_os();

    close(dev_fd);

    return res;
}

