#include "signTx.h"
#include "../deps/jsmn/jsmn.h"
#include <uint256.h>
#include "base64.h"

#define NONCE_FIELD     0x001
#define VALUE_FIELD     0x002
#define RECEIVER_FIELD  0x004
#define SENDER_FIELD    0x008
#define GAS_PRICE_FIELD 0x010
#define GAS_LIMIT_FIELD 0x020
#define DATA_FIELD      0x040
#define CHAIN_ID_FIELD  0x080
#define VERSION_FIELD   0x100

#define MANDATORY_FIELDS (    \
        NONCE_FIELD |         \
        VALUE_FIELD |         \
        RECEIVER_FIELD |      \
        SENDER_FIELD |        \
        GAS_PRICE_FIELD |     \
        GAS_LIMIT_FIELD |     \
        CHAIN_ID_FIELD |      \
        VERSION_FIELD         \
    )

static uint8_t setResultSignature();
void makeFeePretty(network_t network);
static int jsoneq(const char *json, jsmntok_t *tok, const char *s);
uint16_t txDataReceived(uint8_t *dataBuffer, uint16_t dataLength);
uint16_t parseData();
bool signTx(void);

// UI for confirming the tx details of the transaction on screen
UX_STEP_NOCB(
    ux_sign_tx_flow_8_step, 
    bnnn_paging, 
    {
      .title = "Receiver",
      .text = tx_context.receiver,
    });
UX_STEP_NOCB(
    ux_sign_tx_flow_9_step,
    bnnn_paging,
    {
      .title = "Amount",
      .text = tx_context.amount,
    });
UX_STEP_NOCB(
    ux_sign_tx_flow_10_step,
    bnnn_paging,
    {
      .title = "Fee",
      .text = tx_context.fee,
    });
UX_STEP_NOCB(
    ux_sign_tx_flow_11_step,
    bnnn_paging,
    {
      .title = "Data",
      .text = tx_context.data,
    });
UX_STEP_VALID(
    ux_sign_tx_flow_12_step, 
    pb, 
    sendResponse(setResultSignature(), true),
    {
      &C_icon_validate_14,
      "Sign transaction",
    });
UX_STEP_VALID(
    ux_sign_tx_flow_13_step, 
    pb,
    sendResponse(0, false),
    {
      &C_icon_crossmark,
      "Reject",
    });

UX_FLOW(ux_sign_tx_flow,
  &ux_sign_tx_flow_8_step,
  &ux_sign_tx_flow_9_step,
  &ux_sign_tx_flow_10_step,
  &ux_sign_tx_flow_11_step,
  &ux_sign_tx_flow_12_step,
  &ux_sign_tx_flow_13_step
);

static uint8_t setResultSignature() {
    uint8_t tx = 0;
    const uint8_t sig_size = 64;
    G_io_apdu_buffer[tx++] = sig_size;
    os_memmove(G_io_apdu_buffer + tx, tx_context.signature, sig_size);
    tx += sig_size;
    return tx;
}

static bool gas_to_fee(uint64_t gas_limit, uint64_t gas_price, uint16_t data_size, char *fee, size_t size)
{
    uint128_t x = {0, GAS_PER_DATA_BYTE};
    uint128_t y = {0, data_size};
    uint128_t z;
    uint128_t gas_unit_for_move_balance; 
    // tx fee formula
    // gasUnitForMoveBalance := (minGasLimit + len(data)*gasPerDataByte)
    // txFEE = gasUnitForMoveBalance * GASPRICE + (gasLimit - gasUnitForMoveBalance) * gasPriceModifier * GASPRICE
    mul128(&x, &y, &z);

    x.elements[1] = MIN_GAS_LIMIT;
    add128(&x, &z, &gas_unit_for_move_balance);

    x.elements[1] = gas_limit;
    minus128(&x, &gas_unit_for_move_balance, &y);

    x.elements[1] = GAS_PRICE_DIVIDER;
    divmod128(&y, &x, &z, &y);

    add128(&gas_unit_for_move_balance, &z, &y);
    
    x.elements[1] = gas_price;
    mul128(&x, &y, &z); /* XXX: there is a one-byte overflow in tostring128(), hence size-1 */
    if (!tostring128(&z, 10, fee, size - 1))
    {
        return false;
    }
    return true;
}

static bool is_digit(char c) {
  return c >= '0' && c <= '9';
}

