/*
 * protocol.c — UART 帧协议驱动
 * ══════════════════════════════════════════════════════════════
 *
 * 【帧格式】
 *   [HEADER 1字节][CMD 1字节][LEN 1字节][DATA N字节][CRC8 1字节]
 *   - PC→MCU: HEADER = 0xAA (FRAME_HEADER_PC2MCU)
 *   - MCU→PC: HEADER = 0xBB (FRAME_HEADER_MCU2PC)
 *   - CRC8 覆盖范围: HEADER + CMD + LEN + DATA (不含尾部 CRC 自身)
 *
 * 【接收架构 — 环形缓冲区 (Ring Buffer)】
 *   USART1 接收中断 (ISR) 将收到的字节写入环形缓冲区 rx_ring[]
 *   主循环 (main loop) 调用 Protocol_Process() 从环形缓冲区逐字节读取并解析帧
 *   - rx_head: ISR 写入位置 (只在中断中修改)
 *   - rx_tail: 主循环读取位置 (只在主循环中修改)
 *   - 缓冲区满时丢弃新数据, 保证不覆盖未读数据
 *   - 头尾指针相等表示缓冲区空
 *
 * 【帧解析状态机 — 5 个状态】
 *   PS_WAIT_HEADER → PS_WAIT_CMD → PS_WAIT_LEN → PS_WAIT_DATA → PS_WAIT_CRC
 *   - 收到 0xAA 后依次解析 CMD、LEN、DATA, 最后校验 CRC
 *   - CRC 校验通过才执行命令, 否则静默丢弃帧
 *   - 任何异常都回到 PS_WAIT_HEADER 重新同步
 *
 * 【发送架构】
 *   主循环中调用 send_frame() 组装帧, 通过 USART 轮询发送
 *   MCU→PC 帧使用 0xBB 作为帧头
 *
 * 【CSV 导出机制】
 *   测试数据通过分块方式发送, 每块 64 字节
 *   CMD_EXPORT_DATA 请求触发, 逐块发送 CMD_EXPORT_CHUNK, 最后发 CMD_EXPORT_DONE
 *   Protocol_ProcessExport() 可在主循环中连续调用完成导出
 */
#include "protocol.h"
#include "menu.h"
#include "system_config.h"
#include "pwm_engine.h"
#include "dac_output.h"
#include "../../SYSTEM/usart/usart.h"
#include <string.h>

/* ── TX 发送缓冲区 ──
 * 用于组帧发送, 最大帧长 = 帧头(1) + CMD(1) + LEN(1) + DATA(最多FRAME_DATA_MAX) + CRC(1)
 * 当前分配 64 字节, 足够容纳所有命令帧
 */
static u8 tx_buf[64];

/* ════════════════════════════════════════════════════════════
 *  RX 环形缓冲区 — 零拷贝接收架构
 * ════════════════════════════════════════════════════════════
 *  - rx_ring[]: 缓冲区数组, 大小由 RX_RING_SIZE 定义 (通常 256 字节)
 *  - rx_head:   ISR 写入位置, 每写入一字节后 (head+1) % SIZE
 *  - rx_tail:   主循环读取位置, 每读取一字节后 (tail+1) % SIZE
 *  - head == tail 表示缓冲区空
 *  - (head+1)%SIZE == tail 表示缓冲区满 (此时丢弃新字节)
 *  - volatile 修饰防止编译器优化, 因为 ISR 和 main 都会访问
 */
static volatile u8 rx_ring[RX_RING_SIZE];
static volatile u16 rx_head = 0;    // 写入位置 (ISR 修改, 主循环只读)
static volatile u16 rx_tail = 0;    // 读取位置 (主循环修改, ISR 只读)

