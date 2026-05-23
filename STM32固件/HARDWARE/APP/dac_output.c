/*
 * dac_output.c — DAC 模拟电压输出驱动
 * ══════════════════════════════════════════════
 *  STM32F103 DAC 通道1 (PA4) 输出 0~3.3V
 *  经外部运放 (MCP6002) ×1.515 放大到 0~5V
 *
 *  电压映射:
 *    vsp_voltage_x100 = 0   → DAC 0    → PA4 0V    → VSP 0.00V
 *    vsp_voltage_x100 = 500 → DAC 4095 → PA4 3.3V  → VSP 5.00V
 *    通用公式: DAC值 = voltage_x100 × 4095 / 500
 */
#include "dac_output.h"
#include "stm32f10x.h"
#include "stm32f10x_dac.h"
#include "stm32f10x_rcc.h"
#include "stm32f10x_gpio.h"

/* DAC 初始化 — PA4 通道1，12位右对齐，不使用触发 */
void DAC_Output_Init(void)
{
    GPIO_InitTypeDef gpio;
    DAC_InitTypeDef dac;

    /* 使能时钟 */
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE);
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_DAC, ENABLE);

    /* PA4 配置为模拟输入 (DAC 输出需要) */
    gpio.GPIO_Pin = GPIO_Pin_4;
    gpio.GPIO_Mode = GPIO_Mode_AIN;
    GPIO_Init(GPIOA, &gpio);

    /* DAC 通道1 配置 */
    DAC_StructInit(&dac);
    /* 触发关闭，软件触发模式 */
    DAC_Init(DAC_Channel_1, &dac);
    DAC_Cmd(DAC_Channel_1, ENABLE);

    /* 初始输出 0V */
    DAC_SetChannel1Data(DAC_Align_12b_R, 0);
}

/* 设置 DAC 输出电压
 * voltage_x100: 目标电压 ×100 (0~500 = 0.00~5.00V)
 * 映射到 DAC 12位值 (0~4095)
 */
void DAC_Output_SetVoltage(u16 voltage_x100)
{
    u16 dac_val;

    if (voltage_x100 > 500) voltage_x100 = 500;

    /* DAC值 = voltage_x100 × 4095 / 500 */
    dac_val = (u16)((u32)voltage_x100 * 4095 / 500);

    DAC_SetChannel1Data(DAC_Align_12b_R, dac_val);
}

/* 关闭 DAC 输出 */
void DAC_Output_Off(void)
{
    DAC_SetChannel1Data(DAC_Align_12b_R, 0);
}
