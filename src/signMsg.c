#include "getPrivateKey.h"
#include "signMsg.h"
#include "utils.h"

typedef struct {
    uint32_t len;
    uint8_t hash[32];
    char strhash[65];
    uint8_t signature[64];
} msg_context_t;

static msg_context_t msg_context;

static uint8_t set_result_signature();
bool sign_message(void);

// UI for confirming the message hash on screen
UX_STEP_NOCB(
    ux_sign_msg_flow_14_step, 
    bnnn_paging, 
    {
      .title = "Hash",
      .text = msg_context.strhash,
    });
UX_STEP_VALID(
    ux_sign_msg_flow_15_step, 
    pb, 
    send_response(set_result_signature(), true),
    {
      &C_icon_validate_14,
      "Sign message",
    });
UX_STEP_VALID(
    ux_sign_msg_flow_16_step, 
    pb,
    send_response(0, false),
    {
      &C_icon_crossmark,
      "Reject",
    });

UX_FLOW(ux_sign_msg_flow,
  &ux_sign_msg_flow_14_step,
  &ux_sign_msg_flow_15_step,
  &ux_sign_msg_flow_16_step
);

void init_msg_context(void) {
    bip32_account = 0;
    bip32_address_index = 0;

    app_state = APP_STATE_IDLE;
}

static uint8_t set_result_signature() {
    uint8_t tx = 0;
    const uint8_t sig_size = 64;
    G_io_apdu_buffer[tx++] = sig_size;
    memmove(G_io_apdu_buffer + tx, msg_context.signature, sig_size);
    tx += sig_size;
    return tx;
}

bool sign_message(void) {
    cx_ecfp_private_key_t privateKey;
    bool success = true;

    if (!getPrivateKey(bip32_account, bip32_address_index, &privateKey)) {
        return false;
    }

    BEGIN_TRY {
        TRY {
            cx_eddsa_sign(&privateKey, CX_RND_RFC6979 | CX_LAST, CX_SHA512, msg_context.hash, 32, NULL, 0, msg_context.signature, 64, NULL);
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

void handle_sign_msg(uint8_t p1, uint8_t p2, uint8_t *data_buffer, uint16_t data_length, volatile unsigned int *flags) {
    if (p1 == P1_FIRST) {
        char tmp[11];
        uint32_t index;
        uint32_t base = 10;
        uint8_t pos = 0;
        // first 4 bytes from data_buffer should be the message length (big endian uint32)
        if (data_length < 4) {
            THROW(ERR_INVALID_MESSAGE);
        }
        app_state = APP_STATE_SIGNING_MESSAGE;
        msg_context.len = U4BE(data_buffer, 0);
        data_buffer += 4;
        data_length -= 4;
        // initialize hash with the constant string to prepend
        cx_keccak_init(&sha3_context, 256);
        cx_hash((cx_hash_t *)&sha3_context, 0, (uint8_t*)PREPEND, sizeof(PREPEND) - 1, NULL, 0);
        // convert message length to string and store it in the variable `tmp`
        for (index = 1; (((index * base) <= msg_context.len) &&
            (((index * base) / base) == index));
            index *= base);
        for (; index; index /= base) {
            tmp[pos++] = '0' + ((msg_context.len / index) % base);
        }
        tmp[pos] = '\0';
        // add the message length to the hash
        cx_hash((cx_hash_t *)&sha3_context, 0, (uint8_t*)tmp, pos, NULL, 0);
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
    if (data_length > msg_context.len) {
        THROW(ERR_MESSAGE_TOO_LONG);
    }

    // add the received message part to the hash and decrease the remaining length
    cx_hash((cx_hash_t *)&sha3_context, 0, data_buffer, data_length, NULL, 0);
    msg_context.len -= data_length;
    if (msg_context.len != 0) {
        THROW(MSG_OK);
    }

    // finalize hash, compute it and store it in `msg_context.strhash` for display
    cx_hash((cx_hash_t *)&sha3_context, CX_LAST, data_buffer, 0, msg_context.hash, 32);
    snprintf(msg_context.strhash, sizeof(msg_context.strhash), "%.*H", sizeof(msg_context.hash), msg_context.hash);

    // sign the hash
    if (!sign_message()) {
        init_msg_context();
        THROW(ERR_SIGNATURE_FAILED);
    }

    app_state = APP_STATE_IDLE;
    ux_flow_init(0, ux_sign_msg_flow, NULL);
    *flags |= IO_ASYNCH_REPLY;
}
