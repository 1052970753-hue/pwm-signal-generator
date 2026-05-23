/*
 * Encoder.c — 旋转编码器 + 按键驱动
 * ══════════════════════════════════════════════════════════════════════
 *  硬件:
 *    编码器 A/B 相: PA6/PA7 → TIM3 编码器模式 (硬件自动计数)
 *    OK 按键:        PB8     → GPIO 内部上拉输入 (低电平有效)
 *
 *  ┌─────────────────────────────────────────────────────────────────┐
 *  │  TIM3 编码器模式 — TI1+TI2 双边沿计数 (4 倍精度)              │
 *  │                                                                 │
 *  │  普通模式 (仅上升沿): 每个脉冲计 1 次                          │
 *  │  本项目模式 (双边沿): 每个脉冲计 4 次 (A 上升+A 下降+         │
 *  │                       B 上升+B 下降)                           │
 *  │                                                                 │
 *  │  旋转方向: 由 A/B 相位差决定                                   │
 *  │    顺时针: 计数器递增 (delta > 0)                              │
 *  │    逆时针: 计数器递减 (delta < 0)                              │
 *  │                                                                 │
 *  │  抖动消除: delta ≥ 4 才判定为一次有效旋转 (即物理上转了一格)  │
 *  │    因为 4x 精度下, 每格产生 4 个计数, 需要全部到位才算         │
 *  └─────────────────────────────────────────────────────────────────┘
 *
 *  ┌─────────────────────────────────────────────────────────────────┐
 *  │  按键事件检测流程                                               │
 *  │                                                                 │
 *  │  按下 ──→ 持续按住 ──→ 释放                                    │
 *  │   │         │            │                                      │
 *  │   │     超过 1000ms?     └→ < 1000ms: 可能是单击或双击         │
 *  │   │         │                                                   │
 *  │   │      是: 返回 LONG_PRESS (长按)                             │
 *  │   │         │                                                   │
 *  │   │     否: 等待释放                                            │
 *  │   │         │                                                   │
 *  │   └─────────┴→ 释放时:                                         │
 *  │                ├─ 首次点击 → 设置 pending_click=1, 等 400ms    │
 *  │                │   ├─ 400ms 内无第二次点击 → 返回 CLICK (单击) │
 *  │                │   └─ 400ms 内有第二次点击 → 返回 DOUBLE_CLICK │
 *  │                └─ 已有待处理单击 + 再次点击 → 返回 DOUBLE_CLICK│
 *  └─────────────────────────────────────────────────────────────────┘
 */
#include "Encoder.h"
#include "../APP/menu_defs.h"  // InputEvent enum: EVENT_NONE, EVENT_CW, etc.

// ── 模块内部状态变量 ──
static s16 enc_last = 0;            // 上次读取的 TIM3 计数值 (用于计算 delta)
static u32 btn_press_ticks = 0;     // 按键持续按下的 tick 数 (1 tick = 1ms)
static u8 btn_ok_prev = 1;          // 上次按键状态: 1=释放, 0=按下
static u32 sys_tick = 0;            // 系统滴答副本 (由 main 通过 Encoder_SetTick 传入)
static u8  pending_click = 0;       // 延迟单击标志: 1=已收到一次点击, 等待可能的第二次
static u32 pending_click_tick = 0;  // 延迟单击的释放时刻 (用于 400ms 超时判断)

#define DOUBLE_CLICK_MS 400         // 双击判定窗口 (ms): 两次点击间隔小于此值→双击
#define LONG_PRESS_TICKS 1000       // 长按阈值 (ms): 持续按住超过此值→长按

