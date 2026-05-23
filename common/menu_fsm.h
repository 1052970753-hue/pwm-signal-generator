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

// VSP mode cursor items
typedef enum {
    VSP_ITEM_VOLTAGE = 0,   // VSP output voltage (0.00~5.00V, 0.01V step)
    VSP_ITEM_ENABLE,        // VSP enable toggle
    NUM_VSP_ITEMS
} VspCursorItem;

// Test mode cursor items
typedef enum {
    TEST_ITEM_CHANNEL = 0,
    TEST_ITEM_FREQ,
    TEST_ITEM_DUTY,
    TEST_ITEM_CYCLES,
    TEST_ITEM_ON_TIME,
    TEST_ITEM_OFF_TIME,
    TEST_ITEM_ON_METHOD,    // 0=PWM, 1=relay, 2=both
    TEST_ITEM_START,
    NUM_TEST_ITEMS          // 8
} TestCursorItem;

// Application modes — 6 modes
typedef enum {
    MODE_PWM_FG = 0,  // Default: dual channel + FG
    MODE_FG,           // FG only, large RPM display
    MODE_CH1,          // CH1 PWM only
    MODE_CH2,          // CH2 PWM only
    MODE_VSP,          // VSP analog voltage output (DAC 0~5V)
    MODE_TEST,         // Auto start/stop test
    NUM_MODES          // 6
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

// Menu state — 6-mode state machine
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
