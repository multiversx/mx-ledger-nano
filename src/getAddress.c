#include <stdint.h>
#include <stddef.h>

#include "bech32.h"
#include "getAddress.h"
#include "getPrivateKey.h"
#include "globals.h"
#include "os.h"
#include "ux.h"
#include "utils.h"

static char address[FULL_ADDRESS_LENGTH];

static uint8_t setResultGetAddress();

// UI interface for validating the address on screen
UX_STEP_NOCB(
    ux_display_public_flow_5_step, 
    bnnn_paging, 
    {
        .title = "Address",
        .text = address,
    });
UX_STEP_VALID(
    ux_display_public_flow_6_step, 
    pb, 
    send_response(setResultGetAddress(), true),
    {
        &C_icon_validate_14,
        "Approve",
    });
UX_STEP_VALID(
    ux_display_public_flow_7_step, 
    pb, 
    send_response(0, false),
    {
        &C_icon_crossmark,
        "Reject",
    });

UX_FLOW(ux_display_public_flow,
    &ux_display_public_flow_5_step,
    &ux_display_public_flow_6_step,
    &ux_display_public_flow_7_step
);

/* return false in case of error, true otherwise */
bool getPublicKey(uint32_t accountNumber, uint32_t index, uint8_t *publicKeyArray) {
    cx_ecfp_private_key_t privateKey;
    cx_ecfp_public_key_t publicKey;
    bool error = false;

    if (!getPrivateKey(accountNumber, index, &privateKey)) {
        return false;
    }

    BEGIN_TRY {
        TRY {
            cx_ecfp_generate_pair(CX_CURVE_Ed25519, &publicKey, &privateKey, 1);
        }
        CATCH_ALL {
            error = true;
        }
        FINALLY {
            memset(&privateKey, 0, sizeof(privateKey));
        }
    }
    END_TRY;

    if (error) {
        return false;
    }

    for (int i = 0; i < 32; i++) {
        publicKeyArray[i] = publicKey.W[64 - i];
    }
    if ((publicKey.W[32] & 1) != 0) {
        publicKeyArray[31] |= 0x80;
    }

    return true;
}

static uint8_t setResultGetAddress() {
    uint8_t tx = 0;
    const uint8_t address_size = strlen(address);

    G_io_apdu_buffer[tx++] = address_size;
    memmove(G_io_apdu_buffer + tx, address, address_size);
    tx += address_size;

    return tx;
}

void getAddressHexFromBinary(uint8_t *publicKey, char *address) {
    const char hex[16] = {'0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'a', 'b', 'c', 'd', 'e', 'f'};
    uint8_t i;

    for (i = 0; i < 32; i++) {
        address[i * 2] = hex[publicKey[i] >> 4];
        address[i * 2 + 1] = hex[publicKey[i] & 0xf];
    }
    address[64] = '\0';
}

void getAddressBech32FromBinary(uint8_t *publicKey, char *address) {
    uint8_t buffer[33];
    char *hrp;

    memmove(buffer, publicKey, 32);
    buffer[32] = '\0';
    hrp = HRP;
    bech32EncodeFromBytes(address, hrp, buffer, 33);
}

void handle_get_address(uint8_t p1, uint8_t p2, uint8_t *dataBuffer, uint16_t dataLength, volatile unsigned int *flags, volatile unsigned int *tx) {
    uint8_t publicKey[32];
    uint32_t account, index;

    if (dataLength != sizeof(uint32_t) * 2) {
        THROW(ERR_INVALID_ARGUMENTS);
        return;
    }

    account = read_uint32_be(dataBuffer);
    index = read_uint32_be(dataBuffer + sizeof(uint32_t));
    if (!getPublicKey(account, index, publicKey)) {
        THROW(ERR_INVALID_ARGUMENTS);
    }

    switch(p2) {
        case P2_DISPLAY_BECH32:
            getAddressBech32FromBinary(publicKey, address);
            break;
        case P2_DISPLAY_HEX:
            getAddressHexFromBinary(publicKey, address);
            break;
        default:
            THROW(ERR_INVALID_ARGUMENTS);
            return;
    }

    if (p1 == P1_NON_CONFIRM) {
        *tx = setResultGetAddress();
        THROW(MSG_OK);
    } else {
        ux_flow_init(0, ux_display_public_flow, NULL);
        *flags |= IO_ASYNCH_REPLY;
    }
}
