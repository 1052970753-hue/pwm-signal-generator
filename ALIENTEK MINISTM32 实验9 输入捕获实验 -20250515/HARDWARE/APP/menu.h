/**
 * ============================================================================
 *  menu.h — 5 模式菜单状态机接口
 * ============================================================================
 *
 *  【模块概述】
 *   本模块是系统的"大脑", 实现了一个基于模式切换的菜单状态机.
 *   用户通过旋转编码器(选择项/调数值)和OK按键(确认/切换)操作菜单,
 *   菜单处理后将参数变更同步到 PWM 引擎和 OLED 显示.
 *
 *  【5 种应用模式】
 *   MODE_PWM_FG — 双通道PWM + 频率计(RPM): 主模式, 同时输出两路PWM并测量转速
 *   MODE_FG     — 纯频率计模式: 仅测量RPM, 不输出PWM
 *   MODE_CH1    — CH1 单通道设置: 调节频率/占空比/使能
 *   MODE_CH2    — CH2 单通道设置: 调节频率/占空比/使能
 *   MODE_TEST   — 自动测试模式: PC配置参数后自动执行加速/稳速/减速循环
 *
 *  【数据流 (输入事件 → 菜单处理 → PWM 更新)】
 *
 *   ┌───────────┐     ┌──────────────┐     ┌────────────┐     ┌──────────┐
 *   │ 旋转编码器 │────▶│ Menu_Process │────▶│ g_params   │────▶│ PWM引擎  │
 *   │ OK 按键    │     │ (状态机逻辑) │     │ (全局参数) │     │ 硬件输出 │
 *   └───────────┘     └──────────────┘     └────────────┘     └──────────┘
 *          │                  │                    │                 │
 *          │                  ▼                    │                 ▼
 *          │         ┌──────────────┐              │          ┌──────────┐
 *          │         │ g_menu 菜单  │              │          │ PA8/PB6  │
 *          │         │ 状态(光标等) │              │          │ PWM 波形 │
 *          │         └──────────────┘              │          └──────────┘
 *          │                                       │
 *          │                                       ▼
 *          │                                ┌────────────┐
 *          │                                │ UI_Render  │
 *          │                                │ OLED 刷新  │
 *          │                                └────────────┘
 *
 *   详细路径:
 *    1. Encoder_Poll() 检测到旋转/按下 → 产生 InputEvent
 *    2. Menu_Process(ev) 根据当前模式和光标位置修改 g_params / g_menu
 *    3. 主循环调用 PWM_UpdateFromParams(&g_params) 将参数写入硬件
 *    4. 主循环调用 UI_Render() 将当前状态显示到 OLED
 *
 *  【核心函数说明】
 *   Menu_Init()      — 初始化所有默认参数 (频率1kHz, 占空比50%, 使能ON)
 *   Menu_Process(ev) — 菜单核心处理函数, 每个主循环调用一次
 *                      根据编码器事件(旋转CW/CCW, 按下ENTER)更新菜单状态
 *
 *  【测试控制函数说明】
 *   Menu_StartTest()        — 启动自动测试 (清空记录, 进入运行状态)
 *   Menu_StopTest()         — 强制停止测试
 *   Menu_SetTestConfig(cfg) — 由PC端通过串口下发测试参数
 *   Menu_AddTestRecord()    — 每个测试周期结束时添加记录
 *   Menu_TestDone()         — 全部周期完成后的回调 (自动触发CSV导出)
 *
 *  【CSV 导出函数说明】
 *   Menu_GetExportCount()     — 获取待导出记录总数
 *   Menu_FormatExportChunk()  — 格式化一条CSV记录到缓冲区
 *   Menu_ExportDone()         — 检查所有记录是否已导出完毕
 *
 *  【全局变量说明】
 *   g_params     — 系统参数结构体 (频率/占空比/使能/分频等), 所有模块共享
 *   g_menu       — 菜单状态 (当前模式/光标位置/编辑标志/子菜单等)
 *   test_running — 测试是否正在运行 (1=运行中, 0=停止)
 *
 * ============================================================================
 */
#ifndef MENU_H
#define MENU_H

#include "sys.h"
#include "menu_defs.h"

/* ── 核心接口 ─────────────────────────────────────────────────────────── */

// 菜单初始化: 设置所有参数为默认值, 初始化菜单状态
// 调用时机: 系统启动时, 在所有外设初始化之后调用
void Menu_Init(void);

