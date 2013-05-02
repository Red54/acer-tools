#ifndef _ACER_DIAG_USB_H
#define _ACER_DIAG_USB_H

struct usb_device_interface {
    int idVendor;
    int idProduct;
    int bInterfaceNumber;
    int mode;
};

int read_hex_int_from_file(char* path);
int get_diag_tty_name(char* name, struct usb_device_interface* query);

#endif // _ACER_DIAG_USB_H
