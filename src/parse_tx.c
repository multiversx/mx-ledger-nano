#include <string.h>

#include <uint256.h>

#include "base64.h"
#include "constants.h"
#include "parse_tx.h"
#include "provide_ESDT_info.h"
#include "sign_tx_hash.h"

#ifndef FUZZING
#include "globals.h"
#endif

static void extract_esdt_value(const char *encoded_data_field, const uint8_t encoded_data_length);
static void set_network(const char *chain_id);
static void set_message_in_amount(const char *message);

// make the eGLD/token amount look pretty. Add decimals, decimal point and
// ticker name
//  example: amount=500, decimal_places=3 => amount=0.5 eGLD
bool make_amount_pretty(char *amount, size_t max_size, const char *ticker, int decimals_places) {
    int len = strlen(amount);
    if ((size_t) len + PRETTY_SIZE >= max_size) {
        return false;
    }
    int missing = decimals_places - len + 1;
    if (missing > 0) {
        if ((size_t)(missing + len + 1) > max_size) {
            return false;
        }
        // missing represents the leading 0s. amount needs to be moved to the right and the missing
        // 0s prepended
        memmove(amount + missing, amount, len + 1);
        // replace remaining leading characters with 0s in the buffer
        memset(amount, '0', missing);
    }
    len = strlen(amount);
    int dotPos = len - decimals_places;
    memmove(amount + dotPos + 1, amount + dotPos, decimals_places + 1);
    amount[dotPos] = '.';
    while (amount[strlen(amount) - 1] == '0') {
        amount[strlen(amount) - 1] = '\0';
    }
    if (amount[strlen(amount) - 1] == '.') {
        amount[strlen(amount) - 1] = '\0';
    }
    char suffix[MAX_TICKER_LEN + 2] = " \0";  // 2 = leading space + trailing \0
    memmove(suffix + 1, ticker, strlen(ticker) + 1);
    memmove(amount + strlen(amount), suffix, strlen(suffix) + 1);

    return true;
}

bool is_hex_digit(char c) {
    return is_digit(c) || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
}

bool parse_int(char *str, size_t size, uint64_t *result) {
    uint64_t min = 0, n = 0;

    for (size_t i = 0; i < size; i++) {
        if (!is_digit(str[i])) {
            return false;
        }
        n = n * 10 + str[i] - '0';
        /* ensure there is no integer overflow */
        if (n < min) {
            return false;
        }
        min = n;
    }
    *result = n;
    return true;
}

bool parse_hex(const char *str, size_t size, uint128_t *result) {
    uint128_t n = {{0, 0}};
    uint128_t tmp = {{0, 0}};

    for (size_t i = 0; i < size; i++) {
        if (!is_hex_digit(str[i])) {
            return false;
        }
        uint128_t digit = {{0, str[i] - '0'}};
        if (str[i] >= 'a') {
            digit.elements[1] = str[i] - 'a' + 10;
        } else if (str[i] >= 'A') {
            digit.elements[1] = str[i] - 'A' + 10;
        }
        shiftl128(&n, 4, &tmp);
        add128(&tmp, &digit, &n);
    }
    *result = n;
    return true;
}

bool gas_to_fee(uint64_t gas_limit,
                uint64_t gas_price,
                uint32_t data_size,
                char *fee,
                size_t size) {
    uint128_t x = {{0, GAS_PER_DATA_BYTE}};
    uint128_t y = {{0, data_size}};
    uint128_t z;
    uint128_t gas_unit_for_move_balance;

    // tx fee formula
    // gas_units_for_move_balance = (min_gas_limit + len(data)*gas_per_data_byte)
    // tx_fee = gas_units_for_move_balance * gas_price + (gas_limit -
    // gas_unit_for_move_balance) * gas_price_modifier * gas_price. The difference
    // is that instead of multiplying with gas_price_modifier we divide by
    // 1/gas_price_modifier and the constant is marked as GAS_PRICE_DIVIER

    mul128(&x, &y, &z);

    x.elements[1] = MIN_GAS_LIMIT;
    add128(&x, &z, &gas_unit_for_move_balance);

    x.elements[1] = gas_limit;
    minus128(&x, &gas_unit_for_move_balance, &y);

    x.elements[1] = GAS_PRICE_DIVIDER;
    divmod128(&y, &x, &z, &y);

    add128(&gas_unit_for_move_balance, &z, &y);

    x.elements[1] = gas_price;
    mul128(&x, &y, &z);
    /* XXX: there is a one-byte overflow in tostring128(), hence size-1 */
    if (!tostring128(&z, BASE_10, fee, size - 1)) {
        return false;
    }
    return true;
}