/* ════════════════════════════════════════════════════════════
 *  帧解析状态机 — 5 个状态
 * ════════════════════════════════════════════════════════════
 *  状态流转:
 *    PS_WAIT_HEADER  → 收到 0xAA → PS_WAIT_CMD
 *    PS_WAIT_CMD     → 保存 CMD  → PS_WAIT_LEN
 *    PS_WAIT_LEN     → 保存 LEN:
 *                      LEN > 缓冲区上限 → 回到 PS_WAIT_HEADER (非法)
 *                      LEN == 0        → PS_WAIT_CRC (无数据帧)
 *                      LEN > 0         → PS_WAIT_DATA
 *    PS_WAIT_DATA    → 逐字节存入 frame_data[], 收满后 → PS_WAIT_CRC
 *    PS_WAIT_CRC     → 计算 CRC 并比对:
 *                      匹配 → dispatch_frame() 执行命令
 *                      不匹配 → 静默丢弃
 *                      无论是否匹配, 都回到 PS_WAIT_HEADER
 */
typedef enum {
    PS_WAIT_HEADER,     // 等待帧头 0xAA
    PS_WAIT_CMD,        // 等待命令字节 (1 字节)
    PS_WAIT_LEN,        // 等待数据长度 (1 字节, 0~FRAME_DATA_MAX)
    PS_WAIT_DATA,       // 接收数据载荷 (LEN 字节)
    PS_WAIT_CRC         // 接收 CRC 校验字节 (1 字节)
} ParseState;

static ParseState ps = PS_WAIT_HEADER;  // 当前状态机状态
static u8 frame_cmd;                     // 当前帧命令字
static u8 frame_len;                     // 当前帧数据长度
static u8 frame_data[FRAME_DATA_MAX];    // 当前帧数据缓冲区
static u8 frame_idx;                     // 数据接收索引 (0 ~ frame_len-1)

/* ── 从环形缓冲区读取一字节 ──
 * 调用前必须确认 rx_available() 为真
 * 返回读取的字节, 并将 tail 指针后移一位 (循环回绕)
 */
static u8 rx_pop(void) {
    if (rx_head == rx_tail) return 0;           // 防御性检查: 缓冲区空则返回 0
    u8 b = rx_ring[rx_tail];                    // 读取 tail 位置的数据
    rx_tail = (rx_tail + 1) % RX_RING_SIZE;     // tail 后移, 取模实现环形
    return b;
}

/* ── 检查环形缓冲区是否有待读数据 ──
 * head != tail 表示有至少一个字节未读
 */
static u8 rx_available(void) {
    return (rx_head != rx_tail);
}

/* ════════════════════════════════════════════════════════════
 *  CRC8-CCITT 校验算法
 * ════════════════════════════════════════════════════════════
 *  多项式: 0x07 (即 x^8 + x^2 + x + 1)
 *  初始值: 0x00
 *  输入:   data 指针 + 长度 len
 *  输出:   8 位 CRC 值
 *
 *  算法流程:
 *    1. crc = 0x00
 *    2. 对每个输入字节:
 *       a. crc ^= data[j]         (异或合并)
 *       b. 循环 8 次:
 *          - 如果最高位为 1: crc = (crc << 1) ^ 0x07  (异或多项式)
 *          - 否则:            crc = crc << 1           (仅左移)
 *    3. 返回 crc
 *
 *  注意: 此实现与模拟器 Python 端的 crc8() 完全一致, 保证 PC 端和 MCU 端可互校验
 */
u8 crc8(const u8 *data, u8 len) {
    u8 crc = 0x00;      // CRC 初始值
    u8 i, j;
    for (j = 0; j < len; j++) {
        crc ^= data[j];                             // 将当前字节异或到 CRC 寄存器
        for (i = 0; i < 8; i++) {                   // 处理 8 个位
            if (crc & 0x80)                         // 检查最高位 (bit7)
                crc = (crc << 1) ^ 0x07;            // 最高位为 1: 左移后异或多项式 0x07
            else
                crc = crc << 1;                     // 最高位为 0: 仅左移
        }
    }
    return crc;
}

