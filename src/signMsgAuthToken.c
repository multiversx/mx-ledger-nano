#include "getPrivateKey.h"
#include "signMsgAuthToken.h"
#include "addressHelpers.h"
#include "utils.h"

typedef struct {
    char address[62];
    uint32_t len;
    uint8_t hash[32];
    uint8_t signature[64];
    char token[MAX_DISPLAY_DATA_SIZE];
} token_auth_context_t;

static token_auth_context_t token_auth_context;

static uint8_t set_result_auth_token();
bool sign_auth_token(void);

// UI for confirming the message hash on screen
UX_STEP_NOCB(
    ux_auth_token_msg_flow_33_step, 
    bnnn_paging, 
    {
      .title = "Address",
      .text = token_auth_context.address,
    });
UX_STEP_NOCB(
    ux_auth_token_msg_flow_34_step, 
    bnnn_paging, 
    {
      .title = "Auth Token",
      .text = token_auth_context.token,
    });    
UX_STEP_VALID(
    ux_auth_token_msg_flow_35_step, 
    pb, 
    send_response(set_result_auth_token(), true),
    {
      &C_icon_validate_14,
      "Authorize",
    });
UX_STEP_VALID(
    ux_auth_token_msg_flow_36_step, 
    pb,
    send_response(0, false),
    {
      &C_icon_crossmark,
      "Reject",
    });

UX_FLOW(ux_auth_token_msg_flow,
  &ux_auth_token_msg_flow_33_step,
  &ux_auth_token_msg_flow_34_step,
  &ux_auth_token_msg_flow_35_step,
  &ux_auth_token_msg_flow_36_step
);

void init_auth_token_context(void) {
    bip32_account = 0;
    bip32_address_index = 0;

    app_state = APP_STATE_IDLE;
}

void update_token_display_data(uint8_t *data_buffer, uint8_t data_length) {
    if(strlen(token_auth_context.token) >= MAX_DISPLAY_DATA_SIZE) {
        return;
    }

    int num_chars_to_show = data_length;
    bool should_append_ellipsis = false;
    if(data_length >= MAX_DISPLAY_DATA_SIZE) {
        num_chars_to_show = MAX_DISPLAY_DATA_SIZE;
        should_append_ellipsis = true;
    }

    memmove(token_auth_context.token, data_buffer, num_chars_to_show);
    token_auth_context.token[num_chars_to_show] = '\0';

    if(should_append_ellipsis) {
        // add "..." at the end to show that the data field is actually longer 
        char ellipsis[] = "...";
        int ellipsisLen = strlen(ellipsis);
        memmove(token_auth_context.token + MAX_DISPLAY_DATA_SIZE - ellipsisLen, ellipsis, ellipsisLen);
        return;
    }
}


static uint8_t set_result_auth_token() {
    uint8_t tx = 0;
    char complete_response[strlen(token_auth_context.address) + 64]; // <addresssignature>
    memmove(complete_response, token_auth_context.address, strlen(token_auth_context.address));
    memmove(complete_response + strlen(token_auth_context.address), token_auth_context.signature, 64);
    const uint8_t response_size = strlen(token_auth_context.address) + 64;

    G_io_apdu_buffer[tx++] = response_size;
    memmove(G_io_apdu_buffer + tx, complete_response, response_size);
    tx += response_size;
    return tx;
}

bool sign_auth_token(void) {
    cx_ecfp_private_key_t privateKey;
    bool success = true;

    if (!getPrivateKey(bip32_account, bip32_address_index, &privateKey)) {
        return false;
    }

    BEGIN_TRY {
        TRY {
            cx_eddsa_sign(&privateKey, CX_RND_RFC6979 | CX_LAST, CX_SHA512, token_auth_context.hash, 32, NULL, 0, token_auth_context.signature, 64, NULL);
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

void handle_auth_token(uint8_t p1, uint8_t p2, uint8_t *data_buffer, uint16_t data_length, volatile unsigned int *flags) {
    if (p1 == P1_FIRST) {
        token_auth_context.token[0] = '\0';
        char tmp[11];
        uint32_t index;
        uint32_t base = 10;
        uint8_t pos = 0;

        uint8_t public_key[32];
        uint32_t account_index, address_index;

        account_index = read_uint32_be(data_buffer);
        address_index = read_uint32_be(data_buffer + sizeof(uint32_t));
        if (!get_public_key(account_index, address_index, public_key)) {
            THROW(ERR_INVALID_ARGUMENTS);
        }

        get_address_bech32_from_binary(public_key, token_auth_context.address);

        app_state = APP_STATE_SIGNING_MESSAGE;
    
        data_buffer += 8;
        data_length -= 8;
 
        token_auth_context.len = U4BE(data_buffer, 0);

        data_buffer += 4;
        data_length -= 4;

        update_token_display_data(data_buffer, data_length);

        // initialize hash with the constant string to prepend
        cx_keccak_init(&sha3_context, 256);
        cx_hash((cx_hash_t *)&sha3_context, 0, (uint8_t*)PREPEND, sizeof(PREPEND) - 1, NULL, 0);

        // convert message length to string and store it in the variable `tmp`
        uint32_t full_message_len = token_auth_context.len + 62;
        for (index = 1; (((index * base) <= full_message_len) &&
            (((index * base) / base) == index));
            index *= base);
        for (; index; index /= base) {
            tmp[pos++] = '0' + ((full_message_len / index) % base);
        }
        tmp[pos] = '\0';

        // add the message length to the hash
        cx_hash((cx_hash_t *)&sha3_context, 0, (uint8_t*)tmp, pos, NULL, 0);

        // add the message length to the hash
        cx_hash((cx_hash_t *)&sha3_context, 0, token_auth_context.address, strlen(token_auth_context.address), NULL, 0);
    }
    else {
      if (p1 != P1_MORE) {
          THROW(ERR_INVALID_P1);
      }
      if (app_state != APP_STATE_SIGNING_MESSAGE) {
          THROW(ERR_INVALID_MESSAGE);
      }
    }
    if (p2 != 0) {
        THROW(ERR_INVALID_ARGUMENTS);
    }
    if (data_length > token_auth_context.len) {
        THROW(ERR_MESSAGE_TOO_LONG);
    }

    // add the received message part to the hash and decrease the remaining length
    cx_hash((cx_hash_t *)&sha3_context, 0, data_buffer, data_length, NULL, 0);

    token_auth_context.len -= data_length;
    if (token_auth_context.len != 0) {
        THROW(MSG_OK);
    }

    // finalize hash, compute it and store it in `msg_context.strhash` for display
    cx_hash((cx_hash_t *)&sha3_context, CX_LAST, data_buffer, 0, token_auth_context.hash, 32);

    // sign the hash
    if (!sign_auth_token()) {
        init_auth_token_context();
        THROW(ERR_SIGNATURE_FAILED);
    }

    app_state = APP_STATE_IDLE;
    *flags |= IO_ASYNCH_REPLY;
    ux_flow_init(0, ux_auth_token_msg_flow, NULL);
}
