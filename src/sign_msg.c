#include "sign_msg.h"
#include "get_private_key.h"
#include "utils.h"
#include "menu.h"
#include "parse_tx.h"

#ifdef HAVE_NBGL
#include "nbgl_use_case.h"
#endif

typedef struct {
    uint32_t len;
    uint8_t hash[HASH_LEN];
    char strhash[2 * HASH_LEN + 1];
    char message[MAX_DISPLAY_MESSAGE_SIZE + 1];
    uint16_t message_received_length;
    uint8_t signature[MESSAGE_SIGNATURE_LEN];
} msg_context_t;

static msg_context_t msg_context;

void init_msg_context(void) {
    bip32_account = 0;
    bip32_address_index = 0;

    app_state = APP_STATE_IDLE;
}

static uint8_t set_result_signature() {
    uint8_t tx = 0;
    G_io_apdu_buffer[tx++] = MESSAGE_SIGNATURE_LEN;
    memmove(G_io_apdu_buffer + tx, msg_context.signature, MESSAGE_SIGNATURE_LEN);
    tx += MESSAGE_SIGNATURE_LEN;
    return tx;
}

#if defined(TARGET_STAX) || defined(TARGET_FLEX)

static nbgl_contentTagValueList_t content;
static nbgl_contentTagValue_t content_pairs_list[2];

static void review_final_callback(bool confirmed) {
    if (confirmed) {
        int tx = set_result_signature();
        send_response(tx, true, false);
        nbgl_useCaseStatus("Message\nsigned", true, ui_idle);
    } else {
        send_response(0, false, false);
        nbgl_useCaseStatus("Message\nrejected", false, ui_idle);
    }
}

static void make_content_list(void) {
    uint8_t step = 0;

    content_pairs_list[step].item = "Hash";
    content_pairs_list[step++].value = msg_context.strhash;
    content_pairs_list[step].item = "Message";
    content_pairs_list[step++].value = msg_context.message;

    content.pairs = content_pairs_list;
    content.callback = NULL;
    content.nbPairs = step;
    content.startIndex = 0;
    content.nbMaxLinesForValue = 2;
    content.token = 0;
    content.smallCaseForValue = false;
    content.wrapping = true;
    content.actionCallback = NULL;
}

static void ui_sign_message_nbgl(void) {
    make_content_list();
    if (found_non_printable_chars) {
        nbgl_useCaseReviewBlindSigning(TYPE_MESSAGE,
                                       &content,
                                       &C_icon_multiversx_logo_64x64,
                                       "Review message to\nsign on " APPNAME "\nnetwork",
                                       "",
                                       "Accept risk and sign message?",
                                       NULL,
                                       review_final_callback);
    } else {
        nbgl_useCaseReview(TYPE_MESSAGE,
                           &content,
                           &C_icon_multiversx_logo_64x64,
                           "Review message to\nsign on " APPNAME "\nnetwork",
                           "",
                           "Sign message on\n" APPNAME " network?",
                           review_final_callback);
    }
}

#else

// UI for confirming the message hash on screen
UX_STEP_NOCB(ux_sign_msg_flow_14_step,
             bnnn_paging,
             {
                 .title = "Hash",
                 .text = msg_context.strhash,
             });
UX_STEP_NOCB(ux_sign_msg_flow_15_step,
             bnnn_paging,
             {
                 .title = "Message",
                 .text = msg_context.message,
             });
UX_STEP_VALID(ux_sign_msg_flow_16_step,
              pb,
              send_response(set_result_signature(), true, true),
              {
                  &C_icon_validate_14,
                  "Sign message",
              });
UX_STEP_VALID(ux_sign_msg_flow_17_step,
              pb,
              send_response(0, false, true),
              {
                  &C_icon_crossmark,
                  "Reject",
              });

UX_FLOW(ux_sign_msg_flow,
        &ux_sign_msg_flow_14_step,
        &ux_sign_msg_flow_15_step,
        &ux_sign_msg_flow_16_step,
        &ux_sign_msg_flow_17_step);

// UI for blind signing
UX_STEP_CB(ux_warning_error_blind_signing_msg_1_step,
           bnnn_paging,
           ui_idle(),
           {
               "Blind signing disabled",
               "Enable in Settings",
           });

UX_STEP_VALID(ux_warning_error_blind_signing_msg_2_step,
              pb,
              send_response(0, false, true),
              {
                  &C_icon_crossmark,
                  "Back",
              });

UX_STEP_NOCB(ux_warning_blind_signing_msg_ahead_step,
             pb,
             {
                 &C_icon_warning,
                 "Blind signing",
             });

UX_STEP_NOCB(ux_warning_accept_blind_signing_msg_step,
             pb,
             {
                 &C_icon_warning,
                 "Accept risk and",
             });

UX_FLOW(ux_error_blind_signing_disabled_msg_flow,
        &ux_warning_error_blind_signing_msg_1_step,
        &ux_warning_error_blind_signing_msg_2_step);

UX_FLOW(ux_blind_sign_msg_flow,
        &ux_warning_blind_signing_msg_ahead_step,
        &ux_sign_msg_flow_14_step,
        &ux_sign_msg_flow_15_step,
        &ux_warning_accept_blind_signing_msg_step,
        &ux_sign_msg_flow_16_step,
        &ux_sign_msg_flow_17_step);

#endif

static bool verify_message(char *message, size_t len) {
    bool has_non_printable_chars = false;
    for (size_t i = 0; i < len; i++) {
        if ((message[i] > 0 && message[i] < 9) || (message[i] > 13 && message[i] < 32) ||
            message[i] > 126) {
            message[i] = '?';
            has_non_printable_chars = true;
        }
    }
    return has_non_printable_chars;
}

