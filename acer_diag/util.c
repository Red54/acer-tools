#include <stdio.h>
#include <sys/select.h>
#include <sys/types.h>
#include <string.h>
#include <stdlib.h>
#include "util.h"

extern int debug;
extern int dev_fd;

struct {
    int mode;
    char description[64];
} acer_device_modes[] = {
    { ACER_MODE_OS, "OS" },
    { ACER_MODE_AMSS, "AMSS" },
    { ACER_MODE_USB_TETHERING, "USB Tethering" },
    { ACER_MODE_DEBUG, "ADB" },
};

static void dump_packet(unsigned char* buf, size_t count, char dir) {
    int i;
    printf("\n");
    for (i=0; i < count; i++) {

	if (i % 16 == 0 && i != 0)
	    printf("\n");

	if (i == 0 || i % 16 == 0) {
	    printf("%c ", dir);
	}

	printf("%02x ", buf[i]);
    }
    printf("\n");
}

void diag_discard_content() {
    unsigned char response[1024];
    diag_read(response, sizeof(response), 0);
    usleep(600);
}

int diag_write(unsigned char* buf, size_t count) {
    int res = 0;
    if (debug) {
	dump_packet(buf, count, '<');
    }
    res = write(dev_fd, buf, count);
    return res;
}

int diag_write_packet(diag_packet* pkt) {
    diag_discard_content();

    return diag_write(pkt->buffer, pkt->length);
}

/*
 * -1 - Error
 * 0, > 0 - Result
 */
int diag_read(unsigned char* buf, size_t count, int timeout) {
    int res = -1;
    fd_set rfds;
    struct timeval tv;

    FD_ZERO(&rfds);
    FD_SET(dev_fd, &rfds);

    tv.tv_sec = timeout;
    tv.tv_usec = 0;

    memset(buf, 0, count);

    res = select(dev_fd + 1, &rfds, NULL, NULL, &tv);
    if (res == -1) {
	perror("select()");
    }
    else {
	res = read(dev_fd, buf, count);
	if (debug) {
	    dump_packet(buf, count, '>');
	}
    }

    return res;
}

/* The caller should free the result */
char* get_device_mode_string(int mode) {
    int i, got_main_mode_flag = 0;
    char *result = malloc(512);
    for (i = 0; i < ARRAY_SIZE(acer_device_modes); i++) {
	if (mode & acer_device_modes[i].mode) {
	    if (got_main_mode_flag == 0) {
		sprintf(result, "%s", acer_device_modes[i].description);
		got_main_mode_flag = 1;
	    }
	    else {
		strcat(result, " +");
		strcat(result, acer_device_modes[i].description);
	    }
	}
    }

    return result;
}

