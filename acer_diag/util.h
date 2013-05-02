#ifndef _ACER_DIAG_UTIL_H
#define _ACER_DIAG_UTIL_H

#include "defines.h"
#include "diag_packet.h"

int diag_write(unsigned char* buf, size_t count);
int diag_write_packet(diag_packet* pkt);

int diag_read(unsigned char* buf, size_t count, int timeout);
void diag_discard_content();

char* get_device_mode_string(int mode);

#endif // _ACER_DIAG_UTIL_H
