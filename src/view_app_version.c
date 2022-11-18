#include "view_app_version.h"
#include "menu.h"
#include "os.h"
#include "utils.h"
#include "ux.h"

static unsigned int app_version_ui_button(unsigned int button_mask,
                                          unsigned int button_mask_counter);

// UI for displaying the app version
static const bagl_element_t app_version_ui[] = {
#if defined(TARGET_NANOX) || defined(TARGET_NANOS2)
    {
        {BAGL_RECTANGLE, 0x00, 0, 0, 128, 64, 0, 0, BAGL_FILL, 0x000000, 0xFFFFFF, 0, 0},
        NULL,
    },
    {
        {BAGL_LABELINE,
         0x01,
         0,
         32,
         128,
         16,
         0,
         0,
         0,
         0xFFFFFF,
         0x000000,
         BAGL_FONT_OPEN_SANS_EXTRABOLD_11px | BAGL_FONT_ALIGNMENT_CENTER,
         0},
        "App version",
    },
    {
        {BAGL_LABELINE,
         0x01,
         0,
         50,
         128,
         16,
         0,
         0,
         0,
         0xFFFFFF,
         0x000000,
         BAGL_FONT_OPEN_SANS_REGULAR_11px | BAGL_FONT_ALIGNMENT_CENTER,
         0},
        APPVERSION,
    },
    {
        {BAGL_ICON, 0x00, 3, 12, 7, 7, 0, 0, 0, 0xFFFFFF, 0x000000, 0, BAGL_GLYPH_ICON_UP},
        NULL,
    },
    {
        {BAGL_ICON, 0x00, 117, 13, 7, 7, 0, 0, 0, 0xFFFFFF, 0x000000, 0, BAGL_GLYPH_ICON_UP},
        NULL,
    },
#else
    {
        {BAGL_RECTANGLE, 0x00, 0, 0, 128, 32, 0, 0, BAGL_FILL, 0x000000, 0xFFFFFF, 0, 0},
        NULL,
    },
    {
        {BAGL_LABELINE,
         0x01,
         0,
         14,
         128,
         16,
         0,
         0,
         0,
         0xFFFFFF,
         0x000000,
         BAGL_FONT_OPEN_SANS_EXTRABOLD_11px | BAGL_FONT_ALIGNMENT_CENTER,
         0},
        "App version",
    },
    {
        {BAGL_LABELINE,
         0x01,
         0,
         30,
         128,
         16,
         0,
         0,
         0,
         0xFFFFFF,
         0x000000,
         BAGL_FONT_OPEN_SANS_REGULAR_11px | BAGL_FONT_ALIGNMENT_CENTER,
         0},
        APPVERSION,
    },
    {
        {BAGL_ICON, 0x00, 3, 12, 7, 7, 0, 0, 0, 0xFFFFFF, 0x000000, 0, BAGL_GLYPH_ICON_UP},
        NULL,
    },
    {
        {BAGL_ICON, 0x00, 117, 13, 7, 7, 0, 0, 0, 0xFFFFFF, 0x000000, 0, BAGL_GLYPH_ICON_UP},
        NULL,
    },
#endif
};

// this function is called by the OS when a button event occurs
static unsigned int app_version_ui_button(unsigned int button_mask,
                                          unsigned int button_mask_counter) {
    (void) (button_mask_counter);

    switch (button_mask) {
        case BUTTON_EVT_RELEASED | BUTTON_LEFT | BUTTON_RIGHT:  // EXIT
            ui_idle();
            break;
    }

    return 0;
}

void view_app_version() {
    UX_DISPLAY(app_version_ui, NULL);
}
