#include "sign_tx_hash.h"
#include "get_private_key.h"
#include "globals.h"
#include "parse_tx.h"
#include "provide_ESDT_info.h"
#include "utils.h"
#include "ux.h"
#include <uint256.h>
#include "menu.h"

#ifdef HAVE_NBGL
#include "nbgl_use_case.h"
#endif

tx_hash_context_t tx_hash_context;
tx_context_t tx_context;
bool should_display_esdt_flow;

static uint8_t set_result_signature() {
    uint8_t tx = 0;
    const uint8_t sig_size = 64;
    G_io_apdu_buffer[tx++] = sig_size;
    memmove(G_io_apdu_buffer + tx, tx_context.signature, sig_size);
    tx += sig_size;
    return tx;
}

static bool sign_tx_hash(uint8_t *data_buffer) {
    cx_ecfp_private_key_t private_key;
    bool success = true;
    int ret_code = 0;

    if (!get_private_key(bip32_account, bip32_address_index, &private_key)) {
        return false;
    }

    cx_hash_no_throw((cx_hash_t *) &sha3_context,
                     CX_LAST,
                     data_buffer,
                     0,
                     tx_hash_context.hash,
                     32);
    ret_code = cx_eddsa_sign_no_throw(&private_key,
                                      CX_SHA512,
                                      tx_hash_context.hash,
                                      32,
                                      tx_context.signature,
                                      64);
    if (ret_code != 0) {
        success = false;
    }
    explicit_bzero(&private_key, sizeof(private_key));

    return success;
}

static bool is_esdt_transfer() {
    bool identifier_len_valid = esdt_info.identifier_len > 0;
    bool has_data = strlen(tx_context.data) > 0;
    bool has_esdt_transfer_prefix = false;
    bool has_registered_esdt_identifier = false;
    bool next_char_after_identifier_is_at_separator = false;
    bool same_chainid = false;

    if (!esdt_info.valid) {
        return false;
    }

    if (has_data && identifier_len_valid) {
        has_esdt_transfer_prefix = strncmp(tx_context.data + DATA_SIZE_LEN - 1,
                                           ESDT_TRANSFER_PREFIX,
                                           ESDT_TRANSFER_PREFIX_LENGTH) == 0;
        has_registered_esdt_identifier =
            strncmp(tx_context.data + DATA_SIZE_LEN - 1 + ESDT_TRANSFER_PREFIX_LENGTH,
                    esdt_info.identifier,
                    esdt_info.identifier_len) == 0;

        size_t identifier_end_position =
            DATA_SIZE_LEN - 1 + ESDT_TRANSFER_PREFIX_LENGTH + esdt_info.identifier_len;
        next_char_after_identifier_is_at_separator =
            (strlen(tx_context.data) > identifier_end_position) &&
            (tx_context.data[identifier_end_position] == '@');

        same_chainid = strncmp(tx_context.chain_id, esdt_info.chain_id, MAX_CHAINID_LEN) == 0;
    }

    return has_esdt_transfer_prefix && has_registered_esdt_identifier &&
           next_char_after_identifier_is_at_separator && same_chainid;
}

#if defined(TARGET_STAX)

static nbgl_layoutTagValueList_t layout;
static nbgl_layoutTagValue_t pairs_list[6];  // 6 info max for ESDT and 6 info max for EGLD

static const nbgl_pageInfoLongPress_t review_final_long_press = {
    .text = "Sign transaction on\n" APPNAME " network?",
    .icon = &C_icon_multiversx_logo_64x64,
    .longPressText = "Hold to sign",
    .longPressToken = 0,
    .tuneId = TUNE_TAP_CASUAL,
};

static void review_final_callback(bool confirmed) {
    if (confirmed) {
        int tx = set_result_signature();
        send_response(tx, true, false);
        nbgl_useCaseStatus("TRANSACTION\nSIGNED", true, ui_idle);
    } else {
        nbgl_reject_transaction_choice();
    }
}

static void update_pair(nbgl_layoutTagValue_t *pair, const char *item, const char *value) {
    pair->item = item;
    pair->value = value;
}

