# PWM 信号发生器 — 电路原理图

## MCU: STM32F103C8T6 (Blue Pill 开发板)

## 引脚分配

| 功能 | 引脚 | 信号方向 | 备注 |
|------|------|---------|------|
| OLED SCL | PA1 | 输出 | 软件 I2C SCL |
| OLED SDA | PA2 | 双向/开漏 | 软件 I2C SDA |
| OLED VCC | 3.3V | 电源 | SSD1306 供电 |
| OLED GND | GND | 电源 | 共地 |
| PWM OUT1 | PA8 | 输出 | TIM1 CH1 → 74HCT04 → BNC1 |
| PWM OUT2 | PB6 | 输出 | TIM4 CH1 → 74HCT04 → BNC2 |
| FG IN | PA0 | 输入 | TIM2 CH1, 5V→2.5V 分压 |
| ENC A | PA6 | 输入 | 旋转编码器 A 相 (TIM3 CH1) |
| ENC B | PA7 | 输入 | 旋转编码器 B 相 (TIM3 CH2) |
| ENC SW / OK | PB8 | 输入 | 编码器按键 (内部上拉) |
| USART1 TX | PA9 | 输出 | CH340C RXD |
| USART1 RX | PA10 | 输入 | CH340C TXD |

## 电源设计

```
USB 5V ──┬── 74HCT04 VCC (5V)
         ├── Blue Pill 5V pin → 板载 AMS1117-3.3 → MCU + OLED + 编码器
         └── GND (共地)
```

## 电平转换电路

### 3.3V PWM → 5V 输出
```
PA8 (3.3V PWM) ──→ 74HCT04 输入 (pin 1)
                   74HCT04 输出 (pin 2) ──→ 100Ω ──→ BNC1 (5V PWM OUT)

PB6 (3.3V PWM) ──→ 74HCT04 输入 (pin 3)
                   74HCT04 输出 (pin 4) ──→ 100Ω ──→ BNC2 (5V PWM OUT)
```

74HCT04 由 5V 供电，输入阈值 ~2V，3.3V 信号可靠触发。

### FG 5V → 3.3V 输入
```
BNC3 (FG IN) ──→ 10kΩ ──┬──→ PA0 (TIM2 CH1)
                         │
                        10kΩ
                         │
                        GND
```
分压比 0.5, 5V → 2.5V (安全且在 STM32 输入规格范围内)

## 旋转编码器接线 (EC11)
```
EC11 A  ──→ PA6 (TIM3 CH1)
EC11 B  ──→ PA7 (TIM3 CH2)
EC11 C  ──→ GND
EC11 SW ──→ PB8 (MCU 内部上拉, OK 按键)
```

## OLED 接线 (SSD1306 软件 I2C)
```
OLED VCC → 3.3V
OLED GND → GND
OLED SCL → PA1
OLED SDA → PA2
```

## USB 转串口电路 (CH340C + Type-C)

板载 CH340C + Type-C 母座，直接 USB 线连接电脑，无需外部 USB-TTL 模块。

### Type-C 接口
```
Type-C VBUS (5V) ──→ CH340C VCC
                   ├── 74HCT04 VCC
                   ├── 扩展排针 5V
                   └── Blue Pill 5V pin → 板载 AMS1117-3.3 → MCU + OLED + 编码器

Type-C D+ ──→ CH340C UD+ (10kΩ 上拉到 3.3V，用于 USB 全速识别)
Type-C D- ──→ CH340C UD-
Type-C GND ──→ 共地
```

### CH340C 连接
```
CH340C TXD ──→ PA10 (USART1 RX)
CH340C RXD ──→ PA9  (USART1 TX)
CH340C VCC ──→ 5V (Type-C VBUS)
CH340C GND ──→ GND

CH340C 时钟: 可选外挂 12MHz 晶振 + 22pF×2 (C 版内置 RC 振荡，推荐外挂确保稳定性)
CH340C 去耦: VCC 引脚旁 100nF 电容
```

