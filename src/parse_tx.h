#pragma once

#include "constants.h"
#include "sign_tx_hash.h"
#include "utils.h"

typedef struct {
    char receiver[FULL_ADDRESS_LENGTH];
    char amount[MAX_AMOUNT_LEN + PRETTY_SIZE];
    uint64_t gas_limit;
    uint64_t gas_price;
    char fee[MAX_AMOUNT_LEN + PRETTY_SIZE];
    char data[MAX_DISPLAY_DATA_SIZE + DATA_SIZE_LEN];
    uint32_t data_size;
    char chain_id[MAX_CHAINID_LEN];
    uint8_t signature[64];
    char esdt_value[MAX_ESDT_VALUE_HEX_COUNT + PRETTY_SIZE];
    char network[8];
    char guardian[FULL_ADDRESS_LENGTH];
    char relayer[FULL_ADDRESS_LENGTH];
} tx_context_t;

extern tx_context_t tx_context;
extern tx_hash_context_t tx_hash_context;

uint16_t parse_data(const uint8_t *data_buffer, uint16_t data_length);
uint16_t parse_esdt_data(void);
