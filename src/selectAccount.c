#include "selectAccount.h"
#include "os.h"
#include "cx.h"
#include "utils.h"

#define ACCOUNT_MENU       true
#define ADDRESS_INDEX_MENU false

bool menu_selection;
static char account[4], address_index[4]; // hold the int_to_string representations
char intAccount, intAddressIndex; // temporary copies of NV variables

void intToString();
static unsigned int select_address_index_ui_button(unsigned int button_mask, unsigned int button_mask_counter);
static unsigned int select_account_ui_button(unsigned int button_mask, unsigned int button_mask_counter);
void ui_address_menu_idle();
void selectAccount();

//////////////////////////////////////////////////////////////////////////////////////
// UI for selecting the Account
static const bagl_element_t select_account_ui[] = {
    {
        {BAGL_RECTANGLE, 0x00, 0, 0, 128, 32, 0, 0, BAGL_FILL, 0x000000,
         0xFFFFFF, 0, 0},
        NULL,
    },
    {
        {BAGL_LABELINE, 0x01, 0, 14, 128, 16, 0, 0, 0, 0xFFFFFF, 0x000000,
         BAGL_FONT_OPEN_SANS_EXTRABOLD_11px | BAGL_FONT_ALIGNMENT_LEFT, 0},
        "Account",
    },
    {
        {BAGL_LABELINE, 0x01, 87, 14, 128, 16, 0, 0, 0, 0xFFFFFF, 0x000000,
         BAGL_FONT_OPEN_SANS_EXTRABOLD_11px | BAGL_FONT_ALIGNMENT_LEFT, 0},
        account,
    },
    {
        {BAGL_LABELINE, 0x01, 0, 32, 128, 16, 0, 0, 0, 0xFFFFFF, 0x000000,
         BAGL_FONT_OPEN_SANS_REGULAR_11px | BAGL_FONT_ALIGNMENT_LEFT, 0},
        "Address",
    },
    {
        {BAGL_LABELINE, 0x01, 90, 32, 128, 16, 0, 0, 0, 0xFFFFFF, 0x000000,
         BAGL_FONT_OPEN_SANS_REGULAR_11px | BAGL_FONT_ALIGNMENT_LEFT, 0},
        address_index,
    },
    {
        {BAGL_ICON, 0x00, 73, 7, 7, 7, 0, 0, 0, 0xFFFFFF, 0x000000, 0,
         BAGL_GLYPH_ICON_LEFT},
        NULL,
    },
    {
        {BAGL_ICON, 0x00, 120, 7, 7, 7, 0, 0, 0, 0xFFFFFF, 0x000000, 0,
         BAGL_GLYPH_ICON_RIGHT},
        NULL,
    },
};

//////////////////////////////////////////////////////////////////////////////////////
// UI for selecting the Address index
static const bagl_element_t select_address_index_ui[] = {
    {
        {BAGL_RECTANGLE, 0x00, 0, 0, 128, 32, 0, 0, BAGL_FILL, 0x000000,
         0xFFFFFF, 0, 0},
        NULL,
    },
    {
        {BAGL_LABELINE, 0x01, 0, 14, 128, 16, 0, 0, 0, 0xFFFFFF, 0x000000,
         BAGL_FONT_OPEN_SANS_REGULAR_11px | BAGL_FONT_ALIGNMENT_LEFT, 0},
        "Account",
    },
    {
        {BAGL_LABELINE, 0x01, 90, 14, 128, 16, 0, 0, 0, 0xFFFFFF, 0x000000,
         BAGL_FONT_OPEN_SANS_REGULAR_11px | BAGL_FONT_ALIGNMENT_LEFT, 0},
        account,
    },
    {
        {BAGL_LABELINE, 0x01, 0, 32, 128, 16, 0, 0, 0, 0xFFFFFF, 0x000000,
         BAGL_FONT_OPEN_SANS_EXTRABOLD_11px | BAGL_FONT_ALIGNMENT_LEFT, 0},
        "Address",
    },
    {
        {BAGL_LABELINE, 0x01, 87, 32, 128, 16, 0, 0, 0, 0xFFFFFF, 0x000000,
         BAGL_FONT_OPEN_SANS_EXTRABOLD_11px | BAGL_FONT_ALIGNMENT_LEFT, 0},
        address_index,
    },
    {
        {BAGL_ICON, 0x00, 73, 25, 7, 7, 0, 0, 0, 0xFFFFFF, 0x000000, 0,
         BAGL_GLYPH_ICON_LEFT},
        NULL,
    },
    {
        {BAGL_ICON, 0x00, 120, 25, 7, 7, 0, 0, 0, 0xFFFFFF, 0x000000, 0,
         BAGL_GLYPH_ICON_RIGHT},
        NULL,
    },
};

//////////////////////////////////////////////////////////////////////////////////////

void intToString() {
    account[0] = intAccount / 100 + 48;
    account[1] = intAccount / 10 % 10 + 48;
    account[2] = intAccount % 10 + 48;
    account[3] = '\0';
    address_index[0] = intAddressIndex / 100 + 48;
    address_index[1] = intAddressIndex / 10 % 10 + 48;
    address_index[2] = intAddressIndex % 10 + 48;
    address_index[3] = '\0';
}

//////////////////////////////////////////////////////////////////////////////////////

static unsigned int select_address_index_ui_button(unsigned int button_mask, unsigned int button_mask_counter) {
    switch (button_mask) {
    case BUTTON_EVT_RELEASED | BUTTON_LEFT | BUTTON_RIGHT: // EXIT
        if (menu_selection == ACCOUNT_MENU) {
            menu_selection = ADDRESS_INDEX_MENU;
            ui_address_menu_idle();
        } else {
            nvm_write((void *)&N_storage.setting_account, &intAccount, 1);
            nvm_write((void *)&N_storage.setting_address_index, &intAddressIndex, 1);
            ui_idle();
        }
        break;
    case BUTTON_LEFT | BUTTON_EVT_RELEASED:
        if (menu_selection == ACCOUNT_MENU) {
            if (intAccount > 0)
                intAccount--;
        } else {
            if (intAddressIndex > 0)
                intAddressIndex--;
        }
        intToString();
        ui_address_menu_idle();
        break;
    case BUTTON_RIGHT | BUTTON_EVT_RELEASED:
        if (menu_selection == ACCOUNT_MENU) {
            if (intAccount < 255)
                intAccount++;
        } else {
            if (intAddressIndex < 255)
                intAddressIndex++;
        }
        intToString();
        ui_address_menu_idle();
        break;
    }
    return 0;
}

//////////////////////////////////////////////////////////////////////////////////////

static unsigned int select_account_ui_button(unsigned int button_mask, unsigned int button_mask_counter) {
    select_address_index_ui_button(button_mask, button_mask_counter);
    return 0;
}

//////////////////////////////////////////////////////////////////////////////////////

void ui_address_menu_idle() {
    if (menu_selection == ACCOUNT_MENU) {
        UX_DISPLAY(select_account_ui, NULL);
    } else {
        UX_DISPLAY(select_address_index_ui, NULL);
    }
}

//////////////////////////////////////////////////////////////////////////////////////

void selectAccount() {
    intAccount = N_storage.setting_account;
    intAddressIndex = N_storage.setting_address_index;
    menu_selection = ACCOUNT_MENU;
    intToString();
    ui_address_menu_idle();
}
