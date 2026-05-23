# PWM 信号发生器

基于 **STM32F103C8T6** 的双通道 PWM 信号发生器，配备 0.96" OLED 显示屏和旋转编码器。

支持 1Hz~100kHz 频率输出、FG 信号采集与 RPM 显示、VSP 模拟电压输出 (0~5V, 0.01V 步进)、自动测试模式 (PWM/继电器 ON/OFF 循环 + 数据记录 + CSV 导出)。附带 **Python PyQt6 桌面模拟器**，可在 PC 上仿真全部操作界面并通过串口连接实物。

---

## 功能特性

### 6 种工作模式

双击旋转编码器循环切换：**PWM-FG → FG → CH1 → CH2 → VSP → TEST**

| 模式 | 说明 |
|------|------|
| **PWM-FG** | 双通道 PWM 参数调节 + FG RPM 实时显示 (默认) |
| **FG** | 纯频率计模式，大号 RPM 数字显示 |
| **CH1** | CH1 单通道 PWM 参数调节 (频率/占空比/使能) |
| **CH2** | CH2 单通道 PWM 参数调节 (频率/占空比/使能) |
| **VSP** | VSP 模拟电压输出，PA4 DAC 经运放放大至 0~5V，0.01V 步进 |
| **TEST** | 自动测试模式，ON/OFF 循环 + RPM 采样 + 数据记录 + CSV 导出 |

### 双通道 PWM 输出

- 频率范围 1Hz~100kHz，连续可调
- 占空比 0%~100%，1% 步进
- CH1 (PA8 TIM1) / CH2 (PB6 TIM4) 独立使能
- 经 74HCT04 转换为 5V TTL 电平，BNC 接口输出

### VSP 模拟电压输出

- PA4 DAC 通道1 (12 位精度)，经 MCP6002 运放 ×1.515 放大到 0~5V
- 电压分辨率 0.01V (存储值 ×100, 0~500)
- OLED 显示 "X.XXV"，带进度条和使能开关
- 可通过串口命令 `CMD_WRITE_VSP` 远程设置

### FG 信号采集

- PA0 TIM2 输入捕获，1μs 分辨率
- 支持硬件分频 (1~99)
- 实时 RPM 计算并显示
- 5V 输入经 10k+10k 电阻分压安全接入

### 自动测试模式

- 配置项：通道 / 频率 / 占空比 / 循环次数 / ON 时间 / OFF 时间 / ON 方式
- ON 方式：PWM 开关 (0) / 继电器通断 (1) / 两者同时 (2)
- 继电器控制引脚 PB5，高电平吸合
- 每轮 ON 阶段采样 RPM (最大值、平均值)
- 异常判定：RPM=0 或偏差 >20% 标记为异常
- 测试记录存储 (最多 200 条)
- CSV 数据导出 (串口分块传输)

### OLED 显示与交互

- 128×64 SSD1306，软件 I2C (PA1/PA2)
- 每种模式独立界面布局
- 选择模式下光标 2Hz 闪烁指示当前选中项
- 模式切换时播放列扫溶解/重组动画

### 旋转编码器操作

| 操作 | 功能 |
|------|------|
| 旋转 | 修改当前参数值 |
| 短按 | 切换通道使能 / 测试中停止 |
| 长按 | 进入选择模式 (旋转移动光标，短按退出) |
| 双击 | 切换工作模式 |

---

## 系统架构

```
┌──────────────────────────────────────────────────────┐
│           PC (Python PyQt6 模拟器)                    │
│  ┌────────────────────────────────────────────────┐  │
│  │  OLED 128×64 仿真 (4x 缩放)                    │  │
│  │  旋钮编码器模拟 (鼠标/键盘)                     │  │
│  │  6 模式切换 / 串口控制 / CSV 导出              │  │
│  └────────────────────────────────────────────────┘  │
│                  │ USB CDC (115200)                   │
└──────────────────┼───────────────────────────────────┘
                   │ Type-C
┌──────────────────┼───────────────────────────────────┐
│  CH340C ── USART1                                    │
│                                                      │
│  STM32F103C8T6 (72MHz, 20KB SRAM, 64KB Flash)       │
│                                                      │
│  PA8 ── TIM1 CH1 ── 74HCT04 ── BNC1 (PWM OUT1)      │
│  PB6 ── TIM4 CH1 ── 74HCT04 ── BNC2 (PWM OUT2)      │
│  PA0 ── TIM2 CH1 ◄── 分压 ◄── BNC3 (FG IN)          │
│  PA4 ── DAC CH1  ── 运放 ×1.515 ── VSP (0~5V)       │
│  PB5 ── GPIO ── 继电器驱动 ── 15V 通断控制           │
│                                                      │
│  PA1/PA2 ── 软件 I2C ── SSD1306 OLED                │
│  PA6/PA7 ── TIM3 编码器模式 ── 旋钮编码器 EC11      │
│  PB8 ── GPIO (内部上拉) ── OK 按键                   │
│                                                      │
│  底部 2×7 排针 ── PA3/4/5, PB0/1/5/7/9/10/11/12     │
│                  + 5V + GND (扩展板接口)              │
└──────────────────────────────────────────────────────┘
```

