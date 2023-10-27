/*******************************************************************************
 *   (c) 2016 Ledger
 *   (c) 2018 ZondaX GmbH
 *   (c) 2020 Elrond Ltd
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

#include "get_address.h"
#include "globals.h"
#include "menu.h"
#include "provide_ESDT_info.h"
#include "set_address.h"
#include "sign_msg.h"
#include "sign_msg_auth_token.h"
#include "sign_tx_hash.h"
#include "utils.h"

#define CLA                       0xED
#define INS_GET_APP_VERSION       0x01
#define INS_GET_APP_CONFIGURATION 0x02
#define INS_GET_ADDR              0x03
#define INS_SIGN_TX               0x04
#define INS_SET_ADDR              0x05
#define INS_SIGN_MSG              0x06
#define INS_SIGN_TX_HASH          0x07
#define INS_PROVIDE_ESDT_INFO     0x08
#define INS_GET_ADDR_AUTH_TOKEN   0x09

#define OFFSET_CLA   0
#define OFFSET_INS   1
#define OFFSET_P1    2
#define OFFSET_P2    3
#define OFFSET_LC    4
#define OFFSET_CDATA 5

unsigned char G_io_seproxyhal_spi_buffer[IO_SEPROXYHAL_BUFFER_SIZE_B];
esdt_info_t esdt_info;

#ifdef HAVE_BAGL
void io_seproxyhal_display(const bagl_element_t *element);
#endif

void handle_apdu(volatile unsigned int *flags, volatile unsigned int *tx);
void elrond_main(void);
unsigned char io_event(unsigned char channel);
unsigned short io_exchange_al(unsigned char channel, unsigned short tx_len);
void app_exit(void);
void nv_app_state_init();

void handle_apdu(volatile unsigned int *flags, volatile unsigned int *tx) {
    unsigned short sw = 0;
    uint16_t ret;

    BEGIN_TRY {
        TRY {
            if (G_io_apdu_buffer[OFFSET_CLA] != CLA) {
                THROW(ERR_WRONG_CLA);
            }

            switch (G_io_apdu_buffer[OFFSET_INS]) {
                case INS_GET_APP_VERSION:
                    *tx = strlen(APPVERSION);
                    memcpy(G_io_apdu_buffer, APPVERSION, *tx);
                    THROW(MSG_OK);
                    break;

                case INS_GET_APP_CONFIGURATION:
                    G_io_apdu_buffer[0] = (N_storage.setting_contract_data ? 0x01 : 0x00);
                    // G_io_apdu_buffer[1] and G_io_apdu_buffer[2] are not to be taken into
                    // account anymore since now those variables are 32 bit long, but we
                    // still expect 6 bytes transmitted to maintain compatibility with the
                    // web wallet. Account index should be read from bytes 6->9, while
                    // address index should be read from bytes 10->13 (Big Endian)
                    G_io_apdu_buffer[3] = LEDGER_MAJOR_VERSION;
                    G_io_apdu_buffer[4] = LEDGER_MINOR_VERSION;
                    G_io_apdu_buffer[5] = LEDGER_PATCH_VERSION;

                    G_io_apdu_buffer[6] = bip32_account >> 24;
                    G_io_apdu_buffer[7] = bip32_account >> 16;
                    G_io_apdu_buffer[8] = bip32_account >> 8;
                    G_io_apdu_buffer[9] = bip32_account;

                    G_io_apdu_buffer[10] = bip32_address_index >> 24;
                    G_io_apdu_buffer[11] = bip32_address_index >> 16;
                    G_io_apdu_buffer[12] = bip32_address_index >> 8;
                    G_io_apdu_buffer[13] = bip32_address_index;

                    *tx = 14;
                    THROW(MSG_OK);
                    break;

                case INS_GET_ADDR:
                    handle_get_address(G_io_apdu_buffer[OFFSET_P1],
                                       G_io_apdu_buffer[OFFSET_P2],
                                       G_io_apdu_buffer + OFFSET_CDATA,
                                       G_io_apdu_buffer[OFFSET_LC],
                                       flags,
                                       tx);
                    break;

                case INS_GET_ADDR_AUTH_TOKEN:
                    handle_auth_token(G_io_apdu_buffer[OFFSET_P1],
                                      G_io_apdu_buffer + OFFSET_CDATA,
                                      G_io_apdu_buffer[OFFSET_LC],
                                      flags);
                    break;

                case INS_SET_ADDR:
                    ret = handle_set_address(G_io_apdu_buffer + OFFSET_CDATA,
                                             G_io_apdu_buffer[OFFSET_LC]);
                    THROW(ret);
                    break;

                case INS_SIGN_TX:
                    // sign tx is deprecated in this version. Hash signing should be used
                    THROW(ERR_SIGN_TX_DEPRECATED);
                    break;

                case INS_SIGN_MSG:
                    handle_sign_msg(G_io_apdu_buffer[OFFSET_P1],
                                    G_io_apdu_buffer + OFFSET_CDATA,
                                    G_io_apdu_buffer[OFFSET_LC],
                                    flags);
                    break;

                case INS_SIGN_TX_HASH:
                    handle_sign_tx_hash(G_io_apdu_buffer[OFFSET_P1],
                                        G_io_apdu_buffer + OFFSET_CDATA,
                                        G_io_apdu_buffer[OFFSET_LC],
                                        flags);
                    break;

                case INS_PROVIDE_ESDT_INFO:
                    ret = handle_provide_ESDT_info(G_io_apdu_buffer + OFFSET_CDATA,
                                                   G_io_apdu_buffer[OFFSET_LC],
                                                   &esdt_info);
                    THROW(ret);
                    break;

                default:
                    THROW(ERR_UNKNOWN_INSTRUCTION);
                    break;
            }
        }
        CATCH(EXCEPTION_IO_RESET) {
            THROW(EXCEPTION_IO_RESET);
        }
        CATCH_OTHER(e) {
            switch (e & 0xF000) {
                case 0x6000:
                    sw = e;
                    break;
                case MSG_OK:
                    // All is well
                    sw = e;
                    break;
                default:
                    // Internal error
                    sw = 0x6800 | (e & 0x7FF);
                    break;
            }
            // Unexpected exception => report
            G_io_apdu_buffer[*tx] = sw >> 8;
            G_io_apdu_buffer[*tx + 1] = sw;
            *tx += 2;
        }
        FINALLY {
        }
    }
    END_TRY;
}

void elrond_main(void) {
    volatile unsigned int rx = 0;
    volatile unsigned int tx = 0;
    volatile unsigned int flags = 0;

    init_msg_context();
    init_tx_context();
    esdt_info.valid = false;

    // DESIGN NOTE: the bootloader ignores the way APDU are fetched. The only
    // goal is to retrieve APDU.
    // When APDU are to be fetched from multiple IOs, like NFC+USB+BLE, make
    // sure the io_event is called with a
    // switch event, before the apdu is replied to the bootloader. This avoid
    // APDU injection faults.
    for (;;) {
        volatile unsigned short sw = 0;

        BEGIN_TRY {
            TRY {
                rx = tx;
                tx = 0;  // ensure no race in catch_other if io_exchange throws
                         // an error
                rx = io_exchange(CHANNEL_APDU | flags, rx);
                flags = 0;

                // no apdu received, well, reset the session, and reset the
                // bootloader configuration
                if (rx == 0) {
                    THROW(0x6982);
                }

                handle_apdu(&flags, &tx);
            }
            CATCH(EXCEPTION_IO_RESET) {
                THROW(EXCEPTION_IO_RESET);
            }
            CATCH_OTHER(e) {
                switch (e & 0xF000) {
                    case 0x6000:
                        sw = e;
                        break;
                    case MSG_OK:
                        // All is well
                        sw = e;
                        break;
                    default:
                        // Internal error
                        sw = 0x6800 | (e & 0x7FF);
                        break;
                }
                if (e != MSG_OK) {
                    flags &= ~IO_ASYNCH_REPLY;
                }
                // Unexpected exception => report
                G_io_apdu_buffer[tx] = sw >> 8;
                G_io_apdu_buffer[tx + 1] = sw;
                tx += 2;
            }
            FINALLY {
            }
        }
        END_TRY;
    }

    // return_to_dashboard:
    return;
}

#ifdef HAVE_BAGL
// override point, but nothing more to do
void io_seproxyhal_display(const bagl_element_t *element) {
    io_seproxyhal_display_default((bagl_element_t *) element);
}
#endif

unsigned char io_event(unsigned char channel) {
    // nothing done with the event, throw an error on the transport layer if
    // needed
    (void) (channel);

    // can't have more than one tag in the reply, not supported yet.
    switch (G_io_seproxyhal_spi_buffer[0]) {
        case SEPROXYHAL_TAG_FINGER_EVENT:
            UX_FINGER_EVENT(G_io_seproxyhal_spi_buffer);
            break;

        case SEPROXYHAL_TAG_BUTTON_PUSH_EVENT:
#ifdef HAVE_BAGL
            UX_BUTTON_PUSH_EVENT(G_io_seproxyhal_spi_buffer);
#endif  // HAVE_BAGL

            break;

        case SEPROXYHAL_TAG_STATUS_EVENT:
            if (G_io_apdu_media == IO_APDU_MEDIA_USB_HID &&
                !(U4BE(G_io_seproxyhal_spi_buffer, 3) &
                  SEPROXYHAL_TAG_STATUS_EVENT_FLAG_USB_POWERED)) {
                THROW(EXCEPTION_IO_RESET);
            }
            /* fallthrough */
            __attribute__((fallthrough));
        case SEPROXYHAL_TAG_DISPLAY_PROCESSED_EVENT:
