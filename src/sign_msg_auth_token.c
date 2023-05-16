#include "sign_msg_auth_token.h"
#include "address_helpers.h"
#include "get_private_key.h"
#include "utils.h"
#include "menu.h"

#ifdef HAVE_NBGL
#include "nbgl_use_case.h"
#endif

typedef struct {
    char address[BECH32_ADDRESS_LEN];
    uint32_t len;
    uint8_t hash[HASH_LEN];
    uint8_t signature[MESSAGE_SIGNATURE_LEN];
    char token[MAX_DISPLAY_DATA_SIZE];
} token_auth_context_t;

static token_auth_context_t token_auth_context;

static uint8_t set_result_auth_token(void) {
    uint8_t tx = 0;
    char complete_response[strlen(token_auth_context.address) +
                           MESSAGE_SIGNATURE_LEN];  // <addresssignature>
    memmove(complete_response, token_auth_context.address, strlen(token_auth_context.address));
    memmove(complete_response + strlen(token_auth_context.address),
            token_auth_context.signature,
            MESSAGE_SIGNATURE_LEN);
    const uint8_t response_size = strlen(token_auth_context.address) + MESSAGE_SIGNATURE_LEN;

    G_io_apdu_buffer[tx++] = response_size;
    memmove(G_io_apdu_buffer + tx, complete_response, response_size);
    tx += response_size;
    return tx;
}

#if defined(TARGET_STAX)

static nbgl_layoutTagValueList_t layout;
static nbgl_layoutTagValue_t pairs_list[2];

static const nbgl_pageInfoLongPress_t review_final_long_press = {
    .text = "Sign message on\n" APPNAME " network?",
    .icon = &C_icon_multiversx_logo_64x64,
    .longPressText = "Hold to sign",
    .longPressToken = 0,
    .tuneId = TUNE_TAP_CASUAL,
};

