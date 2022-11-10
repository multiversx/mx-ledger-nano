#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "cx.h"
#include "os.h"

bool get_private_key(uint32_t account_index,
                     uint32_t address_index,
                     cx_ecfp_private_key_t *private_key);
