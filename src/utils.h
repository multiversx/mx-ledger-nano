#ifndef _UTILS_H_
#define _UTILS_H_

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

uint32_t readUint32BE(uint8_t *buffer);

void sendResponse(uint8_t tx, bool approve);

#endif
