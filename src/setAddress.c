#include "globals.h"
#include "utils.h"

// set the account and address index for the derivation path
uint16_t handleSetAddress(uint8_t *dataBuffer, uint16_t dataLength) {
    if (dataLength != sizeof(uint32_t) * 2) {
        return ERR_INVALID_ARGUMENTS;
    }

    uint32_t account, address_index;

    account = readUint32BE(dataBuffer);
    address_index = readUint32BE(dataBuffer + sizeof(uint32_t));

    bip32_account = account;
    bip32_address_index = address_index;

    return MSG_OK;
}
