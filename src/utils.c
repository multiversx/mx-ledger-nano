#include "os.h"
#include "cx.h"
#include <stdbool.h>
#include <stdlib.h>
#include "utils.h"
#include "menu.h"
#include "bech32.h"
#include "globals.h"

static const uint32_t HARDENED_OFFSET = 0x80000000;
static const uint32_t derivePath[BIP32_PATH] = {
  44 | HARDENED_OFFSET,
  COIN_TYPE_EGLD | HARDENED_OFFSET,
  0 | HARDENED_OFFSET,
  0 | HARDENED_OFFSET,
  0 | HARDENED_OFFSET
};

// readUint32BE reads 4 bytes from the buffer and returns an uint32_t with big endian encoding
uint32_t readUint32BE(uint8_t *buffer) {
  return (buffer[0] << 24) | (buffer[1] << 16) | (buffer[2] << 8) | (buffer[3]);
}

void getAddressHexFromBinary(uint8_t *publicKey, char *address) {
    const char hex[16] = {'0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'a', 'b', 'c', 'd', 'e', 'f'};
    uint8_t i;

    for (i = 0; i < 32; i++) {
        address[i * 2] = hex[publicKey[i] >> 4];
        address[i * 2 + 1] = hex[publicKey[i] & 0xf];
    }
    address[64] = '\0';
}

void getAddressBech32FromBinary(uint8_t *publicKey, char *address) {
    uint8_t buffer[33];
    char *hrp;

    os_memmove(buffer, publicKey, 32);
    buffer[32] = '\0';
    hrp = HRP;
    bech32EncodeFromBytes(address, hrp, buffer, 33);
}

/* return false in case of error, true otherwise */
bool getPublicKey(uint32_t accountNumber, uint32_t index, uint8_t *publicKeyArray) {
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
            os_memset(&privateKey, 0, sizeof(privateKey));
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

bool getPrivateKey(uint32_t accountNumber, uint32_t index, cx_ecfp_private_key_t *privateKey) {
    uint8_t privateKeyData[32];
    uint32_t bip32Path[BIP32_PATH];
    bool success = true;

    os_memmove(bip32Path, derivePath, sizeof(derivePath));

    bip32Path[2] = accountNumber | HARDENED_OFFSET;
    bip32Path[4] = index | HARDENED_OFFSET;

    BEGIN_TRY {
        TRY {
            os_perso_derive_node_bip32_seed_key(HDW_ED25519_SLIP10, CX_CURVE_Ed25519, bip32Path, BIP32_PATH, privateKeyData, NULL, NULL, 0);
            cx_ecfp_init_private_key(CX_CURVE_Ed25519, privateKeyData, 32, privateKey);
        }
        CATCH_ALL {
            success = false;
        }
        FINALLY {
            os_memset(privateKeyData, 0, sizeof(privateKeyData));
        }
    }
    END_TRY;

    return success;
}

void sendResponse(uint8_t tx, bool approve) {
    uint16_t response = MSG_OK;
    
    if (!approve)
        response = ERR_USER_DENIED;
    G_io_apdu_buffer[tx++] = response >> 8;
    G_io_apdu_buffer[tx++] = response & 0xff;
    // Send back the response, do not restart the event loop
    io_exchange(CHANNEL_APDU | IO_RETURN_AFTER_TX, tx);
    // Display back the original UX
    ui_idle();
}
