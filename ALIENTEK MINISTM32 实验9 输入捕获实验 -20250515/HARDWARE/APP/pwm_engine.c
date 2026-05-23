/*
 * pwm_engine.c — 双通道 PWM 频率/占空比控制
 * ══════════════════════════════════════════════════════════════════════
 *  硬件通道:
 *    CH1: TIM1 CH1 (PA8) — 高级定时器, 支持 MOE(主输出使能) 和死区
 *    CH2: TIM4 CH1 (PB6) — 通用定时器
 *
 *  ┌─────────────────────────────────────────────────────────────────┐
 *  │  自适应预分频算法 — 根据目标频率自动选择最优 PSC              │
 *  │                                                                 │
 *  │  目标: 保证 ARR ≥ 100 (占空比分辨率 ≥ 1%), 且 ARR ≤ 65535    │
 *  │                                                                 │
 *  │  PWM 频率公式: freq = 72MHz / (PSC+1) / (ARR+1)               │
 *  │  推导: ARR+1 = 72MHz / (PSC+1) / freq                         │
 *  │                                                                 │
 *  │  4 个频率段:                                                    │
 *  │  ┌──────────────┬─────────┬──────────────┬───────────────────┐  │
 *  │  │ 频率范围      │  PSC    │ 计数时钟      │ 典型 ARR         │  │
 *  │  ├──────────────┼─────────┼──────────────┼───────────────────┤  │
 *  │  │ 1 ~ 100 Hz   │  7199   │ 10 kHz       │ 100 ~ 10000      │  │
 *  │  │ 100 ~ 1 kHz  │  719    │ 100 kHz      │ 100 ~ 1000       │  │
 *  │  │ 1k ~ 10 kHz  │  71     │ 1 MHz        │ 100 ~ 1000       │  │
 *  │  │ >10 kHz      │  7      │ 9 MHz        │ 100 ~ 900        │  │
 *  │  └──────────────┴─────────┴──────────────┴───────────────────┘  │
 *  │                                                                 │
 *  │  占空比计算: CCR = (ARR+1) × duty% / 100                       │
 *  │  例如: ARR=999, duty=50 → CCR = 1000×50/100 = 500              │
 *  └─────────────────────────────────────────────────────────────────┘
 *
 *  TIM1 高级定时器特殊要求:
 *    - TIM1 的 PWM 输出需要 MOE (Main Output Enable) 位使能
 *    - 否则 PA8 无输出 (通用定时器 TIM4 无此限制)
 *    - TIM1_PWM_Init() 中已处理 MOE 使能
 */
#include "pwm_engine.h"
#include "../TIMER/timer.h"

// 缓存当前各通道的占空比百分比值 (0~100)
// 用于 PWM_EnableChannel() 恢复占空比 (从关闭状态重新开启时)
static u8  ch1_duty = 0, ch2_duty = 0;

// ── 自适应预分频计算函数 ──
//
// 输入: freq — 目标频率 (Hz), 范围 1 ~ 100000
// 输出: *psc — 预分频值, *arr — 自动重装载值
//
// 算法逻辑:
//   Step 1: 根据频率范围选择 PSC, 将 72MHz 分频到合适的计数时钟
//           选择原则: 计数时钟越高越好 (精度高), 但要保证 ARR 不溢出
//   Step 2: 计算 ARR = timer_clk / freq - 1
//   Step 3: 限幅保护
//           - ARR < 100 时强制定为 100 (保证占空比分辨率 ≥ 1%)
//           - ARR > 65535 时截断为 65535 (16 位定时器上限)
//
// 为什么 ARR ≥ 100 很重要?
//   占空比分辨率 = 1/(ARR+1) × 100%
//   ARR=99  → 分辨率 = 1%   (每步 1%)
//   ARR=999 → 分辨率 = 0.1% (每步 0.1%)
//   如果 ARR 太小, 占空比调节会很粗糙
static void calc_psc_arr(u32 freq, u16 *psc, u16 *arr) {
    // Step 1: 选择预分频值 (4 个频率段)
    if (freq <= 100) {
        // 低频段: 72MHz / 7200 = 10kHz 计数时钟
        // freq=1Hz 时 ARR=10000-1=9999, 分辨率极好
        *psc = 7199;
    } else if (freq <= 1000) {
        // 中频段: 72MHz / 720 = 100kHz 计数时钟
        // freq=500Hz 时 ARR=200-1=199
        *psc = 719;
    } else if (freq <= 10000) {
        // 高频段: 72MHz / 72 = 1MHz 计数时钟
        // freq=5kHz 时 ARR=200-1=199
        *psc = 71;
    } else {
        // 超高频段: 72MHz / 8 = 9MHz 计数时钟
        // freq=50kHz 时 ARR=180-1=179
        // 此段 ARR 最小, 分辨率最低, 但仍保证 ≥100
        *psc = 7;
    }

    // Step 2: 计算 ARR = 定时器计数时钟 / 目标频率 - 1
    // timer_clk = 72000000 / (psc+1)
    // arr+1 = timer_clk / freq  →  arr = timer_clk / freq - 1
    u32 timer_clk = 72000000UL / (*psc + 1);
    u32 a = timer_clk / freq;

    // Step 3: 限幅保护
    if (a < 100) a = 100;       // 最低保证 1% 占空比分辨率
    if (a > 65535) a = 65535;   // 16 位定时器最大值
    *arr = (u16)(a - 1);        // ARR 寄存器值 = 周期计数 - 1
}