---

## 硬件设计

### 引脚分配

| 功能 | 引脚 | 外设 | 说明 |
|------|------|------|------|
| PWM OUT1 | PA8 | TIM1 CH1 | → 74HCT04 → BNC1 |
| PWM OUT2 | PB6 | TIM4 CH1 | → 74HCT04 → BNC2 |
| FG IN | PA0 | TIM2 CH1 | ← 10k+10k 分压 ← BNC3 |
| VSP DAC | PA4 | DAC CH1 | → MCP6002 运放 → VSP OUT |
| 继电器 | PB5 | GPIO | → S8050 驱动 → 继电器 |
| 编码器 A | PA6 | TIM3 CH1 | EC11 旋转编码器 |
| 编码器 B | PA7 | TIM3 CH2 | EC11 旋转编码器 |
| OK 按键 | PB8 | GPIO (上拉) | 编码器内置按键 |
| OLED SCL | PA1 | 软件 I2C | SSD1306 |
| OLED SDA | PA2 | 软件 I2C | SSD1306 |
| USART TX | PA9 | USART1 | → CH340C RXD |
| USART RX | PA10 | USART1 | ← CH340C TXD |

### 电平转换

```
3.3V PWM ──→ 74HCT04 ──→ 5V TTL ──→ 100Ω ──→ BNC
```

### FG 分压

```
BNC (5V) ──→ 10kΩ ──┬──→ PA0 (2.5V)
                     └──→ 10kΩ ──→ GND
```

### DAC 放大 (3.3V → 5V)

```
PA4 (0~3.3V) ──→ R1(10k)─GND ──→ (+) MCP6002 ──→ VSP OUT (0~5V)
                                  (-) ──┬──→ R2(5.1k) ──→ VSP OUT
                                        └──→ R1(10k) ──→ GND

增益 = 1 + 5.1/10 = 1.515
```

### 扩展板 (6Pin 3.96mm 连接器)

| Pin | 信号 | 说明 |
|-----|------|------|
| 1 | 310V | 整流后高压直流 |
| 2 | NC | 空脚 |
| 3 | GND | 共地 |
| 4 | 15V | DC-DC 输出 (经继电器控制) |
| 5 | VSP | 0~5V 模拟电压 (DAC 放大后) |
| 6 | FG | 转速反馈信号 |

扩展板集成：整流桥 KBP210、HLK-2M24 AC-DC、LM2596 DC-DC、SRD-05VDC 继电器、MCP6002 运放。

详见 [hardware/expansion_board.md](hardware/expansion_board.md)。

### 物料清单 (约 ¥50)

| 物料 | 型号 | 数量 | 用途 |
|------|------|------|------|
| MCU | STM32F103C8T6 (Blue Pill) | 1 | 主控 |
| OLED | SSD1306 0.96" 128×64 | 1 | 显示 |
| 编码器 | EC11 带按键 | 1 | 输入 |
| 电平转换 | 74HCT04 | 1 | 3.3V→5V |
| USB 转串口 | CH340C | 1 | 通信 |
| 运放 | MCP6002 (SOT-23-8) | 1 | DAC 放大 |
| BNC 母座 | 焊接式 | 3 | 信号接口 |
| 排母 | 2×7P 2.54mm | 1 | 扩展接口 |
| 电阻 | 10kΩ×3, 5.1kΩ×1, 100Ω×2 | 若干 | 分压/反馈/限流 |
| 电容 | 22pF×2, 100nF×4, 10μF | 若干 | 去耦/滤波/晶振 |