/* ════════════════════════════════════════════════════════════
 *  USART1 中断服务函数 — 环形缓冲区接收
 * ════════════════════════════════════════════════════════════
 *  覆盖 usart.c 中的 __weak 定义 (标准库中的弱定义函数)
 *  实现零拷贝接收: ISR 直接写入环形缓冲区, 无需中间缓冲
 *
 *  流程:
 *    1. 检查 RXNE (接收缓冲区非空) 标志
 *    2. 读取 USART1->DR 获取接收到的字节
 *    3. 计算下一个写入位置 next = (head+1) % SIZE
 *    4. 如果 next != tail (缓冲区未满), 写入数据并更新 head
 *    5. 如果缓冲区满, 丢弃该字节 (不更新 head, 防止覆盖未读数据)
 *    6. 清除 RXNE 中断标志
 */
void USART1_IRQHandler(void) {
    if (USART_GetITStatus(USART1, USART_IT_RXNE) != RESET) {
        u8 byte = USART_ReceiveData(USART1);                    // 读取接收到的字节
        u16 next = (rx_head + 1) % RX_RING_SIZE;                // 计算下一个写入位置
        if (next != rx_tail) {                                   // 缓冲区未满才写入
            rx_ring[rx_head] = byte;                             // 写入数据
            rx_head = next;                                      // 更新写入指针
        }
        // 如果缓冲区满 (next == tail), 直接丢弃该字节
        USART_ClearITPendingBit(USART1, USART_IT_RXNE);          // 清除中断标志
    }
}

/* ── 协议初始化 ──
 * USART1 已由 usart.c 的 uart_init() 完成硬件初始化
 * 此处确保 RXNE 中断已开启 (可重复调用, USART_ITConfig 内部幂等)
 */
void Protocol_Init(void) {
    USART_ITConfig(USART1, USART_IT_RXNE, ENABLE);
}

/* ════════════════════════════════════════════════════════════
 *  发送辅助函数
 * ════════════════════════════════════════════════════════════ */

/* ── 底层 UART 发送 (轮询方式) ──
 * 逐字节发送, 每发送前等待 TXE (发送缓冲区空) 标志
 * 阻塞式发送, 适用于低频率命令响应场景
 */
static void uart_send(const u8 *data, u16 len) {
    u16 i;
    for (i = 0; i < len; i++) {
        while (USART_GetFlagStatus(USART1, USART_FLAG_TXE) == RESET);  // 等待发送缓冲区空
        USART_SendData(USART1, data[i]);                                // 发送一个字节
    }
}

/* ── 组帧并发送 ──
 * 帧格式: [0xBB][CMD][LEN][...DATA...][CRC8]
 *   - tx_buf[0] = FRAME_HEADER_MCU2PC (0xBB)
 *   - tx_buf[1] = 命令字
 *   - tx_buf[2] = 数据长度
 *   - tx_buf[3..3+LEN-1] = 数据载荷 (可选)
 *   - tx_buf[3+LEN] = CRC8 校验 (覆盖前 3+LEN 字节)
 *   - 总长度 = 4 + LEN 字节
 */
static void send_frame(u8 cmd, const u8 *data, u8 len) {
    tx_buf[0] = FRAME_HEADER_MCU2PC;                    // 帧头 0xBB
    tx_buf[1] = cmd;                                    // 命令字
    tx_buf[2] = len;                                    // 数据长度
    if (len > 0 && data != NULL)
        memcpy(tx_buf + 3, data, len);                  // 拷贝数据载荷
    tx_buf[3 + len] = crc8(tx_buf, 3 + len);            // 计算 CRC (覆盖 HEADER+CMD+LEN+DATA)
    uart_send(tx_buf, 4 + len);                         // 发送完整帧
}

/* ── 发送状态数据帧 ──
 * 命令: CMD_READ_STATUS
 * 数据: StatusData 结构体 (包含频率/占空比/使能/FG/RPM/测试状态等)
 */
void Protocol_SendStatus(StatusData *sd) {
    send_frame(CMD_READ_STATUS, (const u8 *)sd, sizeof(StatusData));
}

/* ── 发送空数据确认帧 (ACK) ──
 * 仅包含 [HEADER][CMD][LEN=0][CRC], 无数据载荷
 */
static void send_ack(u8 cmd) {
    send_frame(cmd, NULL, 0);
}

