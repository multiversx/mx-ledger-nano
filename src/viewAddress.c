#include "viewAddress.h"
#include "os.h"
#include "ux.h"
#include "utils.h"

static char address[FULL_ADDRESS_LENGTH];

// UI for displaying the address in either bech32 or hex formats
UX_STEP_NOCB(
    ux_display_public_flow_8_step, 
    bnnn_paging, 
    {
      .title = "Address",
      .text = address,
    });
UX_STEP_VALID(
    ux_display_public_flow_9_step,
    pb,
    ui_idle(),
    {
      &C_icon_back,
      "Back",
    });
UX_FLOW(ux_view_address_flow,
  &ux_display_public_flow_8_step,
  &ux_display_public_flow_9_step
);

void viewAddress(uint32_t account, uint32_t index, bool asBech32) {
    uint8_t publicKey[32];

    getPublicKey(account, index, publicKey);
    if (asBech32)
        getAddressBech32FromBinary(publicKey, address);
    else
        getAddressHexFromBinary(publicKey, address);

    ux_flow_init(0, ux_view_address_flow, NULL);
}
