#include <stdint.h>
#include <stddef.h>

#include "bech32.h"
#include "globals.h"
#include "getPrivateKey.h"
#include "os.h"
#include "ux.h"

/* return false in case of error, true otherwise */
bool get_public_key(uint32_t accountNumber, uint32_t index, uint8_t *publicKeyArray) {
    cx_ecfp_private_key_t privateKey;
    cx_ecfp_public_key_t publicKey;
    bool error = false;

    if (!getPrivateKey(accountNumber, index, &privateKey)) {
        return false;
    }

    BEGIN_TRY {
        TRY {
            cx_ecfp_generate_pair(CX_CURVE_Ed25519, &publicKey, &privateKey, 1);
        }
        CATCH_ALL {
            error = true;
        }
        FINALLY {
            memset(&privateKey, 0, sizeof(privateKey));
        }
    }
    END_TRY;

    if (error) {
        return false;
    }

    for (int i = 0; i < 32; i++) {
        publicKeyArray[i] = publicKey.W[64 - i];
    }
    if ((publicKey.W[32] & 1) != 0) {
        publicKeyArray[31] |= 0x80;
    }

    return true;
}

void get_address_hex_from_binary(uint8_t *publicKey, char *address) {
    const char hex[16] = {'0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'a', 'b', 'c', 'd', 'e', 'f'};
    uint8_t i;

    for (i = 0; i < 32; i++) {
        address[i * 2] = hex[publicKey[i] >> 4];
        address[i * 2 + 1] = hex[publicKey[i] & 0xf];
    }
    address[64] = '\0';
}

void get_address_bech32_from_binary(uint8_t *publicKey, char *address) {
    uint8_t buffer[33];
    char *hrp;

    memmove(buffer, publicKey, 32);
    buffer[32] = '\0';
    hrp = HRP;
    bech32EncodeFromBytes(address, hrp, buffer, 33);
}
