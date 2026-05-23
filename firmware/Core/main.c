#include "system_config.h"
#include "oled_ssd1306.h"
#include "encoder.h"
#include "menu.h"
#include "pwm_engine.h"
#include "fg_capture.h"
#include "ui_render.h"
#include "protocol.h"

static uint16_t rpm = 0;
static volatile uint32_t sys_tick = 0;
static uint8_t blink_flag = 0;
static uint32_t blink_tick = 0;

void SystemClock_Config(void) {
    RCC_DeInit();
    RCC_HSEConfig(RCC_HSE_ON);
    while (RCC_WaitForHSEStartUp() != SUCCESS);

    RCC_PLLConfig(RCC_PLLSource_HSE_Div1, RCC_PLLMul_9);
    RCC_PLLCmd(ENABLE);
    while (RCC_GetFlagStatus(RCC_FLAG_PLLRDY) == RESET);

    RCC_SYSCLKConfig(RCC_SYSCLKSource_PLLCLK);
    while (RCC_GetSYSCLKSource() != 0x08);

    SystemCoreClockUpdate();
    SysTick_Config(SystemCoreClock / 1000);
}

void SysTick_Handler(void) {
    sys_tick++;
}

int main(void) {
    SystemClock_Config();

    OLED_Init();
    Encoder_Init();
    PWM_Init();
    FG_Init();
    Protocol_Init();
    Menu_Init();

    uint32_t last_render = 0;
    uint32_t last_status = 0;

    while (1) {
        // Handle incoming UART frames
        Protocol_Process();

        InputEvent ev = Encoder_Poll();
        if (ev != EVENT_NONE) {
            Menu_Process(ev);
            if (g_menu.dirty) {
                PWM_UpdateFromParams(&g_params);
                g_menu.dirty = 0;
            }
        }

        // Blink cursor every 400ms (matching simulator)
        if (sys_tick - blink_tick >= 400) {
            blink_tick = sys_tick;
            blink_flag = !blink_flag;
        }

        if (sys_tick - last_render >= 50) {
            last_render = sys_tick;
            rpm = FG_CalculateRPM(g_params.fg_div, g_params.fg_pulses_per_rev);
            UI_Render(&g_params, rpm, blink_flag);
        }

        if (sys_tick - last_status >= 500) {
            last_status = sys_tick;
            StatusData sd;
            sd.ch1_freq_hz = g_params.ch1_freq_hz;
            sd.ch1_duty_pct = g_params.ch1_duty_pct;
            sd.ch1_enabled = g_params.ch1_enabled;
            sd.ch2_freq_hz = g_params.ch2_freq_hz;
            sd.ch2_duty_pct = g_params.ch2_duty_pct;
            sd.ch2_enabled = g_params.ch2_enabled;
            sd.fg_freq_mhz = FG_GetFrequency_mHz();
            sd.fg_div = g_params.fg_div;
            sd.rpm = rpm;
            Protocol_SendStatus(&sd);
        }
    }
}
