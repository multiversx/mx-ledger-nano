#ifndef _UTILS_H_
#define _UTILS_H_

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

uint32_t read_uint32_be(uint8_t *buffer);

void send_response(uint8_t tx, bool approve);

#endif