/* ════════════════════════════════════════════════════════════
 *  命令处理函数 — 每个命令对应一个 handler
 * ════════════════════════════════════════════════════════════ */

/* ── CMD_WRITE_PWM: PC 写入 PWM 参数 ──
 * 数据: PwmWriteReq 结构体 {channel, freq_hz, duty_pct, enable}
 * 流程:
 *   1. 校验数据长度 == sizeof(PwmWriteReq)
 *   2. 根据 channel (1/2) 写入 g_params 对应的频率/占空比/使能
 *   3. 调用 PWM_UpdateFromParams() 更新定时器输出
 *   4. 标记 dirty=1 通知 UI 刷新
 *   5. 回复 ACK
 */
static void handle_write_pwm(void) {
    if (frame_len != sizeof(PwmWriteReq)) return;       // 长度校验
    PwmWriteReq *req = (PwmWriteReq *)frame_data;       // 将原始数据解释为结构体

    if (req->channel == 1) {
        g_params.ch1_freq_hz = req->freq_hz;            // 设置 CH1 频率
        g_params.ch1_duty_pct = req->duty_pct;          // 设置 CH1 占空比
        g_params.ch1_enabled = req->enable;              // 设置 CH1 使能
    } else if (req->channel == 2) {
        g_params.ch2_freq_hz = req->freq_hz;            // 设置 CH2 频率
        g_params.ch2_duty_pct = req->duty_pct;          // 设置 CH2 占空比
        g_params.ch2_enabled = req->enable;              // 设置 CH2 使能
    }
    PWM_UpdateFromParams(&g_params);                     // 更新 PWM 定时器输出
    g_menu.dirty = 1;                                    // 标记 UI 需要刷新
    send_ack(CMD_WRITE_PWM);                             // 回复写入确认
}

/* ── CMD_WRITE_FG_DIV: PC 设置 FG 分频系数 ──
 * 数据: FgDivReq 结构体 {div}
 * fg_div 范围: 1~99
 * 修改后标记 dirty, 由 main.c 更新 FG 输出
 */
static void handle_write_fg_div(void) {
    if (frame_len != sizeof(FgDivReq)) return;
    FgDivReq *req = (FgDivReq *)frame_data;
    if (req->div >= 1 && req->div <= 99)                // 边界检查
        g_params.fg_div = req->div;
    g_menu.dirty = 1;
    send_ack(CMD_WRITE_FG_DIV);
}

/* ── CMD_KEY_EVENT: PC 模拟按键事件 ──
 * 数据: KeyEventReq 结构体 {event}
 * event 值: EVENT_CLICK / EVENT_DOUBLE_CLICK / EVENT_LONG_PRESS / EVENT_CW / EVENT_CCW
 * 流程:
 *   1. 校验 event 值合法
 *   2. 调用 Menu_Process() 处理事件 (模拟旋转编码器/按键)
 *   3. 如果 menu 标记 dirty, 调用 PWM_UpdateFromParams() 同步输出
 *   4. 回复 ACK
 * 用途: PC 端 GUI 可远程控制设备的所有操作
 */
static void handle_key_event(void) {
    if (frame_len != sizeof(KeyEventReq)) return;
    KeyEventReq *req = (KeyEventReq *)frame_data;
    if (req->event <= EVENT_LONG_PRESS) {               // 校验事件值合法
        Menu_Process((InputEvent)req->event);            // 传递给菜单状态机
        if (g_menu.dirty) {                              // 如果参数被修改
            PWM_UpdateFromParams(&g_params);             // 更新 PWM 输出
            g_menu.dirty = 0;                            // 清除脏标志
        }
    }
    send_ack(CMD_KEY_EVENT);
}

/* ── CMD_READ_STATUS: PC 查询当前系统状态 ──
 * 无输入数据
 * 输出: StatusData 结构体, 包含:
 *   - CH1/CH2 频率、占空比、使能状态
 *   - FG 频率 (mHz)、分频系数、RPM
 *   - 当前模式、测试状态、循环计数
 * 注意: fg_freq_mhz 和 rpm 由 main.c 实时更新, 此处读取全局变量即可
 */
