#include "os.h"
#include "cx.h"
#include <stdbool.h>
#include <stdlib.h>
#include "utils.h"
#include "menu.h"
#include "bech32.h"
#include "globals.h"
#include <uint256.h>

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

bool getPrivateKey(uint32_t account, uint32_t addressIndex, cx_ecfp_private_key_t *privateKey) {
    uint8_t privateKeyData[32];
    uint32_t bip32Path[BIP32_PATH];
    bool success = true;

    os_memmove(bip32Path, derivePath, sizeof(derivePath));

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

// make the eGLD amount look pretty. Add decimals, decimal point and ticker name
bool makeAmountPretty(char *amount, size_t max_size, network_t network) {
    int len = strlen(amount);
    if ((size_t)len + PRETTY_SIZE >= max_size) {
        return false;
    }
    int missing = DECIMAL_PLACES - len + 1;
    if (missing > 0) {
        os_memmove(amount + missing, amount, len + 1);
        os_memset(amount, '0', missing);
    }
    len = strlen(amount);
    int dotPos = len - DECIMAL_PLACES;
    os_memmove(amount + dotPos + 1, amount + dotPos, DECIMAL_PLACES + 1);
    amount[dotPos] = '.';
    while (amount[strlen(amount) - 1] == '0') {
        amount[strlen(amount) - 1] = '\0';
    }
    if (amount[strlen(amount) - 1] == '.') {
        amount[strlen(amount) - 1] = '\0';
    }
    char suffix[MAX_TICKER_LEN+2] = " \0"; // 2 = leading space + trailing \0
    os_memmove(suffix + 1, TICKER_MAINNET, sizeof(TICKER_MAINNET));
    if (network == NETWORK_TESTNET) {
        os_memmove(suffix + 1, TICKER_TESTNET, sizeof(TICKER_TESTNET));
    }
    os_memmove(amount + strlen(amount), suffix, strlen(suffix) + 1);

    return true;
}

void computeDataSize(char *base64, uint32_t b64len) {
    // calculate the ASCII size of the data field
    tx_context.data_size = b64len;
    // take padding bytes into consideration
    if (tx_context.data_size < MAX_DISPLAY_DATA_SIZE)
        if (b64len > 1) {
            if (base64[b64len - 1] == '=')
                tx_context.data_size--;
            if (base64[b64len - 2] == '=')
                tx_context.data_size--;
        }
    int len = sizeof(tx_context.data);
    // prepare the first display page, which contains the data field size
    char str_size[DATA_SIZE_LEN] = "[Size:       0] ";
    // sprintf equivalent workaround
    for (uint32_t ds = tx_context.data_size, idx = 13; ds > 0; ds /= 10, idx--)
        str_size[idx] = '0' + ds % 10;
    int size_len = strlen(str_size);
    // shift the actual data field to the right in order to make room for inserting the size in the first page
    os_memmove(tx_context.data + size_len, tx_context.data, len - size_len);
    // insert the data size in front of the actual data field
    os_memmove(tx_context.data, str_size, size_len);
    int data_end = size_len + tx_context.data_size;
    if (tx_context.data_size > MAX_DISPLAY_DATA_SIZE)
        data_end = size_len + MAX_DISPLAY_DATA_SIZE;
    tx_context.data[data_end] = '\0';
}

bool parse_int(char *str, size_t size, uint64_t *result) {
    uint64_t min = 0, n = 0;

    for (size_t i = 0; i < size; i++) {
        if (!is_digit(str[i]))
            return false;
        n = n * 10 + str[i] - '0';
        /* ensure there is no integer overflow */
        if (n < min)
            return false;
        min = n;
    }
    *result = n;
    return true;
}

bool is_digit(char c) {
  return c >= '0' && c <= '9';
}

bool gas_to_fee(uint64_t gas_limit, uint64_t gas_price, uint32_t data_size, char *fee, size_t size)
{
    uint128_t x = {0, GAS_PER_DATA_BYTE};
    uint128_t y = {0, data_size};
    uint128_t z;
    uint128_t gas_unit_for_move_balance; 
    // tx fee formula
    // gasUnitForMoveBalance := (minGasLimit + len(data)*gasPerDataByte)
    // txFEE = gasUnitForMoveBalance * GASPRICE + (gasLimit - gasUnitForMoveBalance) * gasPriceModifier * GASPRICE
    mul128(&x, &y, &z);

    x.elements[1] = MIN_GAS_LIMIT;
    add128(&x, &z, &gas_unit_for_move_balance);

    x.elements[1] = gas_limit;
    minus128(&x, &gas_unit_for_move_balance, &y);

    x.elements[1] = GAS_PRICE_DIVIDER;
    divmod128(&y, &x, &z, &y);

    add128(&gas_unit_for_move_balance, &z, &y);
    
    x.elements[1] = gas_price;
    mul128(&x, &y, &z); /* XXX: there is a one-byte overflow in tostring128(), hence size-1 */
    if (!tostring128(&z, 10, fee, size - 1))
    {
        return false;
    }
    return true;
}

bool valid_amount(char *amount, size_t size) {
  for (size_t i = 0; i < size; i++) {
      if (!is_digit(amount[i])) {
            return false;
      }
  }
  return true;
}
