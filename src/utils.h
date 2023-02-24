#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define ARRAY_COUNT(array) (sizeof(array) / sizeof(array[0]))

uint32_t read_uint32_be(uint8_t* buffer);

void send_response(uint8_t tx, bool approve, bool back_to_idle);

void uint32_t_to_char_array(uint32_t const input, char* output);

void convert_to_hex_str(char* destination,
                        size_t destination_size,
                        const uint8_t* source,
                        size_t source_size);
