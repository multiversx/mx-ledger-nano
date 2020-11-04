#include "signMsg.h"

msg_context_t msg_context;

static uint8_t setResultSignature();
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
    sendResponse(setResultSignature(), true),
    {
      &C_icon_validate_14,
      "Sign message",
    });
UX_STEP_VALID(
    ux_sign_msg_flow_16_step, 
    pb,
    sendResponse(0, false),
    {
      &C_icon_crossmark,
      "Reject",
    });

UX_FLOW(ux_sign_msg_flow,
  &ux_sign_msg_flow_14_step,
  &ux_sign_msg_flow_15_step,
  &ux_sign_msg_flow_16_step
);

static uint8_t setResultSignature() {
    uint8_t tx = 0;
    const uint8_t sig_size = 64;
    G_io_apdu_buffer[tx++] = sig_size;
    os_memmove(G_io_apdu_buffer + tx, msg_context.signature, sig_size);
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
            os_memset(&privateKey, 0, sizeof(privateKey));
        }
    }
    END_TRY;

    return success;
}

void handleSignMsg(uint8_t p1, uint8_t p2, uint8_t *dataBuffer, uint16_t dataLength, volatile unsigned int *flags, volatile unsigned int *tx) {
    if (p1 == P1_FIRST) {
        char tmp[11];
        uint32_t index;
        uint32_t base = 10;
        uint8_t pos = 0;
        // first 4 bytes from dataBuffer should be the message length (big endian uint32)
        if (dataLength < 4)
            THROW(ERR_INVALID_MESSAGE);
        msg_context.state = APP_STATE_SIGNING_MESSAGE;
        msg_context.len = U4BE(dataBuffer, 0);
        dataBuffer += 4;
        dataLength -= 4;
        // initialize hash with the constant string to prepend
        cx_keccak_init(&msg_context.sha3, 256);
        cx_hash((cx_hash_t *)&msg_context.sha3, 0, (uint8_t*)PREPEND, sizeof(PREPEND) - 1, NULL, 0);
        // convert message length to string and store it in the variable `tmp`
        for (index = 1; (((index * base) <= msg_context.len) &&
            (((index * base) / base) == index));
            index *= base);
        for (; index; index /= base) {
            tmp[pos++] = '0' + ((msg_context.len / index) % base);
        }
        tmp[pos] = '\0';
        // add the message length to the hash
        cx_hash((cx_hash_t *)&msg_context.sha3, 0, (uint8_t*)tmp, pos, NULL, 0);
    }
    else if (p1 != P1_MORE) {
        THROW(ERR_INVALID_P1);
    }
    if (p2 != 0) {
        THROW(ERR_INVALID_ARGUMENTS);
    }
    if ((p1 == P1_MORE) && (msg_context.state != APP_STATE_SIGNING_MESSAGE)) {
        THROW(ERR_INVALID_MESSAGE);
    }
    if (dataLength > msg_context.len) {
        THROW(ERR_MESSAGE_TOO_LONG);
    }
    // add the received message part to the hash and decrease the remaining length
    cx_hash((cx_hash_t *)&msg_context.sha3, 0, dataBuffer, dataLength, NULL, 0);
    msg_context.len -= dataLength;
    if (msg_context.len == 0) {
        // finalize hash, compute it and store it in `msg_context.strhash` for display
        cx_hash((cx_hash_t *)&msg_context.sha3, CX_LAST, dataBuffer, 0, msg_context.hash, 32);
        snprintf(msg_context.strhash, sizeof(msg_context.strhash), "%.*H", sizeof(msg_context.hash), msg_context.hash);
        // sign the hash
        if (!sign_message()) {
            THROW(ERR_SIGNATURE_FAILED);
        }
        msg_context.state = APP_STATE_IDLE;
        ux_flow_init(0, ux_sign_msg_flow, NULL);
        *flags |= IO_ASYNCH_REPLY;
    } else {
        THROW(MSG_OK);
    }
}
