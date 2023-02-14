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

void int_to_char_array(const int input, char* result, size_t max_size) {
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

int myAtoi(char* str) {
    int res = 0;

    for (int i = 0; str[i] != '\0'; ++i) res = res * 10 + str[i] - '0';

    return res;
}

void seconds_to_time(char* input, char* output, size_t max_size) {
    /*
        Converts number of seconds to a time string. Example: 123 -> "2min 3 sec."; 1234 -> "20min
       34 sec."; 12345 -> "3h 25min 45 sec."
    */
    int h, m, s;
    int num_seconds = myAtoi(input);

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
    }

    if (current_temp_output_index < max_size) {
        memmove(output, temp_output, current_temp_output_index);
        output[current_temp_output_index] = '\0';
        return;
    }

    memmove(output, input, strlen(input));
    memmove(output + strlen(input), " sec", 8);
}

int compute_token_display(const char* input, char* display) {
    /*
        Tries to decode the auth token to extract the hostname and the ttl. Returns -1 if the token
       is invalid. Example:
        bG9jYWxob3N0.f68177510756edce45eca84b94544a6eacdfa36e69dfd3b8f24c4010d1990751.300.eyJ0aW1lc3RhbXAiOjE2NzM5NzIyNDR9
            ^                                                                         ^
        hostname                                                                    ttl

        will write into display: "Authorizing localhost for 5min"
    */

    if (strlen(input) == 0) {
        return -1;
    }

    char hostname[36];
    char encoded_hostname[100];
    char ttl[10];

    int second_dot_position = 0;
    int dot_count = 0;
    int hostname_len = 0;
    int ttl_len = 0;

    char ttl_display[40];

    for (int i = 0; input[i]; i++) {
        if (input[i] != '.') {
            continue;
        }
        dot_count++;
        if (dot_count == 1) {
            hostname_len = i;
            memmove(encoded_hostname, input, hostname_len);
            encoded_hostname[hostname_len] = '\0';

            int padding_count = strlen(encoded_hostname) % 4;
            if (padding_count != 0) {
                for (int j = 0; j < padding_count; j++) {
                    encoded_hostname[hostname_len + j] = '=';
                }
                encoded_hostname[hostname_len + padding_count] = '\0';
            }
            size_t encoded_len = strlen(encoded_hostname);
            if (encoded_len > 36) {
                encoded_len = 36;
            }
            if (!base64decode(hostname, encoded_hostname, encoded_len)) {
                memmove(display, "l64", 3);
                return -1;
            }
            size_t decoded_hostname_len = strlen(hostname);
            for (int j = strlen(hostname) - 1; j > 0; j--) {
                if (hostname[j] != BASE_64_INVALID_CHAR) {
                    break;
                }
                decoded_hostname_len--;
            }
            if (decoded_hostname_len < strlen(hostname)) {
                memmove(hostname, hostname, decoded_hostname_len);
                hostname[decoded_hostname_len] = '\0';
            }
            if (encoded_len < strlen(encoded_hostname)) {
                int hostname_length = strlen(hostname);
                hostname[hostname_length - 1] = '.';
                hostname[hostname_length - 2] = '.';
                hostname[hostname_length - 3] = '.';
            }
        } else if (dot_count == 2) {
            second_dot_position = i;
        } else if (dot_count == 3) {
            ttl_len = i - second_dot_position - 1;
            if (ttl_len > 10) {
                memmove(ttl_display, "N/A time", 8);
                ttl_display[8] = '\0';
            } else {
                seconds_to_time(ttl, ttl_display, 40);
            }
            memmove(ttl, input + i - ttl_len, ttl_len);
            ttl[ttl_len] = '\0';
            break;
        }
    }

    if (dot_count < 3) {
        return -1;
    }

    memmove(display, "Authorizing ", 12);
    memmove(display + 12, hostname, strlen(hostname));
    memmove(display + 12 + strlen(hostname), " for ", 5);
    memmove(display + 12 + strlen(hostname) + 5, ttl_display, strlen(ttl_display));
    display[12 + strlen(hostname) + 5 + strlen(ttl_display)] = '\0';

    return 0;
}
