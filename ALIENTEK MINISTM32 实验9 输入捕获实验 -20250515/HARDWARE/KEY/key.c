/*
 * key.c — 按键驱动 (精简版)
 * ══════════════════════════════════════════════════════════════
 *
 * 【硬件配置】
 *   仅使用 1 个按键: KEY1 = PB8
 *   原始 ALIENTEK 开发板有 4 个按键 (WK_UP/KEY0/KEY1/KEY2)
 *   本项目精简为单按键, 通过旋转编码器处理大部分输入
 *
 * 【PB8 配置】
 *   - 上拉输入 (IPU): 默认高电平, 按下接地变为低电平
 *   - 按键接地: PB8 → 按键 → GND
 *   - 松开: PB8 被内部上拉电阻拉高 → KEY1 == 1
 *   - 按下: PB8 接地 → KEY1 == 0
 *
 * 【JTAG 复用说明】
 *   PB8 默认是 JTAG 引脚 (NJTRST)
 *   必须调用 GPIO_Remap_SWJ_JTAGDisable 禁用 JTAG 功能
 *   才能将 PB8 用作普通 GPIO 输入
 *   注意: SWD 调试仍然可用 (PA13/PA14), 仅禁用 JTAG
 *
 * 【按键扫描逻辑】
 *   KEY_Scan(mode):
 *     mode=0: 支持连续按 (每次调用都可能返回按下)
 *     mode=1: 不支持连续按 (必须松开后再次按下才返回)
 *   返回值: KEY1_PRES (按下事件), 0 (无事件)
 *   10ms 消抖: 检测到按下后延时 10ms 再次确认
 */
#include "key.h"
#include "delay.h"

/* ── 按键 GPIO 初始化 ──
 * 1. 开启 GPIOB 时钟
 * 2. 禁用 JTAG (保留 SWD), 释放 PB8 用作普通 GPIO
 * 3. 配置 PB8 为上拉输入
 */
void KEY_Init(void) {
    GPIO_InitTypeDef GPIO_InitStructure;
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE);

    /* ── 禁用 JTAG, 保留 SWD ──
     * GPIO_Remap_SWJ_JTAGDisable: 禁用 JTAG (释放 PB3/PB4/PA13/PA14/PB8)
     * SWD 调试接口 (PA13=SWDIO, PA14=SWCLK) 仍然可用
     * 这是使用 PB8 作为 GPIO 的前提条件
     */
    GPIO_PinRemapConfig(GPIO_Remap_SWJ_JTAGDisable, ENABLE);

    /* ── PB8 上拉输入 ──
     * IPU = Input Pull-Up
     * 内部上拉电阻 (~40kΩ) 将 PB8 默认拉到 VCC
     * 按键按下时 PB8 接地, 形成低电平
     */
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_8;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPU;          // 上拉输入
    GPIO_Init(GPIOB, &GPIO_InitStructure);
}

/* ── 按键扫描函数 ──
 * 参数 mode:
 *   0 — 不支持连续按: 松开后才能再次触发 (使用静态变量 key_up 记录状态)
 *   1 — 支持连续按: 每次调用都重置 key_up, 只要按下就返回
 *
 * 扫描流程:
 *   1. 如果 mode=1, 强制 key_up=1 (允许连续检测)
 *   2. 如果 key_up=1 且 KEY1==0 (按下):
 *      a. 消抖延时 10ms
 *      b. 再次检测 KEY1==0 → 返回 KEY1_PRES
 *      c. 设置 key_up=0 (防止重复触发)
 *   3. 如果 KEY1==1 (松开), 恢复 key_up=1
 *   4. 返回 0 (无按键事件)
 */
u8 KEY_Scan(u8 mode) {
    static u8 key_up = 1;               // 按键松开标志 (1=已松开, 0=按下中)

    if (mode) key_up = 1;               // mode=1: 强制重置, 支持连续按

    if (key_up && (KEY1 == 0)) {        // 已松开 且 检测到按下 (低电平)
        delay_ms(10);                    // 10ms 消抖
        key_up = 0;                      // 标记为按下中 (防止重复触发)
        if (KEY1 == 0) return KEY1_PRES; // 再次确认按下, 返回按键事件
    } else if (KEY1 == 1) {             // 按键松开 (高电平)
        key_up = 1;                      // 恢复松开标志
    }

    return 0;                            // 无按键事件
}