// ── 编码器初始化 ──
// 配置 TIM3 为编码器模式, PA6/PA7 为浮空输入, PB8 为上拉输入
void Encoder_Init(void) {
    // 使能 TIM3 和 GPIO 时钟
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM3, ENABLE);
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA | RCC_APB2Periph_GPIOB, ENABLE);

    // 编码器 A/B 相引脚: PA6, PA7 — 浮空输入
    // 浮空输入: 电平由外部编码器信号决定, 内部无上下拉
    GPIO_InitTypeDef gpio;
    gpio.GPIO_Pin = GPIO_Pin_6 | GPIO_Pin_7;
    gpio.GPIO_Mode = GPIO_Mode_IN_FLOATING;
    GPIO_Init(GPIOA, &gpio);

    // OK 按键引脚: PB8 — 内部上拉输入
    // 上拉输入: 默认读到高电平 (1), 按键按下时接地 → 读到低电平 (0)
    gpio.GPIO_Pin = GPIO_Pin_8;
    gpio.GPIO_Mode = GPIO_Mode_IPU;
    GPIO_Init(GPIOB, &gpio);

    // TIM3 时基配置
    // Period=65535: 16 位计数器最大值, 编码器模式下足够使用
    // 无预分频: 72MHz 直接驱动, 编码器信号直接作为时钟源
    TIM_TimeBaseInitTypeDef tim;
    tim.TIM_Period = 65535;
    tim.TIM_Prescaler = 0;
    tim.TIM_ClockDivision = 0;
    tim.TIM_CounterMode = TIM_CounterMode_Up;
    TIM_TimeBaseInit(TIM3, &tim);

    // 编码器接口配置: TI1+TI2 模式, 两个通道均为上升沿触发
    // TI1+TI2 = 4x 精度: A 相上升沿+A 相下降沿+B 相上升沿+B 相下降沿都计数
    // 每物理格产生 4 个计数值, 提供最高分辨率
    TIM_EncoderInterfaceConfig(TIM3, TIM_EncoderMode_TI12,
        TIM_ICPolarity_Rising, TIM_ICPolarity_Rising);

    // 输入滤波配置: 0x0F = 连续 4 个时钟周期电平一致才确认
    // 作用: 消除机械触点抖动 (约几十μs 的毛刺)
    // 滤波器采样频率 = 定时器时钟, 4 个周期 @72MHz ≈ 56ns, 足够消除抖动
    TIM_ICInitTypeDef ic;
    ic.TIM_Channel = TIM_Channel_1;
    ic.TIM_ICPolarity = TIM_ICPolarity_Rising;
    ic.TIM_ICSelection = TIM_ICSelection_DirectTI;
    ic.TIM_ICPrescaler = TIM_ICPSC_DIV1;
    ic.TIM_ICFilter = 0x0F;
    TIM_ICInit(TIM3, &ic);
    ic.TIM_Channel = TIM_Channel_2;
    TIM_ICInit(TIM3, &ic);

    // 清零计数器并启动定时器
    TIM_SetCounter(TIM3, 0);
    TIM_Cmd(TIM3, ENABLE);
}

// ── 传入系统滴答 ──
// 由 main() 每次主循环调用, 将全局 sys_tick 传入本模块
// 用于按键长按 (1000ms) 和双击 (400ms) 的时间测量
void Encoder_SetTick(u32 tick) {
    sys_tick = tick;
}

