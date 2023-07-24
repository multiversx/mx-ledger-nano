#pragma once

#include <stdbool.h>

typedef enum { NETWORK_MAINNET = 0, NETWORK_TESTNET = 1 } network_t;

typedef enum { CONTRACT_DATA_ENABLED = true, CONTRACT_DATA_DISABLED = false } contract_data_t;

#define MSG_OK                     0x9000
#define ERR_USER_DENIED            0x6985
#define ERR_UNKNOWN_INSTRUCTION    0x6D00  // unknown INS
#define ERR_WRONG_CLA              0x6E00
#define ERR_SIGNATURE_FAILED       0x6E10
#define ERR_INVALID_ARGUMENTS      0x6E01
#define ERR_INVALID_MESSAGE        0x6E02  // signTxWithHash
#define ERR_INVALID_P1             0x6E03  // signTxWithHash
#define ERR_MESSAGE_TOO_LONG       0x6E04  // signTxWithHash
#define ERR_RECEIVER_TOO_LONG      0x6E05  // signTxWithHash
#define ERR_AMOUNT_TOO_LONG        0x6E06  // signTxWithHash
#define ERR_CONTRACT_DATA_DISABLED 0x6E07  // signTxWithHash
#define ERR_MESSAGE_INCOMPLETE     0x6E08  // signTxWithHash
#define ERR_WRONG_TX_VERSION       0x6E09  // signTxWithHash
#define ERR_NONCE_TOO_LONG         0x6E0A  // signTxWithHash
#define ERR_INVALID_AMOUNT         0x6E0B  // signTxWithHash
#define ERR_INVALID_FEE            0x6E0C  // signTxWithHash
#define ERR_PRETTY_FAILED          0x6E0D  // signTxWithHash
#define ERR_DATA_TOO_LONG          0x6E0E  // singTxWithHash
#define ERR_WRONG_TX_OPTIONS       0x6E0F  // signTxWithHash
#define ERR_SIGN_TX_DEPRECATED     0x6E11  // signTx - deprecated
#define ERR_INVALID_ESDT_SIGNATURE 0x6E12
#define ERR_INDEX_OUT_OF_BOUNDS    0x6E13
#define ERR_INVALID_ESDT           0x6E14

#define FULL_ADDRESS_LENGTH 65  // hex address is 64 characters + \0 = 65
#define BIP32_PATH          5
#define COIN_TYPE_EGLD      508UL
#define TICKER_MAINNET      "EGLD"
#define TICKER_TESTNET      "xEGLD"
#define HRP                 "erd"
#define DECIMAL_PLACES      18
#define MAINNET_CHAIN_ID    "1"
#define TESTNET_CHAIN_ID    "T"
#define DEVNET_CHAIN_ID     "D"
#define MAINNET_NETWORK     "Mainnet"
#define TESTNET_NETWORK     "Testnet"
#define DEVNET_NETWORK      "Devnet"
#define UNKNOWN_NETWORK     "Unknown"
#define TX_VERSION          1
#define TX_HASH_VERSION     2
#define TX_HASH_OPTIONS     1

// common defines and types for sign tx and sign tx hash

#define MAX_AMOUNT_LEN                     32
#define MAX_BUFFER_LEN                     500
#define MAX_ESDT_TRANSFER_DATA_SIZE        100
#define MAX_DATA_SIZE                      400  // 400 in base64 = 300 in ASCII
#define MAX_DISPLAY_DATA_SIZE              64UL  // must be multiple of 4
#define DATA_SIZE_LEN                      17
#define MAX_CHAINID_LEN                    4
#define MAX_TICKER_LEN                     10
#define MAX_UINT32_LEN                     10  // len(f"{0xffffffff:d}")
#define MAX_UINT64_LEN                     20  // len(f"{0xffffffffffffffff:d}")
#define MAX_UINT128_LEN                    39  // len(f"{0xffffffffffffffffffffffffffffffff:d}")
#define MAX_AUTH_TOKEN_ORIGIN_SIZE         37
#define MAX_AUTH_TOKEN_TTL_SIZE            41
#define AUTH_TOKEN_DISPLAY_MAX_SIZE        100
#define AUTH_TOKEN_ENCODED_ORIGIN_MAX_SIZE 101
#define AUTH_TOKEN_ENCODED_TTL_MAX_SIZE    11
#define AUTH_TOKEN_INVALID_RET_CODE        -1
#define AUTH_TOKEN_BAD_REQUEST_RET_CODE    -2
#define AUTH_TOKEN_ADDRESS_INDICES_SIZE    8
#define AUTH_TOKEN_TOKEN_LEN_FIELD_SIZE    4
#define AUTH_TOKEN_INVALID_ORIGIN_PREFIX   "multiversx://"
#define MAX_INT_NUM_DIGITS                 10
#define PRETTY_SIZE                        (2 + MAX_TICKER_LEN)  // additional space for "0." and " eGLD"
#define GAS_PER_DATA_BYTE                  1500
#define GAS_PRICE_DIVIDER                  100
#define MIN_GAS_LIMIT                      50000
#define MAX_AUTH_TOKEN_LEN                 100
#define BECH32_ADDRESS_LEN                 62
#define HASH_LEN                           32
#define MESSAGE_SIGNATURE_LEN              64
#define SHA3_KECCAK_BITS                   256
#define PUBLIC_KEY_LEN                     32
#define BASE_10                            10
#define TX_SIGN_FLOW_SIZE                  9
#define ESDT_TRANSFER_FLOW_SIZE            9
#define BASE_64_INVALID_CHAR               '?'
#define SC_ARGS_SEPARATOR                  '@'
#define MAX_ESDT_VALUE_HEX_COUNT           32
#define INVALID_INDEX                      -1
#define ESDT_CODE_VALUE_TOO_HIGH           '1'
#define ESDT_CODE_VALUE_OK                 '2'
#define ESDT_VALUE_MAX_LENGTH              40
#define ESDT_TRANSFER_PREFIX               "ESDTTransfer@"
#define ESDT_VALUE_N_A                     "N/A"
#define ESDT_VALUE_TOO_LONG                "<value too big to display>"
#define ESDT_VALUE_HEX_PARSE_ERR           "<value cannot be parsed to hex>"
#define ESDT_TRANSFER_PREFIX_LENGTH        strlen(ESDT_TRANSFER_PREFIX)

static const char PREPEND[] =
    "\x17"
    "Elrond Signed Message:\n";
