/**
  ******************************************************************************
  * @file    GPIO/IOToggle/stm32f10x_it.c
  * @author  MCD Application Team
  * @version V3.5.0
  * @date    08-April-2011
  * @brief   Main Interrupt Service Routines.
  *          This file provides template for all exceptions handler and peripherals
  *          interrupt service routine.
  ******************************************************************************
  * @attention
  *
  * THE PRESENT FIRMWARE WHICH IS FOR GUIDANCE ONLY AIMS AT PROVIDING CUSTOMERS
  * WITH CODING INFORMATION REGARDING THEIR PRODUCTS IN ORDER FOR THEM TO SAVE
  * TIME. AS A RESULT, STMICROELECTRONICS SHALL NOT BE HELD LIABLE FOR ANY
  * DIRECT, INDIRECT OR CONSEQUENTIAL DAMAGES WITH RESPECT TO ANY CLAIMS ARISING
  * FROM THE CONTENT OF SUCH FIRMWARE AND/OR THE USE MADE BY CUSTOMERS OF THE
  * CODING INFORMATION CONTAINED HEREIN IN CONNECTION WITH THEIR PRODUCTS.
  *
  * <h2><center>&copy; COPYRIGHT 2011 STMicroelectronics</center></h2>
  ******************************************************************************
  */

/* Includes ------------------------------------------------------------------*/
#include "stm32f10x_it.h"


/* ══════════════════════════════════════════════════════════════════════════════
 *  Cortex-M3 内核异常处理函数
 *  这些是系统级异常的处理入口, 发生时通常意味着程序出现了严重错误
 * ══════════════════════════════════════════════════════════════════════════════ */

// NMI (不可屏蔽中断) — 由硬件错误触发, 无法被屏蔽
// 在本项目中未使用, 保持空实现
void NMI_Handler(void)
{
}

// HardFault (硬件错误) — 最常见的致命异常
// 常见原因: 非法内存访问 (空指针解引用)、除零、栈溢出、非法指令等
// 一旦发生, 程序无法恢复, 进入死循环等待看门狗复位或调试器介入
void HardFault_Handler(void)
{
  /* Go to infinite loop when Hard Fault exception occurs */
  while (1)
  {
  }
}

// MemManage (内存管理错误) — MPU 相关违规
// 常见原因: 访问了 MPU 禁止的区域、特权级违规等
// STM32F103 未启用 MPU, 此异常一般不会触发
void MemManage_Handler(void)
{
  /* Go to infinite loop when Memory Manage exception occurs */
  while (1)
  {
  }
}

// BusFault (总线错误) — AHB/APB 总线访问错误
// 常见原因: 访问了不存在的外设地址、Flash/SRAM 损坏等
void BusFault_Handler(void)
{
  /* Go to infinite loop when Bus Fault exception occurs */
  while (1)
  {
  }
}

// UsageFault (用法错误) — 指令执行错误
// 常见原因: 未定义的指令、非法的 Thumb 状态切换、未对齐访问等
void UsageFault_Handler(void)
{
  /* Go to infinite loop when Usage Fault exception occurs */
  while (1)
  {
  }
}

// SVC (系统服务调用) — 用于 RTOS 的系统调用
// 本项目为裸机程序, 未使用 SVC, 保持空实现
void SVC_Handler(void)
{
}

// DebugMon (调试监控) — 调试器相关的异常
// 通过 JTAG/SWD 调试时可能触发, 保持空实现
void DebugMon_Handler(void)
{
}

// PendSV (可挂起的系统调用) — RTOS 上下文切换使用
// 本项目为裸机程序, 未使用 PendSV, 保持空实现
void PendSV_Handler(void)
{
}

/* ══════════════════════════════════════════════════════════════════════════════
 *  SysTick_Handler — 系统滴答中断服务函数
 *
 *  触发频率: 每 1ms 一次 (由 main() 中 SysTick_Config(SystemCoreClock/1000) 配置)
 *
 *  工作原理:
 *    1. Cortex-M3 内核的 SysTick 定时器从 SystemCoreClock/1000 = 72000 开始倒计数
 *    2. 计数到 0 时触发 SysTick_Handler 中断, 同时自动重装载初值
 *    3. 中断服务函数将 sys_tick 变量递增 (++)
 *    4. 主循环通过 sys_tick 的差值判断时间间隔, 实现各任务的定时调度
 *
 *  与主循环的关系:
 *    - main.c 中声明: volatile u32 sys_tick = 0;
 *    - 此文件通过 extern 引用该变量, 在中断中修改
 *    - volatile 关键字确保编译器每次都从内存读取, 不使用寄存器缓存
 *    - 主循环中的 Encoder_SetTick(sys_tick) 将时间传递给编码器模块
 *      用于长按 (1000ms) 和双击 (400ms) 的计时判断
 *
 *  注意: 中断服务函数应尽量简短, 仅做 ++ 操作 (约 3 个 CPU 周期)
 *        不要在中断中做任何耗时操作 (如浮点运算、串口打印等)
 * ══════════════════════════════════════════════════════════════════════════════ */
extern volatile u32 sys_tick;  // 外部声明, 变量定义在 main.c 中

void SysTick_Handler(void)
{
    sys_tick++;  // 递增系统滴答计数, 每次 +1 代表 1ms 过去
}

/******************************************************************************/
/*                 STM32F10x Peripherals Interrupt Handlers                   */
/*  Add here the Interrupt Handler for the used peripheral(s) (PPP), for the  */
/*  available peripheral interrupt handler's name please refer to the startup */
/*  file (startup_stm32f10x_xx.s).                                            */
/******************************************************************************/
