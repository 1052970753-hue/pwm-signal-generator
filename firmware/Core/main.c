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

// ── Test state machine ──
// Phases: 0=OFF, 1=ON (sampling), 2=done
static uint8_t  test_phase = 0;      // 0=OFF period, 1=ON period, 2=done
static uint8_t  test_current_cycle = 0;
static uint32_t test_phase_start = 0; // sys_tick when current phase started

// RPM sampling during ON period
static uint16_t test_rpm_max = 0;
static uint32_t test_rpm_sum = 0;
static uint16_t test_rpm_count = 0;

// Target RPM = freq * 60 / pulses_per_rev
static uint16_t calc_target_rpm(uint32_t freq_hz) {
    return (uint16_t)((uint64_t)freq_hz * 60 / g_params.fg_pulses_per_rev);
}

static void test_start_cycle(void) {
    test_phase = 1;
    test_phase_start = sys_tick;
    test_rpm_max = 0;
    test_rpm_sum = 0;
    test_rpm_count = 0;

    // Enable PWM on test channel
    uint8_t ch = Menu_GetTestChannel();
    PWM_SetChannel(ch, Menu_GetTestFreq(), Menu_GetTestDuty());
    PWM_EnableChannel(ch, 1);
}

static void test_handle_tick(void) {
    if (!Menu_IsTestRunning()) {
        if (test_phase != 0) {
            // Test was stopped externally — clean up
            test_phase = 0;
            uint8_t ch = Menu_GetTestChannel();
            PWM_EnableChannel(ch, 0);
        }
        return;
    }

    if (test_phase == 2) return; // already done

    uint32_t elapsed = sys_tick - test_phase_start;

    if (test_phase == 1) {
        // ON period — sample RPM
        uint16_t current_rpm = FG_CalculateRPM(g_params.fg_div, g_params.fg_pulses_per_rev);
        if (current_rpm > test_rpm_max) test_rpm_max = current_rpm;
        test_rpm_sum += current_rpm;
        test_rpm_count++;

        uint32_t on_ms = (uint32_t)Menu_GetTestOnSec() * 1000;
        if (elapsed >= on_ms) {
            // ON period ended — record data
            uint16_t rpm_avg = test_rpm_count > 0 ? (uint16_t)(test_rpm_sum / test_rpm_count) : 0;
            uint16_t target = calc_target_rpm(Menu_GetTestFreq());

            // Error if RPM is 0 or off by >20%
            uint8_t error = 0;
            if (rpm_avg == 0) {
                error = 1;
            } else if (target > 0) {
                int32_t diff = (int32_t)rpm_avg - (int32_t)target;
                if (diff < 0) diff = -diff;
                if (diff * 100 / target > 20) error = 1;
            }

            // Startup OK if RPM > 0 within first second
            uint8_t startup_ok = (test_rpm_max > 0) ? 1 : 0;

            test_current_cycle++;
            Menu_AddTestRecord(test_current_cycle, target, test_rpm_max, rpm_avg, error, startup_ok);

            // Disable PWM — start OFF period
            uint8_t ch = Menu_GetTestChannel();
            PWM_EnableChannel(ch, 0);
            test_phase = 0;
            test_phase_start = sys_tick;
        }
    } else {
        // OFF period
        uint32_t off_ms = (uint32_t)Menu_GetTestOffSec() * 1000;
        if (elapsed >= off_ms) {
            if (test_current_cycle >= Menu_GetTestCycles()) {
                // All cycles done
                test_phase = 2;
                Menu_TestDone();
            } else {
                // Start next cycle
                test_start_cycle();
            }
        }
    }
}

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
    uint8_t  prev_test_running = 0;

    while (1) {
        // Handle incoming UART frames
        Protocol_Process();

        // Continue CSV export if in progress
        if (!Menu_ExportDone() && Menu_GetExportCount() > 0) {
            Protocol_ProcessExport();
        }

        // Feed sys_tick to encoder for double-click detection
        Encoder_SetTick(sys_tick);

        InputEvent ev = Encoder_Poll();
        if (ev != EVENT_NONE) {
            Menu_Process(ev);
            if (g_menu.dirty) {
                PWM_UpdateFromParams(&g_params);
                g_menu.dirty = 0;
            }
        }

        // Test state machine — detect start
        uint8_t now_running = Menu_IsTestRunning();
        if (now_running && !prev_test_running) {
            // Test just started
            test_current_cycle = 0;
            test_start_cycle();
        }
        prev_test_running = now_running;

        test_handle_tick();

        // Blink cursor every 400ms
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
            sd.mode = g_menu.mode;
            sd.test_state = now_running ? TEST_RUNNING : TEST_IDLE;
            sd.test_cycle = test_current_cycle;
            sd.test_total = Menu_GetTestCycles();
            Protocol_SendStatus(&sd);
        }
    }
}
