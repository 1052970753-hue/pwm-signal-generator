# PWM 信号发生器 — 实施计划

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 构建 STM32F103 双通道 PWM 信号发生器，含 0.96" OLED 菜单控制和 PC 端 Qt 模拟器

**Architecture:** 裸机固件 (C) + PC 模拟器 (Qt 6 C++)，共享菜单状态机定义和通信协议。固件通过 USB CDC 与模拟器通信，模拟器可独立运行或连接硬件。

**Tech Stack:** STM32F103C8T6, CMSIS/StdPeriph, SSD1306 I2C, Qt 6 Widgets, CMake, C++17

---

## 文件结构

```
PWM信号发生器/
├── common/
│   ├── menu_fsm.h           # 菜单状态机枚举&结构体 (共享)
│   └── protocol_defs.h      # 通信协议帧定义 (共享)
├── firmware/
│   └── Core/
│       ├── main.c            # 入口, 初始化, 主循环
│       ├── system_config.h   # 时钟/引脚/外设配置宏
│       ├── oled_ssd1306.c/.h  # SSD1306 I2C 驱动 + 显存
│       ├── encoder.c/.h      # 旋转编码器 (TIM3 编码器模式)
│       ├── menu.c/.h         # 菜单状态机
│       ├── pwm_engine.c/.h   # TIM1/TIM4 PWM 控制
│       ├── fg_capture.c/.h   # TIM2 输入捕获 + RPM 计算
│       ├── protocol.c/.h     # 串口协议解析
│       └── ui_render.c/.h    # OLED 菜单界面渲染
├── simulator/
│   ├── CMakeLists.txt
│   ├── main.cpp
│   ├── mainwindow.h/.cpp     # 主窗口
│   ├── oledwidget.h/.cpp     # SSD1306 像素级渲染 QWidget
│   ├── menuengine.h/.cpp     # 菜单逻辑 (复用 menu_fsm.h)
│   ├── serialcomm.h/.cpp     # QSerialPort 管理
│   ├── protocol.h/.cpp       # 协议编解码 (复用 protocol_defs.h)
│   └── keymap.h/.cpp         # 键盘→按键映射
├── hardware/
│   ├── schematic.md          # 电路原理图 (文本描述)
│   └── BOM.md                # 物料清单
└── docs/superpowers/
    ├── specs/2026-05-17-pwm-signal-generator-design.md
    └── plans/2026-05-17-pwm-signal-generator-plan.md
```

---

### Task 1: 项目骨架 & 共享头文件

**Files:**
- Create: `common/menu_fsm.h`
- Create: `common/protocol_defs.h`

- [ ] **Step 1: 编写 menu_fsm.h 菜单状态机定义**

```c
// common/menu_fsm.h
#ifndef MENU_FSM_H
#define MENU_FSM_H

typedef enum {
    SCREEN_MAIN = 0,
    SCREEN_MAIN_MENU,
    SCREEN_CH1_MENU,
    SCREEN_CH2_MENU,
    SCREEN_FG_MENU,
    SCREEN_SYS_MENU
} MenuScreen;

typedef enum {
    MENU_CH1_FREQ = 0,
    MENU_CH1_DUTY,
    MENU_CH1_ENABLE,
    MENU_CH2_FREQ,
    MENU_CH2_DUTY,
    MENU_CH2_ENABLE,
    MENU_FG_DIV,
    MENU_FG_REFRESH,
    MENU_SYS_BRIGHTNESS,
    MENU_SYS_RESET
} MenuItem;

typedef enum {
    EVENT_NONE = 0,
    EVENT_CW,        // 编码器顺时针
    EVENT_CCW,       // 编码器逆时针
    EVENT_CLICK,     // 编码器按下
    EVENT_LONG_PRESS,// 编码器长按 (1s)
    EVENT_BACK       // BACK 按键
} InputEvent;

#define MENU_CH1_ITEMS 3
#define MENU_CH2_ITEMS 3
#define MENU_FG_ITEMS  2
#define MENU_SYS_ITEMS 2
#define MENU_MAIN_ITEMS 4  // CH1, CH2, FG, 系统

#define OLED_WIDTH  128
#define OLED_HEIGHT 64
#define OLED_LINE_HEIGHT 16  // 中文行高
#define OLED_MAX_LINES 4

#endif
```

- [ ] **Step 2: 编写 protocol_defs.h 协议定义**

```c
// common/protocol_defs.h
#ifndef PROTOCOL_DEFS_H
#define PROTOCOL_DEFS_H

#include <stdint.h>

#define FRAME_HEADER_PC2MCU  0xAA
#define FRAME_HEADER_MCU2PC  0xBB

typedef enum {
    CMD_READ_STATUS  = 0x10,
    CMD_WRITE_PWM    = 0x20,
    CMD_WRITE_FG_DIV = 0x30,
    CMD_OLED_BUFFER  = 0x40,
    CMD_KEY_EVENT    = 0x41
} ProtocolCmd;

// 0x10 响应: 状态数据
typedef struct __attribute__((packed)) {
    uint32_t ch1_freq_hz;     // CH1 频率 Hz
    uint8_t  ch1_duty_pct;    // CH1 占空比 %
    uint8_t  ch1_enabled;     // CH1 使能
    uint32_t ch2_freq_hz;
    uint8_t  ch2_duty_pct;
    uint8_t  ch2_enabled;
    uint32_t fg_freq_mhz;     // FG 频率 mHz (毫赫兹精度)
    uint8_t  fg_div;          // FG 分频比 1/2/4/8
    uint16_t rpm;             // 计算RPM
} StatusData;

// 0x20 请求: 写入 PWM 参数
typedef struct __attribute__((packed)) {
    uint8_t  channel;         // 1 or 2
    uint32_t freq_hz;
    uint8_t  duty_pct;
    uint8_t  enable;
} PwmWriteReq;

// 0x30 请求: 写入 FG 分频比
typedef struct __attribute__((packed)) {
    uint8_t div;  // 1,2,4,8
} FgDivReq;

// 0x41 请求: 按键事件
typedef struct __attribute__((packed)) {
    uint8_t event;  // 对应 InputEvent
} KeyEventReq;

uint8_t crc8(const uint8_t *data, uint8_t len);
#endif
```

---

### Task 2: 固件 — system_config.h 系统配置

**Files:**
- Create: `firmware/Core/system_config.h`

- [ ] **Step 1: 编写系统配置头文件**

