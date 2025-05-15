#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "os_print.h"

/*
This implementation is based on the documentation found at
https://en.wikipedia.org/wiki/Base64
*/

typedef struct {
    bool is_valid;
    bool has_non_printable_chars;
} base64decode_result_t;

static bool isBase64Char(char c);
static char base64decode_byte(char c);
static base64decode_result_t base64decode(char *decoded, const char *source, size_t len);

// returns true is the char given as parameter is a valid base64 char and false
// otherwise
static bool isBase64Char(char c) {
    return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c == '+') || (c == '/') ||
           (c == '=') || (c >= '0' && c <= '9');
}

// decode base64 byte
static char base64decode_byte(char c) {
    if (c >= 'A' && c <= 'Z') {
        return c - 'A';
    }
    if (c >= 'a' && c <= 'z') {
        return c - 'a' + 26;
    }
    if (c >= '0' && c <= '9') {
        return c - '0' + 52;
    }
    if (c == '+') {
        return 62;
    }
    if (c == '/') {
        return 63;
    }
    return 0;
}

// decode base64 data
static base64decode_result_t base64decode(char *decoded, const char *source, size_t len) {
    base64decode_result_t result = {.is_valid = false, .has_non_printable_chars = false};
    for (size_t i = 0; i < len / 4; i++) {
        uint32_t data = 0;
        for (int j = 0; j < 4; j++) {
            char c = source[i * 4 + j];
            if (!isBase64Char(c)) {
                return result;
            }
            data <<= 6;
            data |= base64decode_byte(c);
        }
        decoded[i * 3] = data >> 16;
        decoded[i * 3 + 1] = (data >> 8) & 0xFF;
        decoded[i * 3 + 2] = data & 0xFF;
    }
    decoded[len / 4 * 3] = '\0';
    result.is_valid = true;
    // replace non-printable characters with '?'
    for (size_t i = 0; i < len / 4 * 3; i++) {
        if ((decoded[i] > 0 && decoded[i] < 9) || (decoded[i] > 13 && decoded[i] < 32) ||
            decoded[i] > 126) {
            decoded[i] = '?';
            result.has_non_printable_chars = true;
        }
    }
    return result;
}
