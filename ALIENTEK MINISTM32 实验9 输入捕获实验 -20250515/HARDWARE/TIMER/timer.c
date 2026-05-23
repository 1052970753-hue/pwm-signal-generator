/*
 * timer.c — 定时器底层驱动
 * ══════════════════════════════════════════════════════════════
 *
 * 【定时器资源分配】
 *   TIM1 CH1 (PA8): PWM 输出 — 高级定时器 (APB2 总线)
 *     - 优点: 支持互补输出、死区插入 (本项目未使用)
 *     - 特点: 必须使能 MOE (主输出使能) 位, 否则 PWM 无输出
 *     - 用途: CH1 PWM 信号输出
 *
 *   TIM4 CH1 (PB6): PWM 输出 — 通用定时器 (APB1 总线)
 *     - 无 MOE 要求, 配置更简单
 *     - 用途: CH2 PWM 信号输出
 *
 *   TIM2 CH1 (PA0): 输入捕获 — 通用定时器 (APB1 总线)
 *     - 测量 FG 信号高电平脉宽
 *     - 时基 1MHz (1us 分辨率), ARR=0xFFFF
 *     - 上升沿/下降沿交替捕获, 差值即脉宽
 *
 * 【PWM 频率计算公式】
 *   freq = 72MHz / (PSC+1) / (ARR+1)
 *   例如: PSC=71, ARR=999 → freq = 72M / 72 / 1000 = 1000 Hz
 *   占空比: duty = CCR1 / (ARR+1) * 100%
 *   例如: CCR1=500, ARR=999 → duty = 500/1000 = 50%
 *
 * 【fg_period_us 全局变量】
 *   存储最近一次捕获到的高电平脉宽 (微秒)
 *   由 TIM2 中断服务函数更新, main.c 读取后计算 RPM
 *   值为 0 表示无信号输入
 */
#include "timer.h"

/* 全局变量: 捕获到的高电平脉宽 (微秒)
 * - 由 TIM2_IRQHandler() 在每次下降沿捕获时更新
 * - 由 main.c 读取并转换为 RPM
 * - volatile 修饰: 防止编译器优化, 因为 ISR 和 main 都会访问
 * - 值为 0 表示未捕获到有效信号 (无 FG 输入)
 */
volatile u32 fg_period_us = 0;

/* ════════════════════════════════════════════════════════════
 *  TIM1 CH1 PWM 初始化 — PA8 (高级定时器)
 * ════════════════════════════════════════════════════════════
 *  参数:
 *    arr: 自动重装值 (ARR), 决定 PWM 周期 = (arr+1) 个计数周期
 *    psc: 预分频系数, 计数时钟 = 72MHz / (psc+1)
 *  频率: freq = 72MHz / (psc+1) / (arr+1)
 *
 *  【重要: MOE 使能】
 *    TIM1 是高级定时器, 默认 MOE=0 (主输出关闭)
 *    必须调用 TIM_CtrlPWMOutputs(TIM1, ENABLE) 才能在 PA8 输出 PWM
 *    如果忘记这一步, PA8 将保持低电平, 看似"没有 PWM 输出"
 */
