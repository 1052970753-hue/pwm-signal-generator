/*
 * ui_render.c — OLED 5 模式界面渲染
 * ══════════════════════════════════════════════════════════════
 *
 * 【硬件】
 *   128x64 像素 OLED 显示屏
 *   软件 I2C 接口 (PA1=SCL, PA2=SDA)
 *   驱动 API: ALIENTEK OLED 库 (oled.h)
 *
 * 【字体规格】
 *   8x6 字体 (size=8): 每字符 6 像素宽 × 8 像素高
 *     → 水平: 128/6 ≈ 21 字符/行
 *     → 垂直: 64/8 = 8 行
 *   16x8 字体 (size=16): 每字符 8 像素宽 × 16 像素高
 *     → 用于大号数字显示 (如 FG 模式的 RPM)
 *
 * 【渲染流程】
 *   OLED_Clear() → 绘制各元素 → OLED_Refresh()
 *   由 main.c 以 50ms (20fps) 周期调用 UI_Render()
 *
 * 【5 种渲染模式】
 *   render_pwm_fg():    MODE_PWM_FG — 双通道 + 频率计 (5 参数)
 *   render_fg_mode():   MODE_FG — 纯频率计 (大号 RPM)
 *   render_ch_mode():   MODE_CH1/CH2 — 单通道 PWM (3 参数)
 *   render_vsp_mode():  MODE_VSP — VSP 模拟电压 (2 参数)
 *   render_test_mode(): MODE_TEST — 自动测试 (配置/运行)
 */
#include "ui_render.h"
#include "../OLED_IIC/oled.h"
#include "menu.h"

/* ════════════════════════════════════════════════════════════
 *  数字转字符串 — 整数到十进制 ASCII
 * ════════════════════════════════════════════════════════════
 *  参数:
 *    val: 要转换的无符号整数 (u32)
 *    buf: 输出缓冲区 (至少 12 字节)
 *  特点: 无前导零 (0 显示为 "0", 123 显示为 "123")
 *
 *  算法:
 *    1. 特殊处理 val=0 → "0"
 *    2. 从低位到高位逐位取余数, 存入临时数组 tmp[]
 *    3. 将 tmp[] 反向拷贝到 buf[] (变为正序)
 *    4. 添加字符串结束符 '\0'
 */
static void num2str(u32 val, u8 *buf) {
    u8 i = 0, j;
    u8 tmp[12];                                          // 临时缓冲区 (u32 最多 10 位 + 安全余量)
    if (val == 0) { buf[0] = '0'; buf[1] = 0; return; }  // 特殊处理: 0 → "0"
    while (val > 0) { tmp[i++] = '0' + (val % 10); val /= 10; }  // 取低位数字, 存入 tmp (逆序)
    for (j = 0; j < i; j++) buf[j] = tmp[i - 1 - j];    // 反向拷贝: tmp[i-1]→buf[0], tmp[0]→buf[i-1]
    buf[i] = 0;                                           // 字符串结束符
}

/* ════════════════════════════════════════════════════════════
 *  光标标记字符 — 决定当前位置是否显示 ">" 标记
 * ════════════════════════════════════════════════════════════
 *  参数:
 *    cur:  当前光标位置 (g_menu.cursor)
 *    sel:  选择模式标志 (g_menu.selected, 1=选择模式)
 *    idx:  要检查的参数项索引
 *    blink: 闪烁状态 (1=亮, 0=灭)
 *
 *  返回值:
 *    cur == idx 且 sel=0 (调节模式): 始终返回 ">"
 *    cur == idx 且 sel=1 (选择模式): blink=1 返回 ">", blink=0 返回 " " (闪烁效果)
 *    cur != idx: 返回 " " (无标记)
 *
 *  闪烁效果: 选择模式下光标以 1Hz 频率闪烁 (由 main.c 的 blink 变量控制)
 *  用于区分调节模式 (常亮) 和选择模式 (闪烁)
 */
static const u8* marker_str(u8 cur, u8 sel, u8 idx, u8 blink) {
    if (cur == idx) {                                    // 当前项就是光标位置
        if (sel) return blink ? (const u8 *)">" : (const u8 *)" ";  // 选择模式: 闪烁
        return (const u8 *)">";                          // 调节模式: 常亮
    }
    return (const u8 *)" ";                              // 非光标位置: 空格
}