static void start_review(void) {
    uint8_t step = 0;

    if (should_display_esdt_flow) {
        update_pair(&pairs_list[step++], "Token", esdt_info.ticker);
        update_pair(&pairs_list[step++], "Value", tx_context.amount);
        update_pair(&pairs_list[step++], "Receiver", tx_context.receiver);
        update_pair(&pairs_list[step++], "Fee", tx_context.fee);
        if (strlen(tx_context.guardian) > 0) {
            update_pair(&pairs_list[step++], "Guardian", tx_context.guardian);
        }
        update_pair(&pairs_list[step++], "Network", tx_context.network);
    } else {
        update_pair(&pairs_list[step++], "Receiver", tx_context.receiver);
        update_pair(&pairs_list[step++], "Amount", tx_context.amount);
        update_pair(&pairs_list[step++], "Fee", tx_context.fee);
        if (tx_context.data_size > 0) {
            update_pair(&pairs_list[step++], "Data", tx_context.data);
        }
        if (strlen(tx_context.guardian) > 0) {
            update_pair(&pairs_list[step++], "Guardian", tx_context.guardian);
        }
        update_pair(&pairs_list[step++], "Network", tx_context.network);
    }

    layout.nbMaxLinesForValue = 0;
    layout.smallCaseForValue = true;
    layout.wrapping = true;
    layout.pairs = pairs_list;
    layout.nbPairs = step;

    nbgl_useCaseStaticReview(&layout,
                             &review_final_long_press,
                             "Reject transaction",
                             review_final_callback);
}

static void ui_sign_tx_hash_nbgl(void) {
    if (should_display_esdt_flow) {
        nbgl_useCaseReviewStart(&C_icon_multiversx_logo_64x64,
                                "Review transaction to\nsend ESDT on\n" APPNAME " network",
                                "",
                                "Reject transaction",
                                start_review,
                                nbgl_reject_transaction_choice);
    } else {
        nbgl_useCaseReviewStart(&C_icon_multiversx_logo_64x64,
                                "Review transaction to\nsend EGLD on\n" APPNAME " network",
                                "",
                                "Reject transaction",
                                start_review,
                                nbgl_reject_transaction_choice);
    }
}

#else

const ux_flow_step_t *tx_flow[TX_SIGN_FLOW_SIZE];
const ux_flow_step_t *esdt_flow[ESDT_TRANSFER_FLOW_SIZE];

// UI for confirming the ESDT transfer on screen
UX_STEP_NOCB(ux_transfer_esdt_flow_24_step,
             bnnn_paging,
             {
                 .title = "Token",
                 .text = esdt_info.ticker,
             });
UX_STEP_NOCB(ux_transfer_esdt_flow_25_step,
             bnnn_paging,
             {
                 .title = "Value",
                 .text = tx_context.amount,
             });
UX_STEP_NOCB(ux_transfer_esdt_flow_26_step,
             bnnn_paging,
             {
                 .title = "Receiver",
                 .text = tx_context.receiver,
             });
UX_STEP_NOCB(ux_transfer_esdt_flow_27_step,
             bnnn_paging,
             {
                 .title = "Fee",
                 .text = tx_context.fee,
             });
UX_STEP_NOCB(ux_transfer_esdt_flow_31_step,
             bnnn_paging,
             {
                 .title = "Guardian",
                 .text = tx_context.guardian,
             });
UX_STEP_NOCB(ux_transfer_esdt_flow_28_step,
             bnnn_paging,
             {
                 .title = "Network",
                 .text = tx_context.network,
             });
UX_STEP_VALID(ux_transfer_esdt_flow_29_step,
              pb,
              send_response(set_result_signature(), true, true),
              {
                  &C_icon_validate_14,
                  "Confirm transfer",
              });
UX_STEP_VALID(ux_transfer_esdt_flow_30_step,
              pb,
              send_response(0, false, true),
              {
                  &C_icon_crossmark,
                  "Reject",
              });

// UI for confirming the tx details of the transaction on screen
UX_STEP_NOCB(ux_sign_tx_hash_flow_17_step,
             bnnn_paging,
             {
                 .title = "Receiver",
                 .text = tx_context.receiver,
             });
UX_STEP_NOCB(ux_sign_tx_hash_flow_18_step,
             bnnn_paging,
             {
                 .title = "Amount",
                 .text = tx_context.amount,
             });
UX_STEP_NOCB(ux_sign_tx_hash_flow_19_step,
             bnnn_paging,
             {
                 .title = "Fee",
                 .text = tx_context.fee,
             });
