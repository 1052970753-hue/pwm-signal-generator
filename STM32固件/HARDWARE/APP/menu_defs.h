/*
 * menu_defs.h — 应用层类型定义 (全局共享)
 * ══════════════════════════════════════════════
 *  本文件是整个应用层的类型中枢，定义了所有模块共用的枚举、结构体和常量。
 *  依赖本文件的模块: menu.c, ui_render.c, protocol.c, pwm_engine.c, main.c
 *
 *  设计原则:
 *    1. 使用 ALIENTEK 类型系统 (u8/u16/u32 from sys.h)，不依赖 C99 stdint.h
 *    2. 所有结构体使用紧凑排列 (无填充字节)，确保串口协议二进制兼容
 *    3. 枚举值从 0 开始连续编号，便于数组索引和模运算
 */
#ifndef MENU_DEFS_H
#define MENU_DEFS_H

#include "sys.h"

/* ══════════════════════════════════════════════
 *  应用模式 (AppMode)
 * ══════════════════════════════════════════════
 *  用户通过双击旋转编码器在 6 种模式间循环切换:
 *    PWM_FG → FG → CH1 → CH2 → VSP → TEST → PWM_FG ...
 *
 *  每种模式对应一种 OLED 界面布局和一组交互逻辑。
 *  模式编号同时用于 OLED 渲染分发和串口状态上报。
 */
typedef enum {
    MODE_PWM_FG = 0,    // 模式0: 双通道PWM + 频率计 RPM 显示 (默认启动模式)
    MODE_FG,            // 模式1: 纯频率计模式，大号 RPM 数字显示
    MODE_CH1,           // 模式2: CH1 单通道 PWM 参数调节
    MODE_CH2,           // 模式3: CH2 单通道 PWM 参数调节
    MODE_VSP,           // 模式4: VSP 模拟电压输出 (0~5V DAC)
    MODE_TEST,          // 模式5: 自动测试模式 (ON/OFF 循环 + 数据记录)
    NUM_MODES           // 模式总数 (用于模运算切换)
} AppMode;

/* ══════════════════════════════════════════════
 *  PWM-FG 模式光标项 (CursorItem)
 * ══════════════════════════════════════════════
 *  PWM_FG 模式下有 5 个可调节参数项:
 *    CH1 频率 → CH1 占空比 → CH2 频率 → CH2 占空比 → FG 分频
 *
 *  长按 OK 进入选择模式后，旋转编码器在这些项之间移动光标。
 *  短按 OK 根据光标所在区域切换对应通道的使能开关。
 */
typedef enum {
    ITEM_CH1_FREQ = 0,  // CH1 输出频率 (1~100000 Hz)
    ITEM_CH1_DUTY,      // CH1 占空比 (0~100 %)
    ITEM_CH2_FREQ,      // CH2 输出频率 (1~100000 Hz)
    ITEM_CH2_DUTY,      // CH2 占空比 (0~100 %)
    ITEM_FG_DIV,        // FG 输入信号分频系数 (1~99)
    NUM_ITEMS           // 参数项总数
} CursorItem;

/* ══════════════════════════════════════════════
 *  VSP 模式光标项 (VspCursorItem)
 * ══════════════════════════════════════════════
 *  VSP 模式下有 2 个可调节参数项:
 *    输出电压 (0.0~5.0V) → 使能开关
 */
typedef enum {
    VSP_ITEM_VOLTAGE = 0,   // VSP 输出电压 (0.0~5.0V, 0.01V步进, 存储值×100)
    VSP_ITEM_ENABLE,        // VSP 使能开关 (0=关, 1=开)
    NUM_VSP_ITEMS           // VSP 参数项总数
} VspCursorItem;

/* ══════════════════════════════════════════════
 *  测试模式光标项 (TestCursorItem)
 * ══════════════════════════════════════════════
 *  测试模式下有 7 个可配置项，两列布局:
 *    左列: 通道 / 占空比 / ON时间
 *    右列: 频率 / 循环次数 / OFF时间
 *    底部: START 启动按钮
 */
