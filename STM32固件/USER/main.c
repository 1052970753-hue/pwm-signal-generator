/*
 * main.c — PWM 信号发生器主程序
 * ══════════════════════════════════════════════════════════════════════
 *  系统时钟: 72MHz (8MHz HSE × 9 PLL)
 *  SysTick:  1ms 中断 (系统时间基准), 由 Cortex-M3 内核 SysTick 定时器产生
 *
 *  ┌─────────────────────────────────────────────────────────────────┐
 *  │  主循环架构 — 5 个并发任务, 基于时间片轮询 (非 RTOS)            │
 *  │                                                                 │
 *  │  任务 1: 串口协议处理      — 每次循环都执行 (无周期限制)         │
 *  │  任务 2: 编码器轮询+菜单    — 每次循环都执行 (实时响应旋钮)      │
 *  │  任务 3: 测试状态机         — 每次循环都执行 (内部按秒计时)       │
 *  │  任务 4: OLED 渲染          — 50ms 周期 (20fps, 避免闪屏)       │
 *  │  任务 5: 状态上报           — 500ms 周期 (串口心跳包)            │
 *  └─────────────────────────────────────────────────────────────────┘
 *
 *  测试状态机 (自动 ON/OFF 循环):
 *  ┌────────┐   启动   ┌────────┐  ON时间到  ┌────────┐  OFF时间到  ┌────────┐
 *  │ phase0 │ ───────→ │ phase1 │ ─────────→ │ phase0 │ ─────────→ │ phase1 │ ...
 *  │  OFF   │          │  ON    │  (记录数据) │  OFF   │            │  ON    │
 *  └────────┘          └────────┘            └────────┘            └────────┘
 *       │                                       │                      │
 *       │                                   循环次数到?             循环次数到?
 *       │                                       ↓                      ↓
 *       │                                  ┌────────┐             ┌────────┐
 *       └──────────────────────────────────│ phase2 │ ←───────────│        │
 *              外部停止                     │  done  │             │        │
 *                                          └────────┘             └────────┘
 */
#include "stm32f10x.h"
#include "delay.h"
#include "usart.h"
#include "oled.h"
#include "Encoder.h"
#include "../HARDWARE/APP/menu_defs.h"
#include "../HARDWARE/APP/pwm_engine.h"
#include "../HARDWARE/APP/fg_capture.h"
#include "../HARDWARE/APP/ui_render.h"
#include "../HARDWARE/APP/menu.h"
#include "../HARDWARE/APP/protocol.h"

// ══════════════════════════════════════════════════════════════════════
//  全局变量
// ══════════════════════════════════════════════════════════════════════

static u16 rpm = 0;                 // 当前 RPM 值 (每 50ms 由 FG_CalculateRPM 更新一次)

/*
 * sys_tick — 系统滴答计数器, 1ms 精度
 *
 * 工作原理:
 *   - SysTick_Config(SystemCoreClock/1000) 配置内核定时器每 1ms 产生一次中断
 *   - 中断服务函数 SysTick_Handler (在 stm32f10x_it.c 中) 对此变量执行 ++ 操作
 *   - 主循环中通过 sys_tick 的差值来判断各任务的时间间隔
 *
 * 注意: 声明为 volatile, 因为它在中断中被修改, 主循环中读取
 *       编译器不得对其做优化 (如缓存到寄存器)
 */
volatile u32 sys_tick = 0;

static u8 blink_flag = 0;           // 光标闪烁标志位: 0=灭, 1=亮 (每 400ms 翻转一次)
static u32 blink_tick = 0;          // 上次闪烁翻转时的 sys_tick 值

// ══════════════════════════════════════════════════════════════════════
//  测试状态机变量
// ══════════════════════════════════════════════════════════════════════
//
//  test_phase 阶段定义:
//    0 = OFF 阶段 (PWM 关闭, 等待冷却/初始等待)
//    1 = ON  阶段 (PWM 输出, 采样 RPM 数据)
//    2 = done     (所有循环完成, 不再动作, 等待用户手动停止测试)
//
static u8  test_phase = 0;          // 当前阶段 (0/1/2)
static u8  test_current_cycle = 0;  // 当前已完成的循环计数 (从 1 开始递增)
static u32 test_phase_start = 0;    // 当前阶段开始时的 sys_tick (ms), 用于计算 elapsed
static u16 test_rpm_max = 0;        // ON 阶段采样到的最大 RPM (用于判断启动是否成功)
static u32 test_rpm_sum = 0;        // ON 阶段所有 RPM 采样值的累加和 (用于求平均)
static u16 test_rpm_count = 0;      // ON 阶段的采样次数 (用于求平均: 平均值 = sum / count)

