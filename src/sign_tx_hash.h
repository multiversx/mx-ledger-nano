#ifndef _SIGN_TX_HASH_H_
#define _SIGN_TX_HASH_H_

#include <stdint.h>

#define NONCE_FIELD             "nonce"
#define VALUE_FIELD             "value"
#define RECEIVER_FIELD          "receiver"
#define SENDER_FIELD            "sender"
#define GASPRICE_FIELD          "gasPrice"
#define GASLIMIT_FIELD          "gasLimit"
#define DATA_FIELD              "data"
#define CHAINID_FIELD           "chainID"
#define VERSION_FIELD           "version"
#define OPTIONS_FIELD           "options"
#define SENDER_USERNAME_FIELD   "senderUsername"
#define RECEIVER_USERNAME_FIELD "receiverUsername"
#define GUARDIAN_ADDR_FIELD     "guardian"

#define MAX_FIELD_LEN 16
#define MAX_VALUE_LEN 128UL

typedef enum parser_status_e {
    JSON_IDLE,
    JSON_EXPECTING_FIELD,
    JSON_PROCESSING_FIELD,
    JSON_EXPECTING_COLON,
    JSON_EXPECTING_VALUE,
    JSON_PROCESSING_STRING_VALUE,
    JSON_PROCESSING_NUMERIC_VALUE,
    JSON_EXPECTING_COMMA
} parser_status_e;

typedef struct {
    uint8_t hash[32];
    parser_status_e status;
    char current_field[MAX_FIELD_LEN + 1];
    uint8_t current_field_len;
    char current_value[MAX_VALUE_LEN + 1];
    uint32_t current_value_len;
    uint32_t data_field_size;
} tx_hash_context_t;

void init_tx_context(void);
void handle_sign_tx_hash(uint8_t p1,
                         uint8_t *data_buffer,
                         uint16_t data_length,
                         volatile unsigned int *flags);

#endif
