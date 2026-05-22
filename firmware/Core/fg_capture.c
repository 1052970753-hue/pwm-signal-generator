#include "fg_capture.h"

static volatile uint32_t fg_freq_mhz = 0;
static volatile uint32_t last_capture = 0;
static volatile uint32_t curr_capture = 0;

void FG_Init(void) {
    RCC_APB1PeriphClockCmd(FG_TIM_CLK, ENABLE);
    RCC_APB2PeriphClockCmd(FG_GPIO_CLK, ENABLE);

    GPIO_InitTypeDef gpio;
    gpio.GPIO_Pin = FG_PIN;
    gpio.GPIO_Mode = GPIO_Mode_IN_FLOATING;
    GPIO_Init(FG_GPIO, &gpio);

    TIM_TimeBaseInitTypeDef tim;
    tim.TIM_Period = 65535;
    tim.TIM_Prescaler = 71;
    tim.TIM_ClockDivision = 0;
    tim.TIM_CounterMode = TIM_CounterMode_Up;
    TIM_TimeBaseInit(FG_TIM, &tim);

    TIM_ICInitTypeDef ic;
    ic.TIM_Channel = TIM_Channel_1;
    ic.TIM_ICPolarity = TIM_ICPolarity_Rising;
    ic.TIM_ICSelection = TIM_ICSelection_DirectTI;
    ic.TIM_ICPrescaler = TIM_ICPSC_DIV1;
    ic.TIM_ICFilter = 0x0F;
    TIM_ICInit(FG_TIM, &ic);

    TIM_ITConfig(FG_TIM, TIM_IT_CC1, ENABLE);
    NVIC_EnableIRQ(TIM2_IRQn);
    TIM_Cmd(FG_TIM, ENABLE);
}

void TIM2_IRQHandler(void) {
    if (TIM_GetITStatus(FG_TIM, TIM_IT_CC1) != RESET) {
        TIM_ClearITPendingBit(FG_TIM, TIM_IT_CC1);
        last_capture = curr_capture;
        curr_capture = TIM_GetCapture1(FG_TIM);
        if (curr_capture > last_capture) {
            uint32_t period_us = curr_capture - last_capture;
            if (period_us > 0)
                fg_freq_mhz = 1000000000UL / period_us;
        }
    }
}

uint32_t FG_GetFrequency_mHz(void) {
    return fg_freq_mhz;
}

uint16_t FG_CalculateRPM(uint8_t div, uint16_t pulses_per_rev) {
    if (pulses_per_rev == 0) pulses_per_rev = 1;
    if (div == 0) div = 1;
    return (uint16_t)((fg_freq_mhz / 1000 / div) * 60 / pulses_per_rev);
}
