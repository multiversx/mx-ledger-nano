#include "os.h"
#include "cx.h"
#include "globals.h"

void handleSignTx(uint8_t p1, uint8_t p2, uint8_t *dataBuffer, uint16_t dataLength, volatile unsigned int *flags, volatile unsigned int *tx);

#define MAX_AMOUNT_LEN 32
#define MAX_BUFFER_LEN 1024

typedef struct {
    char buffer[MAX_BUFFER_LEN];
    uint16_t bufLen;

    char receiver[FULL_ADDRESS_LENGTH];
    char amount[MAX_AMOUNT_LEN];
    char signature[64];
} tx_context_t;