```c
// firmware/Core/system_config.h
#ifndef SYSTEM_CONFIG_H
#define SYSTEM_CONFIG_H

#include "stm32f10x.h"

// 系统时钟: 72MHz, HSE 8MHz → PLL x9
#define SYSCLK_FREQ  72000000

// ── OLED (I2C1) ──
#define OLED_I2C         I2C1
#define OLED_I2C_CLK     RCC_APB1Periph_I2C1
#define OLED_GPIO        GPIOB
#define OLED_GPIO_CLK    RCC_APB2Periph_GPIOB
#define OLED_SCL_PIN     GPIO_Pin_6   // PB6 → 但PWM2也用PB6!
// 修正: OLED SCL → PB8, OLED SDA → PB9 (重映射)
#define OLED_SCL_PIN     GPIO_Pin_8
#define OLED_SDA_PIN     GPIO_Pin_9
#define OLED_ADDR        0x3C

// ── PWM CH1 (TIM1 CH1) ──
#define PWM1_TIM         TIM1
#define PWM1_TIM_CLK     RCC_APB2Periph_TIM1
#define PWM1_GPIO        GPIOA
#define PWM1_GPIO_CLK    RCC_APB2Periph_GPIOA
#define PWM1_PIN         GPIO_Pin_8
#define PWM1_CHANNEL     1

// ── PWM CH2 (TIM4 CH1) ──
#define PWM2_TIM         TIM4
#define PWM2_TIM_CLK     RCC_APB1Periph_TIM4
#define PWM2_GPIO        GPIOB
#define PWM2_GPIO_CLK    RCC_APB2Periph_GPIOB
#define PWM2_PIN         GPIO_Pin_6
#define PWM2_CHANNEL     1

// ── FG 输入 (TIM2 CH1) ──
#define FG_TIM           TIM2
#define FG_TIM_CLK       RCC_APB1Periph_TIM2
#define FG_GPIO          GPIOA
#define FG_GPIO_CLK      RCC_APB2Periph_GPIOA
#define FG_PIN           GPIO_Pin_0
#define FG_CHANNEL       1

// ── 编码器 (TIM3 编码器模式) ──
#define ENC_TIM          TIM3
#define ENC_TIM_CLK      RCC_APB1Periph_TIM3
#define ENC_GPIO         GPIOB
#define ENC_GPIO_CLK     RCC_APB2Periph_GPIOB
#define ENC_A_PIN        GPIO_Pin_10
#define ENC_B_PIN        GPIO_Pin_11

// ── 按键 ──
#define BTN_GPIO         GPIOB
#define BTN_GPIO_CLK     RCC_APB2Periph_GPIOB
#define BTN_OK_PIN       GPIO_Pin_12
#define BTN_BACK_PIN     GPIO_Pin_13

// ── USB CDC (USART1) ──
#define CDC_USART        USART1
#define CDC_USART_CLK    RCC_APB2Periph_USART1
#define CDC_GPIO         GPIOA
#define CDC_GPIO_CLK     RCC_APB2Periph_GPIOA
#define CDC_TX_PIN       GPIO_Pin_9
#define CDC_RX_PIN       GPIO_Pin_10
#define CDC_BAUDRATE     115200

// ── 运行参数 ──
typedef struct {
    uint32_t ch1_freq_hz;
    uint8_t  ch1_duty_pct;
    uint8_t  ch1_enabled;
    uint32_t ch2_freq_hz;
    uint8_t  ch2_duty_pct;
    uint8_t  ch2_enabled;
    uint8_t  fg_div;
    uint16_t fg_pulses_per_rev;  // 每转脉冲数
} SystemParams;

void SystemClock_Config(void);
void GPIO_Config(void);
void TIM_Config(void);
void I2C_Config(void);
void USART_Config(void);

#endif
```

注意：OLED 引脚修正为 PB8/PB9 避免与 PWM2 PB6 冲突。

---

### Task 3: 固件 — OLED SSD1306 驱动

**Files:**
- Create: `firmware/Core/oled_ssd1306.c`
- Create: `firmware/Core/oled_ssd1306.h`

- [ ] **Step 1: 编写 OLED 驱动头文件**

```c
// firmware/Core/oled_ssd1306.h
#ifndef OLED_SSD1306_H
#define OLED_SSD1306_H

#include "system_config.h"

#define OLED_CMD  0x00
#define OLED_DATA 0x40

void OLED_Init(void);
void OLED_Clear(void);
void OLED_Refresh(void);          // 刷新显存到屏幕
void OLED_SetPixel(uint8_t x, uint8_t y, uint8_t color);
void OLED_DrawChar(uint8_t x, uint8_t y, char c, uint8_t size);  // size: 8或16
void OLED_DrawString(uint8_t x, uint8_t y, const char *str, uint8_t size);
void OLED_DrawNum(uint8_t x, uint8_t y, uint32_t num, uint8_t digits, uint8_t size);
void OLED_DrawLine(uint8_t x0, uint8_t y0, uint8_t x1, uint8_t y1);
void OLED_DrawRect(uint8_t x, uint8_t y, uint8_t w, uint8_t h, uint8_t fill);
void OLED_SetCursor(uint8_t line); // 按行定位光标

extern uint8_t OLED_Buffer[OLED_WIDTH * OLED_HEIGHT / 8];

#endif
```

- [ ] **Step 2: 编写 OLED 驱动实现**

```c
// firmware/Core/oled_ssd1306.c
#include "oled_ssd1306.h"
#include <string.h>

uint8_t OLED_Buffer[OLED_WIDTH * OLED_HEIGHT / 8];

static const uint8_t Font5x7[96][5] = {
    // ... 标准 5x7 ASCII 字体 (96 字符, 从空格到~)
    // 限于篇幅, 此处省略字体数据, 实际使用时完整定义
    // 包含: {0x00,0x00,0x00,0x00,0x00} 空格
    //       到 {0x00,0x00,0x00,0x00,0x00} ~
};

static void OLED_WriteCmd(uint8_t cmd) {
    while(I2C_GetFlagStatus(OLED_I2C, I2C_FLAG_BUSY));
    I2C_GenerateSTART(OLED_I2C, ENABLE);
    while(!I2C_CheckEvent(OLED_I2C, I2C_EVENT_MASTER_MODE_SELECT));
    I2C_Send7bitAddress(OLED_I2C, OLED_ADDR << 1, I2C_Direction_Transmitter);
    while(!I2C_CheckEvent(OLED_I2C, I2C_EVENT_MASTER_TRANSMITTER_MODE_SELECTED));
    I2C_SendData(OLED_I2C, OLED_CMD);
    while(!I2C_CheckEvent(OLED_I2C, I2C_EVENT_MASTER_BYTE_TRANSMITTED));
    I2C_SendData(OLED_I2C, cmd);
    while(!I2C_CheckEvent(OLED_I2C, I2C_EVENT_MASTER_BYTE_TRANSMITTED));
    I2C_GenerateSTOP(OLED_I2C, ENABLE);
}

static void OLED_WriteData(uint8_t data) {
    while(I2C_GetFlagStatus(OLED_I2C, I2C_FLAG_BUSY));
    I2C_GenerateSTART(OLED_I2C, ENABLE);
    while(!I2C_CheckEvent(OLED_I2C, I2C_EVENT_MASTER_MODE_SELECT));
    I2C_Send7bitAddress(OLED_I2C, OLED_ADDR << 1, I2C_Direction_Transmitter);
    while(!I2C_CheckEvent(OLED_I2C, I2C_EVENT_MASTER_TRANSMITTER_MODE_SELECTED));
    I2C_SendData(OLED_I2C, OLED_DATA);
    while(!I2C_CheckEvent(OLED_I2C, I2C_EVENT_MASTER_BYTE_TRANSMITTED));
    I2C_SendData(OLED_I2C, data);
    while(!I2C_CheckEvent(OLED_I2C, I2C_EVENT_MASTER_BYTE_TRANSMITTED));
    I2C_GenerateSTOP(OLED_I2C, ENABLE);
}

void OLED_Init(void) {
    OLED_WriteCmd(0xAE); // Display OFF
    OLED_WriteCmd(0xD5); OLED_WriteCmd(0x80); // Set OSC freq
    OLED_WriteCmd(0xA8); OLED_WriteCmd(0x3F); // MUX ratio 64
    OLED_WriteCmd(0xD3); OLED_WriteCmd(0x00); // Display offset 0
    OLED_WriteCmd(0x40); // Start line 0
    OLED_WriteCmd(0x8D); OLED_WriteCmd(0x14); // Charge pump enable
    OLED_WriteCmd(0x20); OLED_WriteCmd(0x00); // Horizontal addressing
    OLED_WriteCmd(0xA1); // Segment remap (左右镜像)
    OLED_WriteCmd(0xC8); // COM scan direction (上下翻转)
    OLED_WriteCmd(0xDA); OLED_WriteCmd(0x12); // COM pins
    OLED_WriteCmd(0x81); OLED_WriteCmd(0xCF); // Contrast
    OLED_WriteCmd(0xD9); OLED_WriteCmd(0xF1); // Pre-charge
    OLED_WriteCmd(0xDB); OLED_WriteCmd(0x40); // VCOM detect
    OLED_WriteCmd(0xA4); // Resume to RAM
    OLED_WriteCmd(0xA6); // Normal display
    OLED_WriteCmd(0xAF); // Display ON
    OLED_Clear();
    OLED_Refresh();
}

void OLED_Clear(void) {
    memset(OLED_Buffer, 0, sizeof(OLED_Buffer));
}

void OLED_Refresh(void) {
    for(uint8_t page = 0; page < 8; page++) {
        OLED_WriteCmd(0xB0 + page);
        OLED_WriteCmd(0x00);
        OLED_WriteCmd(0x10);
        for(uint8_t col = 0; col < 128; col++) {
            OLED_WriteData(OLED_Buffer[page * 128 + col]);
        }
    }
}

void OLED_SetPixel(uint8_t x, uint8_t y, uint8_t color) {
    if(x >= OLED_WIDTH || y >= OLED_HEIGHT) return;
    if(color)
        OLED_Buffer[x + (y / 8) * OLED_WIDTH] |=  (1 << (y % 8));
    else
        OLED_Buffer[x + (y / 8) * OLED_WIDTH] &= ~(1 << (y % 8));
}

void OLED_DrawChar(uint8_t x, uint8_t y, char c, uint8_t size) {
    if(c < ' ' || c > '~') c = ' ';
    const uint8_t *glyph = Font5x7[c - ' '];
    for(uint8_t i = 0; i < 5; i++) {
        uint8_t line = glyph[i];
        for(uint8_t j = 0; j < 8; j++) {
            if(line & (1 << j))
                OLED_SetPixel(x + i, y + j, 1);
        }
    }
}

void OLED_DrawString(uint8_t x, uint8_t y, const char *str, uint8_t size) {
    while(*str) {
        OLED_DrawChar(x, y, *str, size);
        x += 6;
        str++;
    }
}

void OLED_SetCursor(uint8_t line) {
    // 简化实现: 存储当前行号用于后续绘制定位
    // 实际使用时在 ui_render 中管理
}
```