void TIM1_PWM_Init(u16 arr, u16 psc) {
    GPIO_InitTypeDef GPIO_InitStructure;
    TIM_TimeBaseInitTypeDef TIM_TimeBaseStructure;
    TIM_OCInitTypeDef TIM_OCInitStructure;

    // 开启 TIM1 和 GPIOA 时钟 (TIM1 挂在 APB2 总线)
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_TIM1 | RCC_APB2Periph_GPIOA, ENABLE);

    /* ── GPIO 配置: PA8 复用推挽输出 ──
     * AF_PP = Alternate Function Push-Pull
     * GPIO 由 TIM1_CH1 控制, 而非 GPIO 寄存器直接控制
     */
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_8;              // PA8
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;        // 复用推挽输出
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;      // 50MHz 翻转速度
    GPIO_Init(GPIOA, &GPIO_InitStructure);

    /* ── 时基配置 ──
     * TIM_Period (ARR): 自动重装值, 计数器从 0 计到 ARR 后产生更新事件并回绕
     * TIM_Prescaler (PSC): 预分频, 72MHz/(PSC+1) 得到计数时钟
     * TIM_ClockDivision: 时钟分频 (本项目不使用, 设为 0)
     * TIM_CounterMode: 向上计数模式
     * 例: PSC=71, ARR=999 → 72M/72/1000 = 1kHz PWM
     */
    TIM_TimeBaseStructure.TIM_Period = arr;                // 自动重装值
    TIM_TimeBaseStructure.TIM_Prescaler = psc;             // 预分频
    TIM_TimeBaseStructure.TIM_ClockDivision = 0;           // 不分频
    TIM_TimeBaseStructure.TIM_CounterMode = TIM_CounterMode_Up;  // 向上计数
    TIM_TimeBaseInit(TIM1, &TIM_TimeBaseStructure);

    /* ── 输出比较配置: PWM1 模式 ──
     * TIM_OCMode_PWM1: CNT < CCR1 时输出有效电平 (High), CNT >= CCR1 时无效
     * TIM_Pulse (CCR1): 比较值, 决定占空比 = CCR1/(ARR+1)
     *   初始值为 0, 即占空比 0% (全低)
     * TIM_OCPolarity_High: 有效电平为高
     */
    TIM_OCInitStructure.TIM_OCMode = TIM_OCMode_PWM1;      // PWM 模式 1
    TIM_OCInitStructure.TIM_OutputState = TIM_OutputState_Enable;  // 输出使能
    TIM_OCInitStructure.TIM_Pulse = 0;                     // 初始占空比 0%
    TIM_OCInitStructure.TIM_OCPolarity = TIM_OCPolarity_High;     // 高电平有效
    TIM_OC1Init(TIM1, &TIM_OCInitStructure);                // 初始化 CH1

    /* ── 高级定时器特殊配置 ──
     * TIM_CtrlPWMOutputs: 使能 MOE (Main Output Enable) 位
     * 这是高级定时器 (TIM1/TIM8) 独有的要求
     * 如果不调用此函数, PA8 不会输出 PWM 信号
     */
    TIM_CtrlPWMOutputs(TIM1, ENABLE);                      // *** 关键: 使能 MOE ***

    TIM_OC1PreloadConfig(TIM1, TIM_OCPreload_Enable);      // 使能 CCR1 预装载 (写入后下次更新事件生效)
    TIM_ARRPreloadConfig(TIM1, ENABLE);                    // 使能 ARR 预装载
    TIM_Cmd(TIM1, ENABLE);                                 // 启动定时器
}

/* ════════════════════════════════════════════════════════════
 *  TIM4 CH1 PWM 初始化 — PB6 (通用定时器)
 * ════════════════════════════════════════════════════════════
 *  与 TIM1 配置几乎相同, 区别:
 *    1. TIM4 挂在 APB1 总线 (注意 RCC 时钟不同)
 *    2. 通用定时器无需 TIM_CtrlPWMOutputs (无 MOE 位)
 *    3. 输出引脚为 PB6
 */
void TIM4_PWM_Init(u16 arr, u16 psc) {
    GPIO_InitTypeDef GPIO_InitStructure;
    TIM_TimeBaseInitTypeDef TIM_TimeBaseStructure;
    TIM_OCInitTypeDef TIM_OCInitStructure;

    RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM4, ENABLE);   // TIM4 在 APB1 总线
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE);   // GPIOB 在 APB2 总线

    /* ── GPIO 配置: PB6 复用推挽输出 ── */
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_6;              // PB6
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;        // 复用推挽输出
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOB, &GPIO_InitStructure);

    /* ── 时基配置 (与 TIM1 相同) ── */
    TIM_TimeBaseStructure.TIM_Period = arr;
    TIM_TimeBaseStructure.TIM_Prescaler = psc;
    TIM_TimeBaseStructure.TIM_ClockDivision = 0;
    TIM_TimeBaseStructure.TIM_CounterMode = TIM_CounterMode_Up;
    TIM_TimeBaseInit(TIM4, &TIM_TimeBaseStructure);

    /* ── 输出比较配置: PWM1 模式 (与 TIM1 相同) ── */
    TIM_OCInitStructure.TIM_OCMode = TIM_OCMode_PWM1;
    TIM_OCInitStructure.TIM_OutputState = TIM_OutputState_Enable;
    TIM_OCInitStructure.TIM_Pulse = 0;                     // 初始占空比 0%
    TIM_OCInitStructure.TIM_OCPolarity = TIM_OCPolarity_High;
    TIM_OC1Init(TIM4, &TIM_OCInitStructure);

    // 通用定时器无需 TIM_CtrlPWMOutputs — 这是与 TIM1 的关键区别
    TIM_OC1PreloadConfig(TIM4, TIM_OCPreload_Enable);      // 使能 CCR1 预装载
    TIM_ARRPreloadConfig(TIM4, ENABLE);                    // 使能 ARR 预装载
    TIM_Cmd(TIM4, ENABLE);                                 // 启动定时器
}

