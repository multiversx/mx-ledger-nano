#include "globals.h"
#include "glyphs.h"

#ifndef _MENU_H_
#define _MENU_H_

extern volatile uint8_t setting_contract_data, setting_blind_signing;

void ui_idle(void);
void ui_settings(void);
#endif
