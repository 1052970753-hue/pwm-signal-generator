# PWM Signal Generator / PWM 信号发生器

基于 **STM32F103C8T6** 的双通道 PWM 信号发生器，配备 0.96" OLED 显示屏和旋转编码器，支持 1Hz ~ 100kHz 频率输出、FG 信号采集与 RPM 显示。附带 **Python PyQt6 桌面模拟器**，可在 PC 上仿真全部操作界面。

---

## 功能特性

### 双通道 PWM 输出
- **频率范围:** 1Hz ~ 100kHz，连续可调
- **占空比:** 0% ~ 100%，1% 步进
- **独立开关:** CH1 / CH2 可分别使能/关闭
- **电平输出:** 经 74HCT04 转换为 5V TTL 电平，BNC 接口输出

### FG 信号采集
- 输入频率测量，实时显示 RPM
- 支持硬件分频 (/1, /2, /4, /8)
- 5V 输入经电阻分压安全接入 STM32

### OLED 显示界面
- 128×64 SSD1306 OLED，I2C 接口
- 单屏布局：CH1 左列 / CH2 右列 / FG 底栏
- 实心圆 = ON，空心圆 = OFF
- 光标闪烁指示当前选中项

### 旋转编码器交互
- **正常模式:** 旋钮直接修改当前参数值，短按 OK 切换通道开关
- **选择模式:** 长按 OK 进入，旋钮移动光标选择不同参数项，短按退出

---

## 系统架构

```
┌─────────────────────────────────────────┐
│         PC (Python PyQt6 模拟器)         │
│  ┌────────────────────────────────────┐  │
│  │  OLED 128×64 仿真 (4x 缩放)       │  │
│  │  旋钮编码器模拟 (260px)            │  │
│  └────────────────────────────────────┘  │
│              │ USB CDC (115200)           │
└──────────────┼────────────────────────────┘
               │
┌──────────────┼────────────────────────────┐
│  STM32F103C8T6 (Blue Pill, 72MHz)        │
│                                          │
│  PA8 ── TIM1 CH1 ── 74HCT04 ── BNC1 (PWM OUT1)
│  PB6 ── TIM4 CH1 ── 74HCT04 ── BNC2 (PWM OUT2)
│  PA0 ── TIM2 CH1 ◄── 分压 ◄── BNC3 (FG IN)
│                                          │
│  PB8/PB9 ── I2C1 ── SSD1306 OLED         │
│  PA6/PA7 ── TIM3 ── 旋转编码器 EC11      │
│  PB12 ── OK 按键 (编码器内置)            │
│  PA9/PA10 ── USART1 ── USB-TTL           │
└──────────────────────────────────────────┘
```

---

## 硬件设计

### 引脚分配

| 功能 | 引脚 | 外设 | 备注 |
|------|------|------|------|
| PWM OUT1 | PA8 | TIM1 CH1 | → 74HCT04 → BNC1 |
| PWM OUT2 | PB6 | TIM4 CH1 | → 74HCT04 → BNC2 |
| FG IN | PA0 | TIM2 CH1 | ← 10k+10k 分压 ← BNC3 |
| 编码器 A | PA6 | TIM3 CH1 | EC11 旋转编码器 |
| 编码器 B | PA7 | TIM3 CH2 | EC11 旋转编码器 |
| OK 按键 | PB12 | GPIO (内部上拉) | 编码器内置按键 |
| OLED SCL | PB8 | I2C1 | SSD1306 |
| OLED SDA | PB9 | I2C1 | SSD1306 |
| USART TX | PA9 | USART1 | USB-TTL RX |
| USART RX | PA10 | USART1 | USB-TTL TX |

### 电平转换

```
3.3V PWM ──→ 74HCT04 输入 ──→ 74HCT04 输出 (5V) ──→ 100Ω ──→ BNC
```

74HCT04 由 5V 供电，输入阈值 ~2V，可可靠识别 3.3V 信号。

### FG 分压

```
BNC (5V) ──→ 10kΩ ──┬──→ PA0 (3.3V 安全)
                     │
                    10kΩ
                     │
                    GND
```

分压比 0.5，5V → 2.5V，在 STM32 输入规格范围内。

### 物料清单 (约 ¥60)

| 物料 | 型号 | 数量 | 用途 |
|------|------|------|------|
| STM32F103C8T6 | Blue Pill | 1 | 主控 |
| 0.96" OLED | SSD1306 I2C 128×64 | 1 | 显示 |
| 旋转编码器 | EC11 带按键 | 1 | 输入 |
| 74HCT04 | DIP/SOP-14 | 1 | 电平转换 |
| USB-TTL | CH340G | 1 | 通信/烧录 |
| BNC 母座 | 焊接式 | 3 | 信号接口 |
| 电阻/电容 | 10kΩ×2, 100Ω×2, 100nF×3 | 若干 | 信号调理 |

详见 [hardware/BOM.md](hardware/BOM.md) 和 [hardware/schematic.md](hardware/schematic.md)。

---

## 固件

### 目录结构

