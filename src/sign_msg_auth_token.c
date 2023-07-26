#include "sign_msg_auth_token.h"
#include "address_helpers.h"
#include "get_private_key.h"
#include "utils.h"
#include "menu.h"

#ifdef HAVE_NBGL
#include "nbgl_use_case.h"
#endif

#define PARSED_TOKEN_ORIGIN    (token_auth_context.dot_count == 1)
#define PARSED_TOKEN_BLOCKHASH (token_auth_context.dot_count == 2)
#define PARSED_TOKEN_TTL       (token_auth_context.dot_count == 3)

typedef struct {
    char address[BECH32_ADDRESS_LEN];
    uint32_t len;
    uint8_t hash[HASH_LEN];
    uint8_t signature[MESSAGE_SIGNATURE_LEN];
    char token[AUTH_TOKEN_DISPLAY_MAX_SIZE + 1];
    char auth_token_buffer[AUTH_TOKEN_ENCODED_ORIGIN_MAX_SIZE];
    char auth_origin[AUTH_TOKEN_ENCODED_ORIGIN_MAX_SIZE];
    char auth_ttl[AUTH_TOKEN_ENCODED_TTL_MAX_SIZE];
    int dot_count;
    bool stop_origin_ttl_fetch;
} token_auth_context_t;

static token_auth_context_t token_auth_context;

