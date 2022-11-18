#ifndef _UTILS_H_
#define _UTILS_H_

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

uint32_t read_uint32_be(uint8_t* buffer);

void send_response(uint8_t tx, bool approve);

void uint32_t_to_char_array(uint32_t const input, char* output);

void convert_to_hex_str(char* destination,
                        size_t destination_size,
                        const uint8_t* source,
                        size_t source_size);

#endif
