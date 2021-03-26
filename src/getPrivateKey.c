#include <stdbool.h>
#include <string.h>

#include "constants.h"
#include "getPrivateKey.h"

static const uint32_t HARDENED_OFFSET = 0x80000000;
static const uint32_t derivePath[BIP32_PATH] = {
  44 | HARDENED_OFFSET,
  COIN_TYPE_EGLD | HARDENED_OFFSET,
  0 | HARDENED_OFFSET,
  0 | HARDENED_OFFSET,
  0 | HARDENED_OFFSET
};

bool getPrivateKey(uint32_t account, uint32_t addressIndex, cx_ecfp_private_key_t *privateKey) {
    uint8_t privateKeyData[32];
    uint32_t bip32Path[BIP32_PATH];
    bool success = true;

    memmove(bip32Path, derivePath, sizeof(derivePath));

    bip32Path[2] = account | HARDENED_OFFSET;
    bip32Path[4] = addressIndex | HARDENED_OFFSET;

    BEGIN_TRY {
        TRY {
            os_perso_derive_node_bip32_seed_key(HDW_ED25519_SLIP10, CX_CURVE_Ed25519, bip32Path, BIP32_PATH, privateKeyData, NULL, NULL, 0);
            cx_ecfp_init_private_key(CX_CURVE_Ed25519, privateKeyData, 32, privateKey);
        }
        CATCH_ALL {
            success = false;
        }
        FINALLY {
            memset(privateKeyData, 0, sizeof(privateKeyData));
        }
    }
    END_TRY;

    return success;
}
