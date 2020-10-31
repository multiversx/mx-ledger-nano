#include "signTxHash.h"
#include <uint256.h>
#include "base64.h"
#include "utils.h"

static uint8_t setResultSignature();
bool sign_tx_hash(uint8_t *dataBuffer);

// UI for confirming the tx details of the transaction on screen
UX_STEP_NOCB(
    ux_sign_tx_hash_flow_17_step, 
    bnnn_paging, 
    {
      .title = "Receiver",
      .text = tx_context.receiver,
    });
UX_STEP_NOCB(
    ux_sign_tx_hash_flow_18_step,
    bnnn_paging,
    {
      .title = "Amount",
      .text = tx_context.amount,
    });
UX_STEP_NOCB(
    ux_sign_tx_hash_flow_19_step,
    bnnn_paging,
    {
      .title = "Fee",
      .text = tx_context.fee,
    });
UX_STEP_NOCB(
    ux_sign_tx_hash_flow_20_step,
    bnnn_paging,
    {
      .title = "Data",
      .text = tx_context.data,
    });
UX_STEP_VALID(
    ux_sign_tx_hash_flow_21_step, 
    pb, 
    sendResponse(setResultSignature(), true),
    {
      &C_icon_validate_14,
      "Sign transaction",
    });
UX_STEP_VALID(
    ux_sign_tx_hash_flow_22_step, 
    pb,
    sendResponse(0, false),
    {
      &C_icon_crossmark,
      "Reject",
    });

UX_FLOW(ux_sign_tx_hash_flow,
  &ux_sign_tx_hash_flow_17_step,
  &ux_sign_tx_hash_flow_18_step,
  &ux_sign_tx_hash_flow_19_step,
  &ux_sign_tx_hash_flow_20_step,
  &ux_sign_tx_hash_flow_21_step,
  &ux_sign_tx_hash_flow_22_step
);

static uint8_t setResultSignature() {
    uint8_t tx = 0;
    const uint8_t sig_size = 64;
    G_io_apdu_buffer[tx++] = sig_size;
    os_memmove(G_io_apdu_buffer + tx, tx_context.signature, sig_size);
    tx += sig_size;
    return tx;
}

bool sign_tx_hash(uint8_t *dataBuffer) {
    cx_ecfp_private_key_t privateKey;
    bool success = true;

    if (!getPrivateKey(bip32_account, bip32_address_index, &privateKey)) {
        return false;
    }

    BEGIN_TRY {
        TRY {
            cx_hash((cx_hash_t *)&msg_context.sha3, CX_LAST, dataBuffer, 0, tx_hash_context.hash, 32);
            cx_eddsa_sign(&privateKey, CX_RND_RFC6979 | CX_LAST, CX_SHA512, tx_hash_context.hash, 32, NULL, 0, tx_context.signature, 64, NULL);
        }
        CATCH_ALL {
            success = false;
        }
        FINALLY {
            os_memset(&privateKey, 0, sizeof(privateKey));
        }
    }
    END_TRY;

    return success;
}

void init_context() {
    tx_context.amount[0] = 0;
    tx_context.data[0] = 0;
    tx_context.data_size = 0;
    tx_context.fee[0] = 0;
    tx_context.gas_limit = 0;
    tx_context.gas_price = 0;
    tx_context.receiver[0] = 0;
    tx_hash_context.status = JSON_IDLE;
    cx_keccak_init(&msg_context.sha3, 256);
}

// verify "value" field
uint16_t verify_value(bool *valid) {
    if (strncmp(tx_hash_context.current_field, VALUE_FIELD, strlen(VALUE_FIELD)) == 0) {
        if (tx_hash_context.current_value_len >= sizeof(tx_context.amount))
            return ERR_AMOUNT_TOO_LONG;
        if (!valid_amount(tx_hash_context.current_value, strlen(tx_hash_context.current_value)))
            return ERR_INVALID_AMOUNT;
        os_memmove(tx_context.amount, tx_hash_context.current_value, tx_hash_context.current_value_len);
        *valid = true;
    }
    return MSG_OK;
}

// verify "receiver" field
uint16_t verify_receiver(bool *valid) {
    if (strncmp(tx_hash_context.current_field, RECEIVER_FIELD, strlen(RECEIVER_FIELD)) == 0) {
        if (tx_hash_context.current_value_len >= sizeof(tx_context.receiver))
            return ERR_RECEIVER_TOO_LONG;
        os_memmove(tx_context.receiver, tx_hash_context.current_value, tx_hash_context.current_value_len);
        *valid = true;
    }
    return MSG_OK;
}

// verify "gasPrice" field
uint16_t verify_gasprice(bool *valid) {
    if (strncmp(tx_hash_context.current_field, GASPRICE_FIELD, strlen(GASPRICE_FIELD)) == 0) {
        if (!parse_int(tx_hash_context.current_value, strlen(tx_hash_context.current_value), &tx_context.gas_price))
            return ERR_INVALID_FEE;
        *valid = true;
    }
    return MSG_OK;
}

