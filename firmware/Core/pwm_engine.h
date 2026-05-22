#ifndef PWM_ENGINE_H
#define PWM_ENGINE_H

#include "system_config.h"
#include "../../common/protocol_defs.h"

void PWM_Init(void);
void PWM_SetChannel(uint8_t channel, uint32_t freq_hz, uint8_t duty_pct);
void PWM_EnableChannel(uint8_t channel, uint8_t enable);
void PWM_UpdateFromParams(SystemParams *params);

#endif
