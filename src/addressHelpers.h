#ifndef _ADDRESS_HELPERS_H_
#define _ADDRESS_HELPERS_H_

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

bool get_public_key(uint32_t accountNumber, uint32_t index, uint8_t *publicKeyArray);
void get_address_hex_from_binary(uint8_t *publicKey, char *address);
void get_address_bech32_from_binary(uint8_t *publicKey, char *address);

#endif
