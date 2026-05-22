#ifndef FG_CAPTURE_H
#define FG_CAPTURE_H

#include "system_config.h"

void FG_Init(void);
uint32_t FG_GetFrequency_mHz(void);
uint16_t FG_CalculateRPM(uint8_t div, uint16_t pulses_per_rev);

#endif
