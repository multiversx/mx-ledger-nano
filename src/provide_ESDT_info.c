#include "provide_ESDT_info.h"
#include "constants.h"
#include <string.h>

#ifndef FUZZING
#include <cx.h>

static bool verify_signature(const uint8_t *data_buffer,
                             uint16_t data_length,
                             size_t required_len) {
    uint8_t hash[HASH_LEN];
    cx_sha256_t sha256;
    cx_ecfp_public_key_t tokenKey;

    cx_sha256_init(&sha256);
    cx_hash_no_throw((cx_hash_t *) &sha256, CX_LAST, data_buffer, required_len, hash, 32);

    cx_ecfp_init_public_key_no_throw(CX_CURVE_256K1,
                                     LEDGER_SIGNATURE_PUBLIC_KEY,
                                     sizeof(LEDGER_SIGNATURE_PUBLIC_KEY),
                                     &tokenKey);

    int signature_size = data_length - required_len;
    return cx_ecdsa_verify(&tokenKey,
                           CX_LAST,
                           CX_SHA256,
                           hash,
                           32,
                           data_buffer + required_len,
                           signature_size);
}
#endif

// TODO: refactor the input so signature can be checked before parsing all token
// fields
uint16_t handle_provide_ESDT_info(const uint8_t *data_buffer,
                                  uint16_t data_length,
                                  esdt_info_t *esdt_info_obj) {
    size_t last_required_len = 0;
    size_t required_len = 1;

    // read ticker len
    if (data_length < required_len) {
        return ERR_MESSAGE_INCOMPLETE;
    }
    esdt_info_obj->ticker_len = data_buffer[last_required_len];

    // read ticker
    last_required_len = required_len;
    required_len += esdt_info_obj->ticker_len;
    if (esdt_info_obj->ticker_len >= sizeof(esdt_info_obj->ticker) || data_length < required_len) {
        return ERR_MESSAGE_INCOMPLETE;
    }
    memcpy(esdt_info_obj->ticker, data_buffer + last_required_len, esdt_info_obj->ticker_len);
    esdt_info_obj->ticker[esdt_info_obj->ticker_len] = '\0';

    // read identifier len
    last_required_len = required_len;
    required_len++;
    if (data_length < required_len) {
        return ERR_MESSAGE_INCOMPLETE;
    }
    esdt_info_obj->identifier_len = data_buffer[last_required_len];

    // read identifier
    last_required_len = required_len;
    required_len += esdt_info_obj->identifier_len;
    if (esdt_info_obj->identifier_len >= sizeof(esdt_info_obj->identifier) ||
        data_length < required_len) {
        return ERR_MESSAGE_INCOMPLETE;
    }
    memcpy(esdt_info_obj->identifier,
           data_buffer + last_required_len,
           esdt_info_obj->identifier_len);
    esdt_info_obj->identifier[esdt_info_obj->identifier_len] = '\0';

    // read decimals
    last_required_len = required_len;
    required_len++;
    if (data_length < required_len) {
        return ERR_MESSAGE_INCOMPLETE;
    }
    esdt_info_obj->decimals = data_buffer[last_required_len];

    // read chain id len
    last_required_len = required_len;
    required_len++;
    if (data_length < required_len) {
        return ERR_MESSAGE_INCOMPLETE;
    }
    esdt_info_obj->chain_id_len = data_buffer[last_required_len];

    // read chain id
    last_required_len = required_len;
    required_len += esdt_info_obj->chain_id_len;
    if (esdt_info_obj->chain_id_len >= sizeof(esdt_info_obj->chain_id) ||
        data_length < required_len) {
        return ERR_MESSAGE_INCOMPLETE;
    }
    memcpy(esdt_info_obj->chain_id, data_buffer + last_required_len, esdt_info_obj->chain_id_len);
    esdt_info_obj->chain_id[esdt_info_obj->chain_id_len] = '\0';

#ifndef FUZZING
    if (!verify_signature(data_buffer, data_length, required_len)) {
        return ERR_INVALID_ESDT_SIGNATURE;
    }
#endif

    esdt_info_obj->valid = true;

    return MSG_OK;
}
