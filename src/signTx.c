#include "signTx.h"
#include "os.h"
#include "ux.h"
#include "utils.h"
#include <jsmn.h>

static tx_context_t tx_context;

static uint8_t setResultSignature();
void makeAmountPretty();
static int jsoneq(const char *json, jsmntok_t *tok, const char *s);
uint16_t txDataReceived(uint8_t *dataBuffer, uint16_t dataLength);
uint16_t parseData();
void signTx();
void handleSignTx(uint8_t p1, uint8_t p2, uint8_t *dataBuffer, uint16_t dataLength, volatile unsigned int *flags, volatile unsigned int *tx);

////////////////////////////////////////////////////////////////////////////////
// UI for confirming the receiver and amount of a transaction on screen
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
UX_STEP_VALID(
    ux_sign_tx_flow_10_step, 
    pb, 
    sendResponse(setResultSignature(), true),
    {
      &C_icon_validate_14,
      "Sign transaction",
    });
UX_STEP_VALID(
    ux_sign_tx_flow_11_step, 
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
  &ux_sign_tx_flow_11_step
);

////////////////////////////////////////////////////////////////////////////////

static uint8_t setResultSignature() {
    uint8_t tx = 0;
    const uint8_t sig_size = 64;
    G_io_apdu_buffer[tx++] = sig_size;
    os_memmove(G_io_apdu_buffer + tx, tx_context.signature, sig_size);
    tx += sig_size;
    return tx;
}

////////////////////////////////////////////////////////////////////////////////
// make the ERD amount look pretty. Add decimals and decimal point
void makeAmountPretty() {
    int len = strlen(tx_context.amount);
    int missing = DECIMAL_PLACES - len + 1;
    if (missing > 0) {
        os_memmove(tx_context.amount + missing, tx_context.amount, len + 1);
        os_memset(tx_context.amount, '0', missing);
    }
    len = strlen(tx_context.amount);
    int dotPos = len - DECIMAL_PLACES;
    os_memmove(tx_context.amount + dotPos + 1, tx_context.amount + dotPos, DECIMAL_PLACES + 1);
    tx_context.amount[dotPos] = '.';
    while (tx_context.amount[strlen(tx_context.amount) - 1] == '0') {
        tx_context.amount[strlen(tx_context.amount) - 1] = '\0';
    }
    if (tx_context.amount[strlen(tx_context.amount) - 1] == '.') {
        tx_context.amount[strlen(tx_context.amount) - 1] = '\0';
    }
    char suffix[5] = " ERD\0";
    os_memmove(tx_context.amount + strlen(tx_context.amount), suffix, 5);
}

////////////////////////////////////////////////////////////////////////////////
// helper for comparing json keys
static int jsoneq(const char *json, jsmntok_t *tok, const char *s) {
	short int tokLen = tok->end - tok->start;
    if (tok->type == JSMN_STRING && (int)strlen(s) == tokLen &&	strncmp(json + tok->start, s, tokLen) == 0) {
		return 0;
	}
	return -1;
}

////////////////////////////////////////////////////////////////////////////////
// txDataReceive is called when a signTx APDU is received. it appends the received data to the buffer
uint16_t txDataReceived(uint8_t *dataBuffer, uint16_t dataLength) {
    if (tx_context.bufLen + dataLength >= MAX_BUFFER_LEN)
        return ERR_MESSAGE_TOO_LONG;
    os_memmove(tx_context.buffer + tx_context.bufLen, dataBuffer, dataLength);
    tx_context.bufLen += dataLength;
    tx_context.buffer[tx_context.bufLen] = '\0';
    return MSG_OK;
}

////////////////////////////////////////////////////////////////////////////////
// parseData parses the received tx data
uint16_t parseData() {
    int i, r;

    jsmn_parser p;
    jsmntok_t t[20]; // We expect no more than 20 tokens

    jsmn_init(&p);
    r = jsmn_parse(&p, tx_context.buffer, tx_context.bufLen, t, sizeof(t)/sizeof(t[0]));
    if (r < 1 || t[0].type != JSMN_OBJECT) {
        // unable to parse. maybe we did not receive the entire tx so we return 'ok'
        return MSG_OK;
    }
    int found_args = 0;
    bool hasDataField = false;
    // iterate all json keys
	for (i = 1; i < r; i++) {
        int len = t[i + 1].end - t[i + 1].start;
		if (jsoneq(tx_context.buffer, &t[i], "receiver") == 0) {
            if (len >= FULL_ADDRESS_LENGTH)
                return ERR_RECEIVER_TOO_LONG;
            os_memmove(tx_context.receiver, tx_context.buffer + t[i + 1].start, len);
            tx_context.receiver[len] = '\0';
            found_args++;
        }
		if (jsoneq(tx_context.buffer, &t[i], "value") == 0) {
            if (len >= MAX_AMOUNT_LEN)
                return ERR_AMOUNT_TOO_LONG;
            os_memmove(tx_context.amount, tx_context.buffer + t[i + 1].start, len);
            tx_context.amount[len] = '\0';
            makeAmountPretty();
            found_args++;
        }
		if (jsoneq(tx_context.buffer, &t[i], "data") == 0) {
            hasDataField = true;
        }
    }
    // found_args should be 2 if we identified both receiver and amount
    if (found_args != 2) 
        return ERR_INVALID_MESSAGE;
    // check if the data field is not empty and contract data is not enabled from the menu
    if (hasDataField && N_storage.setting_contract_data == 0)
        return ERR_CONTRACT_DATA_DISABLED;
    return MSG_OK;
}

////////////////////////////////////////////////////////////////////////////////
// signTx performs the actual tx signing after the full tx data has beed received
void signTx() {
    cx_ecfp_private_key_t privateKey;
    BEGIN_TRY {
        TRY {
            getPrivateKey(0, 0, &privateKey);
            cx_eddsa_sign(&privateKey, CX_RND_RFC6979 | CX_LAST, CX_SHA512, tx_context.buffer, tx_context.bufLen, NULL, 0, tx_context.signature, 64, NULL);
        }
        FINALLY {
            os_memset(&privateKey, 0, sizeof(privateKey));
        }
    }
    END_TRY;
}

////////////////////////////////////////////////////////////////////////////////
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
    }
    if (retCode != MSG_OK) {
        THROW(retCode);
        return;
    }

    retCode = parseData();
    if (retCode != MSG_OK) {
        THROW(retCode);
        return;
    }

    signTx();

    ux_flow_init(0, ux_sign_tx_flow, NULL);
    *flags |= IO_ASYNCH_REPLY;
}