static void handle_read_status(void) {
    StatusData sd;
    sd.ch1_freq_hz   = g_params.ch1_freq_hz;            // CH1 频率 (Hz)
    sd.ch1_duty_pct  = g_params.ch1_duty_pct;           // CH1 占空比 (%)
    sd.ch1_enabled   = g_params.ch1_enabled;             // CH1 使能标志
    sd.ch2_freq_hz   = g_params.ch2_freq_hz;            // CH2 频率 (Hz)
    sd.ch2_duty_pct  = g_params.ch2_duty_pct;           // CH2 占空比 (%)
    sd.ch2_enabled   = g_params.ch2_enabled;             // CH2 使能标志
    sd.fg_freq_mhz   = 0;                               // FG 频率 (由 main.c 填入)
    sd.fg_div        = g_params.fg_div;                  // FG 分频系数
    sd.rpm           = 0;                                // RPM (由 main.c 填入)
    sd.mode          = g_menu.mode;                      // 当前菜单模式
    sd.test_state    = Menu_IsTestRunning() ? 1 : 0;    // 测试运行状态
    sd.test_cycle    = 0;                                // 当前测试循环 (由 main.c 填入)
    sd.test_total    = Menu_GetTestCycles();             // 测试总循环数
    sd.vsp_voltage_x10 = g_params.vsp_voltage_x10;      // VSP 电压 ×10
    sd.vsp_enabled     = g_params.vsp_enabled;           // VSP 使能
    sd.test_on_method  = g_params.test_on_method;        // 测试 ON 方式
    send_frame(CMD_READ_STATUS, (const u8 *)&sd, sizeof(StatusData));
}

/* ── CMD_SET_TEST: PC 设置测试参数 ──
 * 数据: TestConfig 结构体 {channel, freq_hz, duty_pct, cycles, on_time_sec, off_time_sec}
 * 委托给 Menu_SetTestConfig() 处理 (含边界检查)
 */
static void handle_set_test(void) {
    if (frame_len != sizeof(TestConfig)) return;
    TestConfig *cfg = (TestConfig *)frame_data;
    Menu_SetTestConfig(cfg);                             // 设置测试参数 (含边界校验)
    send_ack(CMD_SET_TEST);
}

/* ── CMD_START_TEST: PC 启动自动测试 ──
 * 调用 Menu_StartTest(), 将 test_running 置 1, 清空记录和导出索引
 */
static void handle_start_test(void) {
    Menu_StartTest();
    send_ack(CMD_START_TEST);
}

/* ── CMD_STOP_TEST: PC 停止自动测试 ──
 * 调用 Menu_StopTest(), 将 test_running 清零
 */
static void handle_stop_test(void) {
    Menu_StopTest();
    send_ack(CMD_STOP_TEST);
}

/* ── CMD_EXPORT_DATA: PC 请求导出测试 CSV 数据 ──
 * 分块发送机制:
 *   1. 调用 Menu_FormatExportChunk() 获取一块数据 (最多 64 字节)
 *   2. 如果有数据 (len > 0), 发送 CMD_EXPORT_CHUNK 帧
 *   3. 如果无数据 (len == 0), 发送 CMD_EXPORT_DONE 帧表示导出结束
 * 注意: 此函数每次只发送一块, 完整导出需要多次调用或使用 Protocol_ProcessExport()
 */
static void handle_export_data(void) {
    u8 chunk[64];                                        // 分块缓冲区
    u8 len = Menu_FormatExportChunk(chunk, sizeof(chunk));  // 格式化一块 CSV 数据
    if (len > 0) {
        send_frame(CMD_EXPORT_CHUNK, chunk, len);        // 发送数据块
    } else {
        send_frame(CMD_EXPORT_DONE, NULL, 0);            // 导出完成
    }
}

/* ── 主循环连续导出 ──
 * 在主循环中反复调用, 每次发送一块 CSV 数据, 直到全部导出完毕
 * 使用 Menu_ExportDone() 检查是否已完成, 避免重复发送
 */