static void review_final_callback(bool confirmed) {
    if (confirmed) {
        int tx = set_result_auth_token();
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
    pairs_list[0].item = "Address";
    pairs_list[0].value = token_auth_context.address;
    pairs_list[1].item = "Auth Token";
    pairs_list[1].value = token_auth_context.token;
    layout.nbPairs = ARRAY_COUNT(pairs_list);

    nbgl_useCaseStaticReview(&layout,
                             &review_final_long_press,
                             "Reject message",
                             review_final_callback);
}

static void ui_sign_message_auth_token_nbgl(void) {
    nbgl_useCaseReviewStart(&C_icon_multiversx_logo_64x64,
                            "Review message to\nsign on " APPNAME "\nnetwork",
                            "",
                            "Reject message",
                            start_review,
                            nbgl_reject_message_choice);
}

#else

static bool sign_auth_token(void);

// UI for confirming the message hash on screen
UX_STEP_NOCB(ux_auth_token_msg_flow_33_step,
             bnnn_paging,
             {
                 .title = "Address",
                 .text = token_auth_context.address,
             });
UX_STEP_NOCB(ux_auth_token_msg_flow_34_step,
             bnnn_paging,
             {
                 .title = "Auth Token",
                 .text = token_auth_context.token,
             });
UX_STEP_VALID(ux_auth_token_msg_flow_35_step,
              pb,
              send_response(set_result_auth_token(), true, true),
              {
                  &C_icon_validate_14,
                  "Authorize",
              });
UX_STEP_VALID(ux_auth_token_msg_flow_36_step,
              pb,
              send_response(0, false, true),
              {
                  &C_icon_crossmark,
                  "Reject",
              });

UX_FLOW(ux_auth_token_msg_flow,
        &ux_auth_token_msg_flow_33_step,
        &ux_auth_token_msg_flow_34_step,
        &ux_auth_token_msg_flow_35_step,
        &ux_auth_token_msg_flow_36_step);

#endif

static void init_auth_token_context(void) {
    bip32_account = 0;
    bip32_address_index = 0;

    app_state = APP_STATE_IDLE;
}

static void update_token_display_data(uint8_t const *data_buffer, uint8_t const data_length) {
    if (strlen(token_auth_context.token) >= MAX_DISPLAY_DATA_SIZE) {
        return;
    }

    int num_chars_to_show = data_length;
    bool should_append_ellipsis = false;
    if (data_length >= MAX_DISPLAY_DATA_SIZE) {
        num_chars_to_show = MAX_DISPLAY_DATA_SIZE;
        should_append_ellipsis = true;
    }

    memmove(token_auth_context.token, data_buffer, num_chars_to_show);
    token_auth_context.token[num_chars_to_show] = '\0';

    if (should_append_ellipsis) {
        // Overwrite with "..." at the end to show that the data field is actually longer
        token_auth_context.token[sizeof(token_auth_context.token) - 2] = '.';
        token_auth_context.token[sizeof(token_auth_context.token) - 3] = '.';
        token_auth_context.token[sizeof(token_auth_context.token) - 4] = '.';
        return;
    }
}

static bool sign_auth_token(void) {
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
                          token_auth_context.hash,
                          HASH_LEN,
                          NULL,
                          0,
                          token_auth_context.signature,
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

void handle_auth_token(uint8_t p1,
                       uint8_t *data_buffer,
                       uint16_t data_length,
                       volatile unsigned int *flags) {
    /*
        data buffer structure should be:
        <account index> + <address index> + <token length> + <token>
               ^                 ^                 ^            ^
           4 bytes           4 bytes           4 bytes     <token length> bytes

        the account and address indexes, alongside token length are computed in
        the first bulk, while the entire token can come in multiple bulks
    */
    if (p1 == P1_FIRST) {
        memset(token_auth_context.token, 0, sizeof(token_auth_context.token));
        token_auth_context.token[0] = '\0';
        char token_length_str[11];

        // check that the indexes and the length are valid
        if (data_length < 12) {
            THROW(ERR_INVALID_MESSAGE);
        }

        uint8_t public_key[PUBLIC_KEY_LEN];

        uint32_t const account_index = read_uint32_be(data_buffer);
        uint32_t const address_index = read_uint32_be(data_buffer + sizeof(uint32_t));
        if (!get_public_key(account_index, address_index, public_key)) {
            THROW(ERR_INVALID_ARGUMENTS);
        }

        get_address_bech32_from_binary(public_key, token_auth_context.address);

        app_state = APP_STATE_SIGNING_MESSAGE;

        // account and address indexes (4 bytes each) have been read, so skip the
        // first 8 bytes
        data_buffer += 8;
        data_length -= 8;

        token_auth_context.len = U4BE(data_buffer, 0);

        // the token length (4 bytes) has been read, so skip the next 4 bytes
        data_buffer += 4;
        data_length -= 4;

        update_token_display_data(data_buffer, data_length);

        // initialize hash with the constant string to prepend
        cx_keccak_init(&sha3_context, SHA3_KECCAK_BITS);
        cx_hash((cx_hash_t *) &sha3_context, 0, (uint8_t *) PREPEND, sizeof(PREPEND) - 1, NULL, 0);

        // convert message length to string and store it in the variable `tmp`
        uint32_t full_message_len = token_auth_context.len + BECH32_ADDRESS_LEN;
        uint32_t_to_char_array(full_message_len, token_length_str);

        // add the message length to the hash
        cx_hash((cx_hash_t *) &sha3_context,
                0,
                (uint8_t *) token_length_str,
                strlen(token_length_str),
                NULL,
                0);

        // add the message length to the hash
        cx_hash((cx_hash_t *) &sha3_context,
                0,
                (uint8_t *) token_auth_context.address,
                strlen(token_auth_context.address),
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
    if (data_length > token_auth_context.len) {
        THROW(ERR_MESSAGE_TOO_LONG);
    }

    // add the received message part to the hash and decrease the remaining length
    cx_hash((cx_hash_t *) &sha3_context, 0, data_buffer, data_length, NULL, 0);

    token_auth_context.len -= data_length;
    if (token_auth_context.len != 0) {
        THROW(MSG_OK);
    }

    // finalize hash, compute it and store it in `msg_context.strhash` for display
    cx_hash((cx_hash_t *) &sha3_context,
            CX_LAST,
            data_buffer,
            0,
            token_auth_context.hash,
            HASH_LEN);

    // sign the hash
    if (!sign_auth_token()) {
        init_auth_token_context();
        THROW(ERR_SIGNATURE_FAILED);
    }

    app_state = APP_STATE_IDLE;

#if defined(TARGET_STAX)
    ui_sign_message_auth_token_nbgl();
#else
    ux_flow_init(0, ux_auth_token_msg_flow, NULL);
#endif
    *flags |= IO_ASYNCH_REPLY;
}
