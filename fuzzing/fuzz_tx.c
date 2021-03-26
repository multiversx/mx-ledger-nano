#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "parseTx.h"

tx_context_t tx_context;
tx_hash_context_t tx_hash_context;

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
  memset(&tx_hash_context, 0, sizeof(tx_hash_context));
  tx_hash_context.status = JSON_IDLE;
  parse_data(data, size);
  return 0;
}
