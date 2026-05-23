/*
 * fg_capture.c — 频率计 / RPM 计算模块
 * ══════════════════════════════════════════════════════════════════════
 *  硬件: TIM2 CH1 (PA0) 输入捕获
 *
 *  ┌─────────────────────────────────────────────────────────────────┐
 *  │  时基配置                                                       │
 *  │                                                                 │
 *  │  PSC = 71 → 计数时钟 = 72MHz / (71+1) = 1MHz                   │
 *  │  即每个计数 = 1μs, 测量分辨率 = 1μs                            │
 *  │  ARR = 0xFFFF (65535) → 最大可测周期 = 65.535ms                │
 *  │         对应最低可测频率 ≈ 15.3Hz                               │
 *  │                                                                 │
 *  │  测量原理: 捕获高电平脉宽                                        │
 *  │                                                                 │
 *  │     上升沿触发          下降沿触发                               │
 *  │         ↓                   ↓                                   │
 *  │   ┌────────────┐            │                                   │
 *  │   │  高电平区域  │←─ fg_period_us ─→│                            │
 *  │───┘            └───────────────────                             │
 *  │                                                                 │
 *  │  频率计算: f(Hz) = 1 / 高电平时间(s)                            │
 *  │           f(mHz) = 1000 / period(us) × 1000                     │
 *  │                  = 1000000000 / period(us)                      │
 *  │                                                                 │
 *  │  注意: 此方法假设占空比接近 50%, 即高电平时间 ≈ 半周期          │
 *  │        如果占空比非 50%, 测得的 "频率" 实际是 1/脉宽            │
 *  │        本项目 PWM 输出为方波, 占空比可调, 但频率计测量的是      │
 *  │        FG (Function Generator) 信号, 通常为 50% 占空比          │
 *  │                                                                 │
 *  │  RPM 公式:                                                      │
 *  │    RPM = (实测频率_Hz / 分频系数) × 60 / 每转脉冲数            │
 *  │                                                                 │
 *  │    其中:                                                        │
 *  │      分频系数 (fg_div): 编码器信号的分频比, 范围 1~99          │
 *  │      每转脉冲数 (fg_pulses_per_rev): 编码器每转产生的脉冲数    │
 *  │      60: 将 Hz (次/秒) 转换为 RPM (转/分钟)                    │
 *  │                                                                 │
 *  │    例: 实测 freq=2000Hz, div=1, pulses_per_rev=100             │
 *  │        RPM = (2000/1) × 60 / 100 = 1200 RPM                    │
 *  └─────────────────────────────────────────────────────────────────┘
 */
#include "fg_capture.h"
#include "../TIMER/timer.h"

// ── 频率计初始化 ──
// TIM2 输入捕获配置:
//   ARR = 0xFFFF (最大周期, 防止溢出丢数据)
//   PSC = 71    (72MHz / 72 = 1MHz 计数时钟, 1μs 分辨率)
void FG_Init(void) {
    TIM2_Cap_Init(0xFFFF, 71);
}

// ── 获取 FG 信号频率, 单位毫赫兹 (mHz) ──
//
// 返回值: 频率 (mHz), 0 表示无信号
//
// 计算公式:
//   f_mHz = 1,000,000,000 / fg_period_us
//
// 推导:
//   f_Hz = 1 / period_s = 1 / (period_us / 1,000,000) = 1,000,000 / period_us
//   f_mHz = f_Hz × 1000 = 1,000,000,000 / period_us
//
// 例: period_us = 500 (0.5ms 高电平)
//     f_mHz = 1,000,000,000 / 500 = 2,000,000 mHz = 2000 Hz
u32 FG_GetFrequency_mHz(void) {
    u32 period = fg_period_us;          // 读取 TIM2 捕获的高电平时间 (μs)
    if (period == 0) return 0;          // 无信号或信号异常
    return 1000000000UL / period;       // 计算频率 (mHz)
}

// ── 计算 RPM (转速) ──
//
// 参数:
//   div            — 分频系数 (1~99), 对 FG 信号进行分频
//   pulses_per_rev — 每转脉冲数 (编码器每转产生的脉冲数)
//
// 计算步骤:
//   1. 获取实测频率 (mHz)
//   2. 转换为 Hz: freq_hz = freq_mhz / 1000
//   3. 除以分频系数: 实际转速频率 = freq_hz / div
//   4. 转换为 RPM: RPM = 实际转速频率 × 60 / pulses_per_rev
//
// 例: freq_mhz=2000000 (即 2kHz), div=1, pulses_per_rev=100
//     freq_hz = 2000000/1000 = 2000 Hz
//     RPM = (2000/1) × 60 / 100 = 1200 RPM
//
// 注意: div=0 和 pulses_per_rev=0 时强制设为 1, 防止除零错误
u16 FG_CalculateRPM(u8 div, u16 pulses_per_rev) {
    u32 freq_mhz = FG_GetFrequency_mHz();   // 实测频率 (mHz)
    if (div == 0) div = 1;                   // 防止除零
    if (pulses_per_rev == 0) pulses_per_rev = 1;
    u32 freq_hz = freq_mhz / 1000;          // 转换为 Hz
    return (u16)((freq_hz / div) * 60 / pulses_per_rev);  // 计算 RPM
}