```
firmware/Core/
├── main.c            # 主循环：轮询编码器 → 处理事件 → 渲染OLED → 发送状态
├── menu.c / .h       # 双状态菜单引擎 (正常模式 + 选择模式)
├── ui_render.c / .h  # OLED 界面渲染 (单屏布局)
├── encoder.c / .h    # 旋转编码器驱动 (TIM3 编码器模式)
├── pwm_engine.c / .h # PWM 输出引擎 (TIM1/TIM4，频率自适应分频)
├── fg_capture.c / .h # FG 信号采集 (TIM2 输入捕获 + 中断)
├── oled_ssd1306.c/.h # SSD1306 OLED 驱动 (I2C, 像素绘图)
├── protocol.c / .h   # USB CDC 通信协议
└── system_config.h   # 引脚定义 + 系统时钟配置
```

### 菜单状态机

```
┌─────────────┐   长按OK   ┌─────────────┐
│  正常模式    │ ─────────→ │  选择模式    │
│  旋钮=改值   │            │  旋钮=移动   │
│  短按=切通道 │ ←───────── │  短按=退出   │
└─────────────┘   短按OK   └─────────────┘
```

5 个光标项循环：`CH1 Freq → CH1 Duty → CH2 Freq → CH2 Duty → FG Div`

### PWM 频率分段

| 频率段 | 预分频器 | 说明 |
|--------|---------|------|
| 1 ~ 100 Hz | 7199 | 低频精确 |
| 100 ~ 1k Hz | 719 | |
| 1k ~ 10k Hz | 71 | |
| 10k ~ 100k Hz | 7 | 高频 |

---

## 模拟器

### Python PyQt6 模拟器 (`run_simulator.py`)

独立运行的桌面应用，完整仿真 OLED 显示和编码器操作，无需硬件即可验证 UI 交互。

```bash
# 安装依赖
pip install PyQt6

# 运行
python simulator/run_simulator.py
```

### 操作方式

| 鼠标操作 | 功能 |
|---------|------|
| 点击旋钮左侧 (◄) | 逆时针 — 减小值/上移光标 |
| 点击旋钮右侧 (►) | 顺时针 — 增大值/下移光标 |
| 点击中心 (OK) | 短按 — 切换通道 / 退出选择 |
| 长按中心 (OK) | 进入选择模式，光标闪烁 |

### UI 界面

```
┌──────────────────────────────┐
│        PWM_TOOL              │  ← 标题栏
├──────────────┬───────────────┤
│ ● CH1  ON    │ ○ CH2  OFF   │  ← 通道状态
│ Fr    1000Hz │ Fr    1000Hz │  ← 频率
│ >Duty    50% │ Duty    50%  │  ← 占空比 (> = 光标)
├──────────────┴───────────────┤
│ FG   0 RPM          /2      │  ← FG 底栏
└──────────────────────────────┘
```

---

## 通信协议

PC 与 MCU 之间通过 USB CDC (USART1, 115200 baud) 双向通信。

### 帧格式

```
| Header (1B) | CMD (1B) | LEN (1B) | DATA (NB) | CRC8 (1B) |

Header: 0xAA (PC→MCU) / 0xBB (MCU→PC)
CRC8: 多项式 0x07, 对 HEADER+CMD+LEN+DATA 计算
```

### 命令列表

| CMD | 方向 | DATA 结构 | 说明 |
|-----|------|----------|------|
| `0x10` | MCU→PC | `StatusData` (18B) | 查询/上报状态：CH1/CH2 频率、占空比、使能、FG频率、分频、RPM |
| `0x20` | PC→MCU | `PwmWriteReq` (7B) | 写入PWM参数：channel(1/2) + freq_hz + duty_pct + enable |
| `0x30` | PC→MCU | `FgDivReq` (1B) | 写入FG分频比：div(1/2/4/8) |
| `0x41` | PC→MCU | `KeyEventReq` (1B) | 远程按键事件：0=None, 1=CW, 2=CCW, 3=Click, 4=LongPress |

### 通信流程

```
PC ──[0xAA 0x10 0x00 CRC]──→ MCU    # 查询状态
PC ←──[0xBB 0x10 12 DATA CRC]── MCU  # 返回 StatusData

PC ──[0xAA 0x20 0x07 DATA CRC]──→ MCU  # 设置 PWM
PC ←──[0xBB 0x20 0x00 CRC]── MCU       # ACK

PC ──[0xAA 0x41 0x01 0x03 CRC]──→ MCU  # 发送 Click 事件
PC ←──[0xBB 0x41 0x00 CRC]── MCU       # ACK
```

### 远程按键事件映射

| 事件值 | 含义 | 效果 |
|--------|------|------|
| 0 | EVENT_NONE | 无操作 |
| 1 | EVENT_CW | 旋钮顺时针（正常模式=增加值，选择模式=下移光标） |
| 2 | EVENT_CCW | 旋钮逆时针（正常模式=减少值，选择模式=上移光标） |
| 3 | EVENT_CLICK | 短按OK（正常模式=切换通道，选择模式=退出选择） |
| 4 | EVENT_LONG_PRESS | 长按OK（进入选择模式） |

---

## 快速开始

### 硬件搭建

1. 按 [schematic.md](hardware/schematic.md) 接线
2. 烧录固件（使用 ST-Link V2 或 USB-TTL 串口烧录）
3. 上电，OLED 显示主界面
4. 旋转编码器调整参数，短按切换输出

### 模拟器运行

```bash
pip install PyQt6
python simulator/run_simulator.py
```

---

## 许可证

MIT License
