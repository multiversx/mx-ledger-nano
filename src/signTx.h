#include "os.h"
#include "cx.h"
#include "globals.h"

#ifndef _SIGN_TX_H_
#define _SIGN_TX_H_

#define MAX_AMOUNT_LEN        32
#define MAX_BUFFER_LEN        500
#define MAX_DATA_SIZE         400   // 400 in base64 = 300 in ASCII
#define MAX_DISPLAY_DATA_SIZE 64    // must be multiple of 4
#define DATA_SIZE_LEN         17
#define MAX_CHAINID_LEN       32
#define MAX_TICKER_LEN        5
#define MAX_UINT32_LEN        10    // len(f"{0xffffffff:d}")
#define MAX_UINT64_LEN        20    // len(f"{0xffffffffffffffff:d}")
#define MAX_UINT128_LEN       39    // len(f"{0xffffffffffffffffffffffffffffffff:d}")
#define PRETTY_SIZE (2 + MAX_TICKER_LEN) // additional space for "0." and " eGLD"

typedef struct {
    uint8_t buffer[MAX_BUFFER_LEN]; // buffer to hold large transactions that are composed from multiple APDUs
    uint16_t bufLen;

    char receiver[FULL_ADDRESS_LENGTH];
    char amount[MAX_AMOUNT_LEN + PRETTY_SIZE];
    uint64_t gas_limit;
    uint64_t gas_price;
    char fee[MAX_AMOUNT_LEN + PRETTY_SIZE];
    char data[MAX_DISPLAY_DATA_SIZE + DATA_SIZE_LEN];
    uint16_t data_size;
    uint8_t signature[64];
} tx_context_t;

static tx_context_t tx_context;

void handleSignTx(uint8_t p1, uint8_t p2, uint8_t *dataBuffer, uint16_t dataLength, volatile unsigned int *flags, volatile unsigned int *tx);

#endif