/* ════════════════════════════════════════════════════════════
 *  模式 0: PWM-FG 双通道 + 频率计
 * ════════════════════════════════════════════════════════════
 *
 *  OLED 布局 (128×64, 8x6 字体 = 21列×8行):
 *  ┌─────────────────────┐
 *  │     PWM_TOOL        │ 行0 (y=0):   标题居中
 *  │ O CH1 ON   CH2 OFF  │ 行1 (y=8):   通道状态 (O=开, 空格=关)
 *  │>Fr:1000Hz  Fr:1kHz  │ 行2 (y=16):  频率 (带光标)
 *  │ Duty:50%   Duty:50% │ 行3 (y=24):  占空比 (带光标)
 *  │---------------------│ 行4 (y=32):  (未使用)
 *  │                     │ 行5 (y=40):  分隔线
 *  │ FG 1200RPM    /2    │ 行6 (y=48):  FG RPM + 分频系数
 *  │                     │ 行7 (y=56):  (未使用)
 *  └─────────────────────┘
 *
 *  左半屏: CH1 参数    右半屏: CH2 参数
 *  光标项: ITEM_CH1_FREQ(0), ITEM_CH1_DUTY(1), ITEM_CH2_FREQ(2), ITEM_CH2_DUTY(3), ITEM_FG_DIV(4)
 */
static void render_pwm_fg(SystemParams *p, u16 rpm, u8 blink) {
    u8 buf[12];
    u8 cur = g_menu.cursor;                              // 当前光标位置
    u8 sel = g_menu.selected;                            // 选择模式标志

    OLED_Clear();

    // 行0 (y=0): 标题 "PWM_TOOL", 水平居中 (x=24)
    OLED_ShowString(24, 0, "PWM_TOOL", 8, 1);

    // 行1 (y=8): 通道使能状态 — 左 CH1, 右 CH2
    // "O" 表示开启, 空格表示关闭, 后跟通道名和 ON/OFF
    OLED_ShowString(0, 8, (const u8 *)(p->ch1_enabled ? "O CH1 ON " : "  CH1 OFF"), 8, 1);
    OLED_ShowString(66, 8, (const u8 *)(p->ch2_enabled ? "O CH2 ON " : "  CH2 OFF"), 8, 1);

    // 行2 (y=16): 频率显示 — 左半屏 CH1, 右半屏 CH2
    // 光标(x=0) → "Fr:"(x=6) → 数字(x=24) → "Hz"(x=54)
    OLED_ShowString(0, 16, marker_str(cur, sel, ITEM_CH1_FREQ, blink), 8, 1);   // 光标标记
    OLED_ShowString(6, 16, "Fr:", 8, 1);                                        // 标签
    num2str(p->ch1_freq_hz, buf);                                                // 数字转字符串
    OLED_ShowString(24, 16, buf, 8, 1);                                          // 频率值
    OLED_ShowString(54, 16, "Hz", 8, 1);                                         // 单位

    OLED_ShowString(66, 16, marker_str(cur, sel, ITEM_CH2_FREQ, blink), 8, 1);  // 光标标记
    OLED_ShowString(72, 16, "Fr:", 8, 1);                                        // 标签
    num2str(p->ch2_freq_hz, buf);
    OLED_ShowString(90, 16, buf, 8, 1);                                          // 频率值
    OLED_ShowString(120, 16, "Hz", 8, 1);                                        // 单位

    // 行3 (y=24): 占空比显示 — 左半屏 CH1, 右半屏 CH2
    // 光标(x=0) → "Duty:"(x=6) → 数字(x=36) → "%"(x=54)
    OLED_ShowString(0, 24, marker_str(cur, sel, ITEM_CH1_DUTY, blink), 8, 1);   // 光标标记
    OLED_ShowString(6, 24, "Duty:", 8, 1);                                       // 标签
    num2str(p->ch1_duty_pct, buf);
    OLED_ShowString(36, 24, buf, 8, 1);                                           // 占空比值
    OLED_ShowString(54, 24, "%", 8, 1);                                           // 单位

    OLED_ShowString(66, 24, marker_str(cur, sel, ITEM_CH2_DUTY, blink), 8, 1);  // 光标标记
    OLED_ShowString(72, 24, "Duty:", 8, 1);
    num2str(p->ch2_duty_pct, buf);
    OLED_ShowString(102, 24, buf, 8, 1);
    OLED_ShowString(120, 24, "%", 8, 1);

    // 行5 (y=40): 分隔线 — 21 个 "-"
    OLED_ShowString(0, 40, "---------------------", 8, 1);

    // 行6 (y=48): FG RPM + 分频系数
    // "FG" → RPM 数字(x=18) → "RPM"(x=60) → 光标(x=90) → "/"(x=96) → 分频值(x=102)
    OLED_ShowString(0, 48, "FG", 8, 1);                                           // 标签
    num2str(rpm, buf);
    OLED_ShowString(18, 48, buf, 8, 1);                                            // RPM 值
    OLED_ShowString(60, 48, "RPM", 8, 1);                                          // 单位

    OLED_ShowString(90, 48, marker_str(cur, sel, ITEM_FG_DIV, blink), 8, 1);     // 光标标记
    OLED_ShowString(96, 48, "/", 8, 1);                                            // 分隔符
    num2str(p->fg_div, buf);
    OLED_ShowString(102, 48, buf, 8, 1);                                            // 分频系数值

    OLED_Refresh();                                                                // 刷新到屏幕
}

