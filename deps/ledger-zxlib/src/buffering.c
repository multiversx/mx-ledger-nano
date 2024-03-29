/*******************************************************************************
 *   (c) 2018 ZondaX GmbH
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 ********************************************************************************/

#include "buffering.h"

// Ram
append_buffer_delegate append_ram_buffer = NULL;
buffer_state_t ram;

// Flash
append_buffer_delegate append_flash_buffer = NULL;
buffer_state_t flash;

void buffering_init(uint8_t *ram_buffer,
                    uint16_t ram_buffer_size,
                    append_buffer_delegate ram_delegate,
                    uint8_t *flash_buffer,
                    uint16_t flash_buffer_size,
                    append_buffer_delegate flash_delegate) {
    append_ram_buffer = ram_delegate;
    append_flash_buffer = flash_delegate;

    ram.data = ram_buffer;
    ram.size = ram_buffer_size;
    ram.pos = 0;
    ram.in_use = 1;
    ram.initialized = 1;

    flash.data = flash_buffer;
    flash.size = flash_buffer_size;
    flash.pos = 0;
    flash.in_use = 0;
    flash.initialized = 1;
}

void buffering_reset() {
    ram.pos = 0;
    ram.in_use = 1;
    flash.pos = 0;
    flash.in_use = 0;
}

int buffering_append(uint8_t *data, int length) {
    if (ram.in_use) {
        if (ram.size - ram.pos >= length) {
            // RAM in use, append to ram if there is enough space
            append_ram_buffer(&ram, data, length);
            ram.pos += length;
        } else {
            // If RAM is not big enough copy memory to flash
            ram.in_use = 0;
            flash.in_use = 1;
            if (ram.pos > 0) {
                buffering_append(ram.data, ram.pos);
            }
            int num_bytes = buffering_append(data, length);
            ram.pos = 0;
            return num_bytes;
        }
    } else {
        // Flash in use, append to flash
        if (flash.size - flash.pos >= length) {
            append_flash_buffer(&flash, data, length);
            flash.pos += length;
        } else {
            return 0;
        }
    }
    return length;
}

buffer_state_t *buffering_get_ram_buffer() {
    return &ram;
}

buffer_state_t *buffering_get_flash_buffer() {
    return &flash;
}

buffer_state_t *buffering_get_buffer() {
    if (ram.in_use) {
        return &ram;
    }
    return &flash;
}
