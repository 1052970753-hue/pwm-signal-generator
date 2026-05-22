#include "pwm_engine.h"

void PWM_Init(void) {
    RCC_APB2PeriphClockCmd(PWM1_TIM_CLK | PWM1_GPIO_CLK, ENABLE);
    RCC_APB1PeriphClockCmd(PWM2_TIM_CLK, ENABLE);
    RCC_APB2PeriphClockCmd(PWM2_GPIO_CLK, ENABLE);

    GPIO_InitTypeDef gpio;
    gpio.GPIO_Pin = PWM1_PIN;
    gpio.GPIO_Mode = GPIO_Mode_AF_PP;
    gpio.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(PWM1_GPIO, &gpio);

    gpio.GPIO_Pin = PWM2_PIN;
    GPIO_Init(PWM2_GPIO, &gpio);
}

void PWM_SetChannel(uint8_t channel, uint32_t freq_hz, uint8_t duty_pct) {
    TIM_TypeDef *tim = (channel == 1) ? PWM1_TIM : PWM2_TIM;
    if (freq_hz < 1) freq_hz = 1;
    if (freq_hz > 100000) freq_hz = 100000;
    if (duty_pct > 100) duty_pct = 100;

    uint16_t psc, arr;
    if (freq_hz <= 100) {
        psc = 7199; arr = 72000000 / (psc + 1) / freq_hz - 1;
    } else if (freq_hz <= 1000) {
        psc = 719; arr = 72000000 / (psc + 1) / freq_hz - 1;
    } else if (freq_hz <= 10000) {
        psc = 71; arr = 72000000 / (psc + 1) / freq_hz - 1;
    } else {
        psc = 7; arr = 72000000 / (psc + 1) / freq_hz - 1;
    }
    if (arr < 100) arr = 100;

    uint16_t ccr = (uint16_t)((uint32_t)arr * duty_pct / 100);

    TIM_TimeBaseInitTypeDef tim_init;
    tim_init.TIM_Period = arr;
    tim_init.TIM_Prescaler = psc;
    tim_init.TIM_ClockDivision = 0;
    tim_init.TIM_CounterMode = TIM_CounterMode_Up;
    TIM_TimeBaseInit(tim, &tim_init);

    TIM_OCInitTypeDef oc;
    oc.TIM_OCMode = TIM_OCMode_PWM1;
    oc.TIM_OutputState = TIM_OutputState_Enable;
    oc.TIM_Pulse = ccr;
    oc.TIM_OCPolarity = TIM_OCPolarity_High;
    if (channel == 1) {
        TIM_OC1Init(tim, &oc);
        TIM_OC1PreloadConfig(tim, TIM_OCPreload_Enable);
    } else {
        TIM_OC2Init(tim, &oc);
        TIM_OC2PreloadConfig(tim, TIM_OCPreload_Enable);
    }

    TIM_ARRPreloadConfig(tim, ENABLE);
    TIM_Cmd(tim, ENABLE);
    TIM_CtrlPWMOutputs(tim, ENABLE);
}

void PWM_EnableChannel(uint8_t channel, uint8_t enable) {
    TIM_TypeDef *tim = (channel == 1) ? PWM1_TIM : PWM2_TIM;
    if (!enable) {
        TIM_Cmd(tim, DISABLE);
        TIM_CtrlPWMOutputs(tim, DISABLE);
    }
}

void PWM_UpdateFromParams(SystemParams *params) {
    if (params->ch1_enabled)
        PWM_SetChannel(1, params->ch1_freq_hz, params->ch1_duty_pct);
    else
        PWM_EnableChannel(1, 0);

    if (params->ch2_enabled)
        PWM_SetChannel(2, params->ch2_freq_hz, params->ch2_duty_pct);
    else
        PWM_EnableChannel(2, 0);
}