/* ════════════════════════════════════════════════════════════
 *  模式 1: FG 纯频率计 (大号 RPM 显示)
 * ════════════════════════════════════════════════════════════
 *
 *  OLED 布局:
 *  ┌─────────────────────┐
 *  │     FG MODE         │ 行0 (y=0):   标题
 *  │                     │ 行1 (y=8):   (未使用)
 *  │ RPM:                │ 行2 (y=16):  标签
 *  │    12345            │ 行3-4 (y=24-40): 大号 RPM (16x8 字体居中)
 *  │                     │
 *  │---------------------│ 行6 (y=48):  分隔线
 *  │ Div:2               │ 行7 (y=56):  分频系数
 *  └─────────────────────┘
 *
 *  RPM 使用 16x8 字体水平居中显示, 字符数 × 8 像素 = 总宽度
 *  居中 x = (128 - len*8) / 2
 */
static void render_fg_mode(SystemParams *p, u16 rpm, u8 blink) {
    u8 buf[12];

    OLED_Clear();
    // 行0 (y=0): 标题 "FG MODE", 水平居中 (x=30)
    OLED_ShowString(30, 0, "FG MODE", 8, 1);

    // 行2 (y=16): 标签 "RPM:"
    OLED_ShowString(0, 16, "RPM:", 8, 1);

    // 行3-4 (y=24): 大号 RPM 数字 (16x8 字体, 水平居中)
    num2str(rpm, buf);                                   // 数字转字符串
    {
        u8 len = 0;
        const u8 *pp = buf;
        while (*pp++) len++;                             // 计算字符串长度
        // 水平居中: x = (128 - 字符数×8) / 2
        // 例如 5 位数字: x = (128 - 40) / 2 = 44
        OLED_ShowString((128 - len * 8) / 2, 24, buf, 16, 1);
    }

    // 行6 (y=48): 分隔线 — 21 个 "-"
    OLED_ShowString(0, 48, "---------------------", 8, 1);

    // 行7 (y=56): 分频系数 "Div:2"
    OLED_ShowString(0, 56, "Div:", 8, 1);               // 标签
    num2str(p->fg_div, buf);
    OLED_ShowString(24, 56, buf, 8, 1);                  // 分频值

    OLED_Refresh();
}

