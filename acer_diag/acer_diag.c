#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/select.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <dirent.h>

#include "diag_packet.h"
#include "acer_diag.h"
#include "commands.h"

struct {
    int mode;
    char description[64];
} acer_device_modes[] = {
    { ACER_MODE_OS, "OS" },
    { ACER_MODE_AMSS, "AMSS" },
    { ACER_MODE_USB_TETHERING, "USB Tethering" },
    { ACER_MODE_DEBUG, "ADB" },
};

struct usb_device_interface {
    int idVendor;
    int idProduct;
    int bInterfaceNumber;
    int mode;
};

struct usb_device_interface known_usb_devices[] = {
    /* Acer Liquid A1 - MI can be found in qcsera.inf */
    { 0x0502, 0x3202, 0x0, ACER_MODE_OS | ACER_MODE_DEBUG },
    { 0x0502, 0x3203, 0x0, ACER_MODE_OS },
    { 0x0502, 0x3222, 0x2, ACER_MODE_OS | ACER_MODE_USB_TETHERING | ACER_MODE_DEBUG },
    { 0x0502, 0x3223, 0x2, ACER_MODE_OS | ACER_MODE_USB_TETHERING },

    // AMSS
    { 0x0502, 0x9002, 0x0, ACER_MODE_AMSS },
};

// global device file descriptor
static int dev_fd;

// global debug state
static int debug = 0;

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

int diag_read(unsigned char* buf, size_t count, int timeout) {
    int res = 0;
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

/*
 * Get AMSS version:
 *  0xd1, 11, 0x26, 0 * 13, CRC, END
 * Get OS version:
 *  0xd1, 0x29, 0x26, 0 * 81, CRC, END
 */

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
    int node_found_flag = 0;
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

	node_found_flag = 1;
	break;
    }

    /* Our last entry holds the parent interface information */

    /* 
     * 1-1.1:1.0 - bInterfaceNumber is the last digit, but  I am not sure
     * what X in 1-1.1:X.0 means and I am too lazy to look-up now.
     */

    if (!node_found_flag) {
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

    node_found_flag = 0;
    while ((entry = readdir(usb_device_dir_handle)) != NULL) {
	if (!strncmp(entry->d_name, "ttyUSB", 6)) {
	    node_found_flag = 1;
	    break;
	}
    }

    if (node_found_flag) {
	strcpy(name, entry->d_name);
	res = 0;
    }

  free_handle:
    closedir(usb_device_dir_handle);

  out:
    return res;
}

void usage(char *app) {
    int i = 0;
    int mode;
    fprintf(stderr, "Usage: %s [-d] command\n", app);
    fprintf(stderr, "Available commands:\n");

    for (; i < ARRAY_SIZE(acer_diag_commands); i++) {
	mode = acer_diag_commands[i].mode;

	fprintf(stderr, " %-32s %s (%s)\n",
			acer_diag_commands[i].name,
		        acer_diag_commands[i].description,
			acer_diag_commands[i].mode & ACER_MODE_OS ? "OS"
			                                          : "AMSS");
    }
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

int main(int argc, char** argv) {
    char device_name[256];
    char buffer[1024];
    char* command = NULL;
    int i = 0, node_found_flag = 0, opt, command_executed_flag = 0;
    int wrong_mode_flag = 0;
    int res = EXIT_FAILURE;
    int device_mode = ACER_MODE_UNKNOWN;
    char* device_mode_string = NULL;

    memset(device_name, 0, sizeof(device_name));

    while ((opt = getopt(argc, argv, "dh")) != -1) {
	switch(opt) {
	    case 'd': // Debug
		debug = 1;
		break;
	    case 'h': // Help
		usage(argv[0]);
		res = EXIT_SUCCESS;
		goto out;
	    default:
		usage(argv[0]);
		goto out;
	}
    }

    for (i=0; i < ARRAY_SIZE(known_usb_devices); i++) {
	if (debug) {
	    printf("[D] Searching for %04x:%04x (interface %02x)\n",
		    known_usb_devices[i].idVendor,
		    known_usb_devices[i].idProduct,
		    known_usb_devices[i].bInterfaceNumber);
	}

	if (!get_diag_tty_name(device_name, &known_usb_devices[i])) {
	    node_found_flag = 1;
	    device_mode = known_usb_devices[i].mode;
	    break;
	}
    }

    if (!node_found_flag) {
	fprintf(stderr,
		"%s: No diag tty found\n"
		"Please make sure qcserial module is loaded and it knows about your device\n"
		, argv[0]);
	goto out;
    }

    device_mode_string = get_device_mode_string(device_mode);

    snprintf(buffer, sizeof(buffer), "/dev/%s", device_name);

    printf("[I] Using device %s (%s)\n", buffer, device_mode_string);

    dev_fd = open(buffer, O_RDWR | O_EXCL | O_NONBLOCK | O_NOCTTY);

    if (dev_fd < 0) {
	perror(buffer);
	goto out;
    }

    if (optind >= argc) {
	fprintf(stderr, "%s: Command required\n", argv[0]);
	usage(argv[0]);
	goto out;
    }

    command = argv[optind];

    if (!strcmp(command, "test")) {
	res = EXIT_SUCCESS;
	goto out;
    }

    for (i = 0; i < ARRAY_SIZE(acer_diag_commands); i++) {
	if (strcmp(command, acer_diag_commands[i].name))
	    continue;

	if (device_mode & acer_diag_commands[i].mode) {
	    res = acer_diag_commands[i].callback();
	    command_executed_flag = 1;
	    goto out;
	}
	else
	    wrong_mode_flag = 1;
    }

    if (wrong_mode_flag) {
	fprintf(stderr, "%s: Command \"%s\" is not available in %s mode\n",
			argv[0], command,
			device_mode | ACER_MODE_OS ? "OS" : "AMSS");
	goto out;
    }

    if (! command_executed_flag) {
	fprintf(stderr, "%s: Unknown command \"%s\"\n", argv[0], command);
	usage(argv[0]);
    }

out:
    if (dev_fd)
	close(dev_fd);

    if (device_mode_string)
	free(device_mode_string);

    return res;
}