typedef enum {
    TEST_ITEM_CHANNEL = 0,  // 测试通道选择 (1=CH1, 2=CH2)
    TEST_ITEM_FREQ,         // 测试输出频率 (Hz)
    TEST_ITEM_DUTY,         // 测试输出占空比 (%)
    TEST_ITEM_CYCLES,       // 总循环次数 (1~999)
    TEST_ITEM_ON_TIME,      // 每轮 ON 持续时间 (秒, 1~60)
    TEST_ITEM_OFF_TIME,     // 每轮 OFF 持续时间 (秒, 1~60)
    TEST_ITEM_ON_METHOD,    // ON 方式 (0=PWM开关, 1=继电器, 2=两者)
    TEST_ITEM_START,        // 启动测试按钮 (短按触发)
    NUM_TEST_ITEMS          // 测试参数项总数 (9)
} TestCursorItem;

/* ══════════════════════════════════════════════
 *  输入事件 (InputEvent)
 * ══════════════════════════════════════════════
 *  旋转编码器和按键产生的用户输入事件。
 *  由 Encoder_Poll() 检测并返回，由 Menu_Process() 处理。
 *
 *  事件产生机制:
 *    CW/CCW:  TIM3 编码器计数差值 ≥4 时判定
 *    CLICK:   按键按下 <1000ms 释放，且 400ms 内无第二次点击
 *    LONG:    按键持续按住 >1000ms
 *    DBLCLK:  两次短按间隔 <400ms (延迟判定机制)
 */
typedef enum {
    EVENT_NONE = 0,         // 无事件 (轮询空闲)
    EVENT_CW,               // 顺时针旋转 (增加值/下移光标)
    EVENT_CCW,              // 逆时针旋转 (减少值/上移光标)
    EVENT_CLICK,            // 短按 OK (<1000ms, 延迟 400ms 判定)
    EVENT_LONG_PRESS,       // 长按 OK (>1000ms, 进入选择模式)
    EVENT_DOUBLE_CLICK      // 双击 OK (两次短按间隔 <400ms, 切换模式)
} InputEvent;

/* ══════════════════════════════════════════════
 *  测试状态 (TestState)
 * ══════════════════════════════════════════════
 *  自动测试的运行状态，通过串口状态上报通知 PC 端。
 */
typedef enum {
    TEST_IDLE = 0,      // 空闲 (未启动或已完成)
    TEST_RUNNING,       // 运行中 (ON/OFF 循环进行中)
    TEST_DONE           // 完成 (所有循环结束)
} TestState;

/* ══════════════════════════════════════════════
 *  菜单状态 (MenuState)
 * ══════════════════════════════════════════════
 *  菜单状态机的运行时状态，全局唯一实例 g_menu。
 *
 *  cursor:  当前光标位置，取值范围取决于当前模式
 *             PWM_FG: 0~4 (5个参数项)
 *             FG: 无光标
 *             CH1/CH2: 0~2 (频率/占空比/使能)
 *             TEST: 0~6 (7个测试配置项)
 *
 *  selected: 操作模式切换
 *             0 = 调节模式: 旋转编码器修改参数值
 *             1 = 选择模式: 旋转编码器移动光标位置
 *
 *  dirty:    参数修改标志，main.c 检测到后调用 PWM_UpdateFromParams()
 *
 *  mode:     当前应用模式 (AppMode 枚举值)
 *
 *  blink:    光标闪烁状态，400ms 翻转一次
 *            选择模式下光标位置的 ">" 标记会闪烁
 */
typedef struct {
    u8 cursor;      // 当前光标位置 (0-based)
    u8 selected;    // 0=调节模式, 1=选择模式
    u8 dirty;       // 参数已修改标志 (需更新 PWM 输出)
    u8 mode;        // 当前应用模式 (AppMode 枚举值)
    u8 blink;       // 光标闪烁标志 (400ms 翻转)
} MenuState;

