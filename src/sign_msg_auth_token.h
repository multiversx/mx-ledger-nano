#include "cx.h"
#include "globals.h"
#include "os.h"

#ifndef _SIGN_MSG_AUTH_TOKEN_H_
#define _SIGN_MSG_AUTH_TOKEN_H_

void handle_auth_token(uint8_t p1,
                       uint8_t *data_buffer,
                       uint16_t data_length,
                       volatile unsigned int *flags);

#endif
