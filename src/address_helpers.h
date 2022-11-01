#ifndef _ADDRESS_HELPERS_H_
#define _ADDRESS_HELPERS_H_

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

bool get_public_key(uint32_t account_number, uint32_t index,
                    uint8_t *public_key_array);
void get_address_hex_from_binary(const uint8_t *public_key, char *address);
void get_address_bech32_from_binary(const uint8_t *public_key, char *address);

#endif
