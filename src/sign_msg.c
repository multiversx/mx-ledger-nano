#include "sign_msg.h"
#include "get_private_key.h"
#include "utils.h"
#include "menu.h"

#ifdef HAVE_NBGL
#include "nbgl_use_case.h"
#endif

typedef struct {
    uint32_t len;
    uint8_t hash[HASH_LEN];
    char strhash[2 * HASH_LEN + 1];
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

#if defined(TARGET_STAX)

static nbgl_layoutTagValueList_t layout;
static nbgl_layoutTagValue_t pairs_list[1];

static const nbgl_pageInfoLongPress_t review_final_long_press = {
    .text = "Sign message on\n" APPNAME " network?",
    .icon = &C_icon_multiversx_logo_64x64,
    .longPressText = "Hold to sign",
    .longPressToken = 0,
    .tuneId = TUNE_TAP_CASUAL,
};

static void review_final_callback(bool confirmed) {
    if (confirmed) {
        int tx = set_result_signature();
        send_response(tx, true, false);
        nbgl_useCaseStatus("MESSAGE\nSIGNED", true, ui_idle);
    } else {
        nbgl_reject_message_choice();
    }
}

static void start_review(void) {
    layout.nbMaxLinesForValue = 0;
    layout.smallCaseForValue = false;
    layout.wrapping = true;
    layout.pairs = pairs_list;
    pairs_list[0].item = "hash";
    pairs_list[0].value = msg_context.strhash;
    layout.nbPairs = ARRAY_COUNT(pairs_list);

    nbgl_useCaseStaticReview(&layout,
                             &review_final_long_press,
                             "Reject message",
                             review_final_callback);
}

static void ui_sign_message_nbgl(void) {
    nbgl_useCaseReviewStart(&C_icon_multiversx_logo_64x64,
                            "Review message to\nsign on " APPNAME "\nnetwork",
                            "",
                            "Reject message",
                            start_review,
                            nbgl_reject_message_choice);
}

#else

// UI for confirming the message hash on screen
UX_STEP_NOCB(ux_sign_msg_flow_14_step,
             bnnn_paging,
             {
                 .title = "Hash",
                 .text = msg_context.strhash,
             });
UX_STEP_VALID(ux_sign_msg_flow_15_step,
              pb,
              send_response(set_result_signature(), true, true),
              {
                  &C_icon_validate_14,
                  "Sign message",
              });
UX_STEP_VALID(ux_sign_msg_flow_16_step,
              pb,
              send_response(0, false, true),
              {
                  &C_icon_crossmark,
                  "Reject",
              });

UX_FLOW(ux_sign_msg_flow,
        &ux_sign_msg_flow_14_step,
        &ux_sign_msg_flow_15_step,
        &ux_sign_msg_flow_16_step);

#endif

static bool sign_message(void) {
    cx_ecfp_private_key_t private_key;
    bool success = true;

    if (!get_private_key(bip32_account, bip32_address_index, &private_key)) {
        return false;
    }

    BEGIN_TRY {
        TRY {
            cx_eddsa_sign(&private_key,
                          CX_RND_RFC6979 | CX_LAST,
                          CX_SHA512,
                          msg_context.hash,
                          HASH_LEN,
                          NULL,
                          0,
                          msg_context.signature,
                          MESSAGE_SIGNATURE_LEN,
                          NULL);
        }
        CATCH_ALL {
            success = false;
        }
        FINALLY {
            explicit_bzero(&private_key, sizeof(private_key));
        }
    }
    END_TRY;

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
        // initialize hash with the constant string to prepend
        cx_keccak_init(&sha3_context, SHA3_KECCAK_BITS);
        cx_hash((cx_hash_t *) &sha3_context, 0, (uint8_t *) PREPEND, sizeof(PREPEND) - 1, NULL, 0);

        // convert message length to string and store it in the variable
        // `message_length_str`
        uint32_t_to_char_array(msg_context.len, message_length_str);

        // add the message length to the hash
        cx_hash((cx_hash_t *) &sha3_context,
                0,
                (uint8_t *) message_length_str,
                strlen(message_length_str),
                NULL,
                0);
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

    // add the received message part to the hash and decrease the remaining length
    cx_hash((cx_hash_t *) &sha3_context, 0, data_buffer, data_length, NULL, 0);
    msg_context.len -= data_length;
    if (msg_context.len != 0) {
        THROW(MSG_OK);
    }

    // finalize hash, compute it and store it in `msg_context.strhash` for display
    cx_hash((cx_hash_t *) &sha3_context, CX_LAST, data_buffer, 0, msg_context.hash, HASH_LEN);
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

#if defined(TARGET_STAX)
    ui_sign_message_nbgl();
#else
    ux_flow_init(0, ux_sign_msg_flow, NULL);
#endif
    *flags |= IO_ASYNCH_REPLY;
}