// verify "gasLimit" field
uint16_t verify_gaslimit(bool *valid) {
    if (strncmp(tx_hash_context.current_field, GASLIMIT_FIELD, strlen(GASLIMIT_FIELD)) == 0) {
        if (!parse_int(tx_hash_context.current_value, strlen(tx_hash_context.current_value), &tx_context.gas_limit))
            return ERR_INVALID_FEE;
        if (!gas_to_fee(tx_context.gas_limit, tx_context.gas_price, tx_context.fee, sizeof(tx_context.fee) - PRETTY_SIZE))
            return ERR_INVALID_FEE;
        *valid = true;
    }
    return MSG_OK;
}

// verify "data" field
uint16_t verify_data(bool *valid) {
    if (strncmp(tx_hash_context.current_field, DATA_FIELD, strlen(DATA_FIELD)) == 0) {
        if (N_storage.setting_contract_data == 0)
            return ERR_CONTRACT_DATA_DISABLED;
        tx_hash_context.current_value_len = tx_hash_context.current_value_len / 4 * 4;
        char encoded[MAX_DISPLAY_DATA_SIZE];
        uint32_t enc_len = tx_hash_context.current_value_len;
        if (enc_len > MAX_DISPLAY_DATA_SIZE)
            enc_len = MAX_DISPLAY_DATA_SIZE;
        os_memmove(encoded, tx_hash_context.current_value, enc_len);
        uint32_t ascii_len = tx_hash_context.current_value_len;
        if (ascii_len > MAX_DISPLAY_DATA_SIZE) {
            ascii_len = MAX_DISPLAY_DATA_SIZE;
            // add "..." at the end to show that the data field is actually longer 
            char ellipsis[5] = "Li4u"; // "..." base64 encoded
            int ellipsisLen = strlen(ellipsis);
            memmove(encoded + MAX_DISPLAY_DATA_SIZE - ellipsisLen, ellipsis, ellipsisLen);
        }
        if (!base64decode(tx_context.data, encoded, ascii_len)) {
            return ERR_INVALID_MESSAGE;
        }
        computeDataSize(tx_hash_context.current_value, tx_hash_context.current_value_len);
        *valid = true;
    }
    return MSG_OK;
}

// verify "chainID" field
uint16_t verify_chainid(bool *valid) {
    if (strncmp(tx_hash_context.current_field, CHAINID_FIELD, strlen(CHAINID_FIELD)) == 0) {
        network_t network = NETWORK_TESTNET;
        if (strncmp(tx_hash_context.current_value, MAINNET_CHAIN_ID, strlen(MAINNET_CHAIN_ID)) == 0)
            network = NETWORK_MAINNET;
        if (!makeAmountPretty(tx_context.amount, sizeof(tx_context.amount), network) ||
            !makeAmountPretty(tx_context.fee, sizeof(tx_context.fee), network))
            return ERR_PRETTY_FAILED;
        *valid = true;
    }
    return MSG_OK;
}

// verify "version" field
uint16_t verify_version(bool *valid) {
    if (strncmp(tx_hash_context.current_field, VERSION_FIELD, strlen(VERSION_FIELD)) == 0) {
        uint64_t version;
        if (!parse_int(tx_hash_context.current_value, strlen(tx_hash_context.current_value), &version))
            return ERR_INVALID_MESSAGE;
        if (version != TX_HASH_VERSION)
            return ERR_WRONG_TX_VERSION;
        *valid = true;
    }
    return MSG_OK;
}

// verifies if the field and value are valid and stores them
uint16_t process_field(void) {
    if (tx_hash_context.current_field_len == 0 || tx_hash_context.current_value_len == 0)
        return ERR_INVALID_MESSAGE;
    if (tx_hash_context.current_value_len < MAX_VALUE_LEN)
        tx_hash_context.current_value[tx_hash_context.current_value_len++] = '\0';

    bool valid_field = false;
    uint16_t err;
    err = verify_value(&valid_field);
    if (err != MSG_OK)
        return err;
    err = verify_receiver(&valid_field);
    if (err != MSG_OK)
        return err;
    err = verify_gasprice(&valid_field);
    if (err != MSG_OK)
        return err;
    err = verify_gaslimit(&valid_field);
    if (err != MSG_OK)
        return err;
    err = verify_data(&valid_field);
    if (err != MSG_OK)
        return err;
    err = verify_chainid(&valid_field);
    if (err != MSG_OK)
        return err;
    err = verify_version(&valid_field);
    if (err != MSG_OK)
        return err;

    // verify the rest of the fields that are not displayed
    valid_field |= strncmp(tx_hash_context.current_field, NONCE_FIELD, strlen(NONCE_FIELD)) == 0;
    valid_field |= strncmp(tx_hash_context.current_field, SENDER_FIELD, strlen(SENDER_FIELD)) == 0;
    valid_field |= strncmp(tx_hash_context.current_field, OPTIONS_FIELD, strlen(OPTIONS_FIELD)) == 0;

    if (valid_field)
        return MSG_OK;
    else
        return ERR_INVALID_MESSAGE;
}