static bool parse_int(char *str, size_t size, uint64_t *result) {
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

static bool valid_amount(char *amount, size_t size) {
  for (size_t i = 0; i < size; i++) {
      if (!is_digit(amount[i])) {
            return false;
      }
  }
  return true;
}

// make the eGLD amount look pretty. Add decimals, decimal point and ticker name
static bool makeAmountPretty(char *amount, size_t max_size, network_t network) {
    int len = strlen(amount);
    if ((size_t)len + PRETTY_SIZE >= max_size) {
        return false;
    }
    int missing = DECIMAL_PLACES - len + 1;
    if (missing > 0) {
        os_memmove(amount + missing, amount, len + 1);
        os_memset(amount, '0', missing);
    }
    len = strlen(amount);
    int dotPos = len - DECIMAL_PLACES;
    os_memmove(amount + dotPos + 1, amount + dotPos, DECIMAL_PLACES + 1);
    amount[dotPos] = '.';
    while (amount[strlen(amount) - 1] == '0') {
        amount[strlen(amount) - 1] = '\0';
    }
    if (amount[strlen(amount) - 1] == '.') {
        amount[strlen(amount) - 1] = '\0';
    }
    char suffix[MAX_TICKER_LEN+2] = " \0"; // 2 = leading space + trailing \0
    os_memmove(suffix + 1, TICKER_MAINNET, sizeof(TICKER_MAINNET));
    if (network == NETWORK_TESTNET) {
        os_memmove(suffix + 1, TICKER_TESTNET, sizeof(TICKER_TESTNET));
    }
    os_memmove(amount + strlen(amount), suffix, strlen(suffix) + 1);

    return true;
}

// helper for comparing json keys
static int jsoneq(const char *json, jsmntok_t *tok, const char *s) {
	short int tokLen = tok->end - tok->start;
    if (tok->type == JSMN_STRING && (int)strlen(s) == tokLen &&	strncmp(json + tok->start, s, tokLen) == 0) {
		return 0;
	}
	return -1;
}

// txDataReceived is called when a signTx APDU is received. it appends the received data to the buffer
uint16_t txDataReceived(uint8_t *dataBuffer, uint16_t dataLength) {
    if (tx_context.bufLen + dataLength >= MAX_BUFFER_LEN)
        return ERR_MESSAGE_TOO_LONG;
    os_memmove(tx_context.buffer + tx_context.bufLen, dataBuffer, dataLength);
    tx_context.bufLen += dataLength;
    tx_context.buffer[tx_context.bufLen] = '\0';
    return MSG_OK;
}

static bool set_bit(uint64_t *bitmap, uint64_t bit) {
  /* ensure that this field isn't already set and in the right order */
  if (*bitmap >= bit) {
      return false;
  }

  *bitmap |= bit;

  return true;
}

static void computeDataSize(char *base64, int b64len) {
    // calculate the ASCII size of the data field
    tx_context.data_size = b64len / 4 * 3;
    // take padding bytes into consideration
    if (b64len > 1) {
        if (base64[b64len - 1] == '=')
            tx_context.data_size--;
        if (base64[b64len - 2] == '=')
            tx_context.data_size--;
    }
    tx_context.data[tx_context.data_size] = '\0';
    int len = sizeof(tx_context.data);
    // prepare the first display page, which contains the data field size
    char str_size[DATA_SIZE_LEN] = "[  Size: 000  ] ";
    // sprintf equivalent workaround
    str_size[9] = '0' + tx_context.data_size / 100;
    str_size[10] = '0' + (tx_context.data_size / 10) % 10;
    str_size[11] = '0' + tx_context.data_size % 10;
    int size_len = strlen(str_size);
    // shift the actual data field to the right in order to make room for inserting the size in the first page
    os_memmove(tx_context.data + size_len, tx_context.data, len - size_len);
    // insert the data size in front of the actual data field
    os_memmove(tx_context.data, str_size, size_len);
    int data_end = size_len + tx_context.data_size;
    if (tx_context.data_size > MAX_DISPLAY_DATA_SIZE)
        data_end = size_len + MAX_DISPLAY_DATA_SIZE;
    tx_context.data[data_end] = '\0';
}

// parseData parses the received tx data
uint16_t parseData() {
    int i, r;
    jsmn_parser p;
    jsmntok_t t[20]; // We expect no more than 20 tokens

    jsmn_init(&p);
    r = jsmn_parse(&p, tx_context.buffer, tx_context.bufLen, t, sizeof(t)/sizeof(t[0]));
    if (r < 1 || t[0].type != JSMN_OBJECT) {
        return ERR_MESSAGE_INCOMPLETE;
    }
    uint64_t fields_bitmap = 0;
    network_t network = NETWORK_TESTNET;
    // initialize data with an empty string in case the tx doesn't contain the data field
    tx_context.data[0] = '\0';
    computeDataSize(tx_context.data, 0);
    // iterate all json keys
    for (i = 1; i < r; i += 2) {
        int len = t[i + 1].end - t[i + 1].start;
        jsmntype_t type = t[i + 1].type;
        char *str = tx_context.buffer + t[i + 1].start;
        if (jsoneq(tx_context.buffer, &t[i], "nonce") == 0) {
            if (type != JSMN_PRIMITIVE) {
                return ERR_INVALID_MESSAGE;
            }
            if (len > MAX_UINT64_LEN)
                return ERR_NONCE_TOO_LONG;
            if (!set_bit(&fields_bitmap, NONCE_FIELD)) {
                return ERR_INVALID_MESSAGE;
            }
            uint64_t nonce;
            if (!parse_int(str, len, &nonce)) {
                return ERR_INVALID_MESSAGE;
            }
        }
        else if (jsoneq(tx_context.buffer, &t[i], "value") == 0) {
            if (type != JSMN_STRING) {
                return ERR_INVALID_MESSAGE;
            }
            if ((size_t)len >= sizeof(tx_context.amount) - PRETTY_SIZE)
                return ERR_AMOUNT_TOO_LONG;
            if (!set_bit(&fields_bitmap, VALUE_FIELD)) {
                return ERR_INVALID_MESSAGE;
            }
            if (!valid_amount(str, len)) {
                return ERR_INVALID_AMOUNT;
            }
            os_memmove(tx_context.amount, str, len);
            tx_context.amount[len] = '\0';
        }
        else if (jsoneq(tx_context.buffer, &t[i], "receiver") == 0) {
            if (type != JSMN_STRING) {
                return ERR_INVALID_MESSAGE;
            }
            if (len >= FULL_ADDRESS_LENGTH)
                return ERR_RECEIVER_TOO_LONG;
            if (!set_bit(&fields_bitmap, RECEIVER_FIELD)) {
                return ERR_INVALID_MESSAGE;
            }
            os_memmove(tx_context.receiver, str, len);
            tx_context.receiver[len] = '\0';
        }
        else if (jsoneq(tx_context.buffer, &t[i], "sender") == 0) {
            if (type != JSMN_STRING) {
                return ERR_INVALID_MESSAGE;
            }
            if (len >= FULL_ADDRESS_LENGTH)
                return ERR_RECEIVER_TOO_LONG;
            if (!set_bit(&fields_bitmap, SENDER_FIELD)) {
                return ERR_INVALID_MESSAGE;
            }
        }
        else if (jsoneq(tx_context.buffer, &t[i], "gasPrice") == 0) {
            if (type != JSMN_PRIMITIVE) {
                return ERR_INVALID_MESSAGE;
            }
            if (len > MAX_UINT64_LEN)
                return ERR_AMOUNT_TOO_LONG;
            if (!set_bit(&fields_bitmap, GAS_PRICE_FIELD)) {
                return ERR_INVALID_MESSAGE;
            }
            if (!parse_int(str, len, &tx_context.gas_price)) {
                return ERR_INVALID_MESSAGE;
            }
        }
        else if (jsoneq(tx_context.buffer, &t[i], "gasLimit") == 0) {
            if (type != JSMN_PRIMITIVE) {
                return ERR_INVALID_MESSAGE;
            }
            if (len > MAX_UINT64_LEN)
                return ERR_AMOUNT_TOO_LONG;
            if (!set_bit(&fields_bitmap, GAS_LIMIT_FIELD)) {
                return ERR_INVALID_MESSAGE;
            }
            if (!parse_int(str, len, &tx_context.gas_limit)) {
                return ERR_INVALID_MESSAGE;
            }
        }
        else if (jsoneq(tx_context.buffer, &t[i], "data") == 0) {
            if (!set_bit(&fields_bitmap, DATA_FIELD)) {
                return ERR_INVALID_MESSAGE;
            }
            // check if contract data is not enabled from the menu
            if (N_storage.setting_contract_data == 0) {
                return ERR_CONTRACT_DATA_DISABLED;
            }
            if (len % 4 != 0) {
                return ERR_INVALID_MESSAGE;
            }
            if (len > MAX_DATA_SIZE) {
                return ERR_DATA_TOO_LONG;
            }
            char encoded[MAX_DISPLAY_DATA_SIZE];
            int enc_len = len;
            if (len > MAX_DISPLAY_DATA_SIZE)
                enc_len = MAX_DISPLAY_DATA_SIZE;
            os_memmove(encoded, str, enc_len);
            int ascii_len = len;
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
            computeDataSize(str, len);
        }
        else if (jsoneq(tx_context.buffer, &t[i], "chainID") == 0) {
            if (type != JSMN_STRING) {
                return ERR_INVALID_MESSAGE;
            }
            if (len >= MAX_CHAINID_LEN)
                return ERR_INVALID_MESSAGE;
            if (!set_bit(&fields_bitmap, CHAIN_ID_FIELD)) {
                return ERR_INVALID_MESSAGE;
            }
            if (len == strlen(MAINNET_CHAIN_ID) && strncmp(str, MAINNET_CHAIN_ID, len) == 0) {
                network = NETWORK_MAINNET;
            }
        }
        else if (jsoneq(tx_context.buffer, &t[i], "version") == 0) {
            if (type != JSMN_PRIMITIVE) {
                return ERR_INVALID_MESSAGE;
            }
            if (len > MAX_UINT32_LEN)
                return ERR_INVALID_MESSAGE;
            if (!set_bit(&fields_bitmap, VERSION_FIELD)) {
                return ERR_INVALID_MESSAGE;
            }
            uint64_t version;
            if (!parse_int(str, len, &version)) {
                return ERR_INVALID_MESSAGE;
            }
            if (version != TX_VERSION) {
                return ERR_WRONG_TX_VERSION;
            }
        }
        else {
            return ERR_INVALID_MESSAGE;
        }
    }
    // check if we identified the mandatory fields
    if ((fields_bitmap & MANDATORY_FIELDS) != MANDATORY_FIELDS)
        return ERR_INVALID_MESSAGE;

    if (!gas_to_fee(tx_context.gas_limit, tx_context.gas_price, tx_context.data_size, tx_context.fee, sizeof(tx_context.fee) - PRETTY_SIZE)) {
        return ERR_INVALID_FEE;
    }

    if (!makeAmountPretty(tx_context.amount, sizeof(tx_context.amount), network) ||
        !makeAmountPretty(tx_context.fee, sizeof(tx_context.fee), network)) {
        return ERR_PRETTY_FAILED;
    }

    return MSG_OK;
}

// signTx performs the actual tx signing after the full tx data has been received
bool signTx(void) {
    cx_ecfp_private_key_t privateKey;
    bool success = true;

    if (!getPrivateKey(bip32_account, bip32_address_index, &privateKey)) {
        return false;
    }

    BEGIN_TRY {
        TRY {
            cx_eddsa_sign(&privateKey, CX_RND_RFC6979 | CX_LAST, CX_SHA512, tx_context.buffer, tx_context.bufLen, NULL, 0, tx_context.signature, 64, NULL);
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

// handleSignTx handles tx data receiving, parsing and signing. in the end it calls the user confirmation UI
void handleSignTx(uint8_t p1, uint8_t p2, uint8_t *dataBuffer, uint16_t dataLength, volatile unsigned int *flags, volatile unsigned int *tx) {
    uint16_t retCode;
    switch(p1) {
    // first packet
    case P1_FIRST:
        tx_context.bufLen = 0;
        retCode = txDataReceived(dataBuffer, dataLength);
        break;
    // consequent packets if the tx is larger than 255 bytes
    case P1_MORE:
        retCode = txDataReceived(dataBuffer, dataLength);
        break;
    default:
        // unknown packet type
        retCode = ERR_INVALID_P1;
        break;
    }
    if (retCode != MSG_OK) {
        THROW(retCode);
        return;
    }

    retCode = parseData();

    if (retCode == ERR_MESSAGE_INCOMPLETE) {
        THROW(MSG_OK);
        return;
    }

    if (retCode != MSG_OK) {
        THROW(retCode);
        return;
    }

    if (!signTx()) {
        THROW(ERR_SIGNATURE_FAILED);
        return;
    }

    ux_flow_init(0, ux_sign_tx_flow, NULL);
    *flags |= IO_ASYNCH_REPLY;
}
