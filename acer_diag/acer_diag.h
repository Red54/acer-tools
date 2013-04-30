#ifndef _ACER_DIAG_ACER_DIAG_H
#define _ACER_DIAG_ACER_DIAG_H

#define ARRAY_SIZE(a) (sizeof(a) / sizeof(*a))

enum {
    ACER_MODE_OS            = 1,
    ACER_MODE_DEBUG         = 2,
    ACER_MODE_USB_TETHERING = 4,
    ACER_MODE_AMSS          = 16,
    // We won't be able to do anything in these modes for now:
    ACER_MODE_FASTBOOT,
    ACER_MODE_UNKNOWN,
};

#endif // _ACER_DIAG_ACER_DIAG_H