bool valid_amount(char *amount, size_t size) {
    for (size_t i = 0; i < size; i++) {
        if (!is_digit(amount[i])) {
            return false;
        }
    }
    return true;
}

void compute_data_size(uint32_t decodedDataLen) {
    tx_context.data_size = decodedDataLen;
    int len = sizeof(tx_context.data);
    // prepare the first display page, which contains the data field size
    char str_size[DATA_SIZE_LEN] = "[Size:       0] ";
    // sprintf equivalent workaround
    for (uint32_t ds = tx_context.data_size, idx = 13; ds > 0; ds /= 10, idx--) {
        str_size[idx] = '0' + ds % 10;
    }

    int size_len = strlen(str_size);
    // shift the actual data field to the right in order to make room for
    // inserting the size in the first page
    memmove(tx_context.data + size_len, tx_context.data, len - size_len);
    // insert the data size in front of the actual data field
    memmove(tx_context.data, str_size, size_len);
    int data_end = size_len + tx_context.data_size;
    if (tx_context.data_size > MAX_DISPLAY_DATA_SIZE) {
        data_end = size_len + MAX_DISPLAY_DATA_SIZE;
    }
    tx_context.data[data_end] = '\0';
}

// verify "value" field
uint16_t verify_value(bool *valid) {
    if (strncmp(tx_hash_context.current_field, VALUE_FIELD, strlen(VALUE_FIELD)) == 0) {
        if (tx_hash_context.current_value_len >= sizeof(tx_context.amount)) {
            return ERR_AMOUNT_TOO_LONG;
        }
        if (!valid_amount(tx_hash_context.current_value, strlen(tx_hash_context.current_value))) {
            return ERR_INVALID_AMOUNT;
        }
        memmove(tx_context.amount,
                tx_hash_context.current_value,
                tx_hash_context.current_value_len);
        *valid = true;
    }
    return MSG_OK;
}

// verify "receiver" field
uint16_t verify_receiver(bool *valid) {
    if (strncmp(tx_hash_context.current_field, RECEIVER_FIELD, strlen(RECEIVER_FIELD)) == 0 &&
        tx_hash_context.current_field_len == strlen(RECEIVER_FIELD)) {
        if (tx_hash_context.current_value_len >= sizeof(tx_context.receiver)) {
            return ERR_RECEIVER_TOO_LONG;
        }
        memmove(tx_context.receiver,
                tx_hash_context.current_value,
                tx_hash_context.current_value_len);
        *valid = true;
    }
    return MSG_OK;
}

// verify "gasPrice" field
uint16_t verify_gasprice(bool *valid) {
    if (strncmp(tx_hash_context.current_field, GASPRICE_FIELD, strlen(GASPRICE_FIELD)) == 0) {
        if (!parse_int(tx_hash_context.current_value,
                       strlen(tx_hash_context.current_value),
                       &tx_context.gas_price)) {
            return ERR_INVALID_FEE;
        }
        *valid = true;
    }
    return MSG_OK;
}

// verify "gasLimit" field
uint16_t verify_gaslimit(bool *valid) {
    if (strncmp(tx_hash_context.current_field, GASLIMIT_FIELD, strlen(GASLIMIT_FIELD)) == 0) {
        if (!parse_int(tx_hash_context.current_value,
                       strlen(tx_hash_context.current_value),
                       &tx_context.gas_limit)) {
            return ERR_INVALID_FEE;
        }
        *valid = true;
    }
    return MSG_OK;
}

