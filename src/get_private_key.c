#include <stdbool.h>
#include <string.h>

#include "constants.h"
#include "get_private_key.h"

static const uint32_t HARDENED_OFFSET = 0x80000000;
static const uint32_t derive_path[BIP32_PATH] = {44 | HARDENED_OFFSET,
                                                 COIN_TYPE_EGLD | HARDENED_OFFSET,
                                                 0 | HARDENED_OFFSET,
                                                 0 | HARDENED_OFFSET,
                                                 0 | HARDENED_OFFSET};

bool get_private_key(uint32_t account_index,
                     uint32_t address_index,
                     cx_ecfp_private_key_t *private_key) {
    uint8_t private_key_data[32];
    uint32_t bip32_path[BIP32_PATH];
    bool success = true;

    memmove(bip32_path, derive_path, sizeof(derive_path));

    bip32_path[2] = account_index | HARDENED_OFFSET;
    bip32_path[4] = address_index | HARDENED_OFFSET;

    BEGIN_TRY {
        TRY {
            os_perso_derive_node_bip32_seed_key(HDW_ED25519_SLIP10,
                                                CX_CURVE_Ed25519,
                                                bip32_path,
                                                BIP32_PATH,
                                                private_key_data,
                                                NULL,
                                                NULL,
                                                0);
            cx_ecfp_init_private_key(CX_CURVE_Ed25519, private_key_data, 32, private_key);
        }
        CATCH_ALL {
            success = false;
        }
        FINALLY {
            explicit_bzero(private_key_data, sizeof(private_key_data));
        }
    }
    END_TRY;

    return success;
}
