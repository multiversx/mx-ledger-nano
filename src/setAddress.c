#include "utils.h"
#include "os.h"
#include "ux.h"
#include "setAddress.h"

// set the account and address index for the derivation path
void handleSetAddress(uint8_t p1, uint8_t p2, uint8_t *dataBuffer, uint16_t dataLength, volatile unsigned int *flags, volatile unsigned int *tx) {
    UNUSED(p1);
    UNUSED(p2);

    if (dataLength != sizeof(uint32_t) * 2) {
        THROW(ERR_INVALID_ARGUMENTS);
        return;
    }

    uint32_t account, address_index;

    account = readUint32BE(dataBuffer);
    address_index = readUint32BE(dataBuffer + sizeof(uint32_t));

    bip32_account = account;
    bip32_address_index = address_index;
}