/* ════════════════════════════════════════════════════════════
 *  TIM2 CH1 输入捕获初始化 — PA0
 * ════════════════════════════════════════════════════════════
 *  功能: 测量 FG 信号的高电平脉宽
 *  原理: 上升沿触发时记录计数值, 下降沿触发时再记录, 差值即为高电平脉宽
 *  精度: 1MHz 计数时钟 = 1us 分辨率
 *
 *  引脚: PA0 (TIM2_CH1) — 下拉输入, 默认低电平
 *  滤波: ICFilter=0x03, 连续 4 个采样一致才确认边沿, 防止噪声误触发
 *  中断: CC1 中断, 优先级 1
 */

/* ── 捕获状态变量 ──
 * cap_stage: 当前捕获阶段
 *   0 = 等待上升沿 (下一个边沿是上升沿)
 *   1 = 等待下降沿 (已经捕获到上升沿, 等下降沿)
 * cap_rise_val: 上升沿时刻的计数器值 (用于与下降沿值做差)
 *   static 限制作用域, 仅 TIM2_IRQHandler 使用
 */
static u8  cap_stage = 0;       // 0=等待上升沿, 1=等待下降沿
static u16 cap_rise_val = 0;    // 上升沿时刻的计数值

void TIM2_Cap_Init(u16 arr, u16 psc) {
    GPIO_InitTypeDef GPIO_InitStructure;
    TIM_TimeBaseInitTypeDef TIM_TimeBaseStructure;
    TIM_ICInitTypeDef TIM_ICInitStructure;
    NVIC_InitTypeDef NVIC_InitStructure;

    RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM2, ENABLE);   // TIM2 在 APB1
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE);   // GPIOA 在 APB2

    /* ── GPIO 配置: PA0 下拉输入 ──
     * IPD = Input Pull-Down
     * 空闲时 PA0 被内部下拉电阻拉到 GND, 确保无信号时为确定的低电平
     * 防止浮空输入导致随机误触发
     */
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_0;              // PA0 (TIM2_CH1)
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPD;          // 下拉输入
    GPIO_Init(GPIOA, &GPIO_InitStructure);
    GPIO_ResetBits(GPIOA, GPIO_Pin_0);                     // 确保初始为低

    /* ── 时基配置 ──
     * TIM_Period = 0xFFFF (ARR 最大值), 防止在两次捕获间计数器溢出
     * TIM_Prescaler = 71, 72MHz/72 = 1MHz = 1us 分辨率
     * TIM_ClockDivision = TIM_CKD_DIV1, 不做额外分频
     * 向上计数模式, 从 0 计到 0xFFFF 后回绕到 0
     */
    TIM_TimeBaseStructure.TIM_Period = arr;                // 通常 0xFFFF
    TIM_TimeBaseStructure.TIM_Prescaler = psc;             // 通常 71 (1MHz)
    TIM_TimeBaseStructure.TIM_ClockDivision = TIM_CKD_DIV1;
    TIM_TimeBaseStructure.TIM_CounterMode = TIM_CounterMode_Up;
    TIM_TimeBaseInit(TIM2, &TIM_TimeBaseStructure);

    /* ── 输入捕获配置 ──
     * TIM_Channel_1:      使用 CH1 (对应 PA0)
     * TIM_ICPolarity_Rising: 初始捕获上升沿
     * TIM_ICSelection_DirectTI: 直连 (非交叉)
     * TIM_ICPSC_DIV1:     每个边沿都捕获 (不分频)
     * TIM_ICFilter = 0x03: 数字滤波, 连续 4 次采样一致才确认
     *   滤波值 0x03 = fSAMPLING=fDTS/2, N=4
     *   可有效滤除 <2us 的毛刺噪声
     */
    TIM_ICInitStructure.TIM_Channel = TIM_Channel_1;              // CH1
    TIM_ICInitStructure.TIM_ICPolarity = TIM_ICPolarity_Rising;   // 初始捕获上升沿
    TIM_ICInitStructure.TIM_ICSelection = TIM_ICSelection_DirectTI;  // 直连
    TIM_ICInitStructure.TIM_ICPrescaler = TIM_ICPSC_DIV1;        // 不分频
    TIM_ICInitStructure.TIM_ICFilter = 0x03;                     // 4 次采样滤波
    TIM_ICInit(TIM2, &TIM_ICInitStructure);

    /* ── NVIC 中断配置 ──
     * 抢占优先级 1 (低于 USART1 的 3, 但高于一般外设)
     * 使能 TIM2 全局中断
     */
    NVIC_InitStructure.NVIC_IRQChannel = TIM2_IRQn;             // TIM2 中断通道
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 1;   // 抢占优先级
    NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;             // 使能
    NVIC_Init(&NVIC_InitStructure);

    TIM_ITConfig(TIM2, TIM_IT_CC1, ENABLE);    // 使能 CC1 中断 (捕获/比较 1 中断)
    TIM_Cmd(TIM2, ENABLE);                     // 启动 TIM2
}