void Protocol_ProcessExport(void) {
    if (Menu_ExportDone()) return;                       // 已导出完毕, 跳过
    u8 chunk[64];
    u8 len = Menu_FormatExportChunk(chunk, sizeof(chunk));
    if (len > 0) {
        send_frame(CMD_EXPORT_CHUNK, chunk, len);        // 发送数据块
    } else {
        send_frame(CMD_EXPORT_DONE, NULL, 0);            // 发送完成标志
    }
}

/* ── CMD_WRITE_VSP: PC 写入 VSP 参数 ──
 * 数据: VspWriteReq 结构体 {voltage_x10, enabled}
 * voltage_x10 范围: 0~50 (0.0~5.0V)
 * 直接调用 DAC 输出并更新 g_params
 */
static void handle_write_vsp(void) {
    if (frame_len != sizeof(VspWriteReq)) return;
    VspWriteReq *req = (VspWriteReq *)frame_data;

    if (req->voltage_x10 <= 50)
        g_params.vsp_voltage_x10 = req->voltage_x10;
    g_params.vsp_enabled = req->enabled ? 1 : 0;

    if (g_params.vsp_enabled) {
        DAC_Output_SetVoltage(g_params.vsp_voltage_x10);
    } else {
        DAC_Output_Off();
    }
    g_menu.dirty = 1;
    send_ack(CMD_WRITE_VSP);
}

/* ════════════════════════════════════════════════════════════
 *  命令分发器 — 根据 CMD 字节调用对应的处理函数
 * ════════════════════════════════════════════════════════════
 *  支持的命令:
 *    CMD_READ_STATUS  (0x01) — 查询状态
 *    CMD_WRITE_PWM    (0x02) — 写入 PWM 参数
 *    CMD_WRITE_FG_DIV (0x03) — 设置 FG 分频
 *    CMD_KEY_EVENT    (0x04) — 模拟按键
 *    CMD_SET_TEST     (0x05) — 设置测试参数
 *    CMD_START_TEST   (0x06) — 启动测试
 *    CMD_STOP_TEST    (0x07) — 停止测试
 *    CMD_EXPORT_DATA  (0x08) — 导出 CSV 数据
 */
static void dispatch_frame(void) {
    switch (frame_cmd) {
        case CMD_READ_STATUS:  handle_read_status();   break;  // 查询状态
        case CMD_WRITE_PWM:    handle_write_pwm();     break;  // 写入 PWM
        case CMD_WRITE_FG_DIV: handle_write_fg_div();  break;  // 设置 FG 分频
        case CMD_KEY_EVENT:    handle_key_event();      break;  // 按键事件
        case CMD_SET_TEST:     handle_set_test();       break;  // 设置测试
        case CMD_START_TEST:   handle_start_test();     break;  // 启动测试
        case CMD_STOP_TEST:    handle_stop_test();      break;  // 停止测试
        case CMD_EXPORT_DATA:  handle_export_data();    break;  // 导出数据
        case CMD_WRITE_VSP:    handle_write_vsp();      break;  // 写入 VSP
        default: break;                                         // 未知命令, 忽略
    }
}

/* ════════════════════════════════════════════════════════════
 *  帧解析主函数 — 主循环中调用
 * ════════════════════════════════════════════════════════════
 *  从环形缓冲区逐字节消费, 通过 5 状态机提取完整帧
 *
 *  状态机流程:
 *    [PS_WAIT_HEADER] ──收到 0xAA──→ [PS_WAIT_CMD]
 *    [PS_WAIT_CMD] ──保存 cmd──→ [PS_WAIT_LEN]
 *    [PS_WAIT_LEN] ──len==0──→ [PS_WAIT_CRC]
 *                 ──len>0 且合法──→ [PS_WAIT_DATA]
 *                 ──len>缓冲区上限──→ [PS_WAIT_HEADER] (丢弃)
 *    [PS_WAIT_DATA] ──收满 len 字节──→ [PS_WAIT_CRC]
 *    [PS_WAIT_CRC] ──CRC 正确──→ dispatch_frame()
 *                ──CRC 错误──→ 静默丢弃
 *                ──无论结果──→ [PS_WAIT_HEADER]
 *
 *  注意: 此函数应被主循环频繁调用 (如每 1~10ms 一次)
 *  以确保环形缓冲区不会溢出
 */