/* ══════════════════════════════════════════════
 *  系统参数 (SystemParams)
 * ══════════════════════════════════════════════
 *  所有通道参数和 FG 配置的集中存储结构。
 *  全局唯一实例 g_params，被 PWM引擎 / 菜单 / 协议 共享读写。
 *
 *  数据流:
 *    用户旋转编码器 → Menu_Process() 修改 g_params
 *    PC 发送命令 → protocol.c 修改 g_params
 *    main.c 检测 dirty → PWM_UpdateFromParams() 同步到硬件
 */
typedef struct {
    u32 ch1_freq_hz;        // CH1 输出频率 (1~100000 Hz)
    u8  ch1_duty_pct;       // CH1 占空比 (0~100 %)
    u8  ch1_enabled;        // CH1 输出使能 (0=关, 1=开)
    u32 ch2_freq_hz;        // CH2 输出频率 (1~100000 Hz)
    u8  ch2_duty_pct;       // CH2 占空比 (0~100 %)
    u8  ch2_enabled;        // CH2 输出使能 (0=关, 1=开)
    u8  fg_div;             // FG 输入分频系数 (1~99)
    u16 fg_pulses_per_rev;  // FG 每转脉冲数 (默认2, 用于 RPM 计算)
    u16 vsp_voltage_x100;   // VSP 输出电压 ×100 (0~500, 即 0.00~5.00V)
    u8  vsp_enabled;        // VSP 使能 (0=关, 1=开)
    u8  test_on_method;     // 测试 ON 方式 (0=PWM, 1=继电器, 2=两者)
} SystemParams;

/* ══════════════════════════════════════════════
 *  状态上报数据 (StatusData, 29字节)
 * ══════════════════════════════════════════════
 *  MCU → PC 周期性上报 (500ms) 或响应 CMD_READ_STATUS 查询。
 *  字段偏移量必须与模拟器 Python 端 read_status_data() 严格对齐。
 *
 *  内存布局 (紧凑排列, 无填充):
 *    偏移  0: ch1_freq_hz    (4B)
 *    偏移  4: ch1_duty_pct   (1B)
 *    偏移  5: ch1_enabled    (1B)
 *    偏移  6: ch2_freq_hz    (4B)
 *    偏移 10: ch2_duty_pct   (1B)
 *    偏移 11: ch2_enabled    (1B)
 *    偏移 12: fg_freq_mhz    (4B)  FG实测频率(毫赫兹)
 *    偏移 16: fg_div         (1B)
 *    偏移 17: rpm            (2B)  实时转速
 *    偏移 19: mode           (1B)  当前AppMode
 *    偏移 20: test_state     (1B)  TestState
 *    偏移 21: test_cycle     (2B)  当前测试循环编号
 *    偏移 23: test_total     (2B)  总循环数
 */
typedef struct {
    u32 ch1_freq_hz;        // CH1 频率
    u8  ch1_duty_pct;       // CH1 占空比
    u8  ch1_enabled;        // CH1 使能
    u32 ch2_freq_hz;        // CH2 频率
    u8  ch2_duty_pct;       // CH2 占空比
    u8  ch2_enabled;        // CH2 使能
    u32 fg_freq_mhz;        // FG 频率 (毫赫兹, 精度 0.001Hz)
    u8  fg_div;             // FG 分频系数
    u16 rpm;                // 实时转速 (RPM)
    u8  mode;               // 当前应用模式
    u8  test_state;         // 测试状态 (0=空闲, 1=运行, 2=完成)
    u16 test_cycle;         // 当前测试循环
    u16 test_total;         // 总循环数
    u16 vsp_voltage_x100;   // VSP 电压 ×100 (0~500 = 0.00~5.00V)
    u8  vsp_enabled;        // VSP 使能
    u8  test_on_method;     // 测试 ON 方式 (0=PWM, 1=继电器, 2=两者)
} StatusData;  // 29 bytes

/* ══════════════════════════════════════════════
 *  串口请求数据结构 (PC → MCU 命令载荷)
 * ══════════════════════════════════════════════
 */

