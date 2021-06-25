#ifndef _PROVIDE_ESDT_INFO_H_
#define _PROVIDE_ESDT_INFO_H_

#include <stdint.h>

#define MAX_ESDT_TICKER_LEN     32
#define MAX_ESDT_IDENTIFIER_LEN 32
#define MAX_CHAIN_ID_LEN        32
#define ESDT_SIGNATURE_LEN      71

#define ESDT_TRANSFER_PREFIX "ESDTTransfer@"

static const uint8_t const LEDGER_SIGNATURE_PUBLIC_KEY[] = {
    // production key 2019-01-11 03, 0x07PM (erc20signer)
    0x04, 0x0d, 0x04, 0x9d, 0xd5, 0x3d, 0x97, 0x7d, 0x21, 0x92, 0xed, 0xeb, 0xba, 0xac, 0x71,
    0x39, 0x20, 0xda, 0xd2, 0x95, 0x59, 0x9a, 0x09, 0xf0, 0x8c, 0xe6, 0x25, 0x33, 0x37, 0x99,
    0x37, 0x5f, 0xc2, 0x81, 0xda, 0xf0, 0x24, 0x09, 0x66, 0x01, 0x34, 0xd2, 0x98, 0x8c, 0x4f,
    0xd3, 0x52, 0x6e, 0xde, 0x39, 0x4f, 0xa0, 0xe5, 0xdc, 0x3d, 0x3f, 0xb7, 0x30, 0xeb, 0x53,
    0xab, 0x35, 0xa6, 0x57, 0x5c};
// priv:
//     b5:e3:cf:b4:42:b0:ac:ad:f5:f5:d1:ed:1f:7a:c7:
//     b6:ff:c3:28:ab:29:7b:a0:eb:0e:5c:e7:33:8d:e5:
//     49:5e

typedef struct esdt_info_t {
    uint8_t ticker_len;
    char ticker[MAX_ESDT_TICKER_LEN];
    uint8_t identifier_len;
    char identifier[MAX_ESDT_IDENTIFIER_LEN];
    uint8_t decimals;
    uint8_t chain_id_len;
    char chain_id[MAX_CHAIN_ID_LEN];
    char hash[65];
} esdt_info_t;

extern esdt_info_t esdt_info;

uint16_t handleProvideESDTInfo(uint8_t *dataBuffer, uint16_t dataLength);

#endif
