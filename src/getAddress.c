#include "getAddress.h"
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
    sendResponse(setResultGetAddress(), true),
    {
        &C_icon_validate_14,
        "Approve",
    });
UX_STEP_VALID(
    ux_display_public_flow_7_step, 
    pb, 
    sendResponse(0, false),
    {
        &C_icon_crossmark,
        "Reject",
    });

UX_FLOW(ux_display_public_flow,
    &ux_display_public_flow_5_step,
    &ux_display_public_flow_6_step,
    &ux_display_public_flow_7_step
);

static uint8_t setResultGetAddress() {
    uint8_t tx = 0;
    const uint8_t address_size = strlen(address);

    G_io_apdu_buffer[tx++] = address_size;
    os_memmove(G_io_apdu_buffer + tx, address, address_size);
    tx += address_size;

    return tx;
}

void handleGetAddress(uint8_t p1, uint8_t p2, uint8_t *dataBuffer, uint16_t dataLength, volatile unsigned int *flags, volatile unsigned int *tx) {
    uint8_t publicKey[32];
    uint32_t account, index;

    if (dataLength != sizeof(uint32_t) * 2) {
        THROW(ERR_INVALID_ARGUMENTS);
        return;
    }

    account = readUint32BE(dataBuffer);
    index = readUint32BE(dataBuffer + sizeof(uint32_t));
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
