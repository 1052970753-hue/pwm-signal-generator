#include "ui_render.h"
#include "oled_ssd1306.h"
#include "menu.h"
#include <stdio.h>

static char marker(uint8_t cur, uint8_t sel, uint8_t idx, uint8_t blink) {
    if (cur == idx) {
        if (sel) return blink ? '>' : ' ';
        return '>';
    }
    return ' ';
}

static void draw_row(uint8_t x, uint8_t y, uint8_t xr, uint8_t cur, uint8_t sel,
                     uint8_t idx, uint8_t blink, const char *label, const char *val) {
    char m[2] = { marker(cur, sel, idx, blink), 0 };
    OLED_DrawChar(x, y, m[0]);
    OLED_DrawString(x + 6, y, label);
    uint8_t vw = 0;
    const char *p = val;
    while (*p++) vw += 6;
    OLED_DrawString(xr - vw, y, val);
}

// Right-align a string at position (right_edge - offset), returns left x
static uint8_t draw_right(const char *str, uint8_t right_edge, uint8_t y) {
    uint8_t w = 0;
    const char *p = str;
    while (*p++) w += 6;
    OLED_DrawString(right_edge - w, y, str);
    return right_edge - w;
}

// ── Mode 0: PWM-FG (original layout) ──
static void render_pwm_fg(SystemParams *p, uint16_t rpm, uint8_t blink) {
    char buf[12];
    uint8_t cur = g_menu.cursor;
    uint8_t sel = g_menu.selected;

    OLED_DrawRect(0, 0, OLED_WIDTH, OLED_HEIGHT);
    OLED_DrawHLine(1, 126, 11);
    OLED_DrawString(40, 2, "PWM_TOOL");

    // CH1
    OLED_DrawCircle(3, 14, p->ch1_enabled);
    OLED_DrawString(12, 14, "CH1");
    OLED_DrawString(p->ch1_enabled ? 42 : 36, 14, p->ch1_enabled ? "ON" : "OFF");

    snprintf(buf, sizeof(buf), "%luHz", p->ch1_freq_hz);
    draw_row(3, 26, 59, cur, sel, ITEM_CH1_FREQ, blink, "Fr", buf);

    snprintf(buf, sizeof(buf), "%u%%", p->ch1_duty_pct);
    draw_row(3, 38, 59, cur, sel, ITEM_CH1_DUTY, blink, "Duty", buf);

    // CH2
    OLED_DrawVLine(65, 12, 48);
    OLED_DrawCircle(68, 14, p->ch2_enabled);
    OLED_DrawString(77, 14, "CH2");
    OLED_DrawString(p->ch2_enabled ? 107 : 101, 14, p->ch2_enabled ? "ON" : "OFF");

    snprintf(buf, sizeof(buf), "%luHz", p->ch2_freq_hz);
    draw_row(68, 26, 125, cur, sel, ITEM_CH2_FREQ, blink, "Fr", buf);

    snprintf(buf, sizeof(buf), "%u%%", p->ch2_duty_pct);
    draw_row(68, 38, 125, cur, sel, ITEM_CH2_DUTY, blink, "Duty", buf);

    // FG
    OLED_DrawHLine(1, 126, 51);
    OLED_DrawString(3, 54, "FG");
    snprintf(buf, sizeof(buf), "%u", rpm);
    draw_right(buf, 62, 54);
    OLED_DrawString(64, 54, "RPM");

    char m[2] = { marker(cur, sel, ITEM_FG_DIV, blink), 0 };
    OLED_DrawChar(98, 54, m[0]);
    snprintf(buf, sizeof(buf), "/%u", p->fg_div);
    draw_right(buf, 125, 54);
}

// ── Mode 1: FG only — large RPM ──
static void render_fg_mode(SystemParams *p, uint16_t rpm, uint8_t blink) {
    char buf[12];
    uint8_t cur = g_menu.cursor;
    uint8_t sel = g_menu.selected;

    OLED_DrawRect(0, 0, OLED_WIDTH, OLED_HEIGHT);
    OLED_DrawHLine(1, 126, 11);
    OLED_DrawString(38, 2, "FG MODE");

    // Large RPM display: use 2x height by drawing each char twice with offset
    snprintf(buf, sizeof(buf), "%u", rpm);
    uint8_t len = 0;
    const char *p2 = buf;
    while (*p2++) len++;

    // Draw RPM label
    OLED_DrawString(3, 16, "RPM:");

    // Draw large digits using FillRect blocks (8x16 per digit area)
    // Each digit: draw normal char at (x, 16) and (x, 24) to simulate 2-line height
    uint8_t start_x = (128 - (len * 12)) / 2;
    for (uint8_t i = 0; i < len && i < 8; i++) {
        uint8_t x = start_x + i * 12;
        // Upper half
        OLED_DrawChar(x, 20, buf[i]);
        OLED_DrawChar(x + 1, 20, buf[i]);  // slight offset for bold effect
        // Lower half
        OLED_DrawChar(x, 28, buf[i]);
        OLED_DrawChar(x + 1, 28, buf[i]);
    }

    // Divider line
    OLED_DrawHLine(1, 126, 44);

    // FG div at bottom
    OLED_DrawString(3, 50, "FG");
    snprintf(buf, sizeof(buf), "Freq:%lumHz", p->ch1_freq_hz);
    OLED_DrawString(20, 50, buf);

    // /div selectable
    char m[2] = { marker(cur, sel, 0, blink), 0 };
    OLED_DrawChar(98, 50, m[0]);
    snprintf(buf, sizeof(buf), "/%u", p->fg_div);
    draw_right(buf, 125, 50);
}