// ── 根据目标频率计算理论目标 RPM ──
// 公式: RPM = (频率_Hz / 分频系数) × 60 / 每转脉冲数
// 此处简化: freq_hz × 60 / pulses_per_rev (分频已在 FG 端处理)
static u16 calc_target_rpm(u32 freq_hz) {
    return (u16)((freq_hz * 60) / g_params.fg_pulses_per_rev);
}

// ── 启动一个测试周期的 ON 阶段 ──
// 清零统计变量, 设置 PWM 输出, 记录阶段开始时间
static void test_start_cycle(void) {
    test_phase = 1;                 // 进入 ON 阶段
    test_phase_start = sys_tick;    // 记录 ON 起始时刻
    test_rpm_max = 0;               // 重置最大 RPM
    test_rpm_sum = 0;               // 重置累加和
    test_rpm_count = 0;             // 重置采样计数

    // 获取测试通道号并设置 PWM 频率和占空比, 然后开启输出
    u8 ch = Menu_GetTestChannel();
    PWM_SetChannel(ch, Menu_GetTestFreq(), Menu_GetTestDuty());
    PWM_EnableChannel(ch, 1);
}

// ── 测试状态机主处理函数 (每次主循环都调用) ──
//
// 状态转换逻辑:
//   1. 如果测试未运行 → 确保 PWM 关闭, phase 复位为 0
//   2. phase==2 (done) → 什么都不做, 等待外部停止
//   3. phase==1 (ON)  → 每次循环采样 RPM (累加到 sum/count)
//                        ON 时间到达后:
//                          - 计算平均 RPM = sum / count
//                          - 判断异常 (见下方异常判定)
//                          - 记录本轮数据到菜单
//                          - 关闭 PWM, 切回 OFF 阶段
//   4. phase==0 (OFF) → OFF 冷却时间到达后:
//                          - 循环次数未到 → 启动下一轮 ON
//                          - 循环次数已到 → 切到 phase=2 (done)
static void test_handle_tick(void) {
    // ── 测试未运行: 安全关闭并复位状态机 ──
    if (!Menu_IsTestRunning()) {
        if (test_phase != 0) {
            test_phase = 0;
            u8 ch = Menu_GetTestChannel();
            PWM_EnableChannel(ch, 0);   // 确保 PWM 输出关闭
        }
        return;
    }

    if (test_phase == 2) return;  // 已完成, 静默等待外部停止

    u32 elapsed = sys_tick - test_phase_start;  // 当前阶段已过时间 (ms)

    if (test_phase == 1) {
        // ══════════════════════════════════════════════════════════════
        //  ON 阶段 — 每 50ms (主循环 OLED 渲染周期) 采样一次 RPM
        // ══════════════════════════════════════════════════════════════
        u16 current_rpm = FG_CalculateRPM(g_params.fg_div, g_params.fg_pulses_per_rev);

        // 更新最大 RPM (用于判断电机是否成功启动)
        if (current_rpm > test_rpm_max) test_rpm_max = current_rpm;

        // 累加 RPM 用于后续求平均
        test_rpm_sum += current_rpm;
        test_rpm_count++;

        // 检查 ON 阶段是否结束
        u32 on_ms = (u32)Menu_GetTestOnSec() * 1000;
        if (elapsed >= on_ms) {
            // ── ON 时间到达, 本轮采样结束 ──

            // 计算 ON 阶段的平均 RPM
            u16 rpm_avg = test_rpm_count > 0 ? (u16)(test_rpm_sum / test_rpm_count) : 0;

            // 计算理论目标 RPM (根据 PWM 频率和编码器每转脉冲数)
            u16 target = calc_target_rpm(Menu_GetTestFreq());

            // ══════════════════════════════════════════════════════════
            //  异常判定 — 两个条件任一满足即标记 error=1:
            //    条件 1: 平均 RPM == 0 → 传感器无信号或电机未转
            //    条件 2: 偏差 > 20%  → |rpm_avg - target| / target > 0.2
            //            例如 target=1000, rpm_avg=1250 → diff=250, 250/1000=25% > 20% → error
            // ══════════════════════════════════════════════════════════
            u8 error = 0;
            if (rpm_avg == 0) {
                error = 1;  // RPM 为零, 绝对异常
            } else if (target > 0) {
                u32 diff = rpm_avg > target ? rpm_avg - target : target - rpm_avg;
                if (diff * 100 / target > 20) error = 1;  // 偏差超过 20%
            }

            // 启动成功判定: ON 阶段内任意时刻 RPM > 0 即视为启动成功
            u8 startup_ok = (test_rpm_max > 0) ? 1 : 0;

            // 记录本轮测试数据 (cycle, target, max, avg, error, startup_ok)
            test_current_cycle++;
            Menu_AddTestRecord(test_current_cycle, target, test_rpm_max, rpm_avg, error, startup_ok);

            // 关闭 PWM, 切回 OFF 阶段 (等待冷却)
            u8 ch = Menu_GetTestChannel();
            PWM_EnableChannel(ch, 0);
            test_phase = 0;             // 回到 OFF
            test_phase_start = sys_tick; // 记录 OFF 起始时刻
        }
    } else {
        // ══════════════════════════════════════════════════════════════
        //  OFF 阶段 — 等待冷却时间
        // ══════════════════════════════════════════════════════════════
        u32 off_ms = (u32)Menu_GetTestOffSec() * 1000;
        if (elapsed >= off_ms) {
            // OFF 冷却时间到达
            if (test_current_cycle >= Menu_GetTestCycles()) {
                // ── 所有循环已完成 → 进入 done 阶段 ──
                test_phase = 2;
                Menu_TestDone();    // 通知菜单测试结束
            } else {
                // ── 还有循环未完成 → 启动下一轮 ON ──
                test_start_cycle();
            }
        }
    }
}

