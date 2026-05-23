/*
 * dac_output.h — DAC 模拟电压输出驱动
 * ══════════════════════════════════════════════
 *  STM32F103 DAC 通道1 (PA4) 输出 0~3.3V
 *  经外部运放放大到 0~5V 作为 VSP 信号
 */
#ifndef DAC_OUTPUT_H
#define DAC_OUTPUT_H

#include "sys.h"

/* DAC 初始化
 * 配置 PA4 为 DAC 通道1 输出，12位精度
 */
void DAC_Output_Init(void);

/* 设置 DAC 输出电压
 * voltage_x10: 目标电压 ×10 (0~50 = 0.0~5.0V)
 *              DAC 输出 = voltage_x10 / 50.0 × 3.3V
 *              运放放大 1.515 倍后 = voltage_x10 / 10.0 V
 */
void DAC_Output_SetVoltage(u8 voltage_x10);

/* 关闭 DAC 输出 (PA4 输出 0V) */
void DAC_Output_Off(void);

#endif
