#include "os.h"
#include "cx.h"
#include "globals.h"

#ifndef _SIGN_MSG_H_
#define _SIGN_MSG_H_

static const char const PREPEND[] = "\x17"
                                    "Elrond Signed Message:\n";

typedef enum {
  APP_STATE_IDLE,
  APP_STATE_SIGNING_MESSAGE
} app_state_t;

typedef struct {
    uint32_t len;
    uint8_t hash[32];
    char strhash[65];
    cx_sha3_t sha3;
    app_state_t state;
    uint8_t signature[64];
} msg_context_t;

static msg_context_t msg_context;

void handleSignMsg(uint8_t p1, uint8_t p2, uint8_t *dataBuffer, uint16_t dataLength, volatile unsigned int *flags, volatile unsigned int *tx);

#endif
