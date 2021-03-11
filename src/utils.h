#include "os.h"
#include "cx.h"
#include "globals.h"

#ifndef _UTILS_H_
#define _UTILS_H_

bool makeAmountPretty(char *amount, size_t max_size, network_t network);
void computeDataSize(char *base64, uint32_t b64len);
bool gas_to_fee(uint64_t gas_limit, uint64_t gas_price, uint32_t data_size, char *fee, size_t size);
bool is_digit(char c);
bool parse_int(char *str, size_t size, uint64_t *result);
bool valid_amount(char *amount, size_t size);

void getAddressBech32FromBinary(uint8_t *publicKey, char *address);
void getAddressHexFromBinary(uint8_t *publicKey, char *address);

bool getPublicKey(uint32_t accountNumber, uint32_t index, uint8_t *publicKeyArray);

uint32_t readUint32BE(uint8_t *buffer);

bool getPrivateKey(uint32_t accountNumber, uint32_t index, cx_ecfp_private_key_t *privateKey);

void sendResponse(uint8_t tx, bool approve);

    // type            userid    x    y   w    h  str rad fill      fg        bg      fid iid  txt   touchparams...       ]
#define UI_BUTTONS \
    {{BAGL_RECTANGLE   , 0x00,   0,   0, 128,  32, 0, 0, BAGL_FILL, 0x000000, 0xFFFFFF, 0, 0}, NULL, 0, 0, 0, NULL, NULL, NULL},\
    {{BAGL_ICON        , 0x00,   3,  12,   7,   7, 0, 0, 0        , 0xFFFFFF, 0x000000, 0, BAGL_GLYPH_ICON_CROSS  }, NULL, 0, 0, 0, NULL, NULL, NULL },\
    {{BAGL_ICON        , 0x00, 117,  13,   8,   6, 0, 0, 0        , 0xFFFFFF, 0x000000, 0, BAGL_GLYPH_ICON_CHECK  }, NULL, 0, 0, 0, NULL, NULL, NULL }

#define UI_FIRST  1
#define UI_SECOND 0

#define UI_LABELINE(userId, text, isFirst, font, horizontalScrollSpeed)    \
    {                                                                      \
        {                                                                  \
            BAGL_LABELINE,                                                 \
            (userId),                                                      \
            23,                                                            \
            (isFirst) ? 12 : 26,                                           \
            82,                                                            \
            12,                                                            \
            (horizontalScrollSpeed) ? BAGL_STROKE_FLAG_ONESHOT | 10 : 0,   \
            0, 0, 0xFFFFFF, 0x000000,                                      \
            (font)|BAGL_FONT_ALIGNMENT_CENTER,                             \
            horizontalScrollSpeed                                          \
        },                                                                 \
        (text), 0, 0, 0, NULL, NULL, NULL                                  \
    }

#endif