// ── 主循环轮询函数 ──
// 返回值: InputEvent_t 枚举值 (EVENT_NONE / EVENT_CW / EVENT_CCW / EVENT_CLICK /
//                               EVENT_DOUBLE_CLICK / EVENT_LONG_PRESS)
//
// 调用频率: 每次主循环调用一次 (尽可能快, 确保不丢失事件)
InputEvent_t Encoder_Poll(void) {
    // ══════════════════════════════════════════════════════════════════
    //  旋转检测: 读取 TIM3 计数器, 计算增量
    // ══════════════════════════════════════════════════════════════════
    s16 now = TIM_GetCounter(TIM3);     // 当前计数值 (0~65535, 自动回绕)
    s16 delta = now - enc_last;          // 增量: 正=顺时针, 负=逆时针
    enc_last = now;                      // 保存为上次值

    // 读取 OK 按键状态 (0=按下, 1=释放)
    u8 btn_ok = GPIO_ReadInputDataBit(GPIOB, GPIO_Pin_8);

    // ══════════════════════════════════════════════════════════════════
    //  延迟单击机制
    //
    //  问题: 单击和双击的第一次点击无法区分
    //  解决: 第一次点击释放后, 不立即返回 CLICK, 而是等待 400ms
    //        - 400ms 内无第二次点击 → 返回 CLICK (确认是单击)
    //        - 400ms 内有第二次点击 → 返回 DOUBLE_CLICK (见下方)
    //
    //  流程:
    //    按下 → 释放 → pending_click=1, 记录时刻
    //                    ↓ (等待 400ms)
    //                    ├─ 超时无点击 → 返回 EVENT_CLICK
    //                    └─ 再次按下释放 → 返回 EVENT_DOUBLE_CLICK
    // ══════════════════════════════════════════════════════════════════
    if (pending_click) {
        if (sys_tick - pending_click_tick >= DOUBLE_CLICK_MS) {
            // 400ms 超时 → 确认为单击
            pending_click = 0;
            return EVENT_CLICK;
        }
        // 还在等待窗口内, 继续检查是否有新的按键事件
    }

    // ══════════════════════════════════════════════════════════════════
    //  长按检测: 持续按住超过 1000ms
    //
    //  条件: 连续两次轮询 btn_ok 都为 0 (按下状态)
    //  计数: btn_press_ticks 每次轮询 +1 (每次轮询间隔 ≈ 几μs)
    //  注意: 实际精度依赖于主循环频率, 但 1000ms 阈值足够宽松
    //
    //  长按时: 取消待处理的单击 (pending_click=0), 返回 LONG_PRESS
    // ══════════════════════════════════════════════════════════════════
    if (btn_ok == 0 && btn_ok_prev == 0) {
        btn_press_ticks++;
        if (btn_press_ticks > LONG_PRESS_TICKS) {
            // 长按触发
            btn_press_ticks = 0;        // 复位计数 (防止重复触发)
            btn_ok_prev = 0;            // 保持按下状态
            pending_click = 0;          // 取消待处理的单击
            return EVENT_LONG_PRESS;
        }
    }

    // ══════════════════════════════════════════════════════════════════
    //  下降沿检测: 按键刚按下 (1→0 跳变)
    //
    //  此刻只是按下, 还不知道是短按还是长按
    //  初始化按下计数, 等待释放或超时
    // ══════════════════════════════════════════════════════════════════
    if (btn_ok == 0 && btn_ok_prev == 1) {
        btn_press_ticks = 0;            // 开始计时
        btn_ok_prev = 0;                // 更新状态
        return EVENT_NONE;              // 按下不产生事件, 等释放
    }

    // ══════════════════════════════════════════════════════════════════
    //  上升沿检测: 按键刚释放 (0→1 跳变)
    //
    //  此时 btn_press_ticks 记录了按住的持续时间
    //  如果 0 < ticks < 1000ms → 有效短按释放, 进入单击/双击判定
    // ══════════════════════════════════════════════════════════════════
    if (btn_ok == 1 && btn_ok_prev == 0) {
        btn_ok_prev = 1;                // 更新状态为释放
        if (btn_press_ticks > 0 && btn_press_ticks < LONG_PRESS_TICKS) {
            // 有效短按释放 (按住时间 < 1000ms)
            btn_press_ticks = 0;

            if (pending_click) {
                // ── 已有待处理的第一次点击 + 这是第二次释放 → 双击 ──
                pending_click = 0;
                return EVENT_DOUBLE_CLICK;
            }

            // ── 第一次点击释放 → 设置延迟单击, 等待 400ms ──
            pending_click = 1;
            pending_click_tick = sys_tick;  // 记录释放时刻
            return EVENT_NONE;              // 暂不返回事件
        }
        // 按住时间异常 (0 或 >= 1000ms 已被长按处理), 清零
        btn_press_ticks = 0;
    }

    // ══════════════════════════════════════════════════════════════════
    //  旋转事件判定
    //
    //  delta 是 TIM3 计数器的增量 (4x 精度下每格 = ±4)
    //
    //  阈值 ±4 的原因:
    //    - TIM3 工作在 4x 编码器模式, 每转一格产生 4 个计数
    //    - 如果阈值设为 1, 编码器在格边界抖动会产生误触发
    //    - 设为 4 确保必须完整转过一格才触发旋转事件
    //    - 这等效于在硬件滤波之上再加一层软件去抖
    //
    //  delta 范围:
    //    正常旋转: delta = ±4 (转一格)
    //    快速旋转: delta = ±8, ±12, ... (多格跳变)
    //    抖动:     delta = ±1, ±2, ±3 (被过滤)
    // ══════════════════════════════════════════════════════════════════
    if (delta >= 4) return EVENT_CW;       // 顺时针旋转 (delta ≥ 4)
    if (delta <= -4) return EVENT_CCW;      // 逆时针旋转 (delta ≤ -4)

    return EVENT_NONE;                      // 无事件
}
