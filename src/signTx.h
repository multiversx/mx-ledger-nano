#include "os.h"
#include "cx.h"
#include "globals.h"

#ifndef _SIGN_TX_H_
#define _SIGN_TX_H_

void handleSignTx(uint8_t p1, uint8_t p2, uint8_t *dataBuffer, uint16_t dataLength, volatile unsigned int *flags, volatile unsigned int *tx);

#define MAX_AMOUNT_LEN        32
#define MAX_BUFFER_LEN        1024
#define MAX_DATA_SIZE         800   // 800 in base64 = 600 in ASCII
#define MAX_DISPLAY_DATA_SIZE 64    // must be multiple of 4
#define DATA_SIZE_LEN         17
#define MAX_CHAINID_LEN       32
#define MAX_TICKER_LEN        5
#define MAX_UINT32_LEN        10    // len(f"{0xffffffff:d}")
#define MAX_UINT64_LEN        20    // len(f"{0xffffffffffffffff:d}")
#define MAX_UINT128_LEN       39    // len(f"{0xffffffffffffffffffffffffffffffff:d}")

#endif
