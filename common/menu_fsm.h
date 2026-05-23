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

// Test mode cursor items
typedef enum {
    TEST_ITEM_CHANNEL = 0,  // CH1 or CH2
    TEST_ITEM_FREQ,
    TEST_ITEM_DUTY,
    TEST_ITEM_CYCLES,
    TEST_ITEM_ON_TIME,
    TEST_ITEM_OFF_TIME,
    TEST_ITEM_START,
    NUM_TEST_ITEMS
} TestCursorItem;

// Application modes
typedef enum {
    MODE_PWM_FG = 0,  // Default: dual channel + FG
    MODE_FG,           // FG only, large RPM display
    MODE_CH1,          // CH1 PWM only
    MODE_CH2,          // CH2 PWM only
    MODE_TEST,         // Auto start/stop test
    NUM_MODES
} AppMode;

typedef enum {
    EVENT_NONE = 0,
    EVENT_CW,
    EVENT_CCW,
    EVENT_CLICK,
    EVENT_LONG_PRESS,
    EVENT_DOUBLE_CLICK
} InputEvent;

// Test states
typedef enum {
    TEST_IDLE = 0,
    TEST_RUNNING,
    TEST_DONE
} TestState;

#define OLED_WIDTH  128
#define OLED_HEIGHT 64

// Menu state — 5-mode state machine (PWM_FG / FG / CH1 / CH2 / TEST)
typedef struct {
    uint8_t cursor;    // 0..NUM_ITEMS-1
    uint8_t selected;  // 0=normal, 1=select (encoder moves cursor)
    uint8_t dirty;     // set when params changed
    uint8_t mode;      // AppMode
    uint8_t blink;     // cursor blink flag
} MenuState;

#ifdef __cplusplus
}
#endif

#endif