void Protocol_Process(void) {
    while (rx_available()) {                             // 循环处理所有待读字节
        u8 b = rx_pop();                                 // 从环形缓冲区取一个字节

        switch (ps) {

            /* ── 状态 1: 等待帧头 ──
             * 在数据流中扫描 0xAA, 非帧头字节全部丢弃
             * 这样即使通信中丢失了部分帧数据, 也能自动重新同步
             */
            case PS_WAIT_HEADER:
                if (b == FRAME_HEADER_PC2MCU) {          // 收到帧头 0xAA
                    ps = PS_WAIT_CMD;                    // 进入等待命令状态
                }
                // 非帧头字节直接丢弃 (保持 PS_WAIT_HEADER)
                break;

            /* ── 状态 2: 等待命令字节 ──
             * 任何字节都是合法的命令字, 直接保存
             * 后续在 dispatch_frame() 中校验命令是否合法
             */
            case PS_WAIT_CMD:
                frame_cmd = b;                           // 保存命令字节
                ps = PS_WAIT_LEN;                        // 进入等待长度状态
                break;

            /* ── 状态 3: 等待数据长度 ──
             * 三种情况:
             *   LEN > FRAME_DATA_MAX: 非法长度, 回到帧头等待
             *   LEN == 0: 无数据帧, 直接跳到 CRC 等待
             *   LEN > 0 且合法: 进入数据接收状态, 初始化索引
             */
            case PS_WAIT_LEN:
                frame_len = b;                           // 保存数据长度
                if (frame_len > sizeof(frame_data)) {
                    ps = PS_WAIT_HEADER;                 // 长度超出缓冲区, 重置状态机
                } else if (frame_len == 0) {
                    ps = PS_WAIT_CRC;                    // 无数据帧, 直接等 CRC
                } else {
                    frame_idx = 0;                       // 初始化数据接收索引
                    ps = PS_WAIT_DATA;                   // 进入数据接收状态
                }
                break;

            /* ── 状态 4: 接收数据载荷 ──
             * 逐字节存入 frame_data[], 直到收满 frame_len 字节
             * 收满后进入 CRC 等待状态
             */
            case PS_WAIT_DATA:
                frame_data[frame_idx++] = b;             // 存入数据缓冲区
                if (frame_idx >= frame_len) {
                    ps = PS_WAIT_CRC;                    // 数据收满, 等待 CRC
                }
                break;

            /* ── 状态 5: CRC 校验 ──
             * 1. 将收到的 HEADER+CMD+LEN+DATA 重新组装到 tx_buf
             * 2. 调用 crc8() 计算校验值
             * 3. 与收到的 CRC 字节 (b) 比较:
             *    - 匹配: 调用 dispatch_frame() 执行命令
             *    - 不匹配: 静默丢弃 (不回复错误, 等待 PC 超时重发)
             * 4. 无论结果, 回到 PS_WAIT_HEADER 等待下一帧
             */
            case PS_WAIT_CRC: {
                // 重新组装帧数据用于 CRC 校验
                tx_buf[0] = FRAME_HEADER_PC2MCU;         // 帧头 0xAA
                tx_buf[1] = frame_cmd;                   // 命令字节
                tx_buf[2] = frame_len;                   // 长度字节
                if (frame_len > 0)
                    memcpy(tx_buf + 3, frame_data, frame_len);  // 数据载荷
                u8 calc = crc8(tx_buf, 3 + frame_len);   // 计算 CRC (覆盖 HEADER+CMD+LEN+DATA)
                if (calc == b) {
                    dispatch_frame();                    // CRC 匹配, 执行命令
                }
                // CRC 不匹配则静默丢弃
                ps = PS_WAIT_HEADER;                     // 回到初始状态, 等待下一帧
                break;
            }
        }
    }
}
