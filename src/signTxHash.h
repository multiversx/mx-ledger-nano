#include "os.h"
#include "globals.h"

#ifndef _SIGN_TX_HASH_H_
#define _SIGN_TX_HASH_H_

#define NONCE_FIELD    "nonce"
#define VALUE_FIELD    "value"
#define RECEIVER_FIELD "receiver"
#define SENDER_FIELD   "sender"
#define GASPRICE_FIELD "gasPrice"
#define GASLIMIT_FIELD "gasLimit"
#define DATA_FIELD     "data"
#define CHAINID_FIELD  "chainID"
#define VERSION_FIELD  "version"
#define OPTIONS_FIELD  "options"

#define MAX_FIELD_LEN 8
#define MAX_VALUE_LEN 64UL

typedef enum parserStatus_e {
    JSON_IDLE,
    JSON_EXPECTING_FIELD,
    JSON_PROCESSING_FIELD,
    JSON_EXPECTING_COLON,
    JSON_EXPECTING_VALUE,
    JSON_PROCESSING_STRING_VALUE,
    JSON_PROCESSING_NUMERIC_VALUE,
    JSON_EXPECTING_COMMA
} parserStatus_e;

typedef struct {
    uint8_t hash[32];

    parserStatus_e status;
    uint8_t current_field[MAX_FIELD_LEN+1];
    uint8_t current_field_len;
    uint8_t current_value[MAX_VALUE_LEN+1];
    uint32_t current_value_len;
} tx_hash_context_t;

static tx_hash_context_t tx_hash_context;

void handleSignTxHash(uint8_t p1, uint8_t p2, uint8_t *dataBuffer, uint16_t dataLength, volatile unsigned int *flags, volatile unsigned int *tx);

#endif