// verify "data" field
uint16_t verify_data(bool *valid) {
    if (strncmp(tx_hash_context.current_field, DATA_FIELD, strlen(DATA_FIELD)) == 0) {
#ifndef FUZZING
        if (N_storage.setting_contract_data == 0) {
            return ERR_CONTRACT_DATA_DISABLED;
        }
#endif
        tx_hash_context.current_value_len = tx_hash_context.current_value_len / 4 * 4;
        char encoded[MAX_DISPLAY_DATA_SIZE];
        uint32_t enc_len = tx_hash_context.current_value_len;
        if (enc_len > MAX_DISPLAY_DATA_SIZE) {
            enc_len = MAX_DISPLAY_DATA_SIZE;
        }
        memmove(encoded, tx_hash_context.current_value, enc_len);
        uint32_t ascii_len = tx_hash_context.current_value_len;
        if (ascii_len > MAX_DISPLAY_DATA_SIZE) {
            ascii_len = MAX_DISPLAY_DATA_SIZE;
            // add "..." at the end to show that the data field is actually longer
            char ellipsis[5] = "Li4u";  // "..." base64 encoded
            int ellipsisLen = strlen(ellipsis);
            memmove(encoded + MAX_DISPLAY_DATA_SIZE - ellipsisLen, ellipsis, ellipsisLen);
        }
        if (!base64decode(tx_context.data, encoded, ascii_len)) {
            return ERR_INVALID_MESSAGE;
        }
        if (strncmp(tx_context.data, ESDT_TRANSFER_PREFIX, ESDT_TRANSFER_PREFIX_LENGTH) == 0) {
            extract_esdt_value(tx_hash_context.current_value, tx_hash_context.current_value_len);
        }
        compute_data_size(tx_hash_context.data_field_size);
        *valid = true;
    }
    return MSG_OK;
}

static void extract_esdt_value(const char *encoded_data_field, const uint8_t encoded_data_length) {
    if (encoded_data_length == 0) {
        return;
    }
    char data_field[MAX_ESDT_TRANSFER_DATA_SIZE];
    if (!base64decode(data_field, encoded_data_field, encoded_data_length)) {
        return;
    }

    int esdt_value_start_position = INVALID_INDEX;
    for (size_t idx = ESDT_TRANSFER_PREFIX_LENGTH; idx < strlen(data_field); idx++) {
        if (data_field[idx] == SC_ARGS_SEPARATOR) {
            esdt_value_start_position = idx + 1;
            break;
        }
    }
    if (esdt_value_start_position == INVALID_INDEX) {
        return;
    }

    int esdt_value_end_position = INVALID_INDEX;
    for (size_t idx = esdt_value_start_position; idx < strlen(data_field); idx++) {
        if (data_field[idx] == SC_ARGS_SEPARATOR || data_field[idx] == BASE_64_INVALID_CHAR) {
            esdt_value_end_position = idx - 1;
            break;
        }
    }

    if (esdt_value_end_position == INVALID_INDEX) {
        esdt_value_end_position = strlen(data_field);
    }

    if (esdt_value_end_position - esdt_value_start_position + 1 > MAX_ESDT_VALUE_HEX_COUNT) {
        tx_context.esdt_value[0] = ESDT_CODE_VALUE_TOO_HIGH;
        tx_context.esdt_value[1] = '\0';
        return;
    }

    tx_context.esdt_value[0] = ESDT_CODE_VALUE_OK;
    size_t num_chars_to_copy = esdt_value_end_position - esdt_value_start_position + 1;
    memmove(tx_context.esdt_value + 1, data_field + esdt_value_start_position, num_chars_to_copy);
    tx_context.esdt_value[num_chars_to_copy + 1] = '\0';
}