---

### Task 4: 固件 — 编码器驱动

**Files:**
- Create: `firmware/Core/encoder.c`
- Create: `firmware/Core/encoder.h`

- [ ] **Step 1: 编写编码器头文件**

```c
// firmware/Core/encoder.h
#ifndef ENCODER_H
#define ENCODER_H

#include "common/menu_fsm.h"

void Encoder_Init(void);
InputEvent Encoder_Poll(void);     // 非阻塞轮询, 返回事件
int16_t Encoder_GetDelta(void);    // 获取累积增量
void Encoder_Reset(void);

#endif
```

- [ ] **Step 2: 编写编码器实现**

```c
// firmware/Core/encoder.c
#include "encoder.h"
#include "system_config.h"

static int16_t enc_count = 0;
static int16_t enc_last = 0;
static uint32_t btn_press_time = 0;
static uint8_t  btn_ok_prev = 1;

void Encoder_Init(void) {
    // TIM3 编码器模式
    TIM_TimeBaseInitTypeDef TIM_TimeBaseInitStructure;
    TIM_ICInitTypeDef TIM_ICInitStructure;
    
    RCC_APB1PeriphClockCmd(ENC_TIM_CLK, ENABLE);
    RCC_APB2PeriphClockCmd(ENC_GPIO_CLK | BTN_GPIO_CLK, ENABLE);
    
    // PB10, PB11 浮空输入
    GPIO_InitTypeDef GPIO_InitStructure;
    GPIO_InitStructure.GPIO_Pin = ENC_A_PIN | ENC_B_PIN;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING;
    GPIO_Init(ENC_GPIO, &GPIO_InitStructure);
    
    // PB12, PB13 上拉输入
    GPIO_InitStructure.GPIO_Pin = BTN_OK_PIN | BTN_BACK_PIN;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPU;
    GPIO_Init(BTN_GPIO, &GPIO_InitStructure);
    
    TIM_TimeBaseInitStructure.TIM_Period = 65535;
    TIM_TimeBaseInitStructure.TIM_Prescaler = 0;
    TIM_TimeBaseInitStructure.TIM_ClockDivision = 0;
    TIM_TimeBaseInitStructure.TIM_CounterMode = TIM_CounterMode_Up;
    TIM_TimeBaseInit(ENC_TIM, &TIM_TimeBaseInitStructure);
    
    TIM_EncoderInterfaceConfig(ENC_TIM, TIM_EncoderMode_TI12,
        TIM_ICPolarity_Rising, TIM_ICPolarity_Rising);
    
    TIM_ICInitStructure.TIM_Channel = TIM_Channel_1;
    TIM_ICInitStructure.TIM_ICPolarity = TIM_ICPolarity_Rising;
    TIM_ICInitStructure.TIM_ICSelection = TIM_ICSelection_DirectTI;
    TIM_ICInitStructure.TIM_ICPrescaler = TIM_ICPSC_DIV1;
    TIM_ICInitStructure.TIM_ICFilter = 0x0F;  // 去抖滤波
    TIM_ICInit(ENC_TIM, &TIM_ICInitStructure);
    
    TIM_ICInitStructure.TIM_Channel = TIM_Channel_2;
    TIM_ICInit(ENC_TIM, &TIM_ICInitStructure);
    
    TIM_Cmd(ENC_TIM, ENABLE);
}

InputEvent Encoder_Poll(void) {
    int16_t now = TIM_GetCounter(ENC_TIM);
    int16_t delta = (int16_t)(now - enc_last);
    enc_last = now;
    
    uint8_t btn_ok = GPIO_ReadInputDataBit(BTN_GPIO, BTN_OK_PIN);
    uint8_t btn_back = GPIO_ReadInputDataBit(BTN_GPIO, BTN_BACK_PIN);
    
    // BACK 按键
    if(btn_back == 0) return EVENT_BACK;
    
    // OK 按键
    if(btn_ok == 0 && btn_ok_prev == 1) {
        btn_press_time = 0; // 开始计时 (用 SysTick)
        btn_ok_prev = 0;
        return EVENT_NONE;
    }
    if(btn_ok == 0) {
        btn_press_time++;
        if(btn_press_time > 1000) { // 1秒长按
            btn_press_time = 0;
            return EVENT_LONG_PRESS;
        }
        btn_ok_prev = 0;
        return EVENT_NONE;
    }
    if(btn_ok == 1 && btn_ok_prev == 0) {
        btn_ok_prev = 1;
        if(btn_press_time < 1000) {
            btn_press_time = 0;
            return EVENT_CLICK;
        }
    }
    btn_ok_prev = btn_ok;
    
    // 编码器旋转
    if(delta >= 4) {  // 去抖阈值
        enc_count += delta;
        return EVENT_CW;
    } else if(delta <= -4) {
        enc_count += delta;
        return EVENT_CCW;
    }
    
    return EVENT_NONE;
}

int16_t Encoder_GetDelta(void) {
    int16_t d = enc_count;
    enc_count = 0;
    return d;
}

void Encoder_Reset(void) {
    enc_count = 0;
    TIM_SetCounter(ENC_TIM, 0);
    enc_last = 0;
}
```

---

### Task 5: 固件 — 菜单状态机

**Files:**
- Create: `firmware/Core/menu.c`
- Create: `firmware/Core/menu.h`

- [ ] **Step 1: 编写菜单头文件**

```c
// firmware/Core/menu.h
#ifndef MENU_H
#define MENU_H

#include "common/menu_fsm.h"
#include "system_config.h"

typedef struct {
    MenuScreen screen;
    uint8_t    cursor;          // 当前菜单项索引
    uint8_t    editing;         // 是否在参数编辑模式
    MenuItem   edit_item;       // 正在编辑的项
    SystemParams params;        // 运行参数
    uint8_t    dirty;           // 参数变更标记
} MenuState;

void Menu_Init(MenuState *menu);
void Menu_Process(MenuState *menu, InputEvent ev);
uint32_t Menu_GetEditValue(MenuState *menu);
void Menu_SetEditValue(MenuState *menu, uint32_t val);

#endif
```

- [ ] **Step 2: 编写菜单状态机实现**