// ── Mode 2 & 3: CH1 / CH2 PWM only ──
static void render_ch_mode(SystemParams *p, uint8_t channel, uint16_t rpm, uint8_t blink) {
    char buf[16];
    uint8_t cur = g_menu.cursor;
    uint8_t sel = g_menu.selected;

    uint32_t freq;
    uint8_t duty;
    uint8_t enabled;

    if (channel == 1) {
        freq = p->ch1_freq_hz;
        duty = p->ch1_duty_pct;
        enabled = p->ch1_enabled;
    } else {
        freq = p->ch2_freq_hz;
        duty = p->ch2_duty_pct;
        enabled = p->ch2_enabled;
    }

    OLED_DrawRect(0, 0, OLED_WIDTH, OLED_HEIGHT);
    OLED_DrawHLine(1, 126, 11);

    // Title
    if (channel == 1)
        OLED_DrawString(34, 2, "CH1 PWM");
    else
        OLED_DrawString(34, 2, "CH2 PWM");

    // ON/OFF indicator
    OLED_DrawCircle(3, 14, enabled);
    OLED_DrawString(12, 14, enabled ? "ON" : "OFF");

    // Freq — cursor item 0
    snprintf(buf, sizeof(buf), "%luHz", freq);
    draw_row(3, 28, 125, cur, sel, 0, blink, "Freq", buf);

    // Duty — cursor item 1
    snprintf(buf, sizeof(buf), "%u%%", duty);
    draw_row(3, 40, 125, cur, sel, 1, blink, "Duty", buf);

    // Enable toggle — cursor item 2
    draw_row(3, 52, 125, cur, sel, 2, blink, "EN", enabled ? "ON" : "OFF");

    // FG RPM at bottom-right corner
    OLED_DrawHLine(80, 126, 11);
    snprintf(buf, sizeof(buf), "%u", rpm);
    draw_right(buf, 125, 2);
}

// ── Mode 4: TEST ──
static void render_test_mode(SystemParams *p, uint16_t rpm, uint8_t blink) {
    char buf[16];
    uint8_t cur = g_menu.cursor;
    uint8_t sel = g_menu.selected;

    OLED_DrawRect(0, 0, OLED_WIDTH, OLED_HEIGHT);
    OLED_DrawHLine(1, 126, 11);

    if (test_running) {
        // ── Running: show live status ──
        OLED_DrawString(34, 2, "TEST RUN");

        // Current RPM
        snprintf(buf, sizeof(buf), "RPM:%u", rpm);
        OLED_DrawString(3, 16, buf);

        // Channel info
        snprintf(buf, sizeof(buf), "CH%u %luHz %u%%",
                 Menu_GetTestChannel(), Menu_GetTestFreq(), Menu_GetTestDuty());
        OLED_DrawString(3, 28, buf);

        // Cycle progress
        snprintf(buf, sizeof(buf), "Cycle:%u/%u",
                 Menu_GetTestRecordCount(), Menu_GetTestCycles());
        OLED_DrawString(3, 40, buf);

        // Blink ">>>" indicator when running
        if (blink) {
            OLED_DrawString(100, 40, ">>>");
        }

        // Stop hint
        OLED_DrawString(3, 54, "Click=Stop");
    } else {
        // ── Not running: show config menu ──
        OLED_DrawString(31, 2, "TEST MODE");

        // Channel (item 0)
        snprintf(buf, sizeof(buf), "CH%u", Menu_GetTestChannel());
        draw_row(3, 16, 62, cur, sel, TEST_ITEM_CHANNEL, blink, "Ch", buf);

        // Freq (item 1)
        snprintf(buf, sizeof(buf), "%luHz", Menu_GetTestFreq());
        draw_row(66, 16, 125, cur, sel, TEST_ITEM_FREQ, blink, "Fr", buf);

        // Duty (item 2)
        snprintf(buf, sizeof(buf), "%u%%", Menu_GetTestDuty());
        draw_row(3, 28, 62, cur, sel, TEST_ITEM_DUTY, blink, "Duty", buf);

        // Cycles (item 3)
        snprintf(buf, sizeof(buf), "%u", Menu_GetTestCycles());
        draw_row(66, 28, 125, cur, sel, TEST_ITEM_CYCLES, blink, "N", buf);

        // ON time (item 4)
        snprintf(buf, sizeof(buf), "%us", Menu_GetTestOnSec());
        draw_row(3, 40, 62, cur, sel, TEST_ITEM_ON_TIME, blink, "ON", buf);

        // OFF time (item 5)
        snprintf(buf, sizeof(buf), "%us", Menu_GetTestOffSec());
        draw_row(66, 40, 125, cur, sel, TEST_ITEM_OFF_TIME, blink, "OFF", buf);

        // Start button (item 6)
        OLED_DrawHLine(1, 126, 51);
        char m[2] = { marker(cur, sel, TEST_ITEM_START, blink), 0 };
        OLED_DrawChar(40, 54, m[0]);
        OLED_DrawString(48, 54, "START");

        // Record count
        if (Menu_GetTestRecordCount() > 0) {
            snprintf(buf, sizeof(buf), "Rec:%u", Menu_GetTestRecordCount());
            OLED_DrawString(90, 54, buf);
        }
    }
}

// ── Main dispatch ──
void UI_Render(SystemParams *p, uint16_t rpm, uint8_t blink) {
    OLED_Clear();

    switch (g_menu.mode) {
        case MODE_PWM_FG: render_pwm_fg(p, rpm, blink); break;
        case MODE_FG:     render_fg_mode(p, rpm, blink); break;
        case MODE_CH1:    render_ch_mode(p, 1, rpm, blink); break;
        case MODE_CH2:    render_ch_mode(p, 2, rpm, blink); break;
        case MODE_TEST:   render_test_mode(p, rpm, blink); break;
        default:          render_pwm_fg(p, rpm, blink); break;
    }

    OLED_Refresh();
}
