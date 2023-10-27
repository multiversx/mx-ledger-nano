#include "utils.h"
#include "menu.h"
#include "os.h"
#include "base64.h"

#ifdef HAVE_NBGL
#include "nbgl_use_case.h"
#endif

// read_uint32_be reads 4 bytes from the buffer and returns an uint32_t with big
// endian encoding
uint32_t read_uint32_be(uint8_t* buffer) {
    return (buffer[0] << 24) | (buffer[1] << 16) | (buffer[2] << 8) | (buffer[3]);
}

void send_response(uint8_t tx, bool approve, bool back_to_idle) {
    uint16_t response;

    if (approve) {
        response = MSG_OK;
    } else {
        response = ERR_USER_DENIED;
    }

    G_io_apdu_buffer[tx++] = response >> 8;
    G_io_apdu_buffer[tx++] = response & 0xff;
    // Send back the response, do not restart the event loop
    io_exchange(CHANNEL_APDU | IO_RETURN_AFTER_TX, tx);

    if (back_to_idle) {
        // Display back the original UX
        ui_idle();
    }
}

bool is_digit(char c) {
    return c >= '0' && c <= '9';
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

/*
    Converts an integer to a char array. Example: 123 -> ['1', '2', '3']
*/
static void int_to_char_array(const int input, char* result, int max_size) {
    if (input == 0) {
        if (max_size >= 2) {  // the '0' char + the null terminator
            result[0] = '0';
            result[1] = '\0';
            return;
        }
    }
    char digits[MAX_INT_NUM_DIGITS];
    int current_digit_index = 0;
    int input_copy = input;
    while (input_copy > 0) {
        int div = input_copy / 10;
        int mod = input_copy % 10;
        digits[current_digit_index++] = (char) (mod + '0');
        input_copy = div;
    }

    if (current_digit_index >= max_size) {
        const char na_str[] = "N/A";
        if (max_size > (int) (strlen(na_str))) {
            memmove(result, na_str, strlen(na_str) + 1);
        }
        return;
    }

    int current_result_index = 0;
    for (int i = current_digit_index - 1; i >= 0; i--) {
        result[current_result_index++] = digits[i];
    }
    result[current_result_index] = '\0';
}

int atoi(const char* str) {
    int res = 0;

    for (int i = 0; str[i] != '\0'; i++) {
        if (!is_digit(str[i])) {
            return 0;
        }
        res = res * 10 + str[i] - '0';
    }

    return res;
}

/*
   Append the source to destination, at a given index, if the max size is not reached
   It does not add the '\0' char (should be handled by caller)
*/
void append_to_str(const char* source, char* destination, int* index, int max_size) {
    if ((int) (strlen(source) + *index) >= (int) (max_size)) {
        return;
    }

    memmove(destination + *index, source, strlen(source));
    *index += strlen(source);
}

void append_time(char* output, int* index, int value, const char* tag) {
    if (value == 0) {
        return;
    }

    char buffer[MAX_INT_NUM_DIGITS + 1];
    const int max_size = MAX_INT_NUM_DIGITS + 1;
    int_to_char_array(value, buffer, max_size);

    append_to_str(buffer, output, index, max_size);
    append_to_str(tag, output, index, max_size);
}

/*
   Converts number of seconds to a time string. Example: 123 -> "2min 3 sec."; 1234 -> "20min
   34 sec."; 12345 -> "3h 25min 45 sec.". If more than 24h, will display "more than one day"
*/
void seconds_to_time(const char* input, char* output) {
    const int max_size = MAX_AUTH_TOKEN_TTL_SIZE;
    int h, m, s;
    int num_seconds = atoi(input);
    if (num_seconds == 0) {  // invalid TTL
        const char na_str[] = "N/A time";
        if (max_size > (int) (strlen(na_str))) {
            memmove(output, na_str, strlen(na_str) + 1);
        }
        return;
    }

    char temp_output[MAX_AUTH_TOKEN_TTL_SIZE];
    int current_temp_output_index = 0;

    h = num_seconds / 3600;
    m = (num_seconds - (3600 * h)) / 60;
    s = (num_seconds - (3600 * h) - (m * 60));

    if (h > 24) {
        const char more_than_24_msg[] = "more than one day";
        if (max_size > (int) (strlen(more_than_24_msg))) {
            memmove(output, more_than_24_msg, strlen(more_than_24_msg) + 1);
        }
        return;
    }
    append_time(temp_output, &current_temp_output_index, h, "h ");
    append_time(temp_output, &current_temp_output_index, m, "min ");
    append_time(temp_output, &current_temp_output_index, s, "sec.");

    if (current_temp_output_index < max_size) {
        memmove(output, temp_output, current_temp_output_index);
        output[current_temp_output_index] = '\0';
        return;
    }

    const char sec_str[] = " sec";
    if (max_size > (int) (strlen(input) + strlen(sec_str))) {
        memmove(output, input, strlen(input));
        memmove(output + strlen(input), sec_str, strlen(sec_str) + 1);
    }
}

int min(int x, int y) {
    return x < y ? x : y;
}

int replacement_index(int allowed_len, int truncate_str_len) {
    int index = (allowed_len - truncate_str_len) / 2;
    if (index < 0) {
        return 0;
    }

    return index;
}

void truncate_if_needed(const char* source, int max_src_len, char* dest, int max_dest_len) {
    const int len_src = min(max_src_len, (int) strlen(source));
    if (len_src <= max_dest_len) {
        memmove(dest, source, len_src);
        dest[len_src] = 0;
    } else {
        const char truncate_replacement[] = "...";
        const int truncate_str_len = sizeof(truncate_replacement) - 1;
        int truncate_index = replacement_index(max_dest_len, truncate_str_len);
        int remaining_chars_from_src = max_dest_len - truncate_index - truncate_str_len;
        if (remaining_chars_from_src < 0) {
            return;
        }
        int index_after_replacement = truncate_index + truncate_str_len;
        int remaining_source_index = len_src - remaining_chars_from_src;
        if (remaining_source_index < 0) {
            return;
        }

        if (truncate_index > max_dest_len || truncate_index + truncate_str_len > max_dest_len ||
            index_after_replacement + remaining_chars_from_src > max_dest_len ||
            remaining_source_index + remaining_chars_from_src > max_src_len) {
            return;
        }
        memmove(dest, source, (size_t) truncate_index);
        memmove(dest + truncate_index, truncate_replacement, truncate_str_len);
        memmove(dest + index_after_replacement,
                source + remaining_source_index,
                remaining_chars_from_src);
        dest[max_dest_len] = 0;
    }
}

int build_authorizing_message(char* display,
                              const char* origin_display,
                              const char* ttl_display,
                              size_t max_display_size) {
    const char* elements[] = {
        "Authorizing ",
        origin_display,
        " for ",
        ttl_display,
    };

    int num_elements = sizeof(elements) / sizeof(char*);
    int i;
    int final_string_index = 0;
    for (i = 0; i < num_elements; i++) {
        if (final_string_index + strlen(elements[i]) >= max_display_size) {
            return AUTH_TOKEN_INVALID_RET_CODE;
        }

        memmove(display + final_string_index, elements[i], strlen(elements[i]));
        final_string_index += strlen(elements[i]);
    }
    display[final_string_index] = '\0';
    return 0;
}

/*
    Receives the origin as base64 and ttl as number of seconds and computes the token display.
    If successful, will write into display something like: "Authorizing host.com for 5min"
*/
int compute_token_display(const char* received_origin,
                          const char* received_ttl,
                          char* display,
                          size_t max_display_size) {
    if (strlen(received_origin) == 0) {
        return AUTH_TOKEN_INVALID_RET_CODE;
    }

    char decoded_origin_buffer[AUTH_TOKEN_ENCODED_ORIGIN_MAX_SIZE];

    // limit the display size of the display and ttl
    char origin_display[MAX_AUTH_TOKEN_ORIGIN_SIZE];
    char ttl_display[MAX_AUTH_TOKEN_TTL_SIZE];

    int received_origin_size = strlen(received_origin) + 1;
    char encoded_origin[AUTH_TOKEN_ENCODED_ORIGIN_MAX_SIZE +
                        3];  // 3 maximum additional padding chars

    int encoded_origin_size = received_origin_size;
    if (encoded_origin_size > AUTH_TOKEN_ENCODED_ORIGIN_MAX_SIZE) {
        encoded_origin_size = AUTH_TOKEN_ENCODED_ORIGIN_MAX_SIZE;
    }

    int encoded_origin_len = encoded_origin_size - 1;
    memmove(encoded_origin, received_origin, encoded_origin_len);
    encoded_origin[encoded_origin_len] = '\0';

    // since the received base64 field does not include padding, manually add it
    int modifier = strlen(encoded_origin) % 4;
    if (modifier != 0) {
        int padding_count = 4 - modifier;
        for (int j = 0; j < padding_count; j++) {
            encoded_origin[encoded_origin_len + j] = '=';
        }
        encoded_origin[encoded_origin_len + padding_count] = '\0';
    }

    // try to decode the base64 field
    if (!base64decode(decoded_origin_buffer, encoded_origin, strlen(encoded_origin))) {
        return AUTH_TOKEN_INVALID_RET_CODE;
    }

    // don't allow tokens that start with a given prefix to be signed
    if (strncmp(decoded_origin_buffer,
                AUTH_TOKEN_INVALID_ORIGIN_PREFIX,
                strlen(AUTH_TOKEN_INVALID_ORIGIN_PREFIX)) == 0) {
        return AUTH_TOKEN_BAD_REQUEST_RET_CODE;
    }

    // base64decode function can return '?' characters at the end. we'll remove them
    size_t decoded_origin_len = strlen(decoded_origin_buffer);
    for (int j = strlen(decoded_origin_buffer) - 1; j > 0; j--) {
        if (decoded_origin_buffer[j] != BASE_64_INVALID_CHAR) {
            break;
        }
        decoded_origin_len--;
    }
    if (decoded_origin_len < strlen(decoded_origin_buffer)) {
        memmove(decoded_origin_buffer, decoded_origin_buffer, decoded_origin_len);
        decoded_origin_buffer[decoded_origin_len] = '\0';
    }

    truncate_if_needed(decoded_origin_buffer,
                       strlen(decoded_origin_buffer),
                       origin_display,
                       MAX_AUTH_TOKEN_ORIGIN_SIZE);

    // convert the ttl to a display string
    if (strlen(received_ttl) == 0) {
        const char undefined_ttl[] = "N/A time";
        memmove(ttl_display, undefined_ttl, strlen(undefined_ttl) + 1);
    } else {
        seconds_to_time(received_ttl, ttl_display);
    }

    return build_authorizing_message(display, origin_display, ttl_display, max_display_size);
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
