#include "os.h"
#include "cx.h"
#include "globals.h"

#ifndef _SIGN_TX_H_
#define _SIGN_TX_H_

void handleSignTx(uint8_t p1, uint8_t p2, uint8_t *dataBuffer, uint16_t dataLength, volatile unsigned int *flags, volatile unsigned int *tx);

#endif
