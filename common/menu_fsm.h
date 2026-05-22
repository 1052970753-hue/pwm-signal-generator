// common/menu_fsm.h
#ifndef MENU_FSM_H
#define MENU_FSM_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

// Cursor items — flat list matching simulator
typedef enum {
    ITEM_CH1_FREQ = 0,
    ITEM_CH1_DUTY,
    ITEM_CH2_FREQ,
    ITEM_CH2_DUTY,
    ITEM_FG_DIV,
    NUM_ITEMS
} CursorItem;

typedef enum {
    EVENT_NONE = 0,
    EVENT_CW,
    EVENT_CCW,
    EVENT_CLICK,
    EVENT_LONG_PRESS
} InputEvent;

#define OLED_WIDTH  128
#define OLED_HEIGHT 64

// Menu state — single screen, two modes (normal / select)
typedef struct {
    uint8_t cursor;    // 0..NUM_ITEMS-1
    uint8_t selected;  // 0=normal (encoder changes value), 1=select (encoder moves cursor)
    uint8_t dirty;     // set when params changed
} MenuState;

#ifdef __cplusplus
}
#endif

#endif