// CMD_WRITE_PWM (0x20): PC 写入单通道 PWM 参数
typedef struct {
    u8  channel;        // 通道号 (1=CH1, 2=CH2)
    u32 freq_hz;        // 目标频率 (Hz)
    u8  duty_pct;       // 目标占空比 (%)
    u8  enable;         // 使能 (0=关, 1=开)
} PwmWriteReq;

// CMD_WRITE_FG_DIV (0x30): PC 设置 FG 分频系数
typedef struct {
    u8 div;             // 分频系数 (1~99)
} FgDivReq;

// CMD_KEY_EVENT (0x41): PC 模拟按键事件 (远程控制)
typedef struct {
    u8 event;           // InputEvent 枚举值 (0~4)
} KeyEventReq;

// CMD_SET_TEST (0x42): PC 设置自动测试参数
typedef struct {
    u8  channel;            // 测试通道 (1=CH1, 2=CH2)
    u32 freq_hz;            // 测试频率 (Hz)
    u8  duty_pct;           // 测试占空比 (%)
    u16 cycles;             // 循环次数 (1~999)
    u16 on_time_sec;        // ON 持续时间 (秒, 1~60)
    u16 off_time_sec;       // OFF 持续时间 (秒, 1~60)
    u8  on_method;          // ON 方式 (0=PWM, 1=继电器, 2=两者)
} TestConfig;

// CMD_WRITE_VSP (0x60): PC 写入 VSP 参数
typedef struct {
    u16 voltage_x100;       // VSP 电压 ×100 (0~500 = 0.00~5.00V)
    u8  enabled;            // VSP 使能 (0=关, 1=开)
} VspWriteReq;  // 3 bytes

/* ══════════════════════════════════════════════
 *  协议命令字 (ProtocolCmd)
 * ══════════════════════════════════════════════
 *  PC 与 MCU 之间的通信命令编号。
 *  帧格式: [HEADER 0xAA/0xBB][CMD 1B][LEN 1B][DATA NB][CRC8 1B]
 *
 *  命令分类:
 *    0x10:       状态查询/上报
 *    0x20~0x30:  参数写入 (PWM / FG)
 *    0x40~0x44:  控制命令 (按键/测试)
 *    0x50~0x52:  数据导出 (CSV)
 */
typedef enum {
    CMD_READ_STATUS   = 0x10,   // 读状态 (PC→MCU查询, MCU→PC响应, 25B)
    CMD_WRITE_PWM     = 0x20,   // 写PWM参数 (PC→MCU, PwmWriteReq 7B)
    CMD_WRITE_FG_DIV  = 0x30,   // 写FG分频 (PC→MCU, FgDivReq 1B)
    CMD_KEY_EVENT     = 0x41,   // 按键事件 (PC→MCU, KeyEventReq 1B)
    CMD_SET_TEST      = 0x42,   // 设置测试参数 (PC→MCU, TestConfig 13B)
    CMD_START_TEST    = 0x43,   // 启动测试 (PC→MCU, 无数据)
    CMD_STOP_TEST     = 0x44,   // 停止测试 (PC→MCU, 无数据)
    CMD_EXPORT_DATA   = 0x50,   // 请求导出CSV (PC→MCU, 无数据)
    CMD_EXPORT_CHUNK  = 0x51,   // CSV数据块 (MCU→PC, ≤64B)
    CMD_EXPORT_DONE   = 0x52,   // 导出完成 (MCU→PC, 无数据)
    CMD_WRITE_VSP     = 0x60    // 写VSP参数 (PC→MCU, VspWriteReq 3B)
} ProtocolCmd;

// ── 帧头标识 ──
#define FRAME_HEADER_PC2MCU  0xAA  // PC → MCU 帧头字节
#define FRAME_HEADER_MCU2PC  0xBB  // MCU → PC 帧头字节

// ── CRC8-CCITT 校验 ──
// 多项式 0x07, 初始值 0x00
// 校验范围: HEADER + CMD + LEN + DATA (不含 CRC 字节本身)
// 与模拟器 Python 端 crc8() 完全一致
u8 crc8(const u8 *data, u8 len);

#endif
