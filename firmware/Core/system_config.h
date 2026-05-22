#ifndef SYSTEM_CONFIG_H
#define SYSTEM_CONFIG_H

#include "stm32f10x.h"

#define SYSCLK_FREQ  72000000

// OLED (I2C1)
#define OLED_I2C         I2C1
#define OLED_I2C_CLK     RCC_APB1Periph_I2C1
#define OLED_GPIO        GPIOB
#define OLED_GPIO_CLK    RCC_APB2Periph_GPIOB
#define OLED_SCL_PIN     GPIO_Pin_8
#define OLED_SDA_PIN     GPIO_Pin_9
#define OLED_ADDR        0x3C

// PWM CH1 (TIM1 CH1)
#define PWM1_TIM         TIM1
#define PWM1_TIM_CLK     RCC_APB2Periph_TIM1
#define PWM1_GPIO        GPIOA
#define PWM1_GPIO_CLK    RCC_APB2Periph_GPIOA
#define PWM1_PIN         GPIO_Pin_8
#define PWM1_CHANNEL     1

// PWM CH2 (TIM4 CH1)
#define PWM2_TIM         TIM4
#define PWM2_TIM_CLK     RCC_APB1Periph_TIM4
#define PWM2_GPIO        GPIOB
#define PWM2_GPIO_CLK    RCC_APB2Periph_GPIOB
#define PWM2_PIN         GPIO_Pin_6
#define PWM2_CHANNEL     1

// FG Input (TIM2 CH1)
#define FG_TIM           TIM2
#define FG_TIM_CLK       RCC_APB1Periph_TIM2
#define FG_GPIO          GPIOA
#define FG_GPIO_CLK      RCC_APB2Periph_GPIOA
#define FG_PIN           GPIO_Pin_0
#define FG_CHANNEL       1

// Encoder (TIM3 encoder mode) — PA6/PA7 are TIM3 default pins
#define ENC_TIM          TIM3
#define ENC_TIM_CLK      RCC_APB1Periph_TIM3
#define ENC_GPIO         GPIOA
#define ENC_GPIO_CLK     RCC_APB2Periph_GPIOA
#define ENC_A_PIN        GPIO_Pin_6
#define ENC_B_PIN        GPIO_Pin_7

// Buttons
#define BTN_GPIO         GPIOB
#define BTN_GPIO_CLK     RCC_APB2Periph_GPIOB
#define BTN_OK_PIN       GPIO_Pin_12
#define BTN_BACK_PIN     GPIO_Pin_13

// USB CDC (USART1)
#define CDC_USART        USART1
#define CDC_USART_CLK    RCC_APB2Periph_USART1
#define CDC_GPIO         GPIOA
#define CDC_GPIO_CLK     RCC_APB2Periph_GPIOA
#define CDC_TX_PIN       GPIO_Pin_9
#define CDC_RX_PIN       GPIO_Pin_10
#define CDC_BAUDRATE     115200

#endif
