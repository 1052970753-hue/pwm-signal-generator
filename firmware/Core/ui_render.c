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
    // Right-align value at xr
    uint8_t vw = 0;
    const char *p = val;
    while (*p++) vw += 6;
    OLED_DrawString(xr - vw, y, val);
}

void UI_Render(SystemParams *p, uint16_t rpm, uint8_t blink) {
    OLED_Clear();
    char buf[12];
    uint8_t cur = g_menu.cursor;
    uint8_t sel = g_menu.selected;

    // Frame border
    OLED_DrawRect(0, 0, OLED_WIDTH, OLED_HEIGHT);

    // Title bar — separator at y=11
    OLED_DrawHLine(1, 126, 11);
    // "PWM_TOOL" centered: (128 - 8*6) / 2 = 40
    OLED_DrawString(40, 2, "PWM_TOOL");

    // ── CH1 (left col: x=2..61) ──
    OLED_DrawCircle(3, 14, p->ch1_enabled);
    OLED_DrawString(12, 14, "CH1");
    OLED_DrawString(p->ch1_enabled ? 42 : 36, 14, p->ch1_enabled ? "ON" : "OFF");

    snprintf(buf, sizeof(buf), "%luHz", p->ch1_freq_hz);
    draw_row(3, 26, 59, cur, sel, ITEM_CH1_FREQ, blink, "Fr", buf);

    snprintf(buf, sizeof(buf), "%u%%", p->ch1_duty_pct);
    draw_row(3, 38, 59, cur, sel, ITEM_CH1_DUTY, blink, "Duty", buf);

    // ── CH2 (right col: x=67..126) ──
    OLED_DrawVLine(65, 12, 48);
    OLED_DrawCircle(68, 14, p->ch2_enabled);
    OLED_DrawString(77, 14, "CH2");
    OLED_DrawString(p->ch2_enabled ? 107 : 101, 14, p->ch2_enabled ? "ON" : "OFF");

    snprintf(buf, sizeof(buf), "%luHz", p->ch2_freq_hz);
    draw_row(68, 26, 125, cur, sel, ITEM_CH2_FREQ, blink, "Fr", buf);

    snprintf(buf, sizeof(buf), "%u%%", p->ch2_duty_pct);
    draw_row(68, 38, 125, cur, sel, ITEM_CH2_DUTY, blink, "Duty", buf);

    // ── FG (y=52..62, divider at y=51) ──
    OLED_DrawHLine(1, 126, 51);
    OLED_DrawString(3, 54, "FG");

    snprintf(buf, sizeof(buf), "%u", rpm);
    // Right-align RPM number before "RPM" label
    uint8_t nw = 0;
    const char *np = buf;
    while (*np++) nw += 6;
    OLED_DrawString(62 - nw, 54, buf);
    OLED_DrawString(64, 54, "RPM");

    // /div — selectable, right side
    char m[2] = { marker(cur, sel, ITEM_FG_DIV, blink), 0 };
    OLED_DrawChar(98, 54, m[0]);
    snprintf(buf, sizeof(buf), "/%u", p->fg_div);
    uint8_t dw = 0;
    const char *dp = buf;
    while (*dp++) dw += 6;
    OLED_DrawString(125 - dw, 54, buf);

    OLED_Refresh();
}
