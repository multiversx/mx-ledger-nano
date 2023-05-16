#include "utils.h"
#include "menu.h"
#include "os.h"

#ifdef HAVE_NBGL
#include "nbgl_use_case.h"
#endif

// read_uint32_be reads 4 bytes from the buffer and returns an uint32_t with big
// endian encoding
uint32_t read_uint32_be(uint8_t* buffer) {
    return (buffer[0] << 24) | (buffer[1] << 16) | (buffer[2] << 8) | (buffer[3]);
}

void send_response(uint8_t tx, bool approve, bool back_to_idle) {
    PRINTF("send_response\n");
    uint16_t response;

    if (approve) {
        PRINTF("approve\n");
        response = MSG_OK;
    } else {
        PRINTF("!approve\n");
        response = ERR_USER_DENIED;
    }

    PRINTF("G_io_apdu_buffer\n");
    G_io_apdu_buffer[tx++] = response >> 8;
    G_io_apdu_buffer[tx++] = response & 0xff;
    // Send back the response, do not restart the event loop
    PRINTF("pre io_exchange_custom\n");
    io_exchange(CHANNEL_APDU | IO_RETURN_AFTER_TX, tx);
    PRINTF("post io_exchange_custom\n");

    if (back_to_idle) {
        PRINTF("back_to_idle\n");
        // Display back the original UX
        ui_idle();
    }
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

#if defined(TARGET_STAX)

static void message_rejection(void) {
    send_response(0, false, false);
    nbgl_useCaseStatus("Message\nrejected", false, ui_idle);
}

void nbgl_reject_message_choice(void) {
    nbgl_useCaseConfirm("Reject message?",
                        NULL,
                        "Yes, reject",
                        "Go back to message",
                        message_rejection);
}

static void transaction_rejection(void) {
    send_response(0, false, false);
    nbgl_useCaseStatus("Transaction\nrejected", false, ui_idle);
}

void nbgl_reject_transaction_choice(void) {
    nbgl_useCaseConfirm("Reject transaction?",
                        NULL,
                        "Yes, reject",
                        "Go back to transaction",
                        transaction_rejection);
}

#endif