// verify "chainID" field
uint16_t verify_chainid(bool *valid) {
    if (strncmp(tx_hash_context.current_field, CHAINID_FIELD, strlen(CHAINID_FIELD)) == 0) {
        const char *ticker = TICKER_TESTNET;
        if (strncmp(tx_hash_context.current_value, MAINNET_CHAIN_ID, strlen(MAINNET_CHAIN_ID)) ==
            0) {
            ticker = TICKER_MAINNET;
        }
        memmove(tx_context.chain_id,
                tx_hash_context.current_value,
                tx_hash_context.current_value_len);
        set_network(tx_hash_context.current_value);

        if (!gas_to_fee(tx_context.gas_limit,
                        tx_context.gas_price,
                        tx_context.data_size,
                        tx_context.fee,
                        sizeof(tx_context.fee) - PRETTY_SIZE)) {
            return ERR_INVALID_FEE;
        }

        if (!make_amount_pretty(tx_context.amount,
                                sizeof(tx_context.amount),
                                ticker,
                                DECIMAL_PLACES) ||
            !make_amount_pretty(tx_context.fee, sizeof(tx_context.fee), ticker, DECIMAL_PLACES)) {
            return ERR_PRETTY_FAILED;
        }
        *valid = true;
    }
    return MSG_OK;
}

static void set_network(const char *chain_id) {
    if (strncmp(chain_id, MAINNET_CHAIN_ID, strlen(MAINNET_CHAIN_ID)) == 0) {
        memmove(tx_context.network, MAINNET_NETWORK, strlen(MAINNET_NETWORK));
        tx_context.network[strlen(MAINNET_NETWORK)] = '\0';
        return;
    }
    if (strncmp(chain_id, TESTNET_CHAIN_ID, strlen(TESTNET_CHAIN_ID)) == 0) {
        memmove(tx_context.network, TESTNET_NETWORK, strlen(TESTNET_NETWORK));
        tx_context.network[strlen(TESTNET_NETWORK)] = '\0';
        return;
    }
    if (strncmp(chain_id, DEVNET_CHAIN_ID, strlen(DEVNET_CHAIN_ID)) == 0) {
        memmove(tx_context.network, DEVNET_NETWORK, strlen(DEVNET_NETWORK));
        tx_context.network[strlen(DEVNET_NETWORK)] = '\0';
        return;
    }

    memmove(tx_context.network, UNKNOWN_NETWORK, strlen(UNKNOWN_NETWORK));
    tx_context.network[strlen(UNKNOWN_NETWORK)] = '\0';
}

// verify "version" field
uint16_t verify_version(bool *valid) {
    if (strncmp(tx_hash_context.current_field, VERSION_FIELD, strlen(VERSION_FIELD)) == 0) {
        uint64_t version;
        if (!parse_int(tx_hash_context.current_value,
                       strlen(tx_hash_context.current_value),
                       &version)) {
            return ERR_INVALID_MESSAGE;
        }
        if (version < TX_HASH_VERSION) {
            return ERR_WRONG_TX_VERSION;
        }
        *valid = true;
    }
    return MSG_OK;
}

// verify "options" field
uint16_t verify_options(bool *valid) {
    if (strncmp(tx_hash_context.current_field, OPTIONS_FIELD, strlen(OPTIONS_FIELD)) == 0) {
        uint64_t options;
        if (!parse_int(tx_hash_context.current_value,
                       strlen(tx_hash_context.current_value),
                       &options)) {
            return ERR_INVALID_MESSAGE;
        }
        if (options < TX_HASH_OPTIONS) {
            return ERR_WRONG_TX_OPTIONS;
        }
        *valid = true;
    }
    return MSG_OK;
}

// verify "guardian" field
uint16_t verify_guardian(bool *valid) {
    if (strncmp(tx_hash_context.current_field, GUARDIAN_ADDR_FIELD, strlen(GUARDIAN_ADDR_FIELD)) ==
        0) {
        if (tx_hash_context.current_value_len >= sizeof(tx_context.guardian)) {
            return ERR_INVALID_MESSAGE;
        }
        memmove(tx_context.guardian,
                tx_hash_context.current_value,
                tx_hash_context.current_value_len);
        *valid = true;
    }
    return MSG_OK;
}

