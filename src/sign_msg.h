#include "cx.h"
#include "globals.h"
#include "os.h"

#ifndef _SIGN_MSG_H_
#define _SIGN_MSG_H_

void init_msg_context(void);
void handle_sign_msg(uint8_t p1,
                     uint8_t *data_buffer,
                     uint16_t data_length,
                     volatile unsigned int *flags);

#endif
