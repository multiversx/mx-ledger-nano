#include "globals.h"
#include "os.h"
#include "ux.h"

ux_state_t G_ux;
bolos_ux_params_t G_ux_params;

// display stepped screens
unsigned int ux_step;
unsigned int ux_step_count;
const internal_storage_t N_storage_real;

// selected account global variables
uint32_t bip32_account;
uint32_t bip32_address_index;

cx_sha3_t sha3_context;
app_state_t app_state;
