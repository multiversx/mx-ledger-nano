#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef HAVE_NBGL
#include "nbgl_page.h"
#endif

#define ARRAY_COUNT(array) (sizeof(array) / sizeof(array[0]))

uint32_t read_uint32_be(uint8_t* buffer);

void send_response(uint8_t tx, bool approve);

void uint32_t_to_char_array(uint32_t const input, char* output);

void convert_to_hex_str(char* destination,
                        size_t destination_size,
                        const uint8_t* source,
                        size_t source_size);


#ifdef HAVE_NBGL
extern nbgl_page_t *pageContext;
void releaseContext(void);
#endif
