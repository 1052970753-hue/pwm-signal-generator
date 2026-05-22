# PWM 信号发生器 — 电路原理图

## MCU: STM32F103C8T6 (Blue Pill 开发板)

## 引脚分配

| 功能 | 引脚 | 信号方向 | 备注 |
|------|------|---------|------|
| OLED SCL | PB8 | 输出 | I2C1 SCL, 3.3V |
| OLED SDA | PB9 | 双向 | I2C1 SDA, 3.3V |
| OLED VCC | 3.3V | 电源 | SSD1306 供电 |
| OLED GND | GND | 电源 | 共地 |
| PWM OUT1 | PA8 | 输出 | TIM1 CH1 → 74HCT04 → BNC1 |
| PWM OUT2 | PB6 | 输出 | TIM4 CH1 → 74HCT04 → BNC2 |
| FG IN | PA0 | 输入 | 5V→3.3V 电阻分压后接入 |
| ENC A | PA6 | 输入 | 旋转编码器 A 相 (TIM3 CH1) |
| ENC B | PA7 | 输入 | 旋转编码器 B 相 (TIM3 CH2) |
| ENC SW | PB12 | 输入 | 编码器按键 (内部上拉, 兼 OK 键) |
| BTN BACK | PB13 | 输入 | 返回按键 (内部上拉) |
| USART1 TX | PA9 | 输出 | USB-TTL 模块 RX |
| USART1 RX | PA10 | 输入 | USB-TTL 模块 TX |

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
                        4.7kΩ
                         │
                        GND
```
分压比: 4.7/(10+4.7) ≈ 0.32, 5V → 1.6V (可靠识别为高电平, STM32 VIH_min = 2.0V...)

**修正:** 使用 10k + 10k 分压, 分压比 0.5, 5V → 2.5V (安全且在规格内)

```
BNC3 (FG IN) ──→ 10kΩ ──┬──→ PA0 (TIM2 CH1)
                         │
                        10kΩ
                         │
                        GND
```

## 旋转编码器接线 (EC11)
```
EC11 A  ──→ PA6 (TIM3 CH1)
EC11 B  ──→ PA7 (TIM3 CH2)
EC11 C  ──→ GND
EC11 SW ──→ PB12 (MCU 内部上拉)
```

## 按键接线
```
BTN BACK ──→ PB13 ──→ GND (按下导通, MCU 内部上拉)
```

## OLED 接线 (SSD1306 I2C)
```
OLED VCC → 3.3V
OLED GND → GND
OLED SCL → PB8
OLED SDA → PB9
```

## USB-TTL 接线 (CH340G)
```
CH340G TXD → PA10 (USART1 RX)
CH340G RXD → PA9  (USART1 TX)
CH340G VCC → 3.3V
CH340G GND → GND
```

## 整体连接框图

```
                    ┌─────────────────────────┐
    EC11 ───────────┤ PA6/PA7 (TIM3) + PB12  │
    BTN ────────────┤ PB13                    │
    OLED ───────────┤ PB8/PB9 (I2C)           │
                    │                         │
                    │    STM32F103C8T6        │
                    │                         │
    FG IN ──[分压]──┤ PA0 (TIM2 CH1)          │
                    │                         │
                    │ PA8 (TIM1 CH1) ──[74HCT04]── PWM OUT1 (5V)
                    │ PB6 (TIM4 CH1) ──[74HCT04]── PWM OUT2 (5V)
                    │                         │
    USB-TTL ────────┤ PA9/PA10 (USART1)       │
                    └─────────────────────────┘
```