#ifdef HAVE_BAGL
            UX_DISPLAYED_EVENT({});
#endif  // HAVE_BAGL
#ifdef HAVE_NBGL
            UX_DEFAULT_EVENT();
#endif  // HAVE_NBGL
            break;

        case SEPROXYHAL_TAG_TICKER_EVENT:
            UX_TICKER_EVENT(G_io_seproxyhal_spi_buffer, {
#if defined(TARGET_NANOS)
                if (UX_ALLOWED) {
                    if (ux_step_count) {
                        // prepare next screen
                        ux_step = (ux_step + 1) % ux_step_count;
                        // redisplay screen
                        UX_REDISPLAY();
                    }
                }
#endif  // TARGET_NANOX
            });
            break;
        default:
            UX_DEFAULT_EVENT();
            break;
    }

    // close the event if not done previously (by a display or whatever)
    if (!io_seproxyhal_spi_is_status_sent()) {
        io_seproxyhal_general_status();
    }

    // command has been processed, DO NOT reset the current APDU transport
    return 1;
}

unsigned short io_exchange_al(unsigned char channel, unsigned short tx_len) {
    switch (channel & ~(IO_FLAGS)) {
        case CHANNEL_KEYBOARD:
            break;

        // multiplexed io exchange over a SPI channel and TLV encapsulated protocol
        case CHANNEL_SPI:
            if (tx_len) {
                io_seproxyhal_spi_send(G_io_apdu_buffer, tx_len);

                if (channel & IO_RESET_AFTER_REPLIED) {
                    reset();
                }
                return 0;  // nothing received from the master so far (it's a tx
                           // transaction)
            } else {
                return io_seproxyhal_spi_recv(G_io_apdu_buffer, sizeof(G_io_apdu_buffer), 0);
            }

        default:
            THROW(INVALID_PARAMETER);
    }
    return 0;
}

