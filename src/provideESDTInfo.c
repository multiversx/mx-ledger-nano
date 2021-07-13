#include "provideESDTInfo.h"
#include "constants.h"
#include <string.h>
#include <ux.h>
#include <lcx_ecdsa.h>
#include <lcx_sha256.h>
#include "globals.h"

esdt_info_t esdt_info;

uint16_t handle_provide_ESDT_info(uint8_t *data_buffer, uint16_t data_length) {
    uint8_t last_required_len = 0;
    uint8_t required_len = 1;
    uint8_t hash[32];
    cx_sha256_t sha256;
    cx_ecfp_public_key_t tokenKey;

    // read ticker len
    if (data_length < required_len) {
        return ERR_MESSAGE_INCOMPLETE;
    }
    esdt_info.ticker_len = data_buffer[last_required_len];
    
    // read ticker
    last_required_len = required_len;
    required_len += esdt_info.ticker_len;
    if (data_length < required_len) {
        return ERR_MESSAGE_INCOMPLETE;
    }
    memmove(esdt_info.ticker, data_buffer + last_required_len, esdt_info.ticker_len);
    esdt_info.ticker[esdt_info.ticker_len] = '\0';
    
    // read identifier len
    last_required_len = required_len;
    required_len++;
    if (data_length < required_len) {
        return ERR_MESSAGE_INCOMPLETE;
    }
    esdt_info.identifier_len = data_buffer[last_required_len];

    // read identifier
    last_required_len = required_len;
    required_len += esdt_info.identifier_len;
    if (data_length < required_len) {
        return ERR_MESSAGE_INCOMPLETE;
    }
    memmove(esdt_info.identifier, data_buffer + last_required_len, esdt_info.identifier_len);
    esdt_info.identifier[esdt_info.identifier_len] = '\0';

    // read decimals
    last_required_len = required_len;
    required_len++;
    if (data_length < required_len) {
        return ERR_MESSAGE_INCOMPLETE;
    }
    esdt_info.decimals = data_buffer[last_required_len];

    // read chain id len
    last_required_len = required_len;
    required_len++;
    if (data_length < required_len) {
        return ERR_MESSAGE_INCOMPLETE;
    }
    esdt_info.chain_id_len = data_buffer[last_required_len];

    // read chain id
    last_required_len = required_len;
    required_len += esdt_info.chain_id_len;
    if (data_length < required_len) {
        return ERR_MESSAGE_INCOMPLETE;
    }
    memmove(esdt_info.chain_id, data_buffer + last_required_len, esdt_info.chain_id_len);
    esdt_info.chain_id[esdt_info.chain_id_len] = '\0';

    cx_sha256_init(&sha256);
    cx_hash((cx_hash_t *) &sha256, CX_LAST, data_buffer, required_len, hash, 32);

    cx_ecfp_init_public_key(CX_CURVE_256K1,
                            LEDGER_SIGNATURE_PUBLIC_KEY,
                            sizeof(LEDGER_SIGNATURE_PUBLIC_KEY),
                            &tokenKey);

    int signature_size = data_length - required_len;
    if (!cx_ecdsa_verify(&tokenKey,
                         CX_LAST,
                         CX_SHA256,
                         hash,
                         32,
                         data_buffer + required_len,
                         signature_size)) {
        return ERR_INVALID_ESDT_SIGNATURE;
    }

    return MSG_OK;
}
