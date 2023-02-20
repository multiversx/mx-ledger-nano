#include "utils.h"
#include "menu.h"
#include "os.h"
#include "base64.h"

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

void int_to_char_array(const int input, char* result, int max_size) {
    /*
        Converts an integer to a char array. Example: 123 -> ['1', '2', '3']
    */
    char digits[max_size];
    int current_digit_index = 0;
    int input_copy = input;
    while (input_copy > 0) {
        int div = input_copy / 10;
        int mod = input_copy % 10;
        digits[current_digit_index++] = (char) (mod + '0');
        input_copy = div;
    }

    if (current_digit_index + 1 > max_size) {
        memmove(result, "N/A", 3);
        return;
    }

    int current_result_index = 0;
    for (int i = current_digit_index - 1; i >= 0; i--) {
        result[current_result_index++] = digits[i];
    }
    result[current_result_index] = '\0';
}

int myAtoi(const char* str) {
    int res = 0;

    for (int i = 0; str[i] != '\0'; ++i) {
        if (!is_digit(str[i])) {
            return 0;
        }
        res = res * 10 + str[i] - '0';
    }

    return res;
}

void seconds_to_time(const char* input, char* output, int max_size) {
    /*
        Converts number of seconds to a time string. Example: 123 -> "2min 3 sec."; 1234 -> "20min
       34 sec."; 12345 -> "3h 25min 45 sec."
    */
    int h, m, s;
    int num_seconds = myAtoi(input);
    if (num_seconds == 0) {  // invalid TTL
        memmove(output, "N/A time", 8);
        output[8] = '\0';
        return;
    }

    char temp_output[max_size];
    int current_temp_output_index = 0;

    h = num_seconds / 3600;
    m = (num_seconds - (3600 * h)) / 60;
    s = (num_seconds - (3600 * h) - (m * 60));

    if (h > 0) {
        char hours_char_arr[10];
        int_to_char_array(h, hours_char_arr, 10);
        memmove(temp_output, hours_char_arr, strlen(hours_char_arr));
        current_temp_output_index += strlen(hours_char_arr);
        memmove(temp_output + current_temp_output_index, "h ", 2);
        current_temp_output_index += 2;
        temp_output[current_temp_output_index] = '\0';
    }
    if (m > 0) {
        char minutes_char_arr[10];
        int_to_char_array(m, minutes_char_arr, 10);
        memmove(temp_output + current_temp_output_index,
                minutes_char_arr,
                strlen(minutes_char_arr));
        current_temp_output_index += strlen(minutes_char_arr);
        memmove(temp_output + current_temp_output_index, "min ", 4);
        current_temp_output_index += 4;
        temp_output[current_temp_output_index] = '\0';
    }

    if (s > 0) {
        char seconds_char_arr[10];
        int_to_char_array(s, seconds_char_arr, 10);
        memmove(temp_output + current_temp_output_index,
                seconds_char_arr,
                strlen(seconds_char_arr));
        current_temp_output_index += strlen(seconds_char_arr);
        memmove(temp_output + current_temp_output_index, "sec.", 4);
        current_temp_output_index += 4;
        temp_output[current_temp_output_index] = '\0';
    }

    if (current_temp_output_index < max_size) {
        memmove(output, temp_output, current_temp_output_index);
        output[current_temp_output_index] = '\0';
        return;
    }

    memmove(output, input, strlen(input));
    memmove(output + strlen(input), " sec", 4);
    output[strlen(input) + 4] = '\0';
}

int build_authorizing_message(char* display,
                              const char* origin_display,
                              const char* ttl_display,
                              size_t max_display_length) {
    // add "Authorizing "
    char authorizing[13] = "Authorizing ";
    authorizing[12] = '\0';
    size_t current_len = strlen(authorizing);
    if (current_len > max_display_length) {
        return AUTH_TOKEN_INVALID_RET_CODE;
    }
    memmove(display, authorizing, strlen(authorizing));

    // add origin
    if (current_len + strlen(origin_display) > max_display_length) {
        return AUTH_TOKEN_INVALID_RET_CODE;
    }
    memmove(display + current_len, origin_display, strlen(origin_display));
    current_len += strlen(origin_display);

    // add " for "
    char for_token[6] = " for ";
    for_token[5] = '\0';
    if (current_len + strlen(for_token) > max_display_length) {
        return AUTH_TOKEN_INVALID_RET_CODE;
    }
    memmove(display + current_len, for_token, strlen(for_token));
    current_len += strlen(for_token);

    // add ttl
    if (current_len + strlen(ttl_display) > max_display_length) {
        return AUTH_TOKEN_INVALID_RET_CODE;
    }
    memmove(display + current_len, ttl_display, strlen(ttl_display));
    current_len += strlen(ttl_display);

    display[current_len] = '\0';
    return 0;
}

int compute_token_display(const char* received_origin,
                          const char* received_ttl,
                          char* display,
                          size_t max_display_length) {
    /*
        Receives the origin as base64 and ttl as number of seconds and computes the token display.
        If successful, will write into display something like: "Authorizing host.com for 5min"
    */

    if (strlen(received_origin) == 0) {
        return AUTH_TOKEN_INVALID_RET_CODE;
    }

    // limit the display size of the display and ttl
    char origin_display[MAX_AUTH_TOKEN_ORIGIN_LEN + 1];
    char ttl_display[MAX_AUTH_TOKEN_TTL_LEN + 1];

    int received_origin_len = strlen(received_origin);
    char encoded_origin[received_origin_len];
    memmove(encoded_origin, received_origin, received_origin_len);

    // since the received base64 field does not include padding, manually add it
    int padding_count = strlen(encoded_origin) % 4;
    if (padding_count != 0) {
        for (int j = 0; j < padding_count; j++) {
            encoded_origin[received_origin_len + j] = '=';
        }
        encoded_origin[received_origin_len + padding_count] = '\0';
    }

    // limit the origin display size
    int origin_max_length = strlen(encoded_origin);
    if (origin_max_length > MAX_AUTH_TOKEN_ORIGIN_LEN) {
        origin_max_length = MAX_AUTH_TOKEN_ORIGIN_LEN;
    }

    // try to decode the base64 field
    if (!base64decode(origin_display, encoded_origin, origin_max_length)) {
        return AUTH_TOKEN_INVALID_RET_CODE;
    }

    // base64decode function can return '?' characters at the end. we'll remove them
    size_t decoded_origin_len = strlen(origin_display);
    for (int j = strlen(origin_display) - 1; j > 0; j--) {
        if (origin_display[j] != BASE_64_INVALID_CHAR) {
            break;
        }
        decoded_origin_len--;
    }
    if (decoded_origin_len < strlen(origin_display)) {
        memmove(origin_display, origin_display, decoded_origin_len);
        origin_display[decoded_origin_len] = '\0';
    }

    // if the origin is longer than wanted, add "..." at the end
    if (origin_max_length < (int) (strlen(encoded_origin))) {
        int origin_length = strlen(origin_display);
        origin_display[origin_length - 1] = '.';
        origin_display[origin_length - 2] = '.';
        origin_display[origin_length - 3] = '.';
    }

    // convert the ttl to a display string
    if (strlen(received_ttl) == 0) {
        memmove(ttl_display, "N/A time", 8);
        ttl_display[8] = '\0';
    } else {
        seconds_to_time(received_ttl, ttl_display, MAX_AUTH_TOKEN_TTL_LEN);
    }

    return build_authorizing_message(display, origin_display, ttl_display, max_display_length);
}
