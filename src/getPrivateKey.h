#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "os.h"
#include "cx.h"

bool getPrivateKey(uint32_t account, uint32_t addressIndex, cx_ecfp_private_key_t *privateKey);