/* ════════════════════════════════════════════════════════════
 *  模式 2/3: CH1/CH2 单通道 PWM
 * ════════════════════════════════════════════════════════════
 *
 *  OLED 布局:
 *  ┌─────────────────────┐
 *  │      CH1 PWM        │ 行0 (y=0):   标题 (CH1 PWM / CH2 PWM)
 *  │ O ON                │ 行1 (y=8):   ON/OFF 状态
 *  │>Freq:1000   Hz      │ 行2 (y=16):  频率 (cursor=0)
 *  │ Duty:50     %       │ 行3 (y=24):  占空比 (cursor=1)
 *  │ EN:ON               │ 行4 (y=32):  使能 (cursor=2)
 *  │                     │ 行5 (y=40):  (未使用)
 *  │            RPM:1234 │ 行6 (y=48):  RPM (右下角)
 *  │                     │ 行7 (y=56):  (未使用)
 *  └─────────────────────┘
 *
 *  channel 参数区分 CH1 和 CH2, 布局完全相同, 仅标题和数据源不同
 */
static void render_ch_mode(SystemParams *p, u8 channel, u16 rpm, u8 blink) {
    u8 buf[12];
    u8 cur = g_menu.cursor;                              // 当前光标 (0=频率, 1=占空比, 2=使能)
    u8 sel = g_menu.selected;                            // 选择模式标志
    u32 freq;
    u8 duty, enabled;

    // 根据通道号提取参数
    if (channel == 1) {
        freq = p->ch1_freq_hz; duty = p->ch1_duty_pct; enabled = p->ch1_enabled;
    } else {
        freq = p->ch2_freq_hz; duty = p->ch2_duty_pct; enabled = p->ch2_enabled;
    }

    OLED_Clear();

    // 行0 (y=0): 标题 — 根据通道号显示 "CH1 PWM" 或 "CH2 PWM"
    if (channel == 1)
        OLED_ShowString(24, 0, "CH1 PWM", 8, 1);
    else
        OLED_ShowString(24, 0, "CH2 PWM", 8, 1);

    // 行1 (y=8): 通道使能状态 — "O ON" 或 "  OFF"
    OLED_ShowString(0, 8, (const u8 *)(enabled ? "O ON " : "  OFF"), 8, 1);

    // 行2 (y=16): 频率 (cursor=0)
    // 光标(x=0) → "Freq:"(x=6) → 数字(x=36) → "Hz"(x=96)
    OLED_ShowString(0, 16, marker_str(cur, sel, 0, blink), 8, 1);   // 光标标记
    OLED_ShowString(6, 16, "Freq:", 8, 1);                          // 标签
    num2str(freq, buf);
    OLED_ShowString(36, 16, buf, 8, 1);                              // 频率值
    OLED_ShowString(96, 16, "Hz", 8, 1);                             // 单位

    // 行3 (y=24): 占空比 (cursor=1)
    // 光标(x=0) → "Duty:"(x=6) → 数字(x=36) → "%"(x=54)
    OLED_ShowString(0, 24, marker_str(cur, sel, 1, blink), 8, 1);   // 光标标记
    OLED_ShowString(6, 24, "Duty:", 8, 1);
    num2str(duty, buf);
    OLED_ShowString(36, 24, buf, 8, 1);
    OLED_ShowString(54, 24, "%", 8, 1);

    // 行4 (y=32): 使能开关 (cursor=2)
    // 光标(x=0) → "EN:"(x=6) → "ON"/"OFF"(x=24)
    OLED_ShowString(0, 32, marker_str(cur, sel, 2, blink), 8, 1);   // 光标标记
    OLED_ShowString(6, 32, "EN:", 8, 1);                             // 标签
    OLED_ShowString(24, 32, (const u8 *)(enabled ? "ON " : "OFF"), 8, 1);  // 状态

    // 行6 (y=48): RPM — 显示在右下角
    // "RPM:"(x=60) → 数字(x=84)
    OLED_ShowString(60, 48, "RPM:", 8, 1);
    num2str(rpm, buf);
    OLED_ShowString(84, 48, buf, 8, 1);

    OLED_Refresh();
}

/* ════════════════════════════════════════════════════════════
 *  模式 4: VSP 模拟电压输出
 * ════════════════════════════════════════════════════════════
 *
 *  OLED 布局:
 *  ┌─────────────────────┐
 *  │     VSP CTRL        │ 行0 (y=0):   标题
 *  │ O ON                │ 行1 (y=8):   ON/OFF 状态
 *  │                     │
 *  │    3.5V             │ 行3 (y=24):  大号电压显示
 *  │ [=========         ]│ 行4 (y=32):  百分比进度条
 *  │---------------------│ 行5 (y=40):  分隔线
 *  │>Vout:3.5V   EN:ON   │ 行6 (y=48):  电压值 + 使能 (cursor=0,1)
 *  │                     │ 行7 (y=56):  (未使用)
 *  └─────────────────────┘
 */
