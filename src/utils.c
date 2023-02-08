#include "utils.h"
#include "menu.h"
#include "os.h"

// read_uint32_be reads 4 bytes from the buffer and returns an uint32_t with big
// endian encoding
uint32_t read_uint32_be(uint8_t* buffer) {
    return (buffer[0] << 24) | (buffer[1] << 16) | (buffer[2] << 8) | (buffer[3]);
}

void send_response(uint8_t tx, bool approve) {
    uint16_t response = MSG_OK;

    if (!approve) response = ERR_USER_DENIED;
    G_io_apdu_buffer[tx++] = response >> 8;
    G_io_apdu_buffer[tx++] = response & 0xff;
    // Send back the response, do not restart the event loop
    io_exchange(CHANNEL_APDU | IO_RETURN_AFTER_TX, tx);
    // Display back the original UX
    ui_idle();
}

// TODO: refactor this function
void uint32_t_to_char_array(uint32_t const input, char* output) {
    uint32_t const base = 10;
    uint32_t index;
    uint8_t pos = 0;
    for (index = 1; (((index * base) <= input) && (((index * base) / base) == index));
         index *= base)
        ;
    for (; index; index /= base) {
        output[pos++] = '0' + ((input / index) % base);
    }
    output[pos] = '\0';
}

void convert_to_hex_str(char* destination,
                        size_t destination_size,
                        const uint8_t* source,
                        size_t source_size) {
    static char hex[] = "0123456789ABCDEF";
    int i = 0;

    if (source_size * 2 > destination_size) {
        source_size = destination_size / 2;
    }

    for (i = 0; i < (int) source_size; i++) {
        destination[(i * 2)] = hex[((source[i] & 0xF0) >> 4)];
        destination[(i * 2) + 1] = hex[((source[i] & 0x0F) >> 0)];
    }
    destination[i * 2] = '\0';
}


int compute_token_display(const char *input, char *display) {
    if(strlen(input) == 0) {
        return -1;
    }
    // Decode the input string
    char decoded[200]; // TODO: extract constant
    if (!base64decode(decoded, input, strlen(input))) {
        return -1;
    }

    // Extract the hostname and time to live
    int dot1 = -1, dot2 = -1;
    int len = strlen(decoded);
    for (int i = 0; i < len; i++) {
        if (decoded[i] == '.') {
            if (dot1 == -1) {
                dot1 = i;
            } else {
                dot2 = i;
                break;
            }
        }
    }
    if (dot1 == -1 || dot2 == -1) {
        return -1;
    }
    decoded[dot1] = '\0';

    char hostname[100]; // TODO: extract constant
    int ttl; // TODO: extract constant
    memmove(hostname, decoded, dot1 + 1);
    ttl = atoi(decoded + dot2 + 1);

    display = printf("Logging into %s for %d seconds", hostname, ttl);
    return 0;
}
