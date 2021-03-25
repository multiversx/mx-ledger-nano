#include "os.h"
#include "cx.h"
#include "globals.h"

#ifndef _SIGN_MSG_H_
#define _SIGN_MSG_H_

static const char const PREPEND[] = "\x17"
                                    "Elrond Signed Message:\n";

void handleSignMsg(uint8_t p1, uint8_t p2, uint8_t *dataBuffer, uint16_t dataLength, volatile unsigned int *flags);

#endif