// verifies if the field and value are valid and stores them
uint16_t process_field(void) {
    if (tx_hash_context.current_field_len == 0 || tx_hash_context.current_value_len == 0) {
        return ERR_INVALID_MESSAGE;
    }
    if (tx_hash_context.current_value_len < MAX_VALUE_LEN) {
        tx_hash_context.current_value[tx_hash_context.current_value_len++] = '\0';
    }
    bool valid_field = false;
    uint16_t err;
    err = verify_value(&valid_field);
    if (err != MSG_OK) {
        return err;
    }
    err = verify_receiver(&valid_field);
    if (err != MSG_OK) {
        return err;
    }
    err = verify_gasprice(&valid_field);
    if (err != MSG_OK) {
        return err;
    }
    err = verify_gaslimit(&valid_field);
    if (err != MSG_OK) {
        return err;
    }
    err = verify_data(&valid_field);
    if (err != MSG_OK) {
        return err;
    }
    err = verify_chainid(&valid_field);
    if (err != MSG_OK) {
        return err;
    }
    err = verify_version(&valid_field);
    if (err != MSG_OK) {
        return err;
    }
    err = verify_options(&valid_field);
    if (err != MSG_OK) {
        return err;
    }
    err = verify_guardian(&valid_field);
    if (err != MSG_OK) {
        return err;
    }

    // verify the rest of the fields that are not displayed
    valid_field |= strncmp(tx_hash_context.current_field, NONCE_FIELD, strlen(NONCE_FIELD)) == 0;
    valid_field |= strncmp(tx_hash_context.current_field, SENDER_FIELD, strlen(SENDER_FIELD)) == 0;
    valid_field |= strncmp(tx_hash_context.current_field,
                           SENDER_USERNAME_FIELD,
                           strlen(SENDER_USERNAME_FIELD)) == 0;
    valid_field |= strncmp(tx_hash_context.current_field,
                           RECEIVER_USERNAME_FIELD,
                           strlen(RECEIVER_USERNAME_FIELD)) == 0;

    if (valid_field) {
        return MSG_OK;
    } else {
        return ERR_INVALID_MESSAGE;
    }
}

