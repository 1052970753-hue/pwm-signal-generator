/**
 * ============================================================================
 *  protocol.h — UART 串口通信帧协议接口
 * ============================================================================
 *
 *  【模块概述】
 *   本模块实现了 MCU 与 PC 上位机之间的二进制帧协议通信.
 *   支持三种通信方向: 状态上报(MCU→PC), 命令下发(PC→MCU), CSV导出(MCU→PC).
 *
 *  【帧格式】
 *   ┌─────────┬─────┬─────┬──────────┬───────┐
 *   │ HEADER  │ CMD │ LEN │   DATA   │ CRC8  │
 *   │  1 Byte │ 1B  │ 1B  │  0~32 B  │  1B   │
 *   └─────────┴─────┴─────┴──────────┴───────┘
 *
 *   字段说明:
 *     HEADER — 帧头标识, 固定值 0xA5, 用于帧同步
 *     CMD    — 命令字, 定义帧的类型和方向 (见下方 CMD 表)
 *     LEN    — DATA 字段的有效字节数 (0~32)
 *     DATA   — 数据载荷, 长度由 LEN 指定
 *     CRC8   — CRC8-CCITT 校验值, 覆盖 HEADER+CMD+LEN+DATA 全部字节
 *
 *  【CRC8-CCITT 算法说明】
 *   多项式: x^8 + x^2 + x + 1 (0x07)
 *   初始值: 0x00
 *   异或输入: 否
 *   异或输出: 否
 *
 *   计算过程 (逐字节):
 *     1. crc = 0x00
 *     2. 对每个数据字节 byte:
 *        a. crc ^= byte
 *        b. 重复 8 次:
 *           - 如果 crc 最高位为1: crc = (crc << 1) ^ 0x07
 *           - 否则: crc = crc << 1
 *     3. 最终 crc 即为 CRC8 校验值
 *
 *   示例: HEADER=0xA5, CMD=0x01, LEN=0x00 (无数据)
 *     CRC8(0xA5, 0x01, 0x00) = 计算结果
 *
 *  【命令字定义】(常见, 详见 protocol.c)
 *   MCU → PC:
 *     STATUS上报  — 周期性发送当前频率/占空比/RPM/状态
 *     CSV数据行   — 测试完成后逐条发送CSV记录
 *     响应帧      — 对PC命令的确认/拒绝回复
 *
 *   PC → MCU:
 *     查询状态    — 请求MCU立即上报一次状态
 *     设置参数    — 修改频率/占空比/使能
 *     测试配置    — 下发自动测试参数
 *     启动/停止   — 控制自动测试
 *     请求导出    — 触发CSV数据导出
 *
 *  【状态上报帧 DATA 格式 (STATUS)】
 *   ┌────────┬────────┬────────┬──────┬──────┬───────┬─────────┐
 *   │ ch1_hz │ ch2_hz │ ch1_du │ch2_du│ch1_en│ ch2_en│  rpm    │
 *   │  4B LE │ 4B LE  │  1B    │ 1B   │  1B  │  1B   │  2B LE  │
 *   └────────┴────────┴────────┴──────┴──────┴───────┴─────────┘
 *   LE = 小端字节序 (Little-Endian)
 *
 *  【使用流程】
 *   1. 系统启动调用 Protocol_Init() (实际上 UART 由 usart.c 初始化, 此函数仅重置状态)
 *   2. 主循环中每 500ms 调用 Protocol_SendStatus() 上报状态
 *   3. 主循环中每帧调用 Protocol_Process() 解析接收缓冲区中的命令
 *   4. 测试完成时调用 Protocol_ProcessExport() 逐块发送CSV数据
 *
 * ============================================================================
 */
#ifndef PROTOCOL_H
#define PROTOCOL_H

#include "sys.h"
#include "menu_defs.h"

// 协议模块初始化
// 功能: 重置协议解析器状态, 清空接收缓冲区
// 注意: UART 硬件初始化由 usart.c 中的 uart_init() 完成, 本函数不涉及硬件配置
// 调用时机: 系统启动时, 在 uart_init() 之后调用
void Protocol_Init(void);

// 发送状态上报帧 (MCU → PC)
// 参数: status — 指向 StatusData 结构体的指针, 包含当前CH1/CH2频率/占空比/使能/RPM
// 帧格式: [0xA5][CMD_STATUS][LEN][StatusData序列化][CRC8]
// 调用时机: 主循环中每 500ms 调用一次 (由定时器或计数器控制周期)
void Protocol_SendStatus(StatusData *status);

// 主循环帧解析函数
// 功能: 从 UART 接收环形缓冲区中读取数据, 尝试解析完整帧
//       如果解析到有效帧, 根据 CMD 类型执行对应操作:
//       - 查询状态: 立即回复一次状态上报
//       - 设置参数: 修改 g_params 并同步到 PWM 引擎
//       - 测试配置: 调用 Menu_SetTestConfig 下发测试参数
//       - 启动/停止: 调用 Menu_StartTest / Menu_StopTest
//       - 请求导出: 触发 CSV 数据导出流程
// 调用时机: 主循环中每帧调用一次
// 注意: 此函数是非阻塞的, 如果缓冲区中没有完整帧则立即返回
void Protocol_Process(void);

// CSV 数据导出 (逐块发送, 避免阻塞)
// 功能: 逐条调用 Menu_FormatExportChunk() 格式化CSV行,
//       然后通过 UART 发送到 PC 端
//       每次调用发送一条记录, 适合在主循环中分散执行
// 帧格式: [0xA5][CMD_EXPORT][LEN][CSV行数据][CRC8]
// 调用时机: PC 发送"请求导出"命令后, 在主循环中重复调用
//           直到 Menu_ExportDone() 返回 1
void Protocol_ProcessExport(void);

#endif
