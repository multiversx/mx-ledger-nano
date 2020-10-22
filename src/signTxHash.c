#include "signTxHash.h"

static uint8_t setResultSignature();
bool sign_tx_hash(void);

// UI for confirming the tx hash on screen
UX_STEP_NOCB(
    ux_sign_tx_hash_flow_17_step, 
    bnnn_paging, 
    {
      .title = "Hash",
      .text = tx_hash_context.strhash,
    });
UX_STEP_VALID(
    ux_sign_tx_hash_flow_18_step, 
    pb, 
    sendResponse(setResultSignature(), true),
    {
      &C_icon_validate_14,
      "Sign tx hash",
    });
UX_STEP_VALID(
    ux_sign_tx_hash_flow_19_step, 
    pb,
    sendResponse(0, false),
    {
      &C_icon_crossmark,
      "Reject",
    });

UX_FLOW(ux_sign_tx_hash_flow,
  &ux_sign_tx_hash_flow_17_step,
  &ux_sign_tx_hash_flow_18_step,
  &ux_sign_tx_hash_flow_19_step
);

static uint8_t setResultSignature() {
    uint8_t tx = 0;
    const uint8_t sig_size = 64;
    G_io_apdu_buffer[tx++] = sig_size;
    os_memmove(G_io_apdu_buffer + tx, tx_hash_context.signature, sig_size);
    tx += sig_size;
    return tx;
}

bool sign_tx_hash(void) {
    cx_ecfp_private_key_t privateKey;
    bool success = true;

    if (!getPrivateKey(bip32_account, bip32_address_index, &privateKey)) {
        return false;
    }

    BEGIN_TRY {
        TRY {
            cx_eddsa_sign(&privateKey, CX_RND_RFC6979 | CX_LAST, CX_SHA512, tx_hash_context.hash, 32, NULL, 0, tx_hash_context.signature, 64, NULL);
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

void handleSignTxHash(uint8_t p1, uint8_t p2, uint8_t *dataBuffer, uint16_t dataLength, volatile unsigned int *flags, volatile unsigned int *tx) {
    if (dataLength != 32)
        THROW(ERR_INVALID_MESSAGE);
    os_memmove(tx_hash_context.hash, dataBuffer, dataLength);
    snprintf(tx_hash_context.strhash, sizeof(tx_hash_context.strhash), "%.*H", sizeof(tx_hash_context.hash), tx_hash_context.hash);
    // sign the hash
    if (!sign_tx_hash())
        THROW(ERR_SIGNATURE_FAILED);
    ux_flow_init(0, ux_sign_tx_hash_flow, NULL);
    *flags |= IO_ASYNCH_REPLY;
}
