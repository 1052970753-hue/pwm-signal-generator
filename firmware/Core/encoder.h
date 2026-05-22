#ifndef ENCODER_H
#define ENCODER_H

#include "../../common/menu_fsm.h"

void Encoder_Init(void);
InputEvent Encoder_Poll(void);

#endif