// parse_data interprets the json marshalized tx
uint16_t parse_data(uint8_t *dataBuffer, uint16_t dataLength) {
    if ((dataLength == 0) && (tx_hash_context.status == JSON_IDLE))
        return ERR_INVALID_MESSAGE;
    uint8_t idx = 0;
    for (;;) {
        if (idx >= dataLength)
            break;
        uint8_t c = dataBuffer[idx];
        idx++;
        switch(tx_hash_context.status) {
            case JSON_IDLE:
                if (c != '{')
                    return ERR_INVALID_MESSAGE;
                tx_hash_context.status = JSON_EXPECTING_FIELD;
                break;
            case JSON_EXPECTING_FIELD:
                if (c != '"')
                    return ERR_INVALID_MESSAGE;
                tx_hash_context.status = JSON_PROCESSING_FIELD;
                tx_hash_context.current_field_len = 0;
                break;
            case JSON_PROCESSING_FIELD:
                if (c == '"') {
                    tx_hash_context.status = JSON_EXPECTING_COLON;
                    break;
                }
                if (tx_hash_context.current_field_len >= MAX_FIELD_LEN)
                    return ERR_INVALID_MESSAGE;
                tx_hash_context.current_field[tx_hash_context.current_field_len++] = c;
                break;
            case JSON_EXPECTING_COLON:
                if (c != ':')
                    return ERR_INVALID_MESSAGE;
                tx_hash_context.status = JSON_EXPECTING_VALUE;
                tx_hash_context.current_value_len = 0;
                break;
            case JSON_EXPECTING_VALUE:
                if (c == '"') {
                    tx_hash_context.status = JSON_PROCESSING_STRING_VALUE;
                    break;
                }
                if (!is_digit(c))
                    return ERR_INVALID_MESSAGE;
                tx_hash_context.status = JSON_PROCESSING_NUMERIC_VALUE;
                tx_hash_context.current_value[tx_hash_context.current_value_len++] = c;
                break;
            case JSON_PROCESSING_STRING_VALUE:
                if (c == '"') {
                    uint16_t err = process_field();
                    if (err != MSG_OK)
                        return err;
                    tx_hash_context.status = JSON_EXPECTING_COMMA;
                    break;
                }
                if (tx_hash_context.current_value_len >= MAX_VALUE_LEN)
                    if (strncmp(tx_hash_context.current_field, DATA_FIELD, tx_hash_context.current_field_len) == 0 &&
                        tx_hash_context.current_field_len == strlen(DATA_FIELD)) {
                        tx_hash_context.current_value_len++;
                        break;
                    } else
                        return ERR_INVALID_MESSAGE;
                tx_hash_context.current_value[tx_hash_context.current_value_len++] = c;
                break;
            case JSON_PROCESSING_NUMERIC_VALUE:
                if (c == '}') {
                    tx_hash_context.status = JSON_IDLE;
                    return process_field();
                }
                if (c == ',') {
                    uint16_t err = process_field();
                    if (err != MSG_OK)
                        return err;
                    tx_hash_context.status = JSON_EXPECTING_FIELD;
                    break;
                }
                if ((tx_hash_context.current_value_len >= MAX_VALUE_LEN) || !is_digit(c))
                    return ERR_INVALID_MESSAGE;
                tx_hash_context.current_value[tx_hash_context.current_value_len++] = c;
                break;
            case JSON_EXPECTING_COMMA:
                if (c == '}') {
                    tx_hash_context.status = JSON_IDLE;
                    return MSG_OK;
                }
                if (c != ',')
                    return ERR_INVALID_MESSAGE;
                tx_hash_context.status = JSON_EXPECTING_FIELD;
                break;
        }
    }
    return MSG_OK;
}

void handleSignTxHash(uint8_t p1, uint8_t p2, uint8_t *dataBuffer, uint16_t dataLength, volatile unsigned int *flags, volatile unsigned int *tx) {
    UNUSED(p2);
    if (p1 == P1_FIRST) {
        init_context();
    } else
        if (p1 != P1_MORE)
            THROW(ERR_INVALID_P1);
    cx_hash((cx_hash_t *)&msg_context.sha3, 0, dataBuffer, dataLength, NULL, 0);
    uint16_t err = parse_data(dataBuffer, dataLength);
    if (err != MSG_OK)
        THROW(err);
    if (tx_hash_context.status != JSON_IDLE)
        THROW(MSG_OK);
    // sign the hash
    if (!sign_tx_hash(dataBuffer))
        THROW(ERR_SIGNATURE_FAILED);

    ux_flow_init(0, ux_sign_tx_hash_flow, NULL);
    *flags |= IO_ASYNCH_REPLY;
}