void app_exit(void) {
    BEGIN_TRY_L(exit) {
        TRY_L(exit) {
            os_sched_exit(-1);
        }
        FINALLY_L(exit) {
        }
    }
    END_TRY_L(exit);
}

void nv_app_state_init() {
    if (N_storage.initialized != 0x01) {
        internal_storage_t storage;
        storage.setting_contract_data = DEFAULT_CONTRACT_DATA;
        storage.initialized = 0x01;
        nvm_write((internal_storage_t *) &N_storage, (void *) &storage, sizeof(internal_storage_t));
    }
}

__attribute__((section(".boot"))) int main(void) {
    // exit critical section
    __asm volatile("cpsie i");

    // ensure exception will work as planned
    os_boot();

    for (;;) {
        UX_INIT();

        BEGIN_TRY {
            TRY {
                io_seproxyhal_init();

                nv_app_state_init();

                USB_power(0);
                USB_power(1);

                ui_idle();  // main menu

#ifdef HAVE_BLE
                BLE_power(0, NULL);
                BLE_power(1, "Nano X");
#endif  // HAVE_BLE

                elrond_main();
            }
            CATCH(EXCEPTION_IO_RESET) {
                // reset IO and UX before continuing
                continue;
            }
            CATCH_ALL {
                break;
            }
            FINALLY {
            }
        }
        END_TRY;
    }
    app_exit();
    return 0;
}