// 菜单事件处理: 接收输入事件, 根据当前模式和光标位置更新状态
// 参数 ev: 输入事件类型 (见 menu_defs.h 中的枚举定义)
//   - INPUT_CW:    编码器顺时针旋转 (数值增大/光标下移)
//   - INPUT_CCW:   编码器逆时针旋转 (数值减小/光标上移)
//   - INPUT_ENTER: 编码器按下确认 (进入编辑/确认选择/切换模式)
// 调用时机: 主循环中每帧调用一次
void Menu_Process(InputEvent ev);

/* ── 测试控制接口 ─────────────────────────────────────────────────────── */

// 查询测试是否正在运行
// 返回值: 1=测试进行中, 0=空闲
u8  Menu_IsTestRunning(void);

// 获取当前测试的目标通道 (0=CH1, 1=CH2)
u8  Menu_GetTestChannel(void);

// 获取当前测试的目标频率 (Hz)
u32 Menu_GetTestFreq(void);

// 获取当前测试的目标占空比 (%)
u8  Menu_GetTestDuty(void);

// 获取当前测试的总循环次数
u16 Menu_GetTestCycles(void);

// 获取稳速运行时间 (秒): 每个周期内保持目标转速的持续时间
u16 Menu_GetTestOnSec(void);

// 获取静止间隔时间 (秒): 每个周期之间PWM关闭的等待时间
u16 Menu_GetTestOffSec(void);

// 获取已记录的测试数据条数
u16 Menu_GetTestRecordCount(void);

// 设置测试配置参数 (由PC端通过串口协议下发)
// 参数 cfg: 指向 TestConfig 结构体的指针, 包含通道/频率/占空比/循环次数等
void Menu_SetTestConfig(TestConfig *cfg);

// 启动自动测试
// 前置条件: 已通过 Menu_SetTestConfig 设置参数
// 动作: 清空历史记录, 设置 test_running=1, 开始第一个周期
void Menu_StartTest(void);

// 强制停止自动测试
// 动作: 禁用PWM输出, 设置 test_running=0
void Menu_StopTest(void);

// 添加一条测试记录 (每个测试周期结束时调用)
// 参数:
//   cycle      — 当前周期序号 (从1开始)
//   target_rpm — 目标转速 (RPM)
//   rpm_max    — 该周期内的最大转速
//   rpm_avg    — 该周期内的平均转速
//   error      — 是否超差 (1=误差超标, 0=合格)
//   startup_ok — 启动是否成功 (1=在规定时间内达到目标转速)
void Menu_AddTestRecord(u16 cycle, u16 target_rpm, u16 rpm_max, u16 rpm_avg,
                        u8 error, u8 startup_ok);

// 测试完成回调
// 触发时机: 所有循环周期执行完毕后自动调用
// 动作: 停止PWM输出, 标记测试结束, 准备CSV导出
void Menu_TestDone(void);

/* ── CSV 导出接口 ─────────────────────────────────────────────────────── */

// 获取待导出的总记录数
// 用途: PC端在接收CSV数据前先获取总条数, 用于进度条显示
u16 Menu_GetExportCount(void);

// 格式化一条CSV记录到缓冲区 (逐条导出, 避免一次性占用过多内存)
// 参数:
//   buf     — 输出缓冲区指针
//   max_len — 缓冲区最大可用字节数
// 返回值: 实际写入的字节数, 0=导出完毕
// CSV格式: cycle,target_rpm,rpm_max,rpm_avg,error,startup_ok\r\n
u8  Menu_FormatExportChunk(u8 *buf, u16 max_len);

// 检查CSV导出是否完成
// 返回值: 1=所有记录已导出完毕, 0=还有数据待导出
u8  Menu_ExportDone(void);

/* ── 全局变量 (供其他模块读取) ────────────────────────────────────────── */

extern SystemParams g_params;   // 系统参数: 频率/占空比/使能/分频/PPR 等
                                // 所有模块可读, 修改后需调用 PWM_UpdateFromParams 同步
extern MenuState g_menu;        // 菜单状态: 当前模式/光标位置/编辑状态等
                                // 仅供 menu 模块内部写入, 外部只读
extern u8 test_running;         // 测试运行标志: 1=进行中, 0=空闲
                                // 供主循环判断是否执行测试调度逻辑

#endif