```c
// firmware/Core/menu.c
#include "menu.h"

// 频率档位表 (Hz)
static const uint32_t freq_table[] = {
    1, 2, 5, 10, 20, 50, 100, 200, 500,
    1000, 2000, 5000, 10000, 20000, 50000, 100000
};
#define FREQ_TABLE_SIZE (sizeof(freq_table) / sizeof(freq_table[0]))

static uint8_t freq_index(uint32_t hz) {
    uint8_t best = 0;
    uint32_t best_diff = 0xFFFFFFFF;
    for(uint8_t i = 0; i < FREQ_TABLE_SIZE; i++) {
        uint32_t diff = (hz > freq_table[i]) ? (hz - freq_table[i]) : (freq_table[i] - hz);
        if(diff < best_diff) { best_diff = diff; best = i; }
    }
    return best;
}

void Menu_Init(MenuState *menu) {
    menu->screen = SCREEN_MAIN;
    menu->cursor = 0;
    menu->editing = 0;
    menu->edit_item = 0;
    menu->dirty = 1;
    menu->params.ch1_freq_hz = 1000;
    menu->params.ch1_duty_pct = 50;
    menu->params.ch1_enabled = 0;
    menu->params.ch2_freq_hz = 1000;
    menu->params.ch2_duty_pct = 50;
    menu->params.ch2_enabled = 0;
    menu->params.fg_div = 2;
    menu->params.fg_pulses_per_rev = 2;
}

void Menu_Process(MenuState *menu, InputEvent ev) {
    if(ev == EVENT_NONE) return;
    menu->dirty = 1;
    
    if(menu->editing) {
        // ── 参数编辑模式 ──
        switch(ev) {
            case EVENT_CW:
                Menu_SetEditValue(menu, Menu_GetEditValue(menu) + 1);
                break;
            case EVENT_CCW:
                Menu_SetEditValue(menu, Menu_GetEditValue(menu) - 1);
                break;
            case EVENT_CLICK:
                menu->editing = 0;  // 确认退出编辑
                break;
            case EVENT_LONG_PRESS:
            case EVENT_BACK:
                menu->editing = 0;  // 取消 (不保存)
                break;
            default: break;
        }
        return;
    }
    
    // ── 菜单导航模式 ──
    switch(ev) {
        case EVENT_CW:
            menu->cursor++;
            break;
        case EVENT_CCW:
            if(menu->cursor > 0) menu->cursor--;
            break;
        case EVENT_CLICK:
            menu->editing = 1;
            break;
        case EVENT_LONG_PRESS:
        case EVENT_BACK:
            // 返回逻辑
            if(menu->screen == SCREEN_MAIN_MENU)
                menu->screen = SCREEN_MAIN;
            else if(menu->screen >= SCREEN_CH1_MENU)
                menu->screen = SCREEN_MAIN_MENU;
            break;
        default: break;
    }
}

uint32_t Menu_GetEditValue(MenuState *menu) {
    switch(menu->edit_item) {
        case MENU_CH1_FREQ:  return menu->params.ch1_freq_hz;
        case MENU_CH1_DUTY:  return menu->params.ch1_duty_pct;
        case MENU_CH1_ENABLE:return menu->params.ch1_enabled;
        case MENU_CH2_FREQ:  return menu->params.ch2_freq_hz;
        case MENU_CH2_DUTY:  return menu->params.ch2_duty_pct;
        case MENU_CH2_ENABLE:return menu->params.ch2_enabled;
        case MENU_FG_DIV:    return menu->params.fg_div;
        default: return 0;
    }
}

void Menu_SetEditValue(MenuState *menu, uint32_t val) {
    switch(menu->edit_item) {
        case MENU_CH1_FREQ:
        case MENU_CH2_FREQ:
            if(val < 1) val = 1;
            if(val > 100000) val = 100000;
            if(menu->edit_item == MENU_CH1_FREQ)
                menu->params.ch1_freq_hz = val;
            else
                menu->params.ch2_freq_hz = val;
            break;
        case MENU_CH1_DUTY:
        case MENU_CH2_DUTY:
            if(val > 100) val = 100;
            if(menu->edit_item == MENU_CH1_DUTY)
                menu->params.ch1_duty_pct = val;
            else
                menu->params.ch2_duty_pct = val;
            break;
        case MENU_CH1_ENABLE:
        case MENU_CH2_ENABLE:
            val = val ? 1 : 0;
            if(menu->edit_item == MENU_CH1_ENABLE)
                menu->params.ch1_enabled = val;
            else
                menu->params.ch2_enabled = val;
            break;
        case MENU_FG_DIV:
            if(val == 1 || val == 2 || val == 4 || val == 8)
                menu->params.fg_div = val;
            break;
        default: break;
    }
}
```

---

### Task 6: 固件 — PWM 引擎

**Files:**
- Create: `firmware/Core/pwm_engine.c`
- Create: `firmware/Core/pwm_engine.h`

- [ ] **Step 1: 编写 PWM 引擎头文件**

```c
// firmware/Core/pwm_engine.h
#ifndef PWM_ENGINE_H
#define PWM_ENGINE_H

#include "system_config.h"

void PWM_Init(void);
void PWM_SetChannel(uint8_t channel, uint32_t freq_hz, uint8_t duty_pct);
void PWM_EnableChannel(uint8_t channel, uint8_t enable);
void PWM_UpdateFromParams(SystemParams *params);

#endif
```

- [ ] **Step 2: 编写 PWM 引擎实现**

```c
// firmware/Core/pwm_engine.c
#include "pwm_engine.h"

typedef struct {
    TIM_TypeDef *tim;
    uint8_t channel;
    uint16_t *ccr;  // 指向对应 CCR 寄存器
} PwmChannel;

void PWM_Init(void) {
    RCC_APB2PeriphClockCmd(PWM1_TIM_CLK | PWM1_GPIO_CLK, ENABLE);
    RCC_APB1PeriphClockCmd(PWM2_TIM_CLK, ENABLE);
    RCC_APB2PeriphClockCmd(PWM2_GPIO_CLK, ENABLE);
    
    // PA8: TIM1 CH1, 推挽复用 50MHz
    GPIO_InitTypeDef gpio;
    gpio.GPIO_Pin = PWM1_PIN;
    gpio.GPIO_Mode = GPIO_Mode_AF_PP;
    gpio.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(PWM1_GPIO, &gpio);
    
    // PB6: TIM4 CH1
    gpio.GPIO_Pin = PWM2_PIN;
    GPIO_Init(PWM2_GPIO, &gpio);
    
    // 默认关闭, 由 SetChannel 时启动
}

void PWM_SetChannel(uint8_t channel, uint32_t freq_hz, uint8_t duty_pct) {
    TIM_TypeDef *tim = (channel == 1) ? PWM1_TIM : PWM2_TIM;
    
    if(freq_hz < 1) freq_hz = 1;
    if(freq_hz > 100000) freq_hz = 100000;
    if(duty_pct > 100) duty_pct = 100;
    
    // 分频器自适应
    uint16_t psc;
    uint16_t arr;
    if(freq_hz <= 100) {
        psc = 7200;
        arr = SYSCLK_FREQ / (psc + 1) / freq_hz - 1;
    } else if(freq_hz <= 1000) {
        psc = 720;
        arr = SYSCLK_FREQ / (psc + 1) / freq_hz - 1;
    } else if(freq_hz <= 10000) {
        psc = 72;
        arr = SYSCLK_FREQ / (psc + 1) / freq_hz - 1;
    } else {
        psc = 7;
        arr = SYSCLK_FREQ / (psc + 1) / freq_hz - 1;
    }
    if(arr < 100) arr = 100;
    
    uint16_t ccr = (uint16_t)((uint32_t)arr * duty_pct / 100);
    
    TIM_TimeBaseInitTypeDef tim_init;
    TIM_OCInitTypeDef oc_init;
    
    TIM_DeInit(tim);
    
    tim_init.TIM_Period = arr;
    tim_init.TIM_Prescaler = psc;
    tim_init.TIM_ClockDivision = 0;
    tim_init.TIM_CounterMode = TIM_CounterMode_Up;
    TIM_TimeBaseInit(tim, &tim_init);
    
    oc_init.TIM_OCMode = TIM_OCMode_PWM1;
    oc_init.TIM_OutputState = TIM_OutputState_Enable;
    oc_init.TIM_Pulse = ccr;
    oc_init.TIM_OCPolarity = TIM_OCPolarity_High;
    if(channel == 1) {
        TIM_OC1Init(tim, &oc_init);
        TIM_OC1PreloadConfig(tim, TIM_OCPreload_Enable);
    } else {
        TIM_OC2Init(tim, &oc_init);
        TIM_OC2PreloadConfig(tim, TIM_OCPreload_Enable);
    }
    
    TIM_ARRPreloadConfig(tim, ENABLE);
    TIM_Cmd(tim, ENABLE);
    TIM_CtrlPWMOutputs(tim, ENABLE);
}

void PWM_EnableChannel(uint8_t channel, uint8_t enable) {
    if(enable) {
        PWM_SetChannel(channel, 1000, 0);  // 默认 1kHz 0% 启动
    } else {
        TIM_TypeDef *tim = (channel == 1) ? PWM1_TIM : PWM2_TIM;
        TIM_Cmd(tim, DISABLE);
        TIM_CtrlPWMOutputs(tim, DISABLE);
    }
}

void PWM_UpdateFromParams(SystemParams *params) {
    if(params->ch1_enabled)
        PWM_SetChannel(1, params->ch1_freq_hz, params->ch1_duty_pct);
    else
        PWM_EnableChannel(1, 0);
    
    if(params->ch2_enabled)
        PWM_SetChannel(2, params->ch2_freq_hz, params->ch2_duty_pct);
    else
        PWM_EnableChannel(2, 0);
}
```

