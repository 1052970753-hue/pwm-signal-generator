#include "menu.h"

SystemParams g_params;
MenuState g_menu;

// Test configuration
uint8_t  test_channel = 1;      // 1=CH1, 2=CH2
uint32_t test_freq = 1000;
uint8_t  test_duty = 50;
uint16_t test_cycles = 10;
uint16_t test_on_sec = 5;
uint16_t test_off_sec = 3;
uint8_t  test_running = 0;
uint8_t  test_cursor = 0;       // TestCursorItem

// Test data recording (ring buffer, max 200 records)
#define TEST_BUF_SIZE 200
typedef struct {
    uint16_t cycle;
    uint16_t target_rpm;
    uint16_t rpm_max;
    uint16_t rpm_avg;
    uint8_t  error;
    uint8_t  startup_ok;
} TestRecord;

TestRecord test_records[TEST_BUF_SIZE];
uint16_t test_record_count = 0;

// Export state
uint16_t export_index = 0;
uint8_t  export_active = 0;

void Menu_Init(void) {
    g_menu.cursor = ITEM_CH1_DUTY;
    g_menu.selected = 0;
    g_menu.dirty = 1;
    g_menu.mode = MODE_PWM_FG;
    g_menu.blink = 0;

    g_params.ch1_freq_hz = 1000;
    g_params.ch1_duty_pct = 50;
    g_params.ch1_enabled = 0;
    g_params.ch2_freq_hz = 1000;
    g_params.ch2_duty_pct = 50;
    g_params.ch2_enabled = 0;
    g_params.fg_div = 2;
    g_params.fg_pulses_per_rev = 2;
}

// ── Value get/set for PWM-FG mode ──

static uint32_t get_cursor_value(void) {
    switch (g_menu.cursor) {
        case ITEM_CH1_FREQ: return g_params.ch1_freq_hz;
        case ITEM_CH1_DUTY: return g_params.ch1_duty_pct;
        case ITEM_CH2_FREQ: return g_params.ch2_freq_hz;
        case ITEM_CH2_DUTY: return g_params.ch2_duty_pct;
        case ITEM_FG_DIV:   return g_params.fg_div;
        default: return 0;
    }
}

static void set_cursor_value(uint32_t val) {
    switch (g_menu.cursor) {
        case ITEM_CH1_FREQ:
            if (val < 1) val = 1;
            if (val > 100000) val = 100000;
            g_params.ch1_freq_hz = val;
            break;
        case ITEM_CH1_DUTY:
            if (val > 100) val = 100;
            g_params.ch1_duty_pct = val;
            break;
        case ITEM_CH2_FREQ:
            if (val < 1) val = 1;
            if (val > 100000) val = 100000;
            g_params.ch2_freq_hz = val;
            break;
        case ITEM_CH2_DUTY:
            if (val > 100) val = 100;
            g_params.ch2_duty_pct = val;
            break;
        case ITEM_FG_DIV:
            if (val < 1) val = 1;
            if (val > 99) val = 99;
            g_params.fg_div = val;
            break;
        default: break;
    }
}

// ── Value get/set for CH1-only / CH2-only modes ──
// cursor: 0=Freq, 1=Duty, 2=Enable

static uint32_t get_ch_cursor_value(uint8_t channel) {
    uint8_t c = g_menu.cursor;
    if (channel == 1) {
        if (c == 0) return g_params.ch1_freq_hz;
        if (c == 1) return g_params.ch1_duty_pct;
    } else {
        if (c == 0) return g_params.ch2_freq_hz;
        if (c == 1) return g_params.ch2_duty_pct;
    }
    return 0;
}

static void set_ch_cursor_value(uint8_t channel, uint32_t val) {
    uint8_t c = g_menu.cursor;
    if (channel == 1) {
        if (c == 0) { if (val<1) val=1; if (val>100000) val=100000; g_params.ch1_freq_hz=val; }
        else if (c == 1) { if (val>100) val=100; g_params.ch1_duty_pct=val; }
    } else {
        if (c == 0) { if (val<1) val=1; if (val>100000) val=100000; g_params.ch2_freq_hz=val; }
        else if (c == 1) { if (val>100) val=100; g_params.ch2_duty_pct=val; }
    }
}

// ── Value get/set for test config ──

static uint32_t get_test_cursor_value(void) {
    switch (test_cursor) {
        case TEST_ITEM_CHANNEL:  return test_channel;
        case TEST_ITEM_FREQ:     return test_freq;
        case TEST_ITEM_DUTY:     return test_duty;
        case TEST_ITEM_CYCLES:   return test_cycles;
        case TEST_ITEM_ON_TIME:  return test_on_sec;
        case TEST_ITEM_OFF_TIME: return test_off_sec;
        default: return 0;
    }
}

