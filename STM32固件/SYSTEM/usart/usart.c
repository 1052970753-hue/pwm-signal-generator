/*
 * usart.c — USART1 串口底层驱动
 * ══════════════════════════════════════════════════════════════
 *
 * 【本文件职责】
 *   1. uart_init(): USART1 硬件初始化 (GPIO + NVIC + USART)
 *   2. printf 重定向: fputc() 通过 USART1 发送, 支持 printf 调试输出
 *
 * 【USART1 IRQ Handler 位置】
 *   USART1_IRQHandler() 已移至 protocol.c 实现
 *   protocol.c 中的版本使用环形缓冲区实现零拷贝接收
 *   如果保留本文件中的版本 (usart.c 原版), 会与 protocol.c 冲突
 *   因此本文件不包含 IRQ Handler, 仅提供初始化和 printf 支持
 *
 * 【引脚分配】
 *   PA9  = USART1_TX (复用推挽输出)
 *   PA10 = USART1_RX (浮空输入)
 *
 * 【波特率】
 *   由 uart_init(bound) 参数指定, 常用值: 9600 / 115200
 *   需要与 PC 端串口工具一致
 */
#include "sys.h"
#include "usart.h"

#if SYSTEM_SUPPORT_OS
#include "includes.h"
#endif

/* ════════════════════════════════════════════════════════════
 *  printf 支持 — 半主机模式禁用
 * ════════════════════════════════════════════════════════════
 *  #pragma import(__use_no_semihosting): 禁用半主机模式
 *    半主机模式是 ARM 调试器的一种 I/O 重定向机制
 *    在实际硬件上运行时必须禁用, 否则会进入 HardFault
 *
 *  __FILE / __stdout: 最小化 FILE 结构体定义
 *    标准 C 库需要 FILE 结构体, 但嵌入式系统不需要真正的文件系统
 *    仅定义 handle 字段满足编译器要求
 *
 *  _sys_exit(): 半主机退出函数的空实现
 *    禁用半主机后需要提供此函数, 否则链接报错
 *
 *  fputc(): printf 底层输出函数
 *    将每个字符通过 USART1 发送
 *    使用轮询方式: 等待 TXE (发送缓冲区空) 标志后写入 DR
 */
#pragma import(__use_no_semihosting)

struct __FILE { int handle; };
FILE __stdout;

void _sys_exit(int x) { x = x; }               // 半主机退出, 空实现

/* fputc: printf 重定向到 USART1
 * 每次输出一个字符, 等待上次发送完成后写入 DR 寄存器
 * SR & 0x40 = TXE (Transmit data register empty) 标志
 */
int fputc(int ch, FILE *f) {
    while ((USART1->SR & 0X40) == 0);           // 等待 TXE=1 (发送寄存器空)
    USART1->DR = (u8)ch;                        // 写入数据寄存器, 启动发送
    return ch;
}

/* ════════════════════════════════════════════════════════════
 *  USART1 初始化函数
 * ════════════════════════════════════════════════════════════
 *  参数: bound — 波特率 (如 115200)
 *
 *  初始化步骤:
 *    1. 开启 USART1 和 GPIOA 时钟 (APB2 总线)
 *    2. 配置 PA9 为复用推挽输出 (TX)
 *    3. 配置 PA10 为浮空输入 (RX)
 *    4. 配置 NVIC: USART1 中断, 抢占优先级 3, 子优先级 3
 *    5. 配置 USART: 波特率/8位/1停止位/无校验/无流控/RX+TX
 *    6. 使能 RXNE 中断 (接收中断)
 *    7. 使能 USART1
 *
 *  注意:
 *    - NVIC 优先级 3 是本系统中最低的 (TIM2=1, 其他外设更低)
 *      确保串口接收不会抢占关键的定时器中断
 *    - RXNE 中断在此开启, 但 ISR 实现在 protocol.c 中
 */
void uart_init(u32 bound) {
    GPIO_InitTypeDef GPIO_InitStructure;
    USART_InitTypeDef USART_InitStructure;
    NVIC_InitTypeDef NVIC_InitStructure;

    // 开启 USART1 和 GPIOA 时钟 — 均在 APB2 总线
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_USART1 | RCC_APB2Periph_GPIOA, ENABLE);

    /* ── TX 引脚: PA9 复用推挽输出 ──
     * AF_PP = Alternate Function Push-Pull
     * USART1_TX 由硬件自动控制, 软件无需直接操作此引脚
     */
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_9;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;        // 复用推挽输出
    GPIO_Init(GPIOA, &GPIO_InitStructure);

    /* ── RX 引脚: PA10 浮空输入 ──
     * IN_FLOATING = 无上拉/下拉
     * 串口 RX 通常需要外部或内部上拉, 但浮空输入在大多数情况下也可工作
     * 如果通信不稳定, 可改为 GPIO_Mode_IPU (上拉输入)
     */
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_10;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING;  // 浮空输入
    GPIO_Init(GPIOA, &GPIO_InitStructure);

    /* ── NVIC 中断优先级配置 ──
     * 抢占优先级 3, 子优先级 3 (本系统最低)
     * 理由: 串口接收是异步事件, 不应抢占 TIM2 输入捕获 (优先级 1)
     */
    NVIC_InitStructure.NVIC_IRQChannel = USART1_IRQn;             // USART1 中断通道
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 3;     // 抢占优先级 3
    NVIC_InitStructure.NVIC_IRQChannelSubPriority = 3;            // 子优先级 3
    NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;               // 使能中断
    NVIC_Init(&NVIC_InitStructure);

    /* ── USART 参数配置 ──
     * 波特率: 由 bound 参数指定 (常见 9600/115200)
     * 字长: 8 位 (标准数据帧)
     * 停止位: 1 位 (最常用配置)
     * 校验: 无 (可选 Even/Odd 增加可靠性, 但降低吞吐)
     * 硬件流控: 无 (3 线制: TX/RX/GND)
     * 模式: 收发均使能
     */
    USART_InitStructure.USART_BaudRate = bound;                                    // 波特率
    USART_InitStructure.USART_WordLength = USART_WordLength_8b;                     // 8 位数据
    USART_InitStructure.USART_StopBits = USART_StopBits_1;                          // 1 位停止位
    USART_InitStructure.USART_Parity = USART_Parity_No;                             // 无校验
    USART_InitStructure.USART_HardwareFlowControl = USART_HardwareFlowControl_None; // 无流控
    USART_InitStructure.USART_Mode = USART_Mode_Rx | USART_Mode_Tx;                // 收发模式

    USART_Init(USART1, &USART_InitStructure);                   // 应用配置

    USART_ITConfig(USART1, USART_IT_RXNE, ENABLE);             // 使能接收中断 (RXNE)
    USART_Cmd(USART1, ENABLE);                                  // 使能 USART1 外设
}
