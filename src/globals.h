#include "os.h"
#include "ux.h"
#include "os_io_seproxyhal.h"

#ifndef _GLOBALS_H_
#define _GLOBALS_H_

#define P1_CONFIRM     0x01
#define P1_NON_CONFIRM 0x00
#define P1_FIRST       0x00
#define P1_MORE        0x80

#define FULL_ADDRESS_LENGTH 65 // hex address is 64 characters + \0 = 65
#define BIP32_PATH          5
#define COIN_TYPE_EGLD      508UL
#define TICKER_MAINNET      "eGLD"
#define TICKER_TESTNET      "XeGLD"
#define HRP                 "erd"
#define DECIMAL_PLACES      18
#define MAINNET_CHAIN_ID    "1"
#define TX_VERSION          1
#define TX_HASH_VERSION     2
#define TX_HASH_OPTIONS     1

typedef enum {
    NETWORK_MAINNET = 0,
    NETWORK_TESTNET = 1
} network_t;

typedef enum {
    CONTRACT_DATA_ENABLED  = true,
    CONTRACT_DATA_DISABLED = false
} contract_data_t;

#define DEFAULT_CONTRACT_DATA CONTRACT_DATA_DISABLED

#define MSG_OK                     0x9000
#define ERR_USER_DENIED            0x6985
#define ERR_UNKNOWN_INSTRUCTION    0x6D00 // unknown INS
#define ERR_WRONG_CLA              0x6E00
#define ERR_SIGNATURE_FAILED       0x6E10
#define ERR_INVALID_ARGUMENTS      0x6E01
#define ERR_INVALID_MESSAGE        0x6E02 // signTxWithHash
#define ERR_INVALID_P1             0x6E03 // signTxWithHash
#define ERR_MESSAGE_TOO_LONG       0x6E04 // signTxWithHash
#define ERR_RECEIVER_TOO_LONG      0x6E05 // signTxWithHash
#define ERR_AMOUNT_TOO_LONG        0x6E06 // signTxWithHash
#define ERR_CONTRACT_DATA_DISABLED 0x6E07 // signTxWithHash
#define ERR_MESSAGE_INCOMPLETE     0x6E08 // signTxWithHash
#define ERR_WRONG_TX_VERSION       0x6E09 // signTxWithHash
#define ERR_NONCE_TOO_LONG         0x6E0A // signTxWithHash
#define ERR_INVALID_AMOUNT         0x6E0B // signTxWithHash
#define ERR_INVALID_FEE            0x6E0C // signTxWithHash
#define ERR_PRETTY_FAILED          0x6E0D // signTxWithHash
#define ERR_DATA_TOO_LONG          0x6E0E // singTxWithHash
#define ERR_WRONG_TX_OPTIONS       0x6E0F // signTxWithHash
#define ERR_SIGN_TX_DEPRECATED     0x6E11 // signTx - deprecated

extern ux_state_t ux;
// display stepped screens
extern unsigned int ux_step;
extern unsigned int ux_step_count;

// selected account global variables
extern uint32_t bip32_account;
extern uint32_t bip32_address_index;

typedef struct internalStorage_t {
    unsigned char setting_contract_data;
    uint8_t initialized;
} internalStorage_t;

extern const internalStorage_t N_storage_real;
#define N_storage (*(volatile internalStorage_t*) PIC(&N_storage_real))

// common defines and types for sign tx and sign tx hash

#define MAX_AMOUNT_LEN        32
#define MAX_BUFFER_LEN        500
#define MAX_DATA_SIZE         400   // 400 in base64 = 300 in ASCII
#define MAX_DISPLAY_DATA_SIZE 64UL  // must be multiple of 4
#define DATA_SIZE_LEN         17
#define MAX_CHAINID_LEN       4
#define MAX_TICKER_LEN        5
#define MAX_UINT32_LEN        10    // len(f"{0xffffffff:d}")
#define MAX_UINT64_LEN        20    // len(f"{0xffffffffffffffff:d}")
#define MAX_UINT128_LEN       39    // len(f"{0xffffffffffffffffffffffffffffffff:d}")
#define PRETTY_SIZE (2 + MAX_TICKER_LEN) // additional space for "0." and " eGLD"
#define GAS_PER_DATA_BYTE     1500
#define GAS_PRICE_DIVIDER     100
#define MIN_GAS_LIMIT         50000

typedef struct {
    char receiver[FULL_ADDRESS_LENGTH];
    char amount[MAX_AMOUNT_LEN + PRETTY_SIZE];
    uint64_t gas_limit;
    uint64_t gas_price;
    char fee[MAX_AMOUNT_LEN + PRETTY_SIZE];
    char data[MAX_DISPLAY_DATA_SIZE + DATA_SIZE_LEN];
    uint32_t data_size;
    uint8_t signature[64];
} tx_context_t;

extern tx_context_t tx_context;

// common types for sign message and sign tx hash

typedef enum {
  APP_STATE_IDLE,
  APP_STATE_SIGNING_MESSAGE
} app_state_t;

typedef struct {
    uint32_t len;
    uint8_t hash[32];
    char strhash[65];
    cx_sha3_t sha3;
    app_state_t state;
    uint8_t signature[64];
} msg_context_t;

extern msg_context_t msg_context;

#endif