static void set_test_cursor_value(int32_t delta) {
    switch (test_cursor) {
        case TEST_ITEM_CHANNEL:
            test_channel = (test_channel == 1) ? 2 : 1;
            break;
        case TEST_ITEM_FREQ:
            test_freq += delta;
            if (test_freq < 1) test_freq = 1;
            if (test_freq > 100000) test_freq = 100000;
            break;
        case TEST_ITEM_DUTY:
            test_duty += delta;
            if (test_duty > 100) test_duty = 100;
            break;
        case TEST_ITEM_CYCLES:
            test_cycles += delta;
            if (test_cycles < 1) test_cycles = 1;
            if (test_cycles > 999) test_cycles = 999;
            break;
        case TEST_ITEM_ON_TIME:
            test_on_sec += delta;
            if (test_on_sec < 1) test_on_sec = 1;
            if (test_on_sec > 60) test_on_sec = 60;
            break;
        case TEST_ITEM_OFF_TIME:
            test_off_sec += delta;
            if (test_off_sec < 1) test_off_sec = 1;
            if (test_off_sec > 60) test_off_sec = 60;
            break;
        default: break;
    }
}

// ── Public test functions ──

uint8_t Menu_IsTestRunning(void) { return test_running; }
uint8_t Menu_GetTestChannel(void) { return test_channel; }
uint32_t Menu_GetTestFreq(void) { return test_freq; }
uint8_t Menu_GetTestDuty(void) { return test_duty; }
uint16_t Menu_GetTestCycles(void) { return test_cycles; }
uint16_t Menu_GetTestOnSec(void) { return test_on_sec; }
uint16_t Menu_GetTestOffSec(void) { return test_off_sec; }
uint16_t Menu_GetTestRecordCount(void) { return test_record_count; }

void Menu_StartTest(void) {
    test_running = 1;
    test_record_count = 0;
    export_index = 0;
}

void Menu_StopTest(void) {
    test_running = 0;
}

void Menu_AddTestRecord(uint16_t cycle, uint16_t target_rpm,
                        uint16_t rpm_max, uint16_t rpm_avg,
                        uint8_t error, uint8_t startup_ok) {
    if (test_record_count < TEST_BUF_SIZE) {
        TestRecord *r = &test_records[test_record_count++];
        r->cycle = cycle;
        r->target_rpm = target_rpm;
        r->rpm_max = rpm_max;
        r->rpm_avg = rpm_avg;
        r->error = error;
        r->startup_ok = startup_ok;
    }
}

void Menu_TestDone(void) {
    test_running = 0;
}

// Export functions
uint16_t Menu_GetExportCount(void) { return test_record_count; }

uint8_t Menu_FormatExportChunk(uint8_t *buf, uint16_t max_len) {
    if (export_index == 0) {
        // First chunk: CSV header
        const char *header = "cycle,target_rpm,rpm_max,rpm_avg,error,startup_ok\r\n";
        uint16_t len = 0;
        while (header[len] && len < max_len - 1) { buf[len] = header[len]; len++; }
        export_index++;
        return len;
    }
    if (export_index - 1 >= test_record_count) return 0; // done

    TestRecord *r = &test_records[export_index - 1];
    // Format one CSV line
    int len = 0;
    // Simple integer formatting without snprintf
    #define PUT_CHAR(c) if (len < max_len) buf[len++] = (c)
    #define PUT_NUM(n) do { \
        char tmp[12]; int t=0; uint32_t v=(n); \
        if(v==0) tmp[t++]='0'; \
        while(v>0){tmp[t++]='0'+v%10;v/=10;} \
        while(t>0) PUT_CHAR(tmp[--t]); \
    } while(0)

    PUT_NUM(r->cycle); PUT_CHAR(',');
    PUT_NUM(r->target_rpm); PUT_CHAR(',');
    PUT_NUM(r->rpm_max); PUT_CHAR(',');
    PUT_NUM(r->rpm_avg); PUT_CHAR(',');
    PUT_NUM(r->error); PUT_CHAR(',');
    PUT_NUM(r->startup_ok);
    PUT_CHAR('\r'); PUT_CHAR('\n');

    #undef PUT_NUM
    #undef PUT_CHAR

    export_index++;
    return len;
}

uint8_t Menu_ExportDone(void) {
    return (export_index - 1 >= test_record_count);
}

// ── Main event processing ──

