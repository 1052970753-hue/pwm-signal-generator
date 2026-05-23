# PWM Signal Generator / PWM 信号发生器

基于 **STM32F103C8T6** 的双通道 PWM 信号发生器，配备 0.96" OLED 显示屏和旋转编码器。支持 1Hz ~ 100kHz 频率输出、FG 信号采集与 RPM 显示、自动测试模式。附带 **Python PyQt6 桌面模拟器**，可在 PC 上仿真全部操作界面。

---

## 功能特性

### 双通道 PWM 输出
- **频率范围:** 1Hz ~ 100kHz，连续可调
- **占空比:** 0% ~ 100%，1% 步进
- **独立开关:** CH1 / CH2 可分别使能/关闭
- **电平输出:** 经 74HCT04 转换为 5V TTL 电平，BNC 接口输出

### FG 信号采集
- 输入频率测量，实时显示 RPM
- 支持硬件分频 (1~99)
- 5V 输入经电阻分压安全接入 STM32

### 自动测试模式
- 配置: 通道/频率/占空比/循环次数/ON时间/OFF时间
- 自动 ON/OFF 循环，每轮采样 RPM (最大值、平均值)
- 异常判定: RPM=0 或偏差 >20% 标记为异常
- 测试记录存储 (最多 200 条)
- CSV 数据导出 (通过串口分块传输)

### 5 种工作模式
双击旋转编码器循环切换: **PWM-FG → FG → CH1 → CH2 → TEST**

| 模式 | 说明 |
|------|------|
| PWM-FG | 双通道 PWM 参数 + FG RPM 显示 (默认) |
| FG | 纯频率计，大号 RPM 显示 |
| CH1 | CH1 单通道 PWM 调节 |
| CH2 | CH2 单通道 PWM 调节 |
| TEST | 自动测试模式 (ON/OFF 循环 + 数据记录) |

### OLED 显示界面
- 128×64 SSD1306 OLED，软件 I2C (PA1/PA2)
- 每种模式独立布局
- 光标闪烁指示当前选中项

### 旋转编码器交互
- **旋转:** 修改当前参数值
- **短按:** 切换通道使能 / 测试中停止
- **长按:** 进入选择模式 (旋转移动光标, 短按退出)
- **双击:** 切换工作模式

---

## 系统架构

```
┌─────────────────────────────────────────┐
│         PC (Python PyQt6 模拟器)         │
│  ┌────────────────────────────────────┐  │
│  │  OLED 128×64 仿真 (4x 缩放)       │  │
│  │  旋钮编码器模拟 (260px)            │  │
│  │  模式切换 / 串口控制 / CSV 导出   │  │
│  └────────────────────────────────────┘  │
│              │ USB CDC (115200)           │
└──────────────┼────────────────────────────┘
               │ Type-C
┌──────────────┼────────────────────────────┐
│  CH340C ── USART1                         │
│                                          │
│  STM32F103C8T6 (Blue Pill, 72MHz)        │
│                                          │
│  PA8 ── TIM1 CH1 ── 74HCT04 ── BNC1 (PWM OUT1)
│  PB6 ── TIM4 CH1 ── 74HCT04 ── BNC2 (PWM OUT2)
│  PA0 ── TIM2 CH1 ◄── 分压 ◄── BNC3 (FG IN)
│                                          │
│  PA1/PA2 ── 软件 I2C ── SSD1306 OLED    │
│  PA6/PA7 ── TIM3 ── 旋转编码器 EC11     │
│  PB8 ── OK 按键 (编码器内置)            │
│                                          │
│  底部 2×7 排针 ── PA3/4/5, PB0/1/5/7/9/10/11/12 + 5V + GND
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
| OK 按键 | PB8 | GPIO (内部上拉) | 编码器内置按键 |
| OLED SCL | PA1 | 软件 I2C | SSD1306 |
| OLED SDA | PA2 | 软件 I2C | SSD1306 |
| USART TX | PA9 | USART1 | → CH340C RXD |
| USART RX | PA10 | USART1 | ← CH340C TXD |

### 电平转换

```
3.3V PWM ──→ 74HCT04 输入 ──→ 74HCT04 输出 (5V) ──→ 100Ω ──→ BNC
```

### FG 分压

```
BNC (5V) ──→ 10kΩ ──┬──→ PA0 (2.5V, 安全)
                     │
                    10kΩ
                     │
                    GND
