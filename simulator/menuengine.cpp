#include "menuengine.h"

MenuEngine::MenuEngine(QObject *parent)
    : QObject(parent)
{
    m_params.ch1_freq_hz = 1000;
    m_params.ch1_duty_pct = 50;
    m_params.ch1_enabled  = 0;
    m_params.ch2_freq_hz = 1000;
    m_params.ch2_duty_pct = 50;
    m_params.ch2_enabled  = 0;
    m_params.fg_div       = 2;
    m_params.fg_pulses_per_rev = 2;

    m_state.screen   = SCREEN_MAIN;
    m_state.cursor   = 0;
    m_state.editing  = 0;
    m_state.edit_item = MENU_CH1_FREQ;
    m_state.dirty    = 1;
}

StatusData MenuEngine::status() const {
    StatusData sd;
    sd.ch1_freq_hz  = m_params.ch1_freq_hz;
    sd.ch1_duty_pct = m_params.ch1_duty_pct;
    sd.ch1_enabled  = m_params.ch1_enabled;
    sd.ch2_freq_hz  = m_params.ch2_freq_hz;
    sd.ch2_duty_pct = m_params.ch2_duty_pct;
    sd.ch2_enabled  = m_params.ch2_enabled;
    sd.fg_freq_mhz  = 0;   // simulator has no real FG
    sd.fg_div       = m_params.fg_div;
    sd.rpm          = 0;
    return sd;
}

uint32_t MenuEngine::getEditValue() const {
    switch (m_state.edit_item) {
        case MENU_CH1_FREQ:   return m_params.ch1_freq_hz;
        case MENU_CH1_DUTY:   return m_params.ch1_duty_pct;
        case MENU_CH1_ENABLE: return m_params.ch1_enabled;
        case MENU_CH2_FREQ:   return m_params.ch2_freq_hz;
        case MENU_CH2_DUTY:   return m_params.ch2_duty_pct;
        case MENU_CH2_ENABLE: return m_params.ch2_enabled;
        case MENU_FG_DIV:     return m_params.fg_div;
        default: return 0;
    }
}

void MenuEngine::setEditValue(uint32_t val) {
    switch (m_state.edit_item) {
        case MENU_CH1_FREQ:
        case MENU_CH2_FREQ:
            if (val < 1) val = 1;
            if (val > 100000) val = 100000;
            if (m_state.edit_item == MENU_CH1_FREQ)
                m_params.ch1_freq_hz = val;
            else
                m_params.ch2_freq_hz = val;
            break;
        case MENU_CH1_DUTY:
        case MENU_CH2_DUTY:
            if (val > 100) val = 100;
            if (m_state.edit_item == MENU_CH1_DUTY)
                m_params.ch1_duty_pct = val;
            else
                m_params.ch2_duty_pct = val;
            break;
        case MENU_CH1_ENABLE:
        case MENU_CH2_ENABLE:
            if (m_state.edit_item == MENU_CH1_ENABLE)
                m_params.ch1_enabled = val ? 1 : 0;
            else
                m_params.ch2_enabled = val ? 1 : 0;
            break;
        case MENU_FG_DIV:
            if (val == 1 || val == 2 || val == 4 || val == 8)
                m_params.fg_div = val;
            break;
        default: break;
    }
    m_state.dirty = 1;
}

void MenuEngine::processEvent(InputEvent ev) {
    if (ev == EVENT_NONE) return;

    if (m_state.editing) {
        uint32_t val = getEditValue();
        switch (ev) {
            case EVENT_CW:  setEditValue(val + 1); break;
            case EVENT_CCW: if (val > 0) setEditValue(val - 1); break;
            case EVENT_CLICK: m_state.editing = 0; break;
            case EVENT_LONG_PRESS:
            case EVENT_BACK: m_state.editing = 0; break;
            default: break;
        }
        return;
    }

    switch (ev) {
        case EVENT_CW:
            if (m_state.screen == SCREEN_MAIN_MENU)
                m_state.cursor = (m_state.cursor < MENU_MAIN_ITEMS - 1) ? m_state.cursor + 1 : m_state.cursor;
            else if (m_state.screen == SCREEN_CH1_MENU)
                m_state.cursor = (m_state.cursor < MENU_CH1_ITEMS - 1) ? m_state.cursor + 1 : m_state.cursor;
            else if (m_state.screen == SCREEN_CH2_MENU)
                m_state.cursor = (m_state.cursor < MENU_CH2_ITEMS - 1) ? m_state.cursor + 1 : m_state.cursor;
            else if (m_state.screen == SCREEN_FG_MENU)
                m_state.cursor = (m_state.cursor < MENU_FG_ITEMS - 1) ? m_state.cursor + 1 : m_state.cursor;
            else if (m_state.screen == SCREEN_SYS_MENU)
                m_state.cursor = (m_state.cursor < MENU_SYS_ITEMS - 1) ? m_state.cursor + 1 : m_state.cursor;
            break;
        case EVENT_CCW:
            if (m_state.cursor > 0) m_state.cursor--;
            break;
        case EVENT_CLICK:
            if (m_state.screen == SCREEN_MAIN) {
                m_state.screen = SCREEN_MAIN_MENU;
                m_state.cursor = 0;
            } else if (m_state.screen == SCREEN_MAIN_MENU) {
                switch (m_state.cursor) {
                    case 0: m_state.screen = SCREEN_CH1_MENU; break;
                    case 1: m_state.screen = SCREEN_CH2_MENU; break;
                    case 2: m_state.screen = SCREEN_FG_MENU; break;
                    case 3: m_state.screen = SCREEN_SYS_MENU; break;
                }
                m_state.cursor = 0;
            } else if (m_state.screen >= SCREEN_CH1_MENU) {
                m_state.editing = 1;
                switch (m_state.screen) {
                    case SCREEN_CH1_MENU:
                        m_state.edit_item = (MenuItem)(MENU_CH1_FREQ + m_state.cursor);
                        break;
                    case SCREEN_CH2_MENU:
                        m_state.edit_item = (MenuItem)(MENU_CH2_FREQ + m_state.cursor);
                        break;
                    case SCREEN_FG_MENU:
                        m_state.edit_item = (MenuItem)(MENU_FG_DIV + m_state.cursor);
                        break;
                    case SCREEN_SYS_MENU:
                        m_state.edit_item = (MenuItem)(MENU_SYS_BRIGHTNESS + m_state.cursor);
                        break;
                    default: break;
                }
            }
            break;
        case EVENT_LONG_PRESS:
        case EVENT_BACK:
            if (m_state.screen == SCREEN_MAIN_MENU)
                m_state.screen = SCREEN_MAIN;
            else if (m_state.screen >= SCREEN_CH1_MENU)
                m_state.screen = SCREEN_MAIN_MENU;
            m_state.cursor = 0;
            break;
        default: break;
    }
}