void Menu_Process(InputEvent ev) {
    if (ev == EVENT_NONE) return;
    g_menu.dirty = 1;

    // Double-click: switch mode (always active)
    if (ev == EVENT_DOUBLE_CLICK) {
        g_menu.mode = (g_menu.mode + 1) % NUM_MODES;
        g_menu.cursor = 0;
        g_menu.selected = 0;
        return;
    }

    // Test mode running: only allow stop via click
    if (g_menu.mode == MODE_TEST && test_running) {
        if (ev == EVENT_CLICK) {
            test_running = 0;
        }
        return;
    }

    // Select mode: encoder moves cursor, click exits
    if (g_menu.selected) {
        uint8_t max_items;
        if (g_menu.mode == MODE_TEST)
            max_items = NUM_TEST_ITEMS;
        else if (g_menu.mode == MODE_PWM_FG)
            max_items = NUM_ITEMS;
        else
            max_items = 3; // Freq, Duty, Enable for CH modes

        switch (ev) {
            case EVENT_CW:
                g_menu.cursor = (g_menu.cursor + 1) % max_items;
                break;
            case EVENT_CCW:
                g_menu.cursor = (g_menu.cursor + max_items - 1) % max_items;
                break;
            case EVENT_CLICK:
                g_menu.selected = 0;
                break;
            default: break;
        }
        return;
    }

    // Normal mode — behavior depends on current app mode
    switch (g_menu.mode) {
        case MODE_PWM_FG:
            switch (ev) {
                case EVENT_CW:
                    set_cursor_value(get_cursor_value() + 1);
                    break;
                case EVENT_CCW: {
                    uint32_t v = get_cursor_value();
                    if (v > 0) set_cursor_value(v - 1);
                    break;
                }
                case EVENT_CLICK:
                    if (g_menu.cursor <= ITEM_CH1_DUTY)
                        g_params.ch1_enabled = !g_params.ch1_enabled;
                    else if (g_menu.cursor <= ITEM_CH2_DUTY)
                        g_params.ch2_enabled = !g_params.ch2_enabled;
                    break;
                case EVENT_LONG_PRESS:
                    g_menu.selected = 1;
                    break;
                default: break;
            }
            break;

        case MODE_FG:
            switch (ev) {
                case EVENT_CW:
                    g_params.fg_div++;
                    if (g_params.fg_div > 99) g_params.fg_div = 99;
                    break;
                case EVENT_CCW:
                    if (g_params.fg_div > 1) g_params.fg_div--;
                    break;
                case EVENT_LONG_PRESS:
                    g_menu.selected = 1;
                    break;
                default: break;
            }
            break;

        case MODE_CH1:
            switch (ev) {
                case EVENT_CW:
                    set_ch_cursor_value(1, get_ch_cursor_value(1) + 1);
                    break;
                case EVENT_CCW: {
                    uint32_t v = get_ch_cursor_value(1);
                    if (v > 0) set_ch_cursor_value(1, v - 1);
                    break;
                }
                case EVENT_CLICK:
                    if (g_menu.cursor == 2)
                        g_params.ch1_enabled = !g_params.ch1_enabled;
                    else
                        g_params.ch1_enabled = !g_params.ch1_enabled;
                    break;
                case EVENT_LONG_PRESS:
                    g_menu.selected = 1;
                    break;
                default: break;
            }
            break;

        case MODE_CH2:
            switch (ev) {
                case EVENT_CW:
                    set_ch_cursor_value(2, get_ch_cursor_value(2) + 1);
                    break;
                case EVENT_CCW: {
                    uint32_t v = get_ch_cursor_value(2);
                    if (v > 0) set_ch_cursor_value(2, v - 1);
                    break;
                }
                case EVENT_CLICK:
                    if (g_menu.cursor == 2)
                        g_params.ch2_enabled = !g_params.ch2_enabled;
                    else
                        g_params.ch2_enabled = !g_params.ch2_enabled;
                    break;
                case EVENT_LONG_PRESS:
                    g_menu.selected = 1;
                    break;
                default: break;
            }
            break;

        case MODE_TEST:
            switch (ev) {
                case EVENT_CW:
                    set_test_cursor_value(1);
                    break;
                case EVENT_CCW:
                    set_test_cursor_value(-1);
                    break;
                case EVENT_CLICK:
                    if (test_cursor == TEST_ITEM_START) {
                        test_running = 1;
                        test_record_count = 0;
                        export_index = 0;
                    }
                    break;
                case EVENT_LONG_PRESS:
                    g_menu.selected = 1;
                    break;
                default: break;
            }
            break;

        default: break;
    }
}