### 元件清单
| 元件 | 数量 | 说明 |
|------|------|------|
| CH340C (SOP-16) | 1 | USB 转串口芯片 |
| Type-C 16P 沉板母座 | 1 | USB 连接器 |
| 12MHz 晶振 | 1 | CH340C 时钟源 |
| 22pF 电容 | 2 | 晶振负载电容 |
| 100nF 电容 | 1 | CH340C 去耦 |
| 10kΩ 电阻 | 1 | D+ 上拉 |

## 扩展排针 (底部 2×7 双排母座)

PCB 底部边缘设 2.54mm 间距双排排针座（母座，向下开口），扩展板从下方插入。引出 STM32 未使用的 GPIO 引脚 + 5V 电源 + GND。

### 引脚定义

| 排针号 | STM32 引脚 | 默认功能 | 备注 |
|--------|-----------|---------|------|
| 1 | PA3 | GPIO | TIM2_CH4, USART2_RX |
| 2 | PA4 | GPIO | SPI1_NSS, DAC1 |
| 3 | PA5 | GPIO | SPI1_SCK, DAC2 |
| 4 | PB0 | GPIO | TIM3_CH3, ADC8 |
| 5 | PB1 | GPIO | TIM3_CH4, ADC9 |
| 6 | PB5 | GPIO | SPI1_MOSI, I2C1_SMBA |
| 7 | PB7 | GPIO | I2C1_SDA, TIM4_CH2 |
| 8 | PB9 | GPIO | I2C1_SDA, CAN_TX |
| 9 | PB10 | GPIO | I2C1_SCL, USART3_TX |
| 10 | PB11 | GPIO | USART3_RX, CAN_RX |
| 11 | PB12 | GPIO | SPI1_NSS, USART3_CK |
| 12 | 5V | 电源 | Type-C VBUS 直供 |
| 13 | GND | 地 | 共地 |
| 14 | GND | 地 | 双地脚确保接触 |

### 排针排列

```
主控板 PCB (元件面朝上):

  ┌──────────────────────────────────────┐
  │                                      │
  │          BNC1  BNC2  BNC3            │  ← 顶部: BNC 接口
  │          [PWM] [PWM] [FG ]           │
  │                                      │
  │    [OLED]    [旋钮]    [Type-C]      │  ← 中部: 显示/操作/USB
  │                                      │
  └──┬──┬──┬──┬──┬──┬──┬──┬──┬──┬──┬──┬──┬──┬──┘
     1  2  3  4  5  6  7  8  9  10 11 12 13 14
    PA3 PA4 PA5 PB0 PB1 PB5 PB7 PB9 10 11 12 5V GND GND
                                                  ← 底部: 扩展排针
```

### 引脚资源汇总

| 分类 | 引脚 | 数量 |
|------|------|------|
| 已使用 (功能) | PA0, PA1, PA2, PA6, PA7, PA8, PA9, PA10, PB6, PB8 | 10 |
| SWD 调试 (保留) | PA13, PA14 | 2 |
| 扩展排针引出 | PA3, PA4, PA5, PB0, PB1, PB5, PB7, PB9, PB10, PB11, PB12 | 11 |
| 电源引出 | 5V, GND×2 | 3 |
| 未引出 | PA11, PA12 (USB D+/D-), PA15/PB3/PB4 (JTAG) | 5 |

## 整体连接框图

```
                         ┌──────────────────────────────┐
     EC11 ───────────────┤ PA6/PA7 (TIM3) + PB8        │
     OLED ───────────────┤ PA1/PA2 (软件I2C)             │
                         │                              │
     Type-C ──[CH340C]──┤ PA9/PA10 (USART1)            │
                         │                              │
                         │     STM32F103C8T6            │
                         │                              │
     FG IN ──[分压]──────┤ PA0 (TIM2 CH1)               │
                         │                              │
                         │ PA8 (TIM1 CH1)─[74HCT04]─PWM OUT1 (5V BNC)
                         │ PB6 (TIM4 CH1)─[74HCT04]─PWM OUT2 (5V BNC)
                         │                              │
     扩展排针 ◄──────────┤ PA3/4/5, PB0/1/5/7/9/10/11/12
                         │         + 5V + GND           │
                         └──────────────────────────────┘
```