详见 [hardware/BOM.md](hardware/BOM.md) 和 [hardware/schematic.md](hardware/schematic.md)。

---

## 固件

### 目录结构

```
STM32固件/                          ← Keil MDK-ARM 工程
├── USER/
│   ├── main.c                      主循环 (5 个并发任务) + 测试状态机
│   ├── stm32f10x_it.c              中断服务 (SysTick 1ms)
│   └── PWM信号发生器.uvprojx        Keil 工程文件
├── HARDWARE/
│   ├── APP/                        应用层 (核心代码)
│   │   ├── menu_defs.h             类型定义 (枚举/结构体/协议常量)
│   │   ├── system_config.h         硬件配置常量
│   │   ├── menu.c / menu.h         6 模式菜单状态机
│   │   ├── pwm_engine.c / .h       双通道 PWM 驱动 (4 频率段自动分频)
│   │   ├── fg_capture.c / .h       频率计 / RPM 计算
│   │   ├── protocol.c / .h         UART 帧协议 (环形缓冲区 + CRC8)
│   │   ├── ui_render.c / .h        OLED 6 模式渲染
│   │   └── dac_output.c / .h       DAC 模拟电压输出驱动
│   ├── Encoder/                    旋转编码器 + 双击检测
│   ├── TIMER/                      定时器底层 (TIM1/2/3/4)
│   ├── OLED_IIC/                   SSD1306 OLED 软件 I2C 驱动
│   ├── DAC/                        STM32 DAC 底层驱动
│   └── KEY/LED/                    简化为 PB8 按键
├── SYSTEM/                         sys / delay / usart (ALIENTEK 库)
├── CORE/                           CMSIS core + startup
└── STM32F10x_FWLib/                ST 标准外设库 V3.5
```

### 主循环架构 (裸机轮询, 非 RTOS)

| 任务 | 周期 | 说明 |
|------|------|------|
| 串口协议处理 | 每次循环 | 解析上位机命令帧 |
| 编码器轮询 + 菜单 | 每次循环 | 实时响应旋钮 |
| 测试状态机 | 每次循环 | ON/OFF 循环计时 |
| OLED 渲染 | 50ms (20fps) | 刷新屏幕 |
| 状态上报 | 500ms (2Hz) | 串口心跳包 |

### PWM 频率分段

| 频率段 | 预分频 | 计数时钟 | 分辨率 |
|--------|--------|---------|--------|
| 1~100 Hz | ÷7200 | 10kHz | 0.1Hz |
| 100~1k Hz | ÷720 | 100kHz | 1Hz |
| 1k~10k Hz | ÷72 | 1MHz | 10Hz |
| 10k~100k Hz | ÷8 | 9MHz | 100Hz |

### 编译

Keil MDK-ARM 打开 `STM32固件/USER/PWM信号发生器.uvprojx`，编译即可。

---

## 模拟器

### Python PyQt6 桌面应用 (`simulator/run_simulator.py`)

完整仿真 OLED 显示和编码器操作，支持串口连接实物硬件同步控制。

```bash
pip install PyQt6 pyserial
python simulator/run_simulator.py
```

也可直接运行项目根目录的 `PWM信号发生器.exe` (PyInstaller 打包)。

### 操作方式

| 操作 | 鼠标 | 键盘 |
|------|------|------|
| 旋转 CW | 点击旋钮右侧 | ↑ / W |
| 旋转 CCW | 点击旋钮左侧 | ↓ / S |
| 短按 OK | 点击旋钮中心 | Enter / Space / Esc |
| 长按 OK | 长按旋钮中心 | L |
| 双击 OK | 双击旋钮中心 | M |
| 跳转模式 | — | 数字键 1~6 |

### 特性

- OLED 128×64 像素渲染，4x 缩放显示
- 模式切换时列扫溶解/重组动画
- 串口自动同步参数 (PWM / FG / VSP)
- 测试数据 CSV 导出 (弹窗保存)
- 长按 800ms / 双击 400ms 延迟检测

---

## 通信协议

PC 与 MCU 通过 USB CDC (USART1, 115200 baud) 双向通信。

### 帧格式

```
[HEADER 1B][CMD 1B][LEN 1B][DATA NB][CRC8 1B]

PC→MCU: HEADER = 0xAA
MCU→PC: HEADER = 0xBB
CRC8-CCITT: 多项式 0x07, 初始值 0x00, 覆盖 HEADER+CMD+LEN+DATA
```

