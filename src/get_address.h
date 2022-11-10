#include "os.h"

#ifndef _GET_ADDRESS_H_
#define _GET_ADDRESS_H_

#define P2_DISPLAY_BECH32 0x00
#define P2_DISPLAY_HEX    0x01

void handle_get_address(uint8_t p1,
                        uint8_t p2,
                        uint8_t *data_buffer,
                        uint16_t data_length,
                        volatile unsigned int *flags,
                        volatile unsigned int *tx);

#endif
