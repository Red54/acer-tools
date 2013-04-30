#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/select.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <dirent.h>

#include "crc16ccit.h"
#include "diag_packet.h"

// #define DEBUG

#define LOCK_PASSWORD   "acer.dfzse,eizdfXD3#($%)@dxiexAA"
#define UNLOCK_PASSWORD "acer.llxdiafkZidf#$i1234(@01xdiP"

enum {
    ACER_MODE_OS,
    ACER_MODE_OS_DEBUG,
    ACER_MODE_FASTBOOT,
    ACER_MODE_AMSS,
    ACER_MODE_UNKNOWN,
};

struct usb_device_interface {
    int idVendor;
    int idProduct;
    int bInterfaceNumber;
    int mode;
};

struct usb_device_interface known_usb_devices[] = {
    { 0x0502, 0x3202, 0x0, ACER_MODE_OS_DEBUG }, // Acer Liquid
    { 0x0502, 0x3203, 0x0, ACER_MODE_OS },	 // Acer Liquid
};

// global device file descriptor
static int dev_fd;

void dump_packet(unsigned char* buf, size_t count, char dir) {
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

int diag_write(unsigned char* buf, size_t count) {
    int res = 0;
#ifdef DEBUG
    dump_packet(buf, count, '<');
#endif
    res = write(dev_fd, buf, count);
    return res;
}

int diag_write_packet(diag_packet* pkt) {
    return diag_write(pkt->buffer, pkt->length);
}

int diag_read(unsigned char* buf, size_t count, int timeout) {
    int res = 0;
    fd_set rfds;
    struct timeval tv;

    FD_ZERO(&rfds);
    FD_SET(dev_fd, &rfds);

    tv.tv_sec = timeout;
    tv.tv_usec = 0;

    res = select(dev_fd + 1, &rfds, NULL, NULL, &tv);
    if (res == -1) {
	perror("select()");
    }
    else {
	res = read(dev_fd, buf, count);
#ifdef DEBUG
	dump_packet(buf, count, '>');
#endif
    }

    return res;
}

/*
 * Get AMSS version:
 *  0xd1, 11, 0x26, 0 * 13, CRC, END
 * Get OS version:
 *  0xd1, 0x29, 0x26, 0 * 81, CRC, END
 */

void cmd_os_unlock() {
    fprintf(stderr, "> Unlock OS\n");
    diag_write(UNLOCK_PASSWORD, strlen(UNLOCK_PASSWORD));
}

void cmd_os_lock() {
    fprintf(stderr, "> Lock OS\n");
    diag_write(LOCK_PASSWORD, strlen(LOCK_PASSWORD));
}

void diag_discard_content() {
    unsigned char response[1024];
    diag_read(response, sizeof(response), 0);
    usleep(600);
}

int cmd_os_get_os_version(char* buffer, size_t length) {
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

    diag_packet_add_trailer(pkt);

    diag_write_packet(pkt);

    diag_packet_free(pkt);

    if (diag_read(response, sizeof(response), 20)) {
	// OS response is NULL-terminated
	result_length = strlen(response + 4);
	if (result_length > length)
	    return 1;

	strncpy(buffer, response + 4, result_length);
	buffer[result_length] = 0;
	return 0;
    };

    return 1;
}

int cmd_os_get_amss_version(char* buffer, size_t length) {
    unsigned char response[256];
    memset(response, 0, sizeof(response));

    diag_discard_content();

    fprintf(stderr, "> Get AMSS Version\n");

    diag_packet *pkt = diag_packet_new();

    diag_packet_add_byte(pkt, 0xd1);
    diag_packet_add_byte(pkt, 0x0b);
    diag_packet_add_byte(pkt, 0x26);

    diag_packet_add_padding(pkt, 0, 13);

    diag_packet_add_trailer(pkt);

    diag_write_packet(pkt);

    diag_packet_free(pkt);

    if (diag_read(response, sizeof(response), 5)) {
	memcpy(buffer, response+4, 11);
	buffer[11] = 0;
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

    diag_packet_add_trailer(pkt);

    /* for some reason we need this twice according to Acer */
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
}

int cmd_amss_reset() {
    fprintf(stderr, "> Reset (AMSS Mode)\n");
    diag_packet *pkt = diag_packet_new();

    diag_packet_add_header(pkt);
    diag_packet_add_byte(pkt, 11);
    diag_packet_add_trailer(pkt);

    diag_write_packet(pkt);

    diag_packet_free(pkt);
}

int read_hex_int_from_file(char* path) {
    int res = -1;
    FILE *fh = fopen(path, "rb");
    if (! fh) {
	perror("fopen()");
	return -1;
    }

    fscanf(fh, "%x", &res);
    fclose(fh);

    return res;
}

int get_diag_tty_name(char* name, struct usb_device_interface* query) {
    /*
     * We could query libusb for 1,2, but for now a simple magic would do.
     * http://stackoverflow.com/questions/4192974/
     *
     * 1. Get all devices out of /sys/bus/usb/devices
     * 2. Foreach device look at idVendor, idProduct.
     * 3. Inside the dir we will find blah.bInterfaceNumber
     * 4. Locate the directory entry starting from ttyUSB
     * 5. That's our serial port name
     */

    int res = -1;
    int node_found = 0;
    int temp_id;
    int idVendor, idProduct, bInterfaceNumber;

    idVendor = query->idVendor;
    idProduct = query->idProduct;
    bInterfaceNumber = query->bInterfaceNumber;

    struct dirent* entry;
    char buf[256];
    char* usb_device_dir = "/sys/bus/usb/devices";

    DIR* usb_device_dir_handle = opendir(usb_device_dir);
    if (!usb_device_dir_handle) {
	perror("opendir()");
	goto out;
    }

    while ((entry = readdir(usb_device_dir_handle)) != NULL) {
	/* this routine has to be smarter */

	if (entry->d_name[0] == '.')
	    continue;

	// idVendor
	snprintf(buf, sizeof(buf), "%s/%s/idVendor",
		usb_device_dir, entry->d_name);

	if (access(buf, R_OK) != 0)
	    continue;

	temp_id = read_hex_int_from_file(buf);
	if (temp_id == -1) {
	    fprintf(stderr, "Error reading from %s\n", buf);
	    break;
	}

	if (temp_id != idVendor)
	    continue;

	// idProduct
	snprintf(buf, sizeof(buf), "%s/%s/idProduct",
		usb_device_dir, entry->d_name);
	temp_id = read_hex_int_from_file(buf);
	if (temp_id == -1) {
	    fprintf(stderr, "Error reading from %s\n", buf);
	    break;
	}

	if (temp_id != idProduct)
	    continue;

	node_found = 1;
	break;
    }

    /* Our last entry holds the parent interface information */

    /* 
     * 1-1.1:1.0 - bInterfaceNumber is the last digit, but  I am not sure
     * what X in 1-1.1:X.0 means and I am too lazy to look-up now.
     */

    if (!node_found) {
	goto free_handle;
    }

    // /sys/bus/usb/devices / blah
    snprintf(buf, sizeof(buf), "%s/%s:1.%d", usb_device_dir, entry->d_name,
					     bInterfaceNumber);
    closedir(usb_device_dir_handle);

    usb_device_dir_handle = opendir(buf);
    if (! usb_device_dir_handle) {
	perror("opendir(usb_device_dir_handle)");
	goto out;
    }

    node_found = 0;
    while ((entry = readdir(usb_device_dir_handle)) != NULL) {
	if (!strncmp(entry->d_name, "ttyUSB", 6)) {
	    node_found = 1;
	    break;
	}
    }

    if (node_found) {
	strcpy(name, entry->d_name);
	res = 0;
    }

  free_handle:
    closedir(usb_device_dir_handle);

  out:
    return res;
}

int main(int argc, char** argv) {
    char device_name[256];
    char buffer[1024];
    int i = 0, node_found = 0;
    int res = EXIT_SUCCESS;
    int device_mode = ACER_MODE_UNKNOWN;

    memset(device_name, 0, sizeof(device_name));

    for (i=0; i < sizeof(known_usb_devices)/sizeof(struct usb_device_interface); i++) {
	printf("Searching for %04x:%04x (interface %02x)\n",
		known_usb_devices[i].idVendor,
		known_usb_devices[i].idProduct,
		known_usb_devices[i].bInterfaceNumber);

	if (!get_diag_tty_name(device_name, &known_usb_devices[i])) {
	    node_found = 1;
	    device_mode = known_usb_devices[i].mode;
	    break;
	}
    }

    if (!node_found) {
	fprintf(stderr,
		"%s: No diag tty found\n"
		"Please make sure qcserial module is loaded and it knows about your device\n"
		, argv[0]);
	return EXIT_FAILURE;
    }

    snprintf(buffer, sizeof(buffer), "/dev/%s", device_name);

    printf("Using device %s (%d)\n", buffer, device_mode);
    dev_fd = open(buffer, O_RDWR | O_EXCL | O_NONBLOCK | O_NOCTTY);

    if (dev_fd < 0) {
	perror(buffer);
	return EXIT_FAILURE;
    }

    cmd_os_unlock();

    if (cmd_os_get_amss_version(buffer, sizeof(buffer))) {
	fprintf(stderr, "%s: Could not get AMSS version", argv[0]);
	res = EXIT_FAILURE;
	goto err_out;
    }

    printf("AMSS Version: %s\n", buffer);

    if (cmd_os_get_os_version(buffer, sizeof(buffer))) {
	fprintf(stderr, "%s: Could not get OS version", argv[0]);
	res = EXIT_FAILURE;
	goto err_out;
    }

    printf("OS Version: %s\n", buffer);

    // cmd_os_reset();

err_out:
    cmd_os_lock();

    close(dev_fd);

    return res;
}

