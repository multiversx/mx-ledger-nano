#include "globals.h"
#include "utils.h"

// set the account and address index for the derivation path
uint16_t handle_set_address(uint8_t *data_buffer, uint16_t data_length) {
    if (data_length != sizeof(uint32_t) * 2) {
        return ERR_INVALID_ARGUMENTS;
    }

    uint32_t account, address_index;

    account = read_uint32_be(data_buffer);
    address_index = read_uint32_be(data_buffer + sizeof(uint32_t));

    bip32_account = account;
    bip32_address_index = address_index;

    return MSG_OK;
}