### 命令列表

| CMD | 方向 | 数据结构 | 大小 | 说明 |
|-----|------|---------|------|------|
| `0x10` | MCU→PC | `StatusData` | 29B | 状态上报 (频率/占空比/RPM/VSP/测试) |
| `0x20` | PC→MCU | `PwmWriteReq` | 7B | 写入 PWM 参数 |
| `0x30` | PC→MCU | `FgDivReq` | 1B | 设置 FG 分频比 |
| `0x41` | PC→MCU | `KeyEventReq` | 1B | 远程按键事件 |
| `0x42` | PC→MCU | `TestConfig` | 13B | 设置测试参数 |
| `0x43` | PC→MCU | — | 0B | 启动测试 |
| `0x44` | PC→MCU | — | 0B | 停止测试 |
| `0x50` | PC→MCU | — | 0B | 请求导出 CSV |
| `0x51` | MCU→PC | CSV 数据块 | ≤64B | CSV 数据块 |
| `0x52` | MCU→PC | — | 0B | 导出完成 |
| `0x60` | PC→MCU | `VspWriteReq` | 3B | 写入 VSP 参数 |

### StatusData 字段 (29 字节)

| 偏移 | 字段 | 类型 | 说明 |
|------|------|------|------|
| 0 | ch1_freq_hz | u32 | CH1 频率 (Hz) |
| 4 | ch1_duty_pct | u8 | CH1 占空比 (%) |
| 5 | ch1_enabled | u8 | CH1 使能 |
| 6 | ch2_freq_hz | u32 | CH2 频率 (Hz) |
| 10 | ch2_duty_pct | u8 | CH2 占空比 (%) |
| 11 | ch2_enabled | u8 | CH2 使能 |
| 12 | fg_freq_mhz | u32 | FG 频率 (毫赫兹) |
| 16 | fg_div | u8 | FG 分频系数 |
| 17 | rpm | u16 | 实时转速 |
| 19 | mode | u8 | 当前模式 (0~5) |
| 20 | test_state | u8 | 测试状态 (0=空闲 1=运行 2=完成) |
| 21 | test_cycle | u16 | 当前循环编号 |
| 23 | test_total | u16 | 总循环数 |
| 25 | vsp_voltage_x100 | u16 | VSP 电压 ×100 (0~500) |
| 27 | vsp_enabled | u8 | VSP 使能 |
| 28 | test_on_method | u8 | 测试 ON 方式 (0=PWM 1=继电器 2=两者) |

### 按键事件

| 值 | 事件 | 效果 |
|----|------|------|
| 0 | EVENT_NONE | 无操作 |
| 1 | EVENT_CW | 顺时针旋转 |
| 2 | EVENT_CCW | 逆时针旋转 |
| 3 | EVENT_CLICK | 短按 OK |
| 4 | EVENT_LONG_PRESS | 长按 OK |

> 双击 (EVENT_DOUBLE_CLICK) 由本地检测，不通过串口发送。

---

## 快速开始

### 硬件

1. 按 [schematic.md](hardware/schematic.md) 焊接并接线
2. 用 ST-Link V2 或串口烧录固件
3. 上电后 OLED 显示 PWM-FG 主界面
4. 旋转编码器调整参数，短按切换输出

### 模拟器

```bash
pip install PyQt6 pyserial
python simulator/run_simulator.py
```

或直接双击 `PWM信号发生器.exe`。

### 连接实物

1. USB Type-C 连接主控板
2. 模拟器选择对应 COM 口，点击「连接」
3. 模拟器参数自动与硬件同步

---

## 项目结构

```
PWM信号发生器/
├── STM32固件/              Keil MDK-ARM 工程 (实际编译的固件)
│   ├── HARDWARE/APP/       应用层核心代码 (6 模式菜单/协议/UI/驱动)
│   ├── USER/               main.c + Keil 工程文件
│   └── ...                 外设库 / 系统驱动 / OLED / 编码器
├── simulator/
│   └── run_simulator.py    PyQt6 桌面模拟器 (单文件)
├── common/
│   └── protocol_defs.h     共享协议定义 (固件 + 模拟器)
├── hardware/               硬件文档 (原理图 / BOM / 扩展板 / 架构)
├── PWM信号发生器.exe        模拟器可执行文件
└── README.md
```

## 许可证

MIT License
