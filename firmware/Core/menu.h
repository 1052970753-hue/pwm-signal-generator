#ifndef MENU_H
#define MENU_H

#include "../../common/menu_fsm.h"
#include "../../common/protocol_defs.h"

void Menu_Init(void);
void Menu_Process(InputEvent ev);

extern SystemParams g_params;
extern MenuState g_menu;

#endif