```

### 物料清单 (约 ¥50)

| 物料 | 型号 | 数量 | 用途 |
|------|------|------|------|
| STM32F103C8T6 | Blue Pill | 1 | 主控 |
| 0.96" OLED | SSD1306 软件 I2C 128×64 | 1 | 显示 |
| 旋转编码器 | EC11 带按键 | 1 | 输入 |
| 74HCT04 | DIP/SOP-14 | 1 | 电平转换 |
| CH340C | SOP-16 | 1 | USB 转串口 |
| Type-C 母座 | 16P 沉板式 | 1 | USB 接口 |
| 晶振 | 12MHz | 1 | CH340C 时钟 |
| BNC 母座 | 焊接式 | 3 | 信号接口 |
| 双排母座 | 2×7P 2.54mm | 1 | 底部扩展排针 |
| 电阻 | 10kΩ×3, 100Ω×2 | 若干 | 信号调理/D+上拉 |
| 电容 | 22pF×2, 100nF×4, 10μF | 若干 | 去耦/滤波/晶振 |

详见 [hardware/schematic.md](hardware/schematic.md) 和 [hardware/BOM.md](hardware/BOM.md)。

---

## 固件

### 目录结构

```
STM32固件/
├── USER/
│   ├── main.c              # 主循环 + 测试状态机
│   ├── stm32f10x_it.c      # 中断服务 (SysTick)
│   └── PWM信号发生器.uvprojx # Keil 工程文件
├── HARDWARE/
│   ├── APP/                # 应用层 (本次开发)
│   │   ├── menu_defs.h     # 类型定义 (枚举/结构体)
│   │   ├── system_config.h # 硬件配置常量
│   │   ├── menu.c/h        # 5 模式菜单状态机
│   │   ├── pwm_engine.c/h  # 双通道 PWM 驱动
│   │   ├── fg_capture.c/h  # 频率计 / RPM 计算
│   │   ├── protocol.c/h    # UART 帧协议
│   │   └── ui_render.c/h   # OLED 5 模式渲染
│   ├── Encoder/            # 旋转编码器 + 双击检测
│   ├── TIMER/              # 定时器底层 (TIM1/2/4)
│   ├── OLED_IIC/           # SSD1306 OLED 驱动
│   └── KEY/LED/            # 简化为仅 PB8 按键
├── SYSTEM/                 # sys/delay/usart (ALIENTEK)
├── CORE/                   # CMSIS core
└── STM32F10x_FWLib/        # 标准外设库 V3.5
```

### PWM 频率分段

| 频率段 | 预分频器 | 计数时钟 | 说明 |
|--------|---------|---------|------|
| 1 ~ 100 Hz | 7199 | 10kHz | 低频精确 |
| 100 ~ 1k Hz | 719 | 100kHz | |
| 1k ~ 10k Hz | 71 | 1MHz | |
| 10k ~ 100k Hz | 7 | 9MHz | 高频 |

### 编译

Keil MDK-ARM 打开 `USER/PWM信号发生器.uvprojx`，编译即可。

---

## 模拟器

### Python PyQt6 模拟器 (`run_simulator.py`)

独立运行的桌面应用，完整仿真 OLED 显示和编码器操作，无需硬件即可验证 UI 交互。

```bash
# 安装依赖
pip install PyQt6 pyserial

# 运行
python simulator/run_simulator.py
```

### 操作方式

| 鼠标操作 | 功能 |
|---------|------|
| 点击旋钮左侧 (◄) | 逆时针 — 减小值 / 上移光标 |
| 点击旋钮右侧 (►) | 顺时针 — 增大值 / 下移光标 |
| 点击中心 (OK) | 短按 — 切换通道使能 |
| 长按中心 (OK) | 进入选择模式 |
| 双击中心 (OK) | 切换工作模式 |
| 键盘 CW/CCW | 旋转编码器 (连接硬件时) |

### 模式切换动画

切换模式时自动播放 **列扫溶解/重组** 动画效果 — 旧界面从左到右逐列溶解消失，新界面从左到右逐列沉降重组。

---

## 通信协议

PC 与 MCU 之间通过 USB CDC (USART1, 115200 baud) 双向通信。

### 帧格式

```
| Header (1B) | CMD (1B) | LEN (1B) | DATA (NB) | CRC8 (1B) |

Header: 0xAA (PC→MCU) / 0xBB (MCU→PC)
CRC8-CCITT: 多项式 0x07, 初始值 0x00, 对 HEADER+CMD+LEN+DATA 计算
```

### 命令列表

| CMD | 方向 | DATA 结构 | 说明 |
|-----|------|----------|------|
| `0x10` | MCU→PC | `StatusData` (25B) | 状态上报 (CH1/CH2/FG/RPM/模式/测试) |
| `0x20` | PC→MCU | `PwmWriteReq` (7B) | 写入 PWM 参数 |
| `0x30` | PC→MCU | `FgDivReq` (1B) | 写入 FG 分频比 |
| `0x41` | PC→MCU | `KeyEventReq` (1B) | 远程按键事件 |
| `0x42` | PC→MCU | `TestConfig` (12B) | 设置测试参数 |
| `0x43` | PC→MCU | (空) | 启动测试 |
| `0x44` | PC→MCU | (空) | 停止测试 |
| `0x50` | PC→MCU | (空) | 请求导出 CSV |
| `0x51` | MCU→PC | CSV 数据块 | CSV 数据块 |
| `0x52` | MCU→PC | (空) | 导出完成 |

### 按键事件映射

| 事件值 | 含义 | 效果 |
|--------|------|------|
| 0 | EVENT_NONE | 无操作 |
| 1 | EVENT_CW | 旋钮顺时针 |
| 2 | EVENT_CCW | 旋钮逆时针 |
| 3 | EVENT_CLICK | 短按 OK |
| 4 | EVENT_LONG_PRESS | 长按 OK |

> 注意: 双击 (EVENT_DOUBLE_CLICK) 由本地检测，不通过串口发送。

---

## 快速开始

### 硬件搭建

1. 按 [schematic.md](hardware/schematic.md) 接线
2. 烧录固件 (使用 ST-Link V2 或 USB-TTL 串口烧录)
3. 上电，OLED 显示 PWM-FG 主界面
4. 旋转编码器调整参数，短按切换输出

### 模拟器运行

```bash
pip install PyQt6
python simulator/run_simulator.py
```

---

## 许可证

MIT License
