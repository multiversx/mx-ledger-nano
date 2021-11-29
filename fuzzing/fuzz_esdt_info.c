#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "provide_ESDT_info.h"

esdt_info_t esdt_info;

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
  handle_provide_ESDT_info(data, size);
  return 0;
}