/* ════════════════════════════════════════════════════════════
 *  TIM2 中断服务函数 — 测量高电平脉宽
 * ════════════════════════════════════════════════════════════
 *
 *  工作流程 (上升沿/下降沿交替捕获):
 *
 *    ┌──────────────────────────────────────────────┐
 *    │  FG 信号:    ____|‾‾‾‾‾‾‾‾‾‾‾‾|____|‾‾‾‾  │
 *    │  cap_stage:  0    1           0    1       │
 *    │              ↑    ↓           ↑    ↓       │
 *    │           上升沿  下降沿    上升沿  下降沿   │
 *    └──────────────────────────────────────────────┘
 *
 *    阶段 0 (cap_stage=0): 捕获到上升沿
 *      - 记录当前计数值到 cap_rise_val
 *      - 切换 cap_stage = 1
 *      - 将触发极性改为下降沿
 *
 *    阶段 1 (cap_stage=1): 捕获到下降沿
 *      - 读取当前计数值 fall_val
 *      - 计算脉宽 = fall_val - cap_rise_val (单位: us)
 *      - 如果 fall_val < cap_rise_val (计数器溢出回绕):
 *        脉宽 = 0xFFFF - cap_rise_val + fall_val
 *      - 将脉宽存入 fg_period_us 全局变量
 *      - 切换 cap_stage = 0
 *      - 将触发极性改回升沿, 等待下一个周期
 *
 *  【计数器溢出回绕处理】
 *    TIM2 计数器为 16 位 (0~0xFFFF), 如果脉宽超过 65.535ms (65535us)
 *    计数器会在两次捕获之间溢出回绕 (0xFFFF → 0)
 *    此时 fall_val < cap_rise_val, 需要用特殊公式计算:
 *      脉宽 = (0xFFFF - cap_rise_val) + fall_val + 1
 *    简化为: 0xFFFF - cap_rise_val + fall_val (忽略 +1 的微小误差)
 */
void TIM2_IRQHandler(void) {
    if (TIM_GetITStatus(TIM2, TIM_IT_CC1) != RESET) {      // 检查 CC1 中断标志
        TIM_ClearITPendingBit(TIM2, TIM_IT_CC1);            // 清除中断标志 (必须手动清除)

        if (cap_stage == 0) {
            /* ── 阶段 0: 捕获到上升沿 ── */
            cap_rise_val = TIM_GetCapture1(TIM2);           // 记录上升沿时刻的计数值
            cap_stage = 1;                                   // 切换到等待下降沿
            // 切换触发极性: 上升沿 → 下降沿
            TIM_OC1PolarityConfig(TIM2, TIM_ICPolarity_Falling);
        } else {
            /* ── 阶段 1: 捕获到下降沿 ── */
            u16 fall_val = TIM_GetCapture1(TIM2);           // 记录下降沿时刻的计数值
            if (fall_val >= cap_rise_val) {
                /* 正常情况: 计数器未溢出 */
                fg_period_us = fall_val - cap_rise_val;     // 脉宽 = 下降沿值 - 上升沿值
            } else {
                /* 计数器溢出回绕: 下降沿值 < 上升沿值 */
                // 例如: rise=0xFFF0, fall=0x0010, 实际脉宽 = (0xFFFF-0xFFF0) + 0x0010 = 0x001F
                fg_period_us = 0xFFFF - cap_rise_val + fall_val;
            }
            cap_stage = 0;                                   // 切换到等待上升沿
            // 切换触发极性: 下降沿 → 上升沿
            TIM_OC1PolarityConfig(TIM2, TIM_ICPolarity_Rising);
        }
    }
}
