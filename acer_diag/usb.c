#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <dirent.h>
#include <unistd.h>

#include "usb.h"

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

