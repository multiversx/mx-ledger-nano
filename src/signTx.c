#include "signTx.h"
#include "os.h"
#include "ux.h"
#include "utils.h"
#include <jsmn.h>

static tx_context_t tx_context;

static uint8_t set_result_signature() {
    uint8_t tx = 0;
    const uint8_t sig_size = 64;
    G_io_apdu_buffer[tx++] = sig_size;
    os_memmove(G_io_apdu_buffer + tx, tx_context.signature, sig_size);
    tx += sig_size;
    return tx;
}

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
    sendSignResponse(set_result_signature(), true),
    {
      &C_icon_validate_14,
      "Sign transaction",
    });
UX_STEP_VALID(
    ux_sign_tx_flow_11_step, 
    pb,
    sendSignResponse(0, false),
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

// helper for comparing json keys
static int jsoneq(const char *json, jsmntok_t *tok, const char *s) {
	if (tok->type == JSMN_STRING && (int) strlen(s) == tok->end - tok->start &&
			strncmp(json + tok->start, s, tok->end - tok->start) == 0) {
		return 0;
	}
	return -1;
}

void handleSignTx(uint8_t p1, uint8_t p2, uint8_t *dataBuffer, uint16_t dataLength, volatile unsigned int *flags, volatile unsigned int *tx) {
    // first packet
    if (p1 == P1_FIRST) {
        if (dataLength >= MAX_BUFFER_LEN) {
            THROW(ERR_MESSAGE_TOO_LONG);
            return;
        }
        os_memmove(tx_context.buffer, dataBuffer, dataLength);
        tx_context.bufLen = dataLength;
    } else
    // consequent packets if the tx is larger than 255 bytes
    if (p1 == P1_MORE) {
        if (tx_context.bufLen + dataLength >= MAX_BUFFER_LEN) {
            THROW(ERR_MESSAGE_TOO_LONG);
            return;
        }
        os_memmove(tx_context.buffer + tx_context.bufLen, dataBuffer, dataLength);
        tx_context.bufLen += dataLength;
    } else {
        // unknown packet type
        THROW(ERR_INVALID_P1);
        return;
    }
    tx_context.buffer[tx_context.bufLen] = '\0';

	int i, r;

	jsmn_parser p;
	jsmntok_t t[20]; /* We expect no more than 20 tokens */

	jsmn_init(&p);
	r = jsmn_parse(&p, tx_context.buffer, tx_context.bufLen, t, sizeof(t)/sizeof(t[0]));
	if (r < 1 || t[0].type != JSMN_OBJECT) {
        // unable to parse. maybe we did not receive the entire tx so we return 'ok'
        THROW(0x9000);
        return;
	}
    int found_args = 0;
    bool hasDataField = false;
    // iterate all json keys
	for (i = 1; i < r; i++) {
        int len = t[i + 1].end - t[i + 1].start;
		if (jsoneq(tx_context.buffer, &t[i], "receiver") == 0) {
            if (len >= FULL_ADDRESS_LENGTH) {
                THROW(ERR_RECEIVER_TOO_LONG);
                return;
            }
            os_memmove(tx_context.receiver, tx_context.buffer + t[i + 1].start, len);
            tx_context.receiver[len] = '\0';
            found_args++;
        }
		if (jsoneq(tx_context.buffer, &t[i], "value") == 0) {
            if (len >= MAX_AMOUNT_LEN) {
                THROW(ERR_AMOUNT_TOO_LONG);
                return;
            }
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
    if (found_args != 2) {
        THROW(ERR_INVALID_MESSAGE);
        return;
    }
    // check if the data field is not empty and contract data is not enabled from the menu
    if (hasDataField && N_storage.setting_contract_data == 0) {
        THROW(ERR_CONTRACT_DATA_DISABLED);
        return;
    }

    cx_ecfp_private_key_t privateKey;
    // sign the tx
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

    ux_flow_init(0, ux_sign_tx_flow, NULL);
    *flags |= IO_ASYNCH_REPLY;
}