// parse_data interprets the json marshalized tx
uint16_t parse_data(const uint8_t *data_buffer, uint16_t data_length) {
    if ((data_length == 0) && (tx_hash_context.status == JSON_IDLE)) {
        return ERR_INVALID_MESSAGE;
    }
    uint8_t idx = 0;
    for (;;) {
        if (idx >= data_length) {
            break;
        }
        uint8_t c = data_buffer[idx];
        idx++;
        switch (tx_hash_context.status) {
            case JSON_IDLE:
                if (c != '{') {
                    return ERR_INVALID_MESSAGE;
                }
                tx_hash_context.status = JSON_EXPECTING_FIELD;
                break;
            case JSON_EXPECTING_FIELD:
                if (c != '"') {
                    return ERR_INVALID_MESSAGE;
                }
                tx_hash_context.status = JSON_PROCESSING_FIELD;
                tx_hash_context.current_field_len = 0;
                break;
            case JSON_PROCESSING_FIELD:
                if (c == '"') {
                    tx_hash_context.status = JSON_EXPECTING_COLON;
                    break;
                }
                if (tx_hash_context.current_field_len >= MAX_FIELD_LEN) {
                    return ERR_INVALID_MESSAGE;
                }
                tx_hash_context.current_field[tx_hash_context.current_field_len++] = c;
                break;
            case JSON_EXPECTING_COLON:
                if (c != ':') {
                    return ERR_INVALID_MESSAGE;
                }
                tx_hash_context.status = JSON_EXPECTING_VALUE;
                tx_hash_context.current_value_len = 0;
                break;
            case JSON_EXPECTING_VALUE:
                if (c == '"') {
                    tx_hash_context.status = JSON_PROCESSING_STRING_VALUE;
                    break;
                }
                if (!is_digit(c)) {
                    return ERR_INVALID_MESSAGE;
                }
                tx_hash_context.status = JSON_PROCESSING_NUMERIC_VALUE;
                tx_hash_context.current_value[tx_hash_context.current_value_len++] = c;
                break;
            case JSON_PROCESSING_STRING_VALUE: {
                bool is_data_field = strncmp(tx_hash_context.current_field,
                                             DATA_FIELD,
                                             tx_hash_context.current_field_len) == 0;
                if (c == '"') {
                    if (is_data_field) {
                        uint32_t data_value_len;
                        // remove additional characters and convert to decoded string length
                        data_value_len = tx_hash_context.current_value_len / 4 * 3;
                        //  remove trailing padding chars from count if any
                        if (tx_hash_context.current_value_len > 2) {
                            // example:
                            // "data": "YQ==",
                            //               ^
                            // idx is 2 positions ahead of last 2 chars from data value, so
                            // idx-2 and idx-3 will contain them
                            if (data_buffer[idx - 2] == '=') {
                                data_value_len--;
                            }
                            if (data_buffer[idx - 3] == '=') {
                                data_value_len--;
                            }
                        }
                        tx_hash_context.data_field_size = data_value_len;
                    }
                    uint16_t err = process_field();
                    if (err != MSG_OK) {
                        return err;
                    }
                    tx_hash_context.status = JSON_EXPECTING_COMMA;
                    break;
                }
                if (tx_hash_context.current_value_len >= MAX_VALUE_LEN) {
                    if (is_data_field && tx_hash_context.current_field_len == strlen(DATA_FIELD)) {
                        tx_hash_context.current_value_len++;
                        break;
                    } else {
                        return ERR_INVALID_MESSAGE;
                    }
                }
                tx_hash_context.current_value[tx_hash_context.current_value_len++] = c;
            } break;
            case JSON_PROCESSING_NUMERIC_VALUE:
                if (c == '}') {
                    tx_hash_context.status = JSON_IDLE;
                    return process_field();
                }
                if (c == ',') {
                    uint16_t err = process_field();
                    if (err != MSG_OK) {
                        return err;
                    }
                    tx_hash_context.status = JSON_EXPECTING_FIELD;
                    break;
                }
                if ((tx_hash_context.current_value_len >= MAX_VALUE_LEN) || !is_digit(c)) {
                    return ERR_INVALID_MESSAGE;
                }
                tx_hash_context.current_value[tx_hash_context.current_value_len++] = c;
                break;
            case JSON_EXPECTING_COMMA:
                if (c == '}') {
                    tx_hash_context.status = JSON_IDLE;
                    return MSG_OK;
                }
                if (c != ',') {
                    return ERR_INVALID_MESSAGE;
                }
                tx_hash_context.status = JSON_EXPECTING_FIELD;
                break;
        }
    }
    return MSG_OK;
}

// parse_esdt_data interprets the ESDT transfer data field of a transaction
uint16_t parse_esdt_data(void) {
    uint128_t value = {{0, 0}};
    bool res;

    if (strlen(tx_context.esdt_value) == 0) {
        set_message_in_amount(ESDT_VALUE_N_A);
        return MSG_OK;
    }

    if ((strlen(tx_context.esdt_value) == 1) &&
        tx_context.esdt_value[0] == ESDT_CODE_VALUE_TOO_HIGH) {
        set_message_in_amount(ESDT_VALUE_TOO_LONG);
        return MSG_OK;
    }

    res = parse_hex(tx_context.esdt_value + 1, strlen(tx_context.esdt_value) - 1, &value);
    if (!res) {
        set_message_in_amount(ESDT_VALUE_HEX_PARSE_ERR);
        return MSG_OK;
    }

    char *amount = tx_context.amount;
    if (!tostring128(&value, BASE_10, amount, ESDT_VALUE_MAX_LENGTH)) {
        return ERR_INVALID_AMOUNT;
    }

    if (!esdt_info.valid) {
        return ERR_INVALID_ESDT;
    }

    if (!make_amount_pretty(amount,
                            sizeof(tx_context.amount),
                            esdt_info.ticker,
                            esdt_info.decimals)) {
        return ERR_PRETTY_FAILED;
    }

    return MSG_OK;
}

static void set_message_in_amount(const char *message) {
    memmove(tx_context.amount, message, strlen(message));
    tx_context.amount[strlen(message)] = '\0';
}
