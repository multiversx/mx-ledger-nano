#include <stddef.h>
#include <stdint.h>

#include "bech32.h"
#include "get_private_key.h"
#include "globals.h"
#include "os.h"
#include "ux.h"

/* return false in case of error, true otherwise */
bool get_public_key(uint32_t account_number, uint32_t index, uint8_t *public_key_array) {
    cx_ecfp_private_key_t private_key;
    cx_ecfp_public_key_t public_key;
    bool error = false;

    if (!get_private_key(account_number, index, &private_key)) {
        return false;
    }

    BEGIN_TRY {
        TRY {
            cx_ecfp_generate_pair(CX_CURVE_Ed25519, &public_key, &private_key, 1);
        }
        CATCH_ALL {
            error = true;
        }
        FINALLY {
            explicit_bzero(&private_key, sizeof(private_key));
        }
    }
    END_TRY;

    if (error) {
        return false;
    }

    for (int i = 0; i < 32; i++) {
        public_key_array[i] = public_key.W[64 - i];
    }
    if ((public_key.W[32] & 1) != 0) {
        public_key_array[31] |= 0x80;
    }

    return true;
}

// TODO: maybe make this function more general and extract to a new file binary
// <-> hex converters
void get_address_hex_from_binary(const uint8_t *public_key, char *address) {
    const char hex[16] =
        {'0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'a', 'b', 'c', 'd', 'e', 'f'};
    uint8_t i;

    for (i = 0; i < 32; i++) {
        address[i * 2] = hex[public_key[i] >> 4];
        address[i * 2 + 1] = hex[public_key[i] & 0xf];
    }
    address[64] = '\0';
}

// TODO: analyze and refactor this function
void get_address_bech32_from_binary(const uint8_t *public_key, char *address) {
    uint8_t buffer[33];

    memcpy(buffer, public_key, 32);
    buffer[32] = '\0';
    bech32EncodeFromBytes(address, HRP, buffer, 33);
}
