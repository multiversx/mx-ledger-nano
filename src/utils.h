#pragma once

#include <stdbool.h>
#include <string.h>
#include <stddef.h>
#include <stdint.h>

#define ARRAY_COUNT(array) (sizeof(array) / sizeof(array[0]))

uint32_t read_uint32_be(uint8_t* buffer);

void send_response(uint8_t tx, bool approve, bool back_to_idle);

bool is_digit(char c);

void uint32_t_to_char_array(uint32_t const input, char* output);

int compute_token_display(const char* encoded_origin,
                          const char* ttl,
                          char* display,
                          size_t max_display_length);

void convert_to_hex_str(char* destination,
                        size_t destination_size,
                        const uint8_t* source,
                        size_t source_size);

#if defined(TARGET_STAX)

void nbgl_reject_message_choice(void);
void nbgl_reject_transaction_choice(void);

#endif
