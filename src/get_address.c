#include <stddef.h>
#include <stdint.h>

#include "address_helpers.h"
#include "bech32.h"
#include "get_address.h"
#include "get_private_key.h"
#include "globals.h"
#include "os.h"
#include "utils.h"
#include "ux.h"
#include "menu.h"

#ifdef HAVE_NBGL
#include "nbgl_use_case.h"
#endif

static char address[FULL_ADDRESS_LENGTH];

static uint8_t set_result_get_address(void) {
    uint8_t tx = 0;
    uint8_t address_size = strlen(address);

    G_io_apdu_buffer[tx++] = address_size;
    memmove(G_io_apdu_buffer + tx, address, address_size);
    tx += address_size;

    return tx;
}

#if defined(TARGET_STAX)

static void address_verification_cancelled(void) {
    send_response(0, false, false);
    nbgl_useCaseStatus("Address verification\ncancelled", false, ui_idle);
}

static void callback_choice(bool match) {
    if (match) {
        send_response(set_result_get_address(), true, false);
        nbgl_useCaseStatus("ADDRESS\nVERIFIED", true, ui_idle);
    } else {
        address_verification_cancelled();
    }
}

static void ui_get_public_key_nbgl(void) {
    nbgl_useCaseAddressReview(address,
                              NULL,
                              &C_icon_multiversx_logo_64x64,
                              "Verify " APPNAME "\naddress",
                              NULL,
                              callback_choice);
}

#else

// UI interface for validating the address on screen
UX_STEP_NOCB(ux_display_public_flow_5_step,
             bnnn_paging,
             {
                 .title = "Address",
                 .text = address,
             });
UX_STEP_VALID(ux_display_public_flow_6_step,
              pb,
              send_response(set_result_get_address(), true, true),
              {
                  &C_icon_validate_14,
                  "Approve",
              });
UX_STEP_VALID(ux_display_public_flow_7_step,
              pb,
              send_response(0, false, true),
              {
                  &C_icon_crossmark,
                  "Reject",
              });

UX_FLOW(ux_display_public_flow,
        &ux_display_public_flow_5_step,
        &ux_display_public_flow_6_step,
        &ux_display_public_flow_7_step);

#endif

void handle_get_address(uint8_t p1,
                        uint8_t p2,
                        uint8_t *data_buffer,
                        uint16_t data_length,
                        volatile unsigned int *flags,
                        volatile unsigned int *tx) {
    uint8_t public_key[PUBLIC_KEY_LEN];
    uint32_t account, index;

    if (data_length != sizeof(uint32_t) * 2) {
        THROW(ERR_INVALID_ARGUMENTS);
        return;
    }

    account = read_uint32_be(data_buffer);
    index = read_uint32_be(data_buffer + sizeof(uint32_t));
    if (!get_public_key(account, index, public_key)) {
        THROW(ERR_INVALID_ARGUMENTS);
    }

    switch (p2) {
        case P2_DISPLAY_BECH32:
            get_address_bech32_from_binary(public_key, address);
            break;
        case P2_DISPLAY_HEX:
            get_address_hex_from_binary(public_key, address);
            break;
        default:
            THROW(ERR_INVALID_ARGUMENTS);
            return;
    }

    if (p1 == P1_NON_CONFIRM) {
        *tx = set_result_get_address();
        THROW(MSG_OK);
    } else {
#if defined(TARGET_STAX)
        ui_get_public_key_nbgl();
#else
        ux_flow_init(0, ux_display_public_flow, NULL);
#endif
        *flags |= IO_ASYNCH_REPLY;
    }
}