UX_STEP_NOCB(ux_sign_tx_hash_flow_20_step,
             bnnn_paging,
             {
                 .title = "Data",
                 .text = tx_context.data,
             });
UX_STEP_NOCB(ux_sign_tx_hash_flow_24_step,
             bnnn_paging,
             {
                 .title = "Guardian",
                 .text = tx_context.guardian,
             });
UX_STEP_NOCB(ux_sign_tx_hash_flow_21_step,
             bnnn_paging,
             {
                 .title = "Network",
                 .text = tx_context.network,
             });
UX_STEP_VALID(ux_sign_tx_hash_flow_22_step,
              pb,
              send_response(set_result_signature(), true, true),
              {
                  &C_icon_validate_14,
                  "Sign transaction",
              });
UX_STEP_VALID(ux_sign_tx_hash_flow_23_step,
              pb,
              send_response(0, false, true),
              {
                  &C_icon_crossmark,
                  "Reject",
              });

static void display_tx_sign_flow() {
    uint8_t step = 0;

    tx_flow[step++] = &ux_sign_tx_hash_flow_17_step;
    tx_flow[step++] = &ux_sign_tx_hash_flow_18_step;
    tx_flow[step++] = &ux_sign_tx_hash_flow_19_step;
    if (tx_context.data_size > 0) {
        tx_flow[step++] = &ux_sign_tx_hash_flow_20_step;
    }
    if (strlen(tx_context.guardian) > 0) {
        tx_flow[step++] = &ux_sign_tx_hash_flow_24_step;
    }
    tx_flow[step++] = &ux_sign_tx_hash_flow_21_step;
    tx_flow[step++] = &ux_sign_tx_hash_flow_22_step;
    tx_flow[step++] = &ux_sign_tx_hash_flow_23_step;
    tx_flow[step++] = FLOW_END_STEP;

    ux_flow_init(0, tx_flow, NULL);
}

static void display_esdt_flow() {
    uint8_t step = 0;

    esdt_flow[step++] = &ux_transfer_esdt_flow_24_step;
    esdt_flow[step++] = &ux_transfer_esdt_flow_25_step;
    esdt_flow[step++] = &ux_transfer_esdt_flow_26_step;
    esdt_flow[step++] = &ux_transfer_esdt_flow_27_step;
    if (strlen(tx_context.guardian) > 0) {
        esdt_flow[step++] = &ux_transfer_esdt_flow_31_step;
    }
    esdt_flow[step++] = &ux_transfer_esdt_flow_28_step;
    esdt_flow[step++] = &ux_transfer_esdt_flow_29_step;
    esdt_flow[step++] = &ux_transfer_esdt_flow_30_step;
    esdt_flow[step++] = FLOW_END_STEP;

    ux_flow_init(0, esdt_flow, NULL);
}

#endif

void init_tx_context() {
    tx_context.amount[0] = 0;
    tx_context.data[0] = 0;
    tx_context.data_size = 0;
    tx_context.fee[0] = 0;
    tx_context.gas_limit = 0;
    tx_context.gas_price = 0;
    tx_context.receiver[0] = 0;
    tx_context.chain_id[0] = 0;
    tx_context.esdt_value[0] = 0;
    tx_context.network[0] = 0;
    tx_context.guardian[0] = 0;
    tx_hash_context.status = JSON_IDLE;
    cx_keccak_init_no_throw(&sha3_context, SHA3_KECCAK_BITS);

    app_state = APP_STATE_IDLE;
}

void handle_sign_tx_hash(uint8_t p1,
                         uint8_t *data_buffer,
                         uint16_t data_length,
                         volatile unsigned int *flags) {
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

    cx_hash_no_throw((cx_hash_t *) &sha3_context, 0, data_buffer, data_length, NULL, 0);
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

    should_display_esdt_flow = false;
    if (is_esdt_transfer()) {
        uint16_t res;
        res = parse_esdt_data();
        if (res != MSG_OK) {
            THROW(res);
        }
        should_display_esdt_flow = true;
    }

    app_state = APP_STATE_IDLE;

#if defined(TARGET_STAX)
    ui_sign_tx_hash_nbgl();
#else
    if (should_display_esdt_flow) {
        display_esdt_flow();
    } else {
        display_tx_sign_flow();
    }
#endif

    *flags |= IO_ASYNCH_REPLY;
}
