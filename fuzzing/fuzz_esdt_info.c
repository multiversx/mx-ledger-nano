#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>

#include "provide_ESDT_info.h"

static esdt_info_t *esdt_info_heap = NULL;

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    if (esdt_info_heap == NULL) {
        esdt_info_heap = malloc(sizeof(*esdt_info_heap));
    }

    handle_provide_ESDT_info(data, size, esdt_info_heap);

    return 0;
}