---

### Task 7: 固件 — FG 捕获引擎

**Files:**
- Create: `firmware/Core/fg_capture.c`
- Create: `firmware/Core/fg_capture.h`

- [ ] **Step 1: 编写 FG 捕获头文件**

```c
// firmware/Core/fg_capture.h
#ifndef FG_CAPTURE_H
#define FG_CAPTURE_H

#include "system_config.h"

void FG_Init(void);
uint32_t FG_GetFrequency_mHz(void);  // 返回频率 (毫赫兹)
uint16_t FG_CalculateRPM(uint8_t div, uint16_t pulses_per_rev);

#endif
```

- [ ] **Step 2: 编写 FG 捕获实现**

```c
// firmware/Core/fg_capture.c
#include "fg_capture.h"

static volatile uint32_t fg_freq_mhz = 0;
static volatile uint32_t last_capture = 0;
static volatile uint32_t curr_capture = 0;
static volatile uint8_t  capture_ready = 0;

void FG_Init(void) {
    RCC_APB1PeriphClockCmd(FG_TIM_CLK, ENABLE);
    RCC_APB2PeriphClockCmd(FG_GPIO_CLK, ENABLE);
    
    // PA0 浮空输入
    GPIO_InitTypeDef gpio;
    gpio.GPIO_Pin = FG_PIN;
    gpio.GPIO_Mode = GPIO_Mode_IN_FLOATING;
    GPIO_Init(FG_GPIO, &gpio);
    
    TIM_TimeBaseInitTypeDef tim_init;
    tim_init.TIM_Period = 65535;
    tim_init.TIM_Prescaler = 71;  // 1MHz 定时器时钟 (1us 分辨率)
    tim_init.TIM_ClockDivision = 0;
    tim_init.TIM_CounterMode = TIM_CounterMode_Up;
    TIM_TimeBaseInit(FG_TIM, &tim_init);
    
    TIM_ICInitTypeDef ic_init;
    ic_init.TIM_Channel = TIM_Channel_1;
    ic_init.TIM_ICPolarity = TIM_ICPolarity_Rising;
    ic_init.TIM_ICSelection = TIM_ICSelection_DirectTI;
    ic_init.TIM_ICPrescaler = TIM_ICPSC_DIV1;
    ic_init.TIM_ICFilter = 0x0F;
    TIM_ICInit(FG_TIM, &ic_init);
    
    TIM_ITConfig(FG_TIM, TIM_IT_CC1, ENABLE);
    NVIC_EnableIRQ(TIM2_IRQn);
    
    TIM_Cmd(FG_TIM, ENABLE);
}

void TIM2_IRQHandler(void) {
    if(TIM_GetITStatus(FG_TIM, TIM_IT_CC1) != RESET) {
        TIM_ClearITPendingBit(FG_TIM, TIM_IT_CC1);
        last_capture = curr_capture;
        curr_capture = TIM_GetCapture1(FG_TIM);
        
        if(curr_capture > last_capture) {
            uint32_t period_us = curr_capture - last_capture;
            if(period_us > 0)
                fg_freq_mhz = 1000000000UL / period_us;  // mHz
        }
        capture_ready = 1;
    }
}

uint32_t FG_GetFrequency_mHz(void) {
    capture_ready = 0;
    return fg_freq_mhz;
}

uint16_t FG_CalculateRPM(uint8_t div, uint16_t pulses_per_rev) {
    uint32_t freq_mhz = FG_GetFrequency_mHz();
    if(pulses_per_rev == 0) pulses_per_rev = 1;
    if(div == 0) div = 1;
    return (uint16_t)((freq_mhz / 1000 / div) * 60 / pulses_per_rev);
}
```

---

### Task 8: 固件 — 串口协议 & UI 渲染

**Files:**
- Create: `firmware/Core/protocol.c`
- Create: `firmware/Core/protocol.h`
- Create: `firmware/Core/ui_render.c`
- Create: `firmware/Core/ui_render.h`

- [ ] **Step 1: 编写协议处理头文件**

```c
// firmware/Core/protocol.h
#ifndef PROTOCOL_H
#define PROTOCOL_H

#include "common/protocol_defs.h"
#include "system_config.h"

void Protocol_Init(void);
void Protocol_ProcessByte(uint8_t byte);
void Protocol_SendStatus(StatusData *status);
void Protocol_SendOLEDBuffer(void);

#endif
```

- [ ] **Step 2: 编写协议处理实现**

```c
// firmware/Core/protocol.c
#include "protocol.h"
#include <string.h>

static uint8_t rx_buf[64];
static uint8_t rx_idx = 0;
static uint8_t rx_len = 0;
static uint8_t rx_cmd = 0;

void Protocol_Init(void) {
    USART_InitTypeDef usart;
    usart.USART_BaudRate = CDC_BAUDRATE;
    usart.USART_WordLength = USART_WordLength_8b;
    usart.USART_StopBits = USART_StopBits_1;
    usart.USART_Parity = USART_Parity_No;
    usart.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
    usart.USART_Mode = USART_Mode_Rx | USART_Mode_Tx;
    USART_Init(CDC_USART, &usart);
    USART_ITConfig(CDC_USART, USART_IT_RXNE, ENABLE);
    NVIC_EnableIRQ(USART1_IRQn);
    USART_Cmd(CDC_USART, ENABLE);
}

void Protocol_SendStatus(StatusData *status) {
    uint8_t buf[sizeof(StatusData) + 5];
    buf[0] = FRAME_HEADER_MCU2PC;
    buf[1] = CMD_READ_STATUS;
    buf[2] = sizeof(StatusData);
    memcpy(buf + 3, status, sizeof(StatusData));
    buf[3 + sizeof(StatusData)] = crc8(buf, 3 + sizeof(StatusData));
    for(int i = 0; i < sizeof(StatusData) + 4; i++)
        USART_SendData(CDC_USART, buf[i]);
}
```

- [ ] **Step 3: 编写 UI 渲染**

```c
// firmware/Core/ui_render.c
#include "ui_render.h"
#include "oled_ssd1306.h"
#include "menu.h"
#include "fg_capture.h"
#include <stdio.h>

static char line_buf[22];

void UI_RenderMain(SystemParams *p, uint16_t rpm) {
    OLED_Clear();
    
    // 第1行: CH1 状态
    sprintf(line_buf, "CH1:%-6luHz %3u%% %s", 
        p->ch1_freq_hz, p->ch1_duty_pct,
        p->ch1_enabled ? "ON " : "OFF");
    OLED_DrawString(0, 0, line_buf, 1);
    
    // 第2行: CH2 状态
    sprintf(line_buf, "CH2:%-6luHz %3u%% %s",
        p->ch2_freq_hz, p->ch2_duty_pct,
        p->ch2_enabled ? "ON " : "OFF");
    OLED_DrawString(0, 16, line_buf, 1);
    
    // 第3行: FG/RPM
    sprintf(line_buf, "FG: %5u RPM /%u", rpm, p->fg_div);
    OLED_DrawString(0, 32, line_buf, 1);
    
    // 第4行: 占空比可视化条
    uint8_t bar_w = p->ch1_duty_pct * 128 / 100;
    OLED_DrawRect(0, 50, bar_w, 6, 1);
    OLED_DrawRect(bar_w, 50, 128 - bar_w, 6, 0);
    
    OLED_Refresh();
}

void UI_RenderMenu(MenuState *menu) {
    OLED_Clear();
    const char *titles[] = {"CH1设置", "CH2设置", "FG设置", "系统设置"};
    
    OLED_DrawString(0, 0, "=== 主菜单 ===", 1);
    for(int i = 0; i < 4; i++) {
        OLED_DrawString(10, 16 + i * 12,
            (menu->cursor == i) ? ">" : " ", 1);
        OLED_DrawString(20, 16 + i * 12, titles[i], 1);
    }
    OLED_Refresh();
}
```

