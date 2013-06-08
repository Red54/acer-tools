#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>

#include "acer_diag.h"
#include "defines.h"
#include "commands.h"
#include "usb.h"
#include "util.h"

struct usb_device_interface known_usb_devices[] = {
    /* Acer Liquid A1 - MI can be found in qcsera.inf */
    // OS
    { 0x0502, 0x3202, 0x0, ACER_MODE_OS | ACER_MODE_DEBUG },
    { 0x0502, 0x3203, 0x0, ACER_MODE_OS },
    { 0x0502, 0x3222, 0x2, ACER_MODE_OS | ACER_MODE_USB_TETHERING | ACER_MODE_DEBUG },
    { 0x0502, 0x3223, 0x2, ACER_MODE_OS | ACER_MODE_USB_TETHERING },

    // AMSS
    { 0x0502, 0x9002, 0x0, ACER_MODE_AMSS },
};

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
		"Please make sure qcserial module is loaded and it knows about your device.\n"
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