static void process_message(uint8_t *message, size_t data_length) {
    uint16_t length_to_copy =
        MIN(data_length, MAX_DISPLAY_MESSAGE_SIZE - msg_context.message_received_length);
    if (length_to_copy > 0) {
        memcpy(msg_context.message + msg_context.message_received_length, message, length_to_copy);

        bool result = verify_message(msg_context.message + msg_context.message_received_length,
                                     length_to_copy);
        if (result) {
            found_non_printable_chars = true;
        }
    }
    msg_context.message_received_length += data_length;

    if (msg_context.message_received_length > MAX_DISPLAY_MESSAGE_SIZE) {
        char ellipsis[3] = "...";
        int ellipsisLen = strlen(ellipsis);
        memcpy(msg_context.message + MAX_DISPLAY_MESSAGE_SIZE - ellipsisLen, ellipsis, ellipsisLen);
    }
    msg_context.message[MAX_DISPLAY_MESSAGE_SIZE] = '\0';
}

static bool sign_message(void) {
    cx_ecfp_private_key_t private_key;
    bool success = true;
    int ret_code = 0;

    if (!get_private_key(bip32_account, bip32_address_index, &private_key)) {
        return false;
    }

    ret_code = cx_eddsa_sign_no_throw(&private_key,
                                      CX_SHA512,
                                      msg_context.hash,
                                      HASH_LEN,
                                      msg_context.signature,
                                      MESSAGE_SIGNATURE_LEN);
    if (ret_code != 0) {
        success = false;
    }
    explicit_bzero(&private_key, sizeof(private_key));

    return success;
}

void handle_sign_msg(uint8_t p1,
                     uint8_t *data_buffer,
                     uint16_t data_length,
                     volatile unsigned int *flags) {
    /*
       data buffer structure should be:
       <message length> + <message>
              ^             ^
          4 bytes      <message length> bytes

       the message length is computed in the first bulk, while the entire message
       can come in multiple bulks
   */
    int err;

    if (p1 == P1_FIRST) {
        char message_length_str[11];
        // first 4 bytes from data_buffer should be the message length (big endian
        // uint32)
        if (data_length < 4) {
            THROW(ERR_INVALID_MESSAGE);
        }
        app_state = APP_STATE_SIGNING_MESSAGE;
        msg_context.len = U4BE(data_buffer, 0);
        data_buffer += 4;
        data_length -= 4;

        found_non_printable_chars = false;
        msg_context.message_received_length = 0;
        memset(msg_context.message, 0, sizeof(msg_context.message));

        // initialize hash with the constant string to prepend
        err = cx_keccak_init_no_throw(&sha3_context, SHA3_KECCAK_BITS);
        if (err != CX_OK) {
            THROW(err);
        }
        err = cx_hash_no_throw((cx_hash_t *) &sha3_context,
                               0,
                               (uint8_t *) PREPEND,
                               sizeof(PREPEND) - 1,
                               NULL,
                               0);
        if (err != CX_OK) {
            THROW(err);
        }

        // convert message length to string and store it in the variable
        // `message_length_str`
        uint32_t_to_char_array(msg_context.len, message_length_str);

        // add the message length to the hash
        err = cx_hash_no_throw((cx_hash_t *) &sha3_context,
                               0,
                               (uint8_t *) message_length_str,
                               strlen(message_length_str),
                               NULL,
                               0);
        if (err != CX_OK) {
            THROW(err);
        }
    } else {
        if (p1 != P1_MORE) {
            THROW(ERR_INVALID_P1);
        }
        if (app_state != APP_STATE_SIGNING_MESSAGE) {
            THROW(ERR_INVALID_MESSAGE);
        }
    }
    if (data_length > msg_context.len) {
        THROW(ERR_MESSAGE_TOO_LONG);
    }

    // add the received message part to the message buffer
    process_message(data_buffer, data_length);

    // add the received message part to the hash and decrease the remaining length
    err = cx_hash_no_throw((cx_hash_t *) &sha3_context, 0, data_buffer, data_length, NULL, 0);
    if (err != CX_OK) {
        THROW(err);
    }
    msg_context.len -= data_length;
    if (msg_context.len != 0) {
        THROW(MSG_OK);
    }

    // finalize hash, compute it and store it in `msg_context.strhash` for display
    err = cx_hash_no_throw((cx_hash_t *) &sha3_context,
                           CX_LAST,
                           data_buffer,
                           0,
                           msg_context.hash,
                           HASH_LEN);
    if (err != CX_OK) {
        THROW(err);
    }

    convert_to_hex_str(msg_context.strhash,
                       sizeof(msg_context.strhash),
                       msg_context.hash,
                       sizeof(msg_context.hash));

    // sign the hash
    if (!sign_message()) {
        init_msg_context();
        THROW(ERR_SIGNATURE_FAILED);
    }

    app_state = APP_STATE_IDLE;

#if defined(TARGET_STAX) || defined(TARGET_FLEX)
    if (found_non_printable_chars && N_storage.setting_blind_signing == 0) {
        disabled_blind_signing_msg_warn();
    } else {
        ui_sign_message_nbgl();
    }
#else
    if (found_non_printable_chars && N_storage.setting_blind_signing == 0) {
        ux_flow_init(0, ux_error_blind_signing_disabled_msg_flow, NULL);
    } else {
        if (found_non_printable_chars) {
            ux_flow_init(0, ux_blind_sign_msg_flow, NULL);
        } else {
            ux_flow_init(0, ux_sign_msg_flow, NULL);
        }
    }
#endif
    *flags |= IO_ASYNCH_REPLY;
}