---

### Task 9: 固件 — main.c 主程序

**Files:**
- Create: `firmware/Core/main.c`

- [ ] **Step 1: 编写主程序**

```c
// firmware/Core/main.c
#include "system_config.h"
#include "oled_ssd1306.h"
#include "encoder.h"
#include "menu.h"
#include "pwm_engine.h"
#include "fg_capture.h"
#include "ui_render.h"
#include "protocol.h"

static MenuState menu;
static uint16_t rpm = 0;
static uint32_t last_render = 0;
static uint32_t last_status = 0;

void SystemClock_Config(void) {
    RCC_DeInit();
    RCC_HSEConfig(RCC_HSE_ON);
    while(RCC_WaitForHSEStartUp() != SUCCESS);
    
    RCC_PLLConfig(RCC_PLLSource_HSE_Div1, RCC_PLLMul_9);  // 8M * 9 = 72M
    RCC_PLLCmd(ENABLE);
    while(RCC_GetFlagStatus(RCC_FLAG_PLLRDY) == RESET);
    
    RCC_SYSCLKConfig(RCC_SYSCLKSource_PLLCLK);
    while(RCC_GetSYSCLKSource() != 0x08);
    
    SystemCoreClockUpdate();
    SysTick_Config(SystemCoreClock / 1000);  // 1ms tick
}

int main(void) {
    SystemClock_Config();
    
    OLED_Init();
    Encoder_Init();
    PWM_Init();
    FG_Init();
    Protocol_Init();
    Menu_Init(&menu);
    
    uint32_t tick = 0;
    
    while(1) {
        InputEvent ev = Encoder_Poll();
        if(ev != EVENT_NONE) {
            Menu_Process(&menu, ev);
            if(menu.dirty) {
                PWM_UpdateFromParams(&menu.params);
                menu.dirty = 0;
            }
        }
        
        // 50ms 刷新 UI
        if(tick - last_render >= 50) {
            last_render = tick;
            if(menu.screen == SCREEN_MAIN) {
                rpm = FG_CalculateRPM(menu.params.fg_div, 
                    menu.params.fg_pulses_per_rev);
                UI_RenderMain(&menu.params, rpm);
            } else {
                UI_RenderMenu(&menu);
            }
        }
        
        // 500ms 发送状态到 PC
        if(tick - last_status >= 500) {
            last_status = tick;
            StatusData sd;
            sd.ch1_freq_hz = menu.params.ch1_freq_hz;
            sd.ch1_duty_pct = menu.params.ch1_duty_pct;
            sd.ch1_enabled = menu.params.ch1_enabled;
            sd.ch2_freq_hz = menu.params.ch2_freq_hz;
            sd.ch2_duty_pct = menu.params.ch2_duty_pct;
            sd.ch2_enabled = menu.params.ch2_enabled;
            sd.fg_freq_mhz = FG_GetFrequency_mHz();
            sd.fg_div = menu.params.fg_div;
            sd.rpm = rpm;
            Protocol_SendStatus(&sd);
        }
        
        tick++;
    }
}

void SysTick_Handler(void) {
    // 系统滴答计数器由 main 中的 tick 变量管理
}
```

---

### Task 10: 模拟器 — Qt 项目 & 主窗口

**Files:**
- Create: `simulator/CMakeLists.txt`
- Create: `simulator/main.cpp`
- Create: `simulator/mainwindow.h`
- Create: `simulator/mainwindow.cpp`

- [ ] **Step 1: 编写 CMakeLists.txt**

```cmake
cmake_minimum_required(VERSION 3.16)
project(PwmSimulator VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets SerialPort)

add_executable(PwmSimulator
    main.cpp
    mainwindow.cpp
    oledwidget.cpp
    menuengine.cpp
    serialcomm.cpp
    protocol.cpp
    keymap.cpp
)

target_include_directories(PwmSimulator PRIVATE
    ${CMAKE_SOURCE_DIR}/../common
    ${CMAKE_SOURCE_DIR}
)

target_link_libraries(PwmSimulator PRIVATE
    Qt6::Widgets
    Qt6::SerialPort
)
```

- [ ] **Step 2: 编写 main.cpp**

```cpp
#include <QApplication>
#include "mainwindow.h"

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);
    app.setApplicationName("PWM 信号发生器");
    
    MainWindow w;
    w.show();
    
    return app.exec();
}
```

- [ ] **Step 3: 编写主窗口头文件**

```cpp
// simulator/mainwindow.h
#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QComboBox>
#include <QLabel>
#include <QPushButton>
#include "oledwidget.h"
#include "menuengine.h"
#include "serialcomm.h"

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent = nullptr);
    
protected:
    void keyPressEvent(QKeyEvent *event) override;
    
private slots:
    void onModeChanged(int index);
    void onSerialConnected();
    void onSerialDisconnected();
    void onSerialData(const QByteArray &data);
    
private:
    void setupUI();
    void processKeyInput(InputEvent ev);
    
    OLEDWidget *oledWidget;
    MenuEngine *menuEngine;
    SerialComm *serial;
    
    QComboBox *modeCombo;
    QComboBox *portCombo;
    QPushButton *connectBtn;
    QLabel *statusLabel;
    
    bool hardwareMode = false;
};

#endif
```

- [ ] **Step 4: 编写主窗口实现**

```cpp
// simulator/mainwindow.cpp
#include "mainwindow.h"
#include "keymap.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QKeyEvent>
#include <QSerialPortInfo>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent), hardwareMode(false)
{
    setWindowTitle("PWM 信号发生器模拟器");
    setMinimumSize(500, 400);
    
    oledWidget = new OLEDWidget(this);
    menuEngine = new MenuEngine(this);
    serial = new SerialComm(this);
    
    connect(serial, &SerialComm::connected, this, &MainWindow::onSerialConnected);
    connect(serial, &SerialComm::disconnected, this, &MainWindow::onSerialDisconnected);
    connect(serial, &SerialComm::dataReceived, this, &MainWindow::onSerialData);
    
    setupUI();
}

void MainWindow::setupUI() {
    QWidget *central = new QWidget(this);
    QHBoxLayout *mainLayout = new QHBoxLayout(central);
    
    // 左侧: OLED 渲染区
    QGroupBox *oledGroup = new QGroupBox("OLED 显示");
    QVBoxLayout *oledLayout = new QVBoxLayout(oledGroup);
    oledWidget->setFixedSize(OLED_WIDTH * 2, OLED_HEIGHT * 2);
    oledLayout->addWidget(oledWidget);
    mainLayout->addWidget(oledGroup);
    
    // 右侧: 控制面板
    QGroupBox *ctrlGroup = new QGroupBox("控制面板");
    QVBoxLayout *ctrlLayout = new QVBoxLayout(ctrlGroup);
    
    // 模式切换
    ctrlLayout->addWidget(new QLabel("工作模式:"));
    modeCombo = new QComboBox();
    modeCombo->addItems({"模拟器模式", "硬件模式"});
    connect(modeCombo, &QComboBox::currentIndexChanged, 
            this, &MainWindow::onModeChanged);
    ctrlLayout->addWidget(modeCombo);
    
    // 串口设置
    ctrlLayout->addWidget(new QLabel("串口:"));
    portCombo = new QComboBox();
    for(const auto &port : QSerialPortInfo::availablePorts())
        portCombo->addItem(port.portName());
    ctrlLayout->addWidget(portCombo);
    
    connectBtn = new QPushButton("连接");
    connect(connectBtn, &QPushButton::clicked, [this]() {
        if(serial->isConnected())
            serial->disconnect();
        else
            serial->connectToPort(portCombo->currentText(), 115200);
    });
    ctrlLayout->addWidget(connectBtn);
    
    // 状态
    statusLabel = new QLabel("状态: 未连接");
    ctrlLayout->addWidget(statusLabel);
    
    // 按键提示
    QLabel *keyHint = new QLabel(
        "按键映射:\n"
        "  ↑↓ = 编码器旋转\n"
        "  Enter = 确认\n"
        "  Backspace = 长按返回\n"
        "  Esc = BACK\n"
        "  Space = 切换使能"
    );
    keyHint->setStyleSheet("color: gray; font-size: 11px;");
    ctrlLayout->addWidget(keyHint);
    
    ctrlLayout->addStretch();
    mainLayout->addWidget(ctrlGroup);
    
    setCentralWidget(central);
}

void MainWindow::keyPressEvent(QKeyEvent *event) {
    InputEvent ev = KeyMap::toInputEvent(event);
    if(ev != EVENT_NONE) {
        processKeyInput(ev);
        return;
    }
    QMainWindow::keyPressEvent(event);
}

void MainWindow::processKeyInput(InputEvent ev) {
    if(hardwareMode && serial->isConnected()) {
        serial->sendKeyEvent(ev);
    } else {
        menuEngine->processEvent(ev);
        oledWidget->renderMenu(menuEngine->state());
    }
}

void MainWindow::onModeChanged(int index) {
    hardwareMode = (index == 1);
    portCombo->setEnabled(hardwareMode);
    connectBtn->setEnabled(hardwareMode);
}

void MainWindow::onSerialConnected() {
    statusLabel->setText("状态: 已连接");
    connectBtn->setText("断开");
}

void MainWindow::onSerialDisconnected() {
    statusLabel->setText("状态: 未连接");
    connectBtn->setText("连接");
}

void MainWindow::onSerialData(const QByteArray &data) {
    // 解析来自 STM32 的帧并更新 OLED 显示
    oledWidget->updateFromProtocolData(data);
}
```

