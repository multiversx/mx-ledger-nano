#include "menu.h"
#include "os.h"
#include "view_app_version.h"
#include "utils.h"

#ifdef HAVE_NBGL
#include "nbgl_use_case.h"
#endif

#if defined(TARGET_STAX)

static const char* const info_types[] = {"Version", APPNAME};
static const char* const info_contents[] = {APPVERSION, "(c) 2022 Ledger"};

static void quit_app_callback(void) {
    os_sched_exit(-1);
}

#define NB_SETTINGS_SWITCHES 1
static nbgl_layoutSwitch_t G_switches[NB_SETTINGS_SWITCHES];

enum {
    SWITCH_CONTRACT_DATA_SET_TOKEN = FIRST_USER_TOKEN,
};

static bool settings_nav_callback(uint8_t page, nbgl_pageContent_t* content) {
    if (page == 0) {
        content->type = SWITCHES_LIST;
        content->switchesList.nbSwitches = NB_SETTINGS_SWITCHES;
        content->switchesList.switches = (nbgl_layoutSwitch_t*) G_switches;
    } else if (page == 1) {
        content->type = INFOS_LIST;
        content->infosList.nbInfos = ARRAY_COUNT(info_types);
        content->infosList.infoTypes = (const char**) info_types;
        content->infosList.infoContents = (const char**) info_contents;
    } else {
        return false;
    }

    return true;
}

static void ui_menu_main(void);
static void ui_menu_settings(void);

static void settingsControlsCallback(int token, uint8_t index) {
    uint8_t new_setting;
    UNUSED(index);
    switch (token) {
        case SWITCH_CONTRACT_DATA_SET_TOKEN:
            G_switches[0].initState = !(G_switches[0].initState);
            if (G_switches[0].initState == OFF_STATE) {
                new_setting = CONTRACT_DATA_DISABLED;
            } else {
                new_setting = CONTRACT_DATA_ENABLED;
            }
            nvm_write((void*) &N_storage.setting_contract_data, &new_setting, 1);
            break;
        default:
            PRINTF("Should not happen !\n");
            break;
    }
}

static void ui_menu_settings(void) {
    G_switches[0].text = "Contract data";
    G_switches[0].subText = "Enable contract data";
    G_switches[0].token = SWITCH_CONTRACT_DATA_SET_TOKEN;
    G_switches[0].tuneId = TUNE_TAP_CASUAL;
    if (N_storage.setting_contract_data == CONTRACT_DATA_DISABLED) {
        G_switches[0].initState = OFF_STATE;
    } else {
        G_switches[0].initState = ON_STATE;
    }
    nbgl_useCaseSettings(APPNAME " settings",
                         0,
                         2,
                         false,
                         ui_menu_main,
                         settings_nav_callback,
                         settingsControlsCallback);
}

static void ui_menu_main(void) {
    nbgl_useCaseHome(APPNAME,
                     &C_icon_multiversx_logo_64x64,
                     "This app confirms actions on\nthe " APPNAME " network.",
                     true,
                     ui_menu_settings,
                     quit_app_callback);
}

#else

const char *const setting_contract_data_getter_values[] = {"No", "Yes", "Back"};
const char *const settings_submenu_getter_values[] = {
    "Contract data",
    "Back",
};
const char *const info_submenu_getter_values[] = {
    "App version",
    "Back",
};

static void setting_contract_data_change(unsigned int contract_data);
static const char *setting_contract_data_getter(unsigned int idx);
static void setting_contract_data_selector(unsigned int idx);
static const char *settings_submenu_getter(unsigned int idx);
static void settings_submenu_selector(unsigned int idx);
static const char *info_submenu_getter(unsigned int idx);
static void info_submenu_selector(unsigned int idx);

// UI interface for the main menu
UX_STEP_NOCB(ux_idle_flow_1_step,
             pb,
             {
                 &C_icon_multiversx_logo,
                 APPNAME,
             });
UX_STEP_VALID(ux_idle_flow_2_step,
              pb,
              ux_menulist_init(0, settings_submenu_getter, settings_submenu_selector),
              {
                  &C_icon_settings,
                  "Settings",
              });
UX_STEP_VALID(ux_idle_flow_3_step,
              pb,
              ux_menulist_init(0, info_submenu_getter, info_submenu_selector),
              {
                  &C_icon_info,
                  "Info",
              });
UX_STEP_VALID(ux_idle_flow_4_step,
              pb,
              os_sched_exit(-1),
              {
                  &C_icon_dashboard_x,
                  "Quit",
              });
UX_FLOW(ux_idle_flow,
        &ux_idle_flow_1_step,
        &ux_idle_flow_2_step,
        &ux_idle_flow_3_step,
        &ux_idle_flow_4_step,
        FLOW_LOOP);

// Contract data submenu:
static void setting_contract_data_change(unsigned int contract_data) {
    nvm_write((void *) &N_storage.setting_contract_data, &contract_data, 1);
    ui_idle();
}

static const char *setting_contract_data_getter(unsigned int idx) {
    if (idx < ARRAYLEN(setting_contract_data_getter_values))
        return setting_contract_data_getter_values[idx];
    return NULL;
}

static void setting_contract_data_selector(unsigned int idx) {
    switch (idx) {
        case CONTRACT_DATA_DISABLED:
            setting_contract_data_change(CONTRACT_DATA_DISABLED);
            break;
        case CONTRACT_DATA_ENABLED:
            setting_contract_data_change(CONTRACT_DATA_ENABLED);
            break;
        default:
            ux_menulist_init(0, settings_submenu_getter, settings_submenu_selector);
            break;
    }
}

// Settings menu:
static const char *settings_submenu_getter(unsigned int idx) {
    if (idx < ARRAYLEN(settings_submenu_getter_values)) return settings_submenu_getter_values[idx];
    return NULL;
}

static void settings_submenu_selector(unsigned int idx) {
    switch (idx) {
        case 0:
            ux_menulist_init_select(0,
                                    setting_contract_data_getter,
                                    setting_contract_data_selector,
                                    N_storage.setting_contract_data);
            break;
        default:
            ui_idle();
            break;
    }
}

// Info menu:
static const char *info_submenu_getter(unsigned int idx) {
    if (idx < ARRAYLEN(info_submenu_getter_values)) return info_submenu_getter_values[idx];
    return NULL;
}

static void info_submenu_selector(unsigned int idx) {
    switch (idx) {
        case 0:
            view_app_version();
            break;
        default:
            ui_idle();
            break;
    }
}

#endif

void ui_idle(void) {
#if defined(TARGET_STAX)
    ui_menu_main();
#else
    // reserve a display stack slot if none yet
    if (G_ux.stack_count == 0) {
        ux_stack_push();
    }
    ux_flow_init(0, ux_idle_flow, NULL);
#endif
}
