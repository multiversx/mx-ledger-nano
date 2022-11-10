#ifndef _GLOBALS_H_
#define _GLOBALS_H_

#include "constants.h"
#include "cx.h"
#include "os.h"
#include "os_io_seproxyhal.h"
#include "ux.h"

#define P1_CONFIRM     0x01
#define P1_NON_CONFIRM 0x00
#define P1_FIRST       0x00
#define P1_MORE        0x80

#define DEFAULT_CONTRACT_DATA CONTRACT_DATA_ENABLED

extern ux_state_t ux;
// display stepped screens
extern unsigned int ux_step;
extern unsigned int ux_step_count;

// selected account global variables
extern uint32_t bip32_account;
extern uint32_t bip32_address_index;

typedef struct internal_storage_t {
    unsigned char setting_contract_data;
    uint8_t initialized;
} internal_storage_t;

extern const internal_storage_t N_storage_real;
#define N_storage (*(volatile internal_storage_t *) PIC(&N_storage_real))

// common types for sign message and sign tx hash

typedef enum { APP_STATE_IDLE, APP_STATE_SIGNING_MESSAGE, APP_STATE_SIGNING_TX } app_state_t;

extern cx_sha3_t sha3_context;
extern app_state_t app_state;

#endif
