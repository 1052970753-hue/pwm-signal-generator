#include "encoder.h"
#include "system_config.h"

static int16_t enc_last = 0;
static uint32_t btn_press_ticks = 0;
static uint8_t btn_ok_prev = 1;

void Encoder_Init(void) {
    RCC_APB1PeriphClockCmd(ENC_TIM_CLK, ENABLE);
    RCC_APB2PeriphClockCmd(ENC_GPIO_CLK | BTN_GPIO_CLK, ENABLE);

    GPIO_InitTypeDef gpio;
    gpio.GPIO_Pin = ENC_A_PIN | ENC_B_PIN;
    gpio.GPIO_Mode = GPIO_Mode_IN_FLOATING;
    GPIO_Init(ENC_GPIO, &gpio);

    gpio.GPIO_Pin = BTN_OK_PIN;
    gpio.GPIO_Mode = GPIO_Mode_IPU;
    GPIO_Init(BTN_GPIO, &gpio);

    TIM_TimeBaseInitTypeDef tim;
    tim.TIM_Period = 65535;
    tim.TIM_Prescaler = 0;
    tim.TIM_ClockDivision = 0;
    tim.TIM_CounterMode = TIM_CounterMode_Up;
    TIM_TimeBaseInit(ENC_TIM, &tim);

    TIM_EncoderInterfaceConfig(ENC_TIM, TIM_EncoderMode_TI12,
        TIM_ICPolarity_Rising, TIM_ICPolarity_Rising);

    TIM_ICInitTypeDef ic;
    ic.TIM_Channel = TIM_Channel_1;
    ic.TIM_ICPolarity = TIM_ICPolarity_Rising;
    ic.TIM_ICSelection = TIM_ICSelection_DirectTI;
    ic.TIM_ICPrescaler = TIM_ICPSC_DIV1;
    ic.TIM_ICFilter = 0x0F;
    TIM_ICInit(ENC_TIM, &ic);
    ic.TIM_Channel = TIM_Channel_2;
    TIM_ICInit(ENC_TIM, &ic);

    TIM_Cmd(ENC_TIM, ENABLE);
}

InputEvent Encoder_Poll(void) {
    int16_t now = TIM_GetCounter(ENC_TIM);
    int16_t delta = (int16_t)(now - enc_last);
    enc_last = now;

    uint8_t btn_ok = GPIO_ReadInputDataBit(BTN_GPIO, BTN_OK_PIN);

    // Long press detection
    if (btn_ok == 0 && btn_ok_prev == 0) {
        btn_press_ticks++;
        if (btn_press_ticks > 1000) {
            btn_press_ticks = 0;
            btn_ok_prev = 0;
            return EVENT_LONG_PRESS;
        }
    }

    // Falling edge (button just pressed)
    if (btn_ok == 0 && btn_ok_prev == 1) {
        btn_press_ticks = 0;
        btn_ok_prev = 0;
        return EVENT_NONE;
    }

    // Rising edge (button released)
    if (btn_ok == 1 && btn_ok_prev == 0) {
        btn_ok_prev = 1;
        if (btn_press_ticks > 0 && btn_press_ticks < 1000) {
            btn_press_ticks = 0;
            return EVENT_CLICK;
        }
        btn_press_ticks = 0;
    }

    if (delta >= 4) return EVENT_CW;
    if (delta <= -4) return EVENT_CCW;

    return EVENT_NONE;
}