---

### Task 11: 模拟器 — OLED Widget & 菜单引擎

**Files:**
- Create: `simulator/oledwidget.h`
- Create: `simulator/oledwidget.cpp`
- Create: `simulator/menuengine.h`
- Create: `simulator/menuengine.cpp`
- Create: `simulator/keymap.h`
- Create: `simulator/keymap.cpp`

- [ ] **Step 1: OLED Widget 头文件**

```cpp
// simulator/oledwidget.h
#ifndef OLEDWIDGET_H
#define OLEDWIDGET_H

#include <QWidget>
#include <QImage>
#include "common/menu_fsm.h"
#include "common/protocol_defs.h"

class OLEDWidget : public QWidget {
    Q_OBJECT
public:
    explicit OLEDWidget(QWidget *parent = nullptr);
    
    void renderMain(const StatusData &status);
    void renderMenu(const MenuState &state);
    void updateFromProtocolData(const QByteArray &data);
    
protected:
    void paintEvent(QPaintEvent *event) override;
    
private:
    QImage buffer;
    void setPixel(int x, int y, bool on);
    void drawChar(int x, int y, char c);
    void drawString(int x, int y, const QString &s);
    void drawBar(int x, int y, int w, int h, float pct);
};

#endif
```

- [ ] **Step 2: OLED Widget 实现**

```cpp
// simulator/oledwidget.cpp
#include "oledwidget.h"
#include <QPainter>
#include <QFontMetrics>

OLEDWidget::OLEDWidget(QWidget *parent)
    : QWidget(parent),
      buffer(OLED_WIDTH, OLED_HEIGHT, QImage::Format_Mono) 
{
    buffer.fill(0);
}

void OLEDWidget::paintEvent(QPaintEvent *) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, false);
    p.fillRect(rect(), Qt::black);
    
    // 2x 像素缩放
    QImage scaled = buffer.scaled(OLED_WIDTH * 2, OLED_HEIGHT * 2,
        Qt::IgnoreAspectRatio, Qt::FastTransformation);
    p.drawImage(0, 0, scaled);
}

void OLEDWidget::setPixel(int x, int y, bool on) {
    if(x >= 0 && x < OLED_WIDTH && y >= 0 && y < OLED_HEIGHT)
        buffer.setPixel(x, y, on ? 1 : 0);
}

void OLEDWidget::renderMain(const StatusData &sd) {
    buffer.fill(0);
    char line[32];
    
    snprintf(line, sizeof(line), "CH1:%-6luHz %3u%% %s",
        sd.ch1_freq_hz, sd.ch1_duty_pct,
        sd.ch1_enabled ? "ON " : "OFF");
    drawString(0, 0, line);
    
    snprintf(line, sizeof(line), "CH2:%-6luHz %3u%% %s",
        sd.ch2_freq_hz, sd.ch2_duty_pct,
        sd.ch2_enabled ? "ON " : "OFF");
    drawString(0, 16, line);
    
    snprintf(line, sizeof(line), "FG: %5u RPM /%u", sd.rpm, sd.fg_div);
    drawString(0, 32, line);
    
    drawBar(0, 50, 128, 6, sd.ch1_duty_pct / 100.0f);
    update();
}

void OLEDWidget::renderMenu(const MenuState &state) {
    buffer.fill(0);
    const char *titles[] = {"CH1设置", "CH2设置", "FG设置", "系统设置"};
    
    drawString(0, 0, "=== 主菜单 ===");
    for(int i = 0; i < 4; i++) {
        drawString(10, 16 + i * 12, (state.cursor == i) ? ">" : " ");
        drawString(20, 16 + i * 12, titles[i]);
    }
    update();
}
```

- [ ] **Step 3: 菜单引擎**

```cpp
// simulator/menuengine.h
#ifndef MENUENGINE_H
#define MENUENGINE_H

#include <QObject>
#include "common/menu_fsm.h"
#include "common/protocol_defs.h"

class MenuEngine : public QObject {
    Q_OBJECT
public:
    explicit MenuEngine(QObject *parent = nullptr);
    
    void processEvent(InputEvent ev);
    StatusData status() const;
    SystemParams params() const;
    
private:
    SystemParams m_params;
    MenuState m_state;
};

#endif
```

```cpp
// simulator/menuengine.cpp
#include "menuengine.h"

MenuEngine::MenuEngine(QObject *parent) : QObject(parent) {
    m_params.ch1_freq_hz = 1000;
    m_params.ch1_duty_pct = 50;
    m_params.ch1_enabled = 0;
    m_params.ch2_freq_hz = 1000;
    m_params.ch2_duty_pct = 50;
    m_params.ch2_enabled = 0;
    m_params.fg_div = 2;
    m_state.screen = SCREEN_MAIN;
    m_state.cursor = 0;
    m_state.editing = 0;
}

void MenuEngine::processEvent(InputEvent ev) {
    // 与固件 menu.c 相同的逻辑 (复用 menu_fsm.h)
    // ... 此处保持与 Task 5 菜单逻辑一致
}
```

- [ ] **Step 4: 键盘映射**

```cpp
// simulator/keymap.h
#ifndef KEYMAP_H
#define KEYMAP_H

#include <QKeyEvent>
#include "common/menu_fsm.h"

class KeyMap {
public:
    static InputEvent toInputEvent(QKeyEvent *event);
};

#endif
```

```cpp
// simulator/keymap.cpp
#include "keymap.h"

InputEvent KeyMap::toInputEvent(QKeyEvent *event) {
    switch(event->key()) {
        case Qt::Key_Up:       return EVENT_CW;
        case Qt::Key_Down:     return EVENT_CCW;
        case Qt::Key_Return:
        case Qt::Key_Enter:    return EVENT_CLICK;
        case Qt::Key_Backspace:return EVENT_LONG_PRESS;
        case Qt::Key_Escape:   return EVENT_BACK;
        default:               return EVENT_NONE;
    }
}
```

---

### Task 12: 模拟器 — 串口通信 & 协议

**Files:**
- Create: `simulator/serialcomm.h`
- Create: `simulator/serialcomm.cpp`
- Create: `simulator/protocol.h`
- Create: `simulator/protocol.cpp`

- [ ] **Step 1: 串口通信**

