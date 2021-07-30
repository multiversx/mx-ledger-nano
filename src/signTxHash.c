#include "globals.h"
#include "signTxHash.h"
#include <uint256.h>
#include "parseTx.h"
#include "getPrivateKey.h"
#include "provideESDTInfo.h"
#include "utils.h"
#include "ux.h"

tx_hash_context_t tx_hash_context;
tx_context_t tx_context;

static uint8_t set_result_signature();
bool sign_tx_hash(uint8_t *data_buffer);
bool is_esdt_transfer();

// UI for confirming the ESDT transfer on screen
UX_STEP_NOCB(
    ux_transfer_esdt_flow_23_step, 
    bnnn_paging, 
    {
      .title = "Token",
      .text = esdt_info.ticker,
    });
UX_STEP_NOCB(
    ux_transfer_esdt_flow_24_step, 
    bnnn_paging, 
    {
      .title = "Value",
      .text = tx_context.amount,
    });
UX_STEP_NOCB(
    ux_transfer_esdt_flow_25_step, 
    bnnn_paging, 
    {
      .title = "Receiver",
      .text = tx_context.receiver,
    });
UX_STEP_NOCB(
    ux_transfer_esdt_flow_26_step, 
    bnnn_paging, 
    {
      .title = "Fee",
      .text = tx_context.fee,
    });
UX_STEP_VALID(
    ux_transfer_esdt_flow_27_step, 
    pb, 
    send_response(set_result_signature(), true),
    {
      &C_icon_validate_14,
      "Confirm transfer",
    });
UX_STEP_VALID(
    ux_transfer_esdt_flow_28_step, 
    pb,
    send_response(0, false),
    {
      &C_icon_crossmark,
      "Reject",
    });

UX_FLOW(ux_transfer_esdt_flow,
  &ux_transfer_esdt_flow_23_step,
  &ux_transfer_esdt_flow_24_step,
  &ux_transfer_esdt_flow_25_step,
  &ux_transfer_esdt_flow_26_step,
  &ux_transfer_esdt_flow_27_step,
  &ux_transfer_esdt_flow_28_step
);

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
    send_response(set_result_signature(), true),
    {
      &C_icon_validate_14,
      "Sign transaction",
    });
UX_STEP_VALID(
    ux_sign_tx_hash_flow_22_step, 
    pb,
    send_response(0, false),
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

static uint8_t set_result_signature() {
    uint8_t tx = 0;
    const uint8_t sig_size = 64;
    G_io_apdu_buffer[tx++] = sig_size;
    memmove(G_io_apdu_buffer + tx, tx_context.signature, sig_size);
    tx += sig_size;
    return tx;
}

bool sign_tx_hash(uint8_t *data_buffer) {
    cx_ecfp_private_key_t privateKey;
    bool success = true;

    if (!getPrivateKey(bip32_account, bip32_address_index, &privateKey)) {
        return false;
    }

    BEGIN_TRY {
        TRY {
            cx_hash((cx_hash_t *)&sha3_context, CX_LAST, data_buffer, 0, tx_hash_context.hash, 32);
            cx_eddsa_sign(&privateKey, CX_RND_RFC6979 | CX_LAST, CX_SHA512, tx_hash_context.hash, 32, NULL, 0, tx_context.signature, 64, NULL);
        }
        CATCH_ALL {
            success = false;
        }
        FINALLY {
            memset(&privateKey, 0, sizeof(privateKey));
        }
    }
    END_TRY;

    return success;
}

void init_tx_context() {
    tx_context.amount[0] = 0;
    tx_context.data[0] = 0;
    tx_context.data_size = 0;
    tx_context.fee[0] = 0;
    tx_context.gas_limit = 0;
    tx_context.gas_price = 0;
    tx_context.receiver[0] = 0;
    tx_context.chain_id[0] = 0;
    tx_hash_context.status = JSON_IDLE;
    cx_keccak_init(&sha3_context, 256);

    app_state = APP_STATE_IDLE;
}

void handle_sign_tx_hash(uint8_t p1, uint8_t *data_buffer, uint16_t data_length, volatile unsigned int *flags) {
    if (p1 == P1_FIRST) {
        init_tx_context();
        app_state = APP_STATE_SIGNING_TX;
    } else {
        if (p1 != P1_MORE) {
            THROW(ERR_INVALID_P1);
        }
        if (app_state != APP_STATE_SIGNING_TX) {
          THROW(ERR_INVALID_MESSAGE);
      }
    }

    cx_hash((cx_hash_t *)&sha3_context, 0, data_buffer, data_length, NULL, 0);
    uint16_t err = parse_data(data_buffer, data_length);
    if (err != MSG_OK) {
        init_tx_context();
        THROW(err);
    }

    if (tx_hash_context.status != JSON_IDLE) {
        THROW(MSG_OK);
    }

    // sign the hash
    if (!sign_tx_hash(data_buffer)) {
        init_tx_context();
        THROW(ERR_SIGNATURE_FAILED);
    }

    bool should_display_esdt_flow = false;
    if (is_esdt_transfer()) {
        uint16_t res;
        res = parse_esdt_data(tx_context.data, tx_context.data_size + DATA_SIZE_LEN);
        if (res != MSG_OK) {
            THROW(res);
        }
        should_display_esdt_flow = true;
    }

    app_state = APP_STATE_IDLE;
    *flags |= IO_ASYNCH_REPLY;

    if(should_display_esdt_flow) {
        ux_flow_init(0, ux_transfer_esdt_flow, NULL);
        return;
    }

    ux_flow_init(0, ux_sign_tx_hash_flow, NULL);
}

bool is_esdt_transfer() {
    bool identifier_len_valid = esdt_info.identifier_len > 0;
    bool has_data = strlen(tx_context.data) > 0;
    bool has_esdt_transfer_prefix = false;
    bool has_registered_esdt_identifier = false;
    bool same_chainid = false;

    if (has_data && identifier_len_valid)  {
        has_esdt_transfer_prefix = strncmp(tx_context.data + DATA_SIZE_LEN - 1, ESDT_TRANSFER_PREFIX, strlen(ESDT_TRANSFER_PREFIX)) == 0;
        has_registered_esdt_identifier = strncmp(tx_context.data + DATA_SIZE_LEN - 1 + strlen(ESDT_TRANSFER_PREFIX), esdt_info.identifier, esdt_info.identifier_len) == 0;
        same_chainid = strncmp(tx_context.chain_id, esdt_info.chain_id, MAX_CHAINID_LEN) == 0;
    }

    return has_esdt_transfer_prefix && has_registered_esdt_identifier && same_chainid;
}
