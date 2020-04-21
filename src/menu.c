#include "menu.h"
#include "os.h"
#include "viewAddress.h"
#include "viewAppVersion.h"
#include "selectAccount.h"

const char* const setting_network_getter_values[] = {
    "Mainnet",
    "Testnet",
    "Back"
};
const char* const setting_contract_data_getter_values[] = {
    "No",
    "Yes",
    "Back"
};
const char* const settings_submenu_getter_values[] = {
    "Select account",
    "Select network",
    "Contract data",
    "Back",
};
const char* const info_submenu_getter_values[] = {
    "View address",
    "View pub key",
    "App version ",
    "Back",
};

volatile uint8_t setting_network;
volatile uint8_t setting_contract_data;

void setting_network_change(unsigned int enabled);
const char* setting_network_getter(unsigned int idx);
void setting_network_selector(unsigned int idx);
void setting_contract_data_change(unsigned int enabled);
const char* setting_contract_data_getter(unsigned int idx);
void setting_contract_data_selector(unsigned int idx);
const char* settings_submenu_getter(unsigned int idx);
void settings_submenu_selector(unsigned int idx);
const char* info_submenu_getter(unsigned int idx);
void info_submenu_selector(unsigned int idx);
void ui_idle(void);

//////////////////////////////////////////////////////////////////////////////////////
// UI interface for the main menu
UX_STEP_NOCB(
    ux_idle_flow_1_step, 
    pb, 
    {
        &C_icon_elrond_logo,
        "Elrond Network",
    });
UX_STEP_VALID(
    ux_idle_flow_2_step,
    pb,
    ux_menulist_init(0, settings_submenu_getter, settings_submenu_selector),
    {
        &C_icon_settings,
        "Settings",
    });
UX_STEP_VALID(
    ux_idle_flow_3_step, 
    pb, 
    ux_menulist_init(0, info_submenu_getter, info_submenu_selector),
    {
        &C_icon_info,
        "Info",
    });
UX_STEP_VALID(
    ux_idle_flow_4_step,
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
    FLOW_LOOP
);

//////////////////////////////////////////////////////////////////////////////////////
// Enable contract data submenu:
void setting_network_change(unsigned int enabled) {
    nvm_write((void *)&N_storage.setting_network, &enabled, 1);
    ui_idle();
}

////////////////////////////////////////////////////////////////////////////////

const char* setting_network_getter(unsigned int idx) {
    if (idx < ARRAYLEN(setting_network_getter_values))
        return setting_network_getter_values[idx];
    return NULL;
}

////////////////////////////////////////////////////////////////////////////////

void setting_network_selector(unsigned int idx) {
    switch(idx) {
    case 0:
        setting_network_change(0);
        break;
    case 1:
        setting_network_change(1);
        break;
    default:
        ux_menulist_init(0, settings_submenu_getter, settings_submenu_selector);
    }
}

//////////////////////////////////////////////////////////////////////////////////////
// Display contract data submenu:
void setting_contract_data_change(unsigned int enabled) {
    nvm_write((void *)&N_storage.setting_contract_data, &enabled, 1);
    ui_idle();
}

////////////////////////////////////////////////////////////////////////////////

const char* setting_contract_data_getter(unsigned int idx) {
    if (idx < ARRAYLEN(setting_contract_data_getter_values))
        return setting_contract_data_getter_values[idx];
    return NULL;
}

////////////////////////////////////////////////////////////////////////////////

void setting_contract_data_selector(unsigned int idx) {
    switch(idx) {
    case 0:
        setting_contract_data_change(0);
        break;
    case 1:
        setting_contract_data_change(1);
        break;
    default:
        ux_menulist_init(0, settings_submenu_getter, settings_submenu_selector);
    }
}

//////////////////////////////////////////////////////////////////////////////////////
// Settings menu:
const char* settings_submenu_getter(unsigned int idx) {
    if (idx < ARRAYLEN(settings_submenu_getter_values))
        return settings_submenu_getter_values[idx];
    return NULL;
}

void settings_submenu_selector(unsigned int idx) {
    switch(idx) {
    case 0:
        selectAccount();
        break;
    case 1:
        ux_menulist_init_select(0, setting_network_getter, setting_network_selector, N_storage.setting_network);
        break;
    case 2:
        ux_menulist_init_select(0, setting_contract_data_getter, setting_contract_data_selector, N_storage.setting_contract_data);
        break;
    default:
        ui_idle();
    }
}

//////////////////////////////////////////////////////////////////////////////////////
// Info menu:
const char* info_submenu_getter(unsigned int idx) {
    if (idx < ARRAYLEN(info_submenu_getter_values))
        return info_submenu_getter_values[idx];
    return NULL;
}

//////////////////////////////////////////////////////////////////////////////////////

void info_submenu_selector(unsigned int idx) {
    switch(idx) {
    case 0:
        viewAddressAsBech32(N_storage.setting_account, N_storage.setting_address_index);
        break;
    case 1:
        viewAddressAsHex(N_storage.setting_account, N_storage.setting_address_index);
        break;
    case 2:
        viewAppVersion();
        break;
    default:
        ui_idle();
    }
}

//////////////////////////////////////////////////////////////////////////////////////

void ui_idle(void) {
    // reserve a display stack slot if none yet
    if(G_ux.stack_count == 0)
        ux_stack_push();
    ux_flow_init(0, ux_idle_flow, NULL);
}
