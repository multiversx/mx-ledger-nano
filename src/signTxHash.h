#include "os.h"
#include "cx.h"
#include "globals.h"

#ifndef _SIGN_TX_HASH_H_
#define _SIGN_TX_HASH_H_

typedef struct {
    uint8_t hash[32];
    char strhash[65];
    uint8_t signature[64];
} tx_hash_context_t;

static tx_hash_context_t tx_hash_context;

void handleSignTxHash(uint8_t p1, uint8_t p2, uint8_t *dataBuffer, uint16_t dataLength, volatile unsigned int *flags, volatile unsigned int *tx);

#endif