```cpp
// simulator/serialcomm.h
#ifndef SERIALCOMM_H
#define SERIALCOMM_H

#include <QObject>
#include <QSerialPort>
#include "common/protocol_defs.h"

class SerialComm : public QObject {
    Q_OBJECT
public:
    explicit SerialComm(QObject *parent = nullptr);
    ~SerialComm();
    
    void connectToPort(const QString &name, int baudRate);
    void disconnect();
    bool isConnected() const;
    
    void sendPwmWrite(const PwmWriteReq &req);
    void sendFgDiv(const FgDivReq &req);
    void sendKeyEvent(InputEvent ev);
    
signals:
    void connected();
    void disconnected();
    void dataReceived(const QByteArray &data);
    
private slots:
    void onReadyRead();
    
private:
    QSerialPort *m_port;
    void sendFrame(uint8_t cmd, const uint8_t *data, uint8_t len);
};

#endif
```

```cpp
// simulator/serialcomm.cpp
#include "serialcomm.h"
#include <QDebug>

SerialComm::SerialComm(QObject *parent) 
    : QObject(parent), m_port(new QSerialPort(this))
{
    connect(m_port, &QSerialPort::readyRead, 
            this, &SerialComm::onReadyRead);
}

SerialComm::~SerialComm() { disconnect(); }

void SerialComm::connectToPort(const QString &name, int baudRate) {
    m_port->setPortName(name);
    m_port->setBaudRate(baudRate);
    m_port->setDataBits(QSerialPort::Data8);
    m_port->setParity(QSerialPort::NoParity);
    m_port->setStopBits(QSerialPort::OneStop);
    if(m_port->open(QIODevice::ReadWrite))
        emit connected();
}

void SerialComm::disconnect() {
    if(m_port->isOpen()) {
        m_port->close();
        emit disconnected();
    }
}

bool SerialComm::isConnected() const {
    return m_port->isOpen();
}

void SerialComm::sendFrame(uint8_t cmd, const uint8_t *data, uint8_t len) {
    if(!m_port->isOpen()) return;
    uint8_t buf[256];
    buf[0] = FRAME_HEADER_PC2MCU;
    buf[1] = cmd;
    buf[2] = len;
    memcpy(buf + 3, data, len);
    buf[3 + len] = crc8(buf, 3 + len);
    m_port->write(reinterpret_cast<const char*>(buf), 4 + len);
}

void SerialComm::sendKeyEvent(InputEvent ev) {
    KeyEventReq req;
    req.event = (uint8_t)ev;
    sendFrame(CMD_KEY_EVENT, reinterpret_cast<const uint8_t*>(&req), sizeof(req));
}

void SerialComm::onReadyRead() {
    QByteArray data = m_port->readAll();
    emit dataReceived(data);
}
```

- [ ] **Step 2: 协议编解码**

```cpp
// simulator/protocol.h
#ifndef SIM_PROTOCOL_H
#define SIM_PROTOCOL_H

#include "common/protocol_defs.h"
#include <QByteArray>

class Protocol {
public:
    static StatusData parseStatusResponse(const QByteArray &data);
    static QByteArray encodeWritePwm(const PwmWriteReq &req);
    static QByteArray encodeWriteFgDiv(const FgDivReq &req);
    static QByteArray encodeKeyEvent(uint8_t event);
    static bool validateFrame(const uint8_t *data, int len);
};

#endif
```

---

### Task 13: 硬件设计文档

**Files:**
- Create: `hardware/schematic.md`
- Create: `hardware/BOM.md`

- [ ] **Step 1: 电路原理图描述**

```markdown
# PWM 信号发生器 — 电路原理图

## MCU: STM32F103C8T6 (Blue Pill 或最小系统板)

## 引脚分配

| 功能 | 引脚 | 备注 |
|------|------|------|
| OLED SCL | PB8 | I2C1 SCL |
| OLED SDA | PB9 | I2C1 SDA |
| PWM OUT1 | PA8 | TIM1 CH1 → 74HCT04 → 5V |
| PWM OUT2 | PB6 | TIM4 CH1 → 74HCT04 → 5V |
| FG IN | PA0 | 5V→3.3V 分压 (10k+4.7k) |
| ENC A | PB10 | 旋转编码器 A |
| ENC B | PB11 | 旋转编码器 B |
| BTN OK | PB12 | 按键 (上拉, 按下=GND) |
| BTN BACK | PB13 | 按键 (上拉, 按下=GND) |
| USB D+/D- | PA12/PA11 | USB (或使用 CH340G 串口) |

## 电源
- 输入: USB 5V 或外部 5V
- MCU: 3.3V (板载 LDO)
- OLED VCC: 3.3V
- 74HCT04 VCC: 5V

## 电平转换
- 3.3V PWM → 74HCT04 (5V 供电) → 5V PWM OUT
- FG 5V → 电阻分压 10k/4.7k → ~3.2V → PA0
```

- [ ] **Step 2: BOM 物料清单**

```markdown
# PWM 信号发生器 — 物料清单 (BOM)

| 序号 | 物料 | 规格 | 数量 | 用途 |
|------|------|------|------|------|
| 1 | STM32F103C8T6 | Blue Pill 开发板 | 1 | 主控 |
| 2 | 0.96" OLED | SSD1306, I2C, 128x64 | 1 | 显示 |
| 3 | 旋转编码器 | EC11, 20脉冲/转, 带按键 | 1 | 操作输入 |
| 4 | 轻触按键 | 6x6mm, 4脚 | 1 | BACK 键 |
| 5 | 74HCT04 | DIP-14 / SOP-14 | 1 | 电平转换 |
| 6 | 电阻 10kΩ | 0805/插件 | 2 | FG 分压, 上拉 |
| 7 | 电阻 4.7kΩ | 0805/插件 | 1 | FG 分压 |
| 8 | 电阻 100Ω | 0805/插件 | 2 | PWM 输出限流 |
| 9 | 电容 100nF | 0805/插件 | 3 | 去耦 |
| 10 | 电容 10μF | 电解/钽 | 1 | 电源滤波 |
| 11 | USB-TTL 模块 | CH340G | 1 | 串口通信/烧录 |
| 12 | BNC 端子 / 排针 | 2P | 3 | 输出端子 |
| 13 | 杜邦线 | 公母各若干 | - | 连接 |
| 14 | 面包板 / PCB | 5x7cm | 1 | 电路搭建 |

## 成本估算: ~50-80 RMB
```

---

### Task 14: CRC8 补充实现

**Files:**
- Create: `common/protocol_crc.c` (供固件使用)
- Modify: `simulator/protocol.cpp` 中添加 CRC8

- [ ] **Step 1: C 语言 CRC8 实现**

```c
// common/protocol_crc.c
#include "protocol_defs.h"

uint8_t crc8(const uint8_t *data, uint8_t len) {
    uint8_t crc = 0x00;
    while(len--) {
        crc ^= *data++;
        for(uint8_t i = 0; i < 8; i++) {
            if(crc & 0x80)
                crc = (crc << 1) ^ 0x07;
            else
                crc <<= 1;
        }
    }
    return crc;
}
```

- [ ] **Step 2: C++ 版本**

```cpp
// 在 simulator/protocol.cpp 中
uint8_t crc8(const uint8_t *data, uint8_t len) {
    uint8_t crc = 0x00;
    while(len--) {
        crc ^= *data++;
        for(int i = 0; i < 8; i++) {
            if(crc & 0x80)
                crc = (crc << 1) ^ 0x07;
            else
                crc <<= 1;
        }
    }
    return crc;
}
```

---

## 执行顺序

**阶段 A (并行):** Task 1 (共享头文件) + Task 13 (硬件文档) — 无依赖

**阶段 B (并行):** Task 2→3→4 (固件基础) + Task 10 (模拟器骨架) — 依赖 Task 1

**阶段 C (并行):** Task 5→6→7 (固件业务逻辑) + Task 11 (模拟器 UI) — 依赖阶段 B

**阶段 D (并行):** Task 8 (协议) + Task 12 (串口) — 依赖 Task 1, 阶段 C

**阶段 E (集成):** Task 9 (main.c) — 依赖所有固件任务

**阶段 F (收尾):** Task 14 (CRC8) — 补充, 无硬依赖