// ══════════════════════════════════════════════════════════════════════
//  main() — 系统入口
// ══════════════════════════════════════════════════════════════════════
int main(void) {
    // ── 系统时钟初始化 ──
    // SystemInit() 由 startup 代码调用, 配置 HSE(8MHz) + PLL(×9) = 72MHz
    SystemInit();

    // ── SysTick 定时器配置 ──
    // SystemCoreClock = 72000000, 除以 1000 得到 72000
    // 即每 72000 个时钟周期触发一次中断 → 1ms 间隔
    // 中断服务函数 SysTick_Handler 会递增 sys_tick 变量
    SysTick_Config(SystemCoreClock / 1000);

    // ── 外设初始化 ──
    uart_init(115200);          // USART1: PA9(TX)/PA10(RX), 波特率 115200
    OLED_Init();                // OLED: PA1(SDA)/PA2(SCL), 软件 I2C
    OLED_ColorTurn(0);          // 正常颜色显示 (非反色)
    OLED_DisplayTurn(0);        // 正常方向显示 (非翻转)
    Encoder_Init();             // 编码器: PA6(A)/PA7(B) TIM3 编码器模式 + PB8(OK) GPIO
    PWM_Init();                 // PWM: PA8(TIM1_CH1) + PB6(TIM4_CH1), 默认 1kHz/0%
    FG_Init();                  // 频率计: PA0(TIM2_CH1) 输入捕获, 1us 分辨率
    Protocol_Init();            // 串口协议: 帧解析状态机初始化
    Menu_Init();                // 菜单: 恢复默认参数, 初始化状态

    // ── 主循环控制变量 ──
    u32 last_render = 0;        // 上次 OLED 渲染时刻 (50ms 周期)
    u32 last_status = 0;        // 上次状态上报时刻 (500ms 周期)
    u8  prev_test_running = 0;  // 上一次测试运行状态 (用于检测上升沿启动)

    // ══════════════════════════════════════════════════════════════════
    //  主循环 — 裸机轮询架构, 5 个并发任务
    // ══════════════════════════════════════════════════════════════════
    while (1) {

        // ┌─────────────────────────────────────────────────────────┐
        // │  任务 1: 串口协议处理 (每次循环执行)                     │
        // │  处理上位机下发的命令帧 (读参数/写参数/开始测试等)       │
        // └─────────────────────────────────────────────────────────┘
        Protocol_Process();

        // CSV 导出: 如果有待导出数据且导出未完成, 每次循环发送一块
        if (!Menu_ExportDone() && Menu_GetExportCount() > 0) {
            Protocol_ProcessExport();
        }

        // ┌─────────────────────────────────────────────────────────┐
        // │  任务 2: 编码器轮询 + 菜单事件处理 (每次循环执行)       │
        // │  传递 sys_tick 给编码器模块用于长按/双击计时             │
        // │  有事件时触发菜单处理, dirty 标志表示参数已修改          │
        // └─────────────────────────────────────────────────────────┘
        Encoder_SetTick(sys_tick);
        InputEvent ev = (InputEvent)Encoder_Poll();
        if (ev != EVENT_NONE) {
            Menu_Process(ev);               // 菜单处理输入事件 (导航/确认/返回等)
            if (g_menu.dirty) {
                // 参数已修改 → 同步更新 PWM 输出
                PWM_UpdateFromParams(&g_params);
                g_menu.dirty = 0;
            }
        }

        // ┌─────────────────────────────────────────────────────────┐
        // │  任务 3: 测试状态机 (每次循环执行, 内部按秒计时)        │
        // │  检测测试启动的上升沿: 从未运行→运行 时触发启动序列     │
        // └─────────────────────────────────────────────────────────┘
        u8 now_running = Menu_IsTestRunning();
        if (now_running && !prev_test_running) {
            // 检测到启动上升沿 → 初始化循环计数, 启动第一个 ON 阶段
            test_current_cycle = 0;
            test_start_cycle();
        }
        prev_test_running = now_running;

        test_handle_tick();                 // 状态机主处理

        // ┌─────────────────────────────────────────────────────────┐
        // │  光标闪烁 (400ms 翻转周期 = 2Hz 闪烁)                   │
        // │  用于菜单编辑模式下指示当前选中的参数位                  │
        // └─────────────────────────────────────────────────────────┘
        if (sys_tick - blink_tick >= 400) {
            blink_tick = sys_tick;
            blink_flag = !blink_flag;       // 0↔1 翻转
        }

        // ┌─────────────────────────────────────────────────────────┐
        // │  任务 4: OLED 渲染 (50ms 周期 = 20fps)                  │
        // │  读取当前 RPM → 调用 UI_Render 刷新屏幕                 │
        // │  20fps 是软件 I2C OLED 的合理帧率, 再高会闪屏           │
        // └─────────────────────────────────────────────────────────┘
        if (sys_tick - last_render >= 50) {
            last_render = sys_tick;
            rpm = FG_CalculateRPM(g_params.fg_div, g_params.fg_pulses_per_rev);
            UI_Render(&g_params, rpm, blink_flag);
        }

        // ┌─────────────────────────────────────────────────────────┐
        // │  任务 5: 状态上报 (500ms 周期 = 2Hz 心跳包)             │
        // │  向上位机发送当前系统状态 (频率/占空比/RPM/测试进度)     │
        // └─────────────────────────────────────────────────────────┘
        if (sys_tick - last_status >= 500) {
            last_status = sys_tick;

            // 填充状态数据结构
            StatusData sd;
            sd.ch1_freq_hz   = g_params.ch1_freq_hz;
            sd.ch1_duty_pct  = g_params.ch1_duty_pct;
            sd.ch1_enabled   = g_params.ch1_enabled;
            sd.ch2_freq_hz   = g_params.ch2_freq_hz;
            sd.ch2_duty_pct  = g_params.ch2_duty_pct;
            sd.ch2_enabled   = g_params.ch2_enabled;
            sd.fg_freq_mhz   = FG_GetFrequency_mHz();  // 实测频率 (毫赫兹)
            sd.fg_div        = g_params.fg_div;
            sd.rpm           = rpm;                      // 最新 RPM 值
            sd.mode          = g_menu.mode;              // 当前菜单模式
            sd.test_state    = now_running ? TEST_RUNNING : TEST_IDLE;
            sd.test_cycle    = test_current_cycle;       // 已完成循环数
            sd.test_total    = Menu_GetTestCycles();     // 总循环数
            Protocol_SendStatus(&sd);
        }
    }
}
