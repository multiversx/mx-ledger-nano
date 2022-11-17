#include "menu.h"
#include "os.h"
#include "view_app_version.h"

const char *const setting_contract_data_getter_values[] = {"No", "Yes", "Back"};
const char *const settings_submenu_getter_values[] = {
    "Contract data",
    "Back",
};
const char *const info_submenu_getter_values[] = {
    "App version",
    "Back",
};

void setting_contract_data_change(unsigned int contract_data);
const char *setting_contract_data_getter(unsigned int idx);
void setting_contract_data_selector(unsigned int idx);
const char *settings_submenu_getter(unsigned int idx);
void settings_submenu_selector(unsigned int idx);
const char *info_submenu_getter(unsigned int idx);
void info_submenu_selector(unsigned int idx);

// UI interface for the main menu
UX_STEP_NOCB(ux_idle_flow_1_step,
             pb,
             {
                 &C_icon_multiversx_logo,
                 "MultiversX",
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
void setting_contract_data_change(unsigned int contract_data) {
    nvm_write((void *) &N_storage.setting_contract_data, &contract_data, 1);
    ui_idle();
}

const char *setting_contract_data_getter(unsigned int idx) {
    if (idx < ARRAYLEN(setting_contract_data_getter_values))
        return setting_contract_data_getter_values[idx];
    return NULL;
}

void setting_contract_data_selector(unsigned int idx) {
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
const char *settings_submenu_getter(unsigned int idx) {
    if (idx < ARRAYLEN(settings_submenu_getter_values)) return settings_submenu_getter_values[idx];
    return NULL;
}

void settings_submenu_selector(unsigned int idx) {
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
const char *info_submenu_getter(unsigned int idx) {
    if (idx < ARRAYLEN(info_submenu_getter_values)) return info_submenu_getter_values[idx];
    return NULL;
}

void info_submenu_selector(unsigned int idx) {
    switch (idx) {
        case 0:
            view_app_version();
            break;
        default:
            ui_idle();
            break;
    }
}

void ui_idle(void) {
    // reserve a display stack slot if none yet
    if (G_ux.stack_count == 0) ux_stack_push();
    ux_flow_init(0, ux_idle_flow, NULL);
}