static void render_vsp_mode(SystemParams *p, u8 blink) {
    u8 buf[12];
    u8 cur = g_menu.cursor;
    u8 sel = g_menu.selected;
    u8 v = p->vsp_voltage_x10;  // 0~50

    OLED_Clear();

    // 行0 (y=0): 标题 "VSP CTRL", 水平居中
    OLED_ShowString(24, 0, "VSP CTRL", 8, 1);

    // 行1 (y=8): ON/OFF 状态
    OLED_ShowString(0, 8, (const u8 *)(p->vsp_enabled ? "O ON " : "  OFF"), 8, 1);

    // 行3 (y=24): 电压值 "X.XV"
    buf[0] = '0' + (v / 10);    // 整数位
    buf[1] = '.';
    buf[2] = '0' + (v % 10);    // 小数位
    buf[3] = 'V';
    buf[4] = 0;
    OLED_ShowString(42, 24, buf, 8, 1);

    // 行4 (y=32): 进度条 (v/50 * 100%)
    {
        u8 bar_len = (u8)((u32)v * 20 / 50);  // 最多 20 个字符
        u8 i;
        OLED_ShowString(4, 32, "[", 8, 1);
        for (i = 0; i < 20; i++) {
            buf[0] = (i < bar_len) ? '=' : ' ';
            buf[1] = 0;
            OLED_ShowString(10 + i * 6, 32, buf, 8, 1);
        }
        OLED_ShowString(128 - 6, 32, "]", 8, 1);
    }

    // 行5 (y=40): 分隔线
    OLED_ShowString(0, 40, "---------------------", 8, 1);

    // 行6 (y=48): Vout 值 + EN 状态
    OLED_ShowString(0, 48, marker_str(cur, sel, VSP_ITEM_VOLTAGE, blink), 8, 1);
    OLED_ShowString(6, 48, "Vout:", 8, 1);
    buf[0] = '0' + (v / 10);
    buf[1] = '.';
    buf[2] = '0' + (v % 10);
    buf[3] = 'V';
    buf[4] = 0;
    OLED_ShowString(36, 48, buf, 8, 1);

    OLED_ShowString(72, 48, marker_str(cur, sel, VSP_ITEM_ENABLE, blink), 8, 1);
    OLED_ShowString(78, 48, "EN:", 8, 1);
    OLED_ShowString(96, 48, (const u8 *)(p->vsp_enabled ? "ON " : "OFF"), 8, 1);

    OLED_Refresh();
}

/* ════════════════════════════════════════════════════════════
 *  模式 5: TEST 自动测试
 * ════════════════════════════════════════════════════════════
 *
 *  两种子状态:
 *
 *  【配置状态 (test_running=0)】
 *  OLED 布局:
 *  ┌─────────────────────┐
 *  │    TEST MODE        │ 行0 (y=0):   标题
 *  │>Ch:CH1    Fr:1kHz   │ 行1 (y=8):   通道 | 频率 (两列)
 *  │ Duty:50%   N:10     │ 行2 (y=16):  占空比 | 次数
 *  │ ON:5s      OFF:3s   │ 行3 (y=24):  ON时间 | OFF时间
 *  │                     │ 行4 (y=32):  (未使用)
 *  │---------------------│ 行5 (y=40):  分隔线
 *  │     START   Rec:5   │ 行6 (y=48):  START 按钮 + 记录数
 *  │                     │ 行7 (y=56):  (未使用)
 *  └─────────────────────┘
 *
 *  【运行状态 (test_running=1)】
 *  OLED 布局:
 *  ┌─────────────────────┐
 *  │    TEST RUN         │ 行0 (y=0):   标题
 *  │ RPM:1234            │ 行1 (y=8):   实时 RPM
 *  │ CH1 1000Hz  50%     │ 行2 (y=16):  当前测试参数
 *  │ Cycle:5/10    >>>   │ 行3 (y=24):  循环进度 + 运行指示 (闪烁)
 *  │                     │ 行4-5:       (未使用)
 *  │ Click=Stop          │ 行6 (y=48):  停止提示
 *  └─────────────────────┘
 */
