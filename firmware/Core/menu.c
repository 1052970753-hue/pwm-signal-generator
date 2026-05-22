#include "menu.h"

SystemParams g_params;
MenuState g_menu;

void Menu_Init(void) {
    g_menu.cursor = ITEM_CH1_DUTY;  // start on CH1 Duty (same as simulator)
    g_menu.selected = 0;
    g_menu.dirty = 1;

    g_params.ch1_freq_hz = 1000;
    g_params.ch1_duty_pct = 50;
    g_params.ch1_enabled = 0;
    g_params.ch2_freq_hz = 1000;
    g_params.ch2_duty_pct = 50;
    g_params.ch2_enabled = 0;
    g_params.fg_div = 2;
    g_params.fg_pulses_per_rev = 2;
}

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

void Menu_Process(InputEvent ev) {
    if (ev == EVENT_NONE) return;
    g_menu.dirty = 1;

    if (g_menu.selected) {
        // Select mode: encoder moves cursor, click exits
        switch (ev) {
            case EVENT_CW:
                g_menu.cursor = (g_menu.cursor + 1) % NUM_ITEMS;
                break;
            case EVENT_CCW:
                g_menu.cursor = (g_menu.cursor + NUM_ITEMS - 1) % NUM_ITEMS;
                break;
            case EVENT_CLICK:
                g_menu.selected = 0;
                break;
            default:
                break;
        }
        return;
    }

    // Normal mode: encoder changes value, click toggles channel, long press enters select
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
            // Short press: toggle channel based on cursor position
            if (g_menu.cursor <= ITEM_CH1_DUTY)
                g_params.ch1_enabled = !g_params.ch1_enabled;
            else if (g_menu.cursor <= ITEM_CH2_DUTY)
                g_params.ch2_enabled = !g_params.ch2_enabled;
            break;
        case EVENT_LONG_PRESS:
            g_menu.selected = 1;
            break;
        default:
            break;
    }
}