static uint8_t set_result_auth_token(void) {
    uint8_t tx = 0;
    char complete_response[BECH32_ADDRESS_LEN + MESSAGE_SIGNATURE_LEN + 1];  // <addresssignature>
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
    layout.smallCaseForValue = true;
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
                            "Review auth token to\nsign on " APPNAME "\nnetwork",
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

static void clean_token_fields(void) {
    token_auth_context.len = 0;
    token_auth_context.dot_count = 0;
    explicit_bzero(token_auth_context.auth_token_buffer,
                   sizeof(token_auth_context.auth_token_buffer));
    explicit_bzero(token_auth_context.auth_origin, sizeof(token_auth_context.auth_origin));
    explicit_bzero(token_auth_context.auth_ttl, sizeof(token_auth_context.auth_ttl));
    explicit_bzero(token_auth_context.token, sizeof(token_auth_context.token));
    explicit_bzero(token_auth_context.hash, sizeof(token_auth_context.hash));
    explicit_bzero(token_auth_context.address, sizeof(token_auth_context.address));
    token_auth_context.stop_origin_ttl_fetch = false;
}

static void init_auth_token_context(void) {
    bip32_account = 0;
    bip32_address_index = 0;

    clean_token_fields();

    app_state = APP_STATE_IDLE;
}

static void move_value_from_buffer(char *buffer,
                                   int buffer_size,
                                   char *destination,
                                   int destination_size,
                                   bool *should_stop_processing) {
    if ((int) (strlen(buffer)) >= destination_size) {
        *should_stop_processing = true;
        explicit_bzero(destination, destination_size);
        explicit_bzero(buffer, buffer_size);
        return;
    }

    memmove(destination, buffer, strlen(buffer));
    explicit_bzero(buffer, buffer_size);
    *should_stop_processing = false;
}

static void handle_auth_token_data(uint8_t const *data_buffer, uint8_t data_length) {
    if (token_auth_context.stop_origin_ttl_fetch) {
        return;
    }

    /*
    This function parses the auth token char by char and extracts the origin and ttl.
    An auth token looks like this. We need to parse the first and the third element and save
    them

    Example:
    bG9jYWxob3N0.f68177510756edce45eca84b94544a6eacdfa36e69dfd3b8f24c4010d1990751.300.eyJ0aW1lc3RhbXAiOjE2NzM5NzIyNDR9
         ^                                                                         ^
      localhost                                                                 300 sec
    */
    for (uint8_t i = 0; i < data_length; i++) {
        if (data_buffer[i] != '.') {
            if (token_auth_context.dot_count == 1) {
                // ignore the second part of the token since we are not interested in it
                continue;
            }

            size_t buffer_len = strlen(token_auth_context.auth_token_buffer);

            if (buffer_len >= AUTH_TOKEN_ENCODED_ORIGIN_MAX_SIZE - 2) {
                // we've reached the max length of the origin
                token_auth_context.stop_origin_ttl_fetch = true;
                return;
            }
            // add the current char to the buffer
            token_auth_context.auth_token_buffer[buffer_len] = data_buffer[i];
            token_auth_context.auth_token_buffer[buffer_len + 1] = '\0';
        } else {
            token_auth_context.dot_count++;
            if (PARSED_TOKEN_ORIGIN) {
                move_value_from_buffer(token_auth_context.auth_token_buffer,
                                       sizeof(token_auth_context.auth_token_buffer),
                                       token_auth_context.auth_origin,
                                       sizeof(token_auth_context.auth_origin),
                                       &token_auth_context.stop_origin_ttl_fetch);
                if (token_auth_context.stop_origin_ttl_fetch) {
                    return;
                }
            }

            if (PARSED_TOKEN_BLOCKHASH) {
                continue;
            }

            if (PARSED_TOKEN_TTL) {
                move_value_from_buffer(token_auth_context.auth_token_buffer,
                                       sizeof(token_auth_context.auth_token_buffer),
                                       token_auth_context.auth_ttl,
                                       sizeof(token_auth_context.auth_ttl),
                                       &token_auth_context.stop_origin_ttl_fetch);
                if (token_auth_context.stop_origin_ttl_fetch) {
                    return;
                }
            }
        }
    }
}

static void update_token_display_data(uint8_t const *data_buffer, uint8_t const data_length) {
    handle_auth_token_data(data_buffer, data_length);
    if (strlen(token_auth_context.token) >= AUTH_TOKEN_DISPLAY_MAX_SIZE) {
        return;
    }

    int num_chars_to_show = data_length;
    bool should_append_ellipsis = false;
    if (data_length >= AUTH_TOKEN_DISPLAY_MAX_SIZE) {
        num_chars_to_show = AUTH_TOKEN_DISPLAY_MAX_SIZE;
        should_append_ellipsis = true;
    }

    memmove(token_auth_context.token, data_buffer, num_chars_to_show);
    token_auth_context.token[num_chars_to_show] = '\0';

    if (should_append_ellipsis) {
        // add "..." at the end to show that the data field is actually longer
        char ellipsis[] = "...";
        int ellipsisLen = strlen(ellipsis);
        memmove(token_auth_context.token + AUTH_TOKEN_DISPLAY_MAX_SIZE - ellipsisLen,
                ellipsis,
                ellipsisLen);
        return;
    }
}

static bool sign_auth_token(void) {
    cx_ecfp_private_key_t private_key;
    bool success = true;

    if (!get_private_key(bip32_account, bip32_address_index, &private_key)) {
        return false;
    }

    int ret_code = cx_eddsa_sign_no_throw(&private_key,
                                          CX_SHA512,
                                          token_auth_context.hash,
                                          HASH_LEN,
                                          token_auth_context.signature,
                                          MESSAGE_SIGNATURE_LEN);
    if (ret_code != 0) {
        success = false;
    }
    explicit_bzero(&private_key, sizeof(private_key));

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
        clean_token_fields();
        token_auth_context.token[0] = '\0';
        char token_length_str[11];

        // check that the indexes and the length are valid
        if (data_length < AUTH_TOKEN_ADDRESS_INDICES_SIZE + AUTH_TOKEN_TOKEN_LEN_FIELD_SIZE) {
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
        data_buffer += AUTH_TOKEN_ADDRESS_INDICES_SIZE;
        data_length -= AUTH_TOKEN_ADDRESS_INDICES_SIZE;

        token_auth_context.len = U4BE(data_buffer, 0);

        // the token length (4 bytes) has been read, so skip the next 4 bytes
        data_buffer += AUTH_TOKEN_TOKEN_LEN_FIELD_SIZE;
        data_length -= AUTH_TOKEN_TOKEN_LEN_FIELD_SIZE;

        update_token_display_data(data_buffer, data_length);

        // initialize hash with the constant string to prepend
        cx_keccak_init_no_throw(&sha3_context, SHA3_KECCAK_BITS);
        cx_hash_no_throw((cx_hash_t *) &sha3_context,
                         0,
                         (uint8_t *) PREPEND,
                         sizeof(PREPEND) - 1,
                         NULL,
                         0);

        // convert message length to string and store it in the variable `tmp`
        uint32_t full_message_len = token_auth_context.len + BECH32_ADDRESS_LEN;
        uint32_t_to_char_array(full_message_len, token_length_str);

        // add the message length to the hash
        cx_hash_no_throw((cx_hash_t *) &sha3_context,
                         0,
                         (uint8_t *) token_length_str,
                         strlen(token_length_str),
                         NULL,
                         0);

        // add the message length to the hash
        cx_hash_no_throw((cx_hash_t *) &sha3_context,
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
    cx_hash_no_throw((cx_hash_t *) &sha3_context, 0, data_buffer, data_length, NULL, 0);

    token_auth_context.len -= data_length;
    if (token_auth_context.len != 0) {
        THROW(MSG_OK);
    }

    // finalize hash and compute it
    cx_hash_no_throw((cx_hash_t *) &sha3_context,
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

    char display[AUTH_TOKEN_DISPLAY_MAX_SIZE];
    int ret_code = compute_token_display(token_auth_context.auth_origin,
                                         token_auth_context.auth_ttl,
                                         display,
                                         AUTH_TOKEN_DISPLAY_MAX_SIZE);
    if (ret_code == 0) {
        memmove(token_auth_context.token, display, strlen(display));
        token_auth_context.token[strlen(display)] = '\0';
    } else if (ret_code == AUTH_TOKEN_BAD_REQUEST_RET_CODE) {
        THROW(ERR_INVALID_MESSAGE);
    }

    app_state = APP_STATE_IDLE;

#if defined(TARGET_STAX)
    ui_sign_message_auth_token_nbgl();
#else
    ux_flow_init(0, ux_auth_token_msg_flow, NULL);
#endif
    *flags |= IO_ASYNCH_REPLY;
}