static void render_test_mode(SystemParams *p, u16 rpm, u8 blink) {
    u8 buf[12];
    u8 cur = g_menu.cursor;
    u8 sel = g_menu.selected;

    OLED_Clear();

    if (test_running) {
        /* ════════ 运行状态 ════════ */

        // 行0 (y=0): 标题 "TEST RUN", 水平居中
        OLED_ShowString(24, 0, "TEST RUN", 8, 1);

        // 行1 (y=8): 实时 RPM
        OLED_ShowString(0, 8, "RPM:", 8, 1);                        // 标签
        num2str(rpm, buf);
        OLED_ShowString(24, 8, buf, 8, 1);                           // RPM 值

        // 行2 (y=16): 当前测试参数 — 通道 + 频率 + 占空比
        OLED_ShowString(0, 16, "CH", 8, 1);                         // "CH"
        buf[0] = '0' + Menu_GetTestChannel(); buf[1] = 0;           // 通道号 "1" 或 "2"
        OLED_ShowString(12, 16, buf, 8, 1);
        num2str(Menu_GetTestFreq(), buf);                            // 测试频率
        OLED_ShowString(24, 16, buf, 8, 1);
        OLED_ShowString(66, 16, "Hz", 8, 1);                        // 单位
        num2str(Menu_GetTestDuty(), buf);                            // 测试占空比
        OLED_ShowString(84, 16, buf, 8, 1);
        OLED_ShowString(102, 16, "%", 8, 1);                        // 单位

        // 行3 (y=24): 循环进度 "Cycle:5/10"
        OLED_ShowString(0, 24, "Cycle:", 8, 1);                     // 标签
        num2str(Menu_GetTestRecordCount(), buf);                     // 当前已完成循环数
        OLED_ShowString(36, 24, buf, 8, 1);
        OLED_ShowString(54, 24, "/", 8, 1);                         // 分隔符
        num2str(Menu_GetTestCycles(), buf);                          // 总循环数
        OLED_ShowString(60, 24, buf, 8, 1);

        // 运行指示 (闪烁的 ">>>"), blink=1 时显示
        if (blink) {
            OLED_ShowString(96, 24, ">>>", 8, 1);                   // 闪烁指示器
        }

        // 行6 (y=48): 停止提示 — "Click=Stop"
        OLED_ShowString(0, 40, "Click=Stop", 8, 1);

    } else {
        /* ════════ 配置状态 ════════ */

        // 行0 (y=0): 标题 "TEST MODE", 水平居中
        OLED_ShowString(18, 0, "TEST MODE", 8, 1);

        // 行1 (y=8): 左列=通道, 右列=频率
        // 左: 光标(x=0) → "Ch:"(x=6) → "CH"(x=24) → "1/2"(x=36)
        OLED_ShowString(0, 8, marker_str(cur, sel, TEST_ITEM_CHANNEL, blink), 8, 1);
        OLED_ShowString(6, 8, "Ch:", 8, 1);
        OLED_ShowString(24, 8, "CH", 8, 1);
        buf[0] = '0' + Menu_GetTestChannel(); buf[1] = 0;
        OLED_ShowString(36, 8, buf, 8, 1);

        // 右: 光标(x=66) → "Fr:"(x=72) → 数字(x=90) → "Hz"(x=120)
        OLED_ShowString(66, 8, marker_str(cur, sel, TEST_ITEM_FREQ, blink), 8, 1);
        OLED_ShowString(72, 8, "Fr:", 8, 1);
        num2str(Menu_GetTestFreq(), buf);
        OLED_ShowString(90, 8, buf, 8, 1);
        OLED_ShowString(120, 8, "Hz", 8, 1);

        // 行2 (y=16): 左列=占空比, 右列=循环次数
        OLED_ShowString(0, 16, marker_str(cur, sel, TEST_ITEM_DUTY, blink), 8, 1);
        OLED_ShowString(6, 16, "Duty:", 8, 1);
        num2str(Menu_GetTestDuty(), buf);
        OLED_ShowString(36, 16, buf, 8, 1);
        OLED_ShowString(54, 16, "%", 8, 1);

        OLED_ShowString(66, 16, marker_str(cur, sel, TEST_ITEM_CYCLES, blink), 8, 1);
        OLED_ShowString(72, 16, "N:", 8, 1);
        num2str(Menu_GetTestCycles(), buf);
        OLED_ShowString(84, 16, buf, 8, 1);

        // 行3 (y=24): 左列=ON时间, 右列=OFF时间
        OLED_ShowString(0, 24, marker_str(cur, sel, TEST_ITEM_ON_TIME, blink), 8, 1);
        OLED_ShowString(6, 24, "ON:", 8, 1);
        num2str(Menu_GetTestOnSec(), buf);
        OLED_ShowString(24, 24, buf, 8, 1);
        OLED_ShowString(42, 24, "s", 8, 1);                        // 秒

        OLED_ShowString(66, 24, marker_str(cur, sel, TEST_ITEM_OFF_TIME, blink), 8, 1);
        OLED_ShowString(72, 24, "OFF:", 8, 1);
        num2str(Menu_GetTestOffSec(), buf);
        OLED_ShowString(96, 24, buf, 8, 1);
        OLED_ShowString(114, 24, "s", 8, 1);                       // 秒

        // 行4 (y=32): ON 方式 (PWM/Relay/Both)
        OLED_ShowString(0, 32, marker_str(cur, sel, TEST_ITEM_ON_METHOD, blink), 8, 1);
        OLED_ShowString(6, 32, "Mode:", 8, 1);
        {
            const u8 *method_names[] = {(const u8 *)"PWM", (const u8 *)"Rly", (const u8 *)"Bth"};
            u8 m = Menu_GetTestOnMethod();
            if (m > 2) m = 0;
            OLED_ShowString(36, 32, method_names[m], 8, 1);
        }

        // 行5 (y=40): 分隔线 — 21 个 "-"
        OLED_ShowString(0, 40, "---------------------", 8, 1);

        // 行6 (y=48): START 按钮 + 记录数
        // 光标(x=36) → "START"(x=42)
        OLED_ShowString(36, 48, marker_str(cur, sel, TEST_ITEM_START, blink), 8, 1);
        OLED_ShowString(42, 48, "START", 8, 1);

        // 右侧显示已有记录数 (仅在有记录时显示)
        if (Menu_GetTestRecordCount() > 0) {
            OLED_ShowString(90, 48, "Rec:", 8, 1);                  // "Rec:"
            num2str(Menu_GetTestRecordCount(), buf);
            OLED_ShowString(114, 48, buf, 8, 1);                    // 记录数
        }
    }

    OLED_Refresh();
}