// ── PWM 模块初始化 ──
// 两个通道默认: 1kHz 频率, 0% 占空比 (输出低电平)
// TIM1 PWM 配置: ARR=999, PSC=71 → freq = 72M/72/1000 = 1kHz
// TIM4 PWM 配置: ARR=999, PSC=71 → freq = 72M/72/1000 = 1kHz
void PWM_Init(void) {
    TIM1_PWM_Init(999, 71);   // CH1: PA8, 高级定时器 (需 MOE 使能)
    TIM4_PWM_Init(999, 71);   // CH2: PB6, 通用定时器
}

// ── 设置指定通道的频率和占空比 ──
//
// 参数:
//   channel  — 通道号 (1 或 2)
//   freq_hz  — 目标频率 (Hz), 范围自动限幅到 [1, 100000]
//   duty_pct — 占空比 (%), 范围自动限幅到 [0, 100]
//
// 流程:
//   1. 参数限幅保护
//   2. 调用 calc_psc_arr() 计算最优 PSC 和 ARR
//   3. 计算 CCR = (ARR+1) × duty_pct / 100
//      例如: ARR=999, duty=25 → CCR = 1000×25/100 = 250
//   4. 写入定时器寄存器 (PSC, ARR, CCR)
//   5. 生成 Update 事件使 PSC 立即生效
//      (STM32 定时器的 PSC 只在 Update 事件时才加载新值)
void PWM_SetChannel(u8 channel, u32 freq_hz, u8 duty_pct) {
    // 参数限幅
    if (freq_hz < 1) freq_hz = 1;
    if (freq_hz > 100000) freq_hz = 100000;
    if (duty_pct > 100) duty_pct = 100;

    // 计算 PSC 和 ARR
    u16 psc, arr;
    calc_psc_arr(freq_hz, &psc, &arr);

    // 计算 CCR (比较值 / 脉宽)
    // CCR 决定了在一个 PWM 周期中, 输出高电平持续多少个计数周期
    // CCR = 0         → 0% 占空比 (全低)
    // CCR = ARR+1     → 100% 占空比 (全高)
    // CCR = (ARR+1)/2 → 50% 占空比
    u16 ccr = (u32)(arr + 1) * duty_pct / 100;

    if (channel == 1) {
        ch1_duty = duty_pct;                // 缓存占空比 (供 EnableChannel 恢复)
        TIM1->PSC = psc;                    // 设置预分频
        TIM1->ARR = arr;                    // 设置自动重装载值 (决定周期)
        TIM1->CCR1 = ccr;                   // 设置通道 1 比较值 (决定脉宽)
        TIM_GenerateEvent(TIM1, TIM_EventSource_Update);  // 立即加载 PSC
    } else if (channel == 2) {
        ch2_duty = duty_pct;
        TIM4->PSC = psc;
        TIM4->ARR = arr;
        TIM4->CCR1 = ccr;
        TIM_GenerateEvent(TIM4, TIM_EventSource_Update);
    }
}

// ── 使能/禁用指定通道的 PWM 输出 ──
//
// 禁用时: CCR 设为 0, 输出恒低电平 (实际效果等同于 PWM 关闭)
// 使能时: 恢复之前缓存的占空比值 (CCR = (ARR+1) × duty% / 100)
//
// 注意: 这里不关闭定时器本身, 只是将 CCR 设为 0
//       好处是可以随时恢复, 无需重新配置定时器
void PWM_EnableChannel(u8 channel, u8 enable) {
    if (channel == 1) {
        if (enable) {
            // 恢复占空比: CCR = (ARR+1) × 缓存的 duty% / 100
            TIM_SetCompare1(TIM1, (u32)(TIM1->ARR + 1) * ch1_duty / 100);
        } else {
            // 关闭输出: CCR = 0 → 恒低电平
            TIM_SetCompare1(TIM1, 0);
        }
    } else if (channel == 2) {
        if (enable) {
            TIM_SetCompare1(TIM4, (u32)(TIM4->ARR + 1) * ch2_duty / 100);
        } else {
            TIM_SetCompare1(TIM4, 0);
        }
    }
}

// ── 从 SystemParams 全局参数同步更新所有通道 ──
//
// 调用时机: 菜单修改参数后 (g_menu.dirty == 1 时)
// 根据 g_params 中的 enabled/freq/duty 设置, 逐通道更新 PWM
void PWM_UpdateFromParams(SystemParams *params) {
    // 通道 1
    if (params->ch1_enabled) {
        PWM_SetChannel(1, params->ch1_freq_hz, params->ch1_duty_pct);
        PWM_EnableChannel(1, 1);    // 启用输出
    } else {
        PWM_EnableChannel(1, 0);    // 禁用输出
    }

    // 通道 2
    if (params->ch2_enabled) {
        PWM_SetChannel(2, params->ch2_freq_hz, params->ch2_duty_pct);
        PWM_EnableChannel(2, 1);
    } else {
        PWM_EnableChannel(2, 0);
    }
}