/* ════════════════════════════════════════════════════════════
 *  渲染分发函数 — 主循环入口
 * ════════════════════════════════════════════════════════════
 *  由 main.c 每 50ms (20fps) 调用一次
 *  根据当前菜单模式 (g_menu.mode) 分派到对应的渲染函数
 *  所有渲染函数遵循相同流程: OLED_Clear → 绘制 → OLED_Refresh
 *
 *  参数:
 *    p:    系统参数指针 (频率/占空比/使能等)
 *    rpm:  当前 RPM 值 (由 main.c 从 fg_period_us 计算)
 *    blink: 闪烁状态 (0/1, 1Hz 频率翻转)
 */
void UI_Render(SystemParams *p, u16 rpm, u8 blink) {
    switch (g_menu.mode) {
        case MODE_PWM_FG: render_pwm_fg(p, rpm, blink); break;     // 双通道 + 频率计
        case MODE_FG:     render_fg_mode(p, rpm, blink); break;    // 纯频率计
        case MODE_CH1:    render_ch_mode(p, 1, rpm, blink); break; // CH1 单通道
        case MODE_CH2:    render_ch_mode(p, 2, rpm, blink); break; // CH2 单通道
        case MODE_VSP:    render_vsp_mode(p, blink); break;        // VSP 模拟电压
        case MODE_TEST:   render_test_mode(p, rpm, blink); break;  // 自动测试
        default:          render_pwm_fg(p, rpm, blink); break;     // 默认回退到 PWM-FG
    }
}
