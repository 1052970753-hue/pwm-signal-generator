#include "protocol.h"
#include <string.h>

static uint8_t rx_buf[64];
static uint8_t rx_idx = 0;

void Protocol_Init(void) {
    RCC_APB2PeriphClockCmd(CDC_USART_CLK | CDC_GPIO_CLK, ENABLE);

    GPIO_InitTypeDef gpio;
    gpio.GPIO_Pin = CDC_TX_PIN;
    gpio.GPIO_Mode = GPIO_Mode_AF_PP;
    gpio.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(CDC_GPIO, &gpio);

    gpio.GPIO_Pin = CDC_RX_PIN;
    gpio.GPIO_Mode = GPIO_Mode_IN_FLOATING;
    GPIO_Init(CDC_GPIO, &gpio);

    USART_InitTypeDef usart;
    usart.USART_BaudRate = CDC_BAUDRATE;
    usart.USART_WordLength = USART_WordLength_8b;
    usart.USART_StopBits = USART_StopBits_1;
    usart.USART_Parity = USART_Parity_No;
    usart.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
    usart.USART_Mode = USART_Mode_Rx | USART_Mode_Tx;
    USART_Init(CDC_USART, &usart);

    USART_ITConfig(CDC_USART, USART_IT_RXNE, ENABLE);
    NVIC_EnableIRQ(USART1_IRQn);
    USART_Cmd(CDC_USART, ENABLE);
}

void Protocol_SendStatus(StatusData *sd) {
    uint8_t buf[sizeof(StatusData) + 5];
    buf[0] = FRAME_HEADER_MCU2PC;
    buf[1] = CMD_READ_STATUS;
    buf[2] = sizeof(StatusData);
    memcpy(buf + 3, sd, sizeof(StatusData));
    buf[3 + sizeof(StatusData)] = crc8(buf, 3 + sizeof(StatusData));

    for (int i = 0; i < (int)(sizeof(StatusData) + 4); i++) {
        while (USART_GetFlagStatus(CDC_USART, USART_FLAG_TXE) == RESET);
        USART_SendData(CDC_USART, buf[i]);
    }
}

void USART1_IRQHandler(void) {
    if (USART_GetITStatus(CDC_USART, USART_IT_RXNE) != RESET) {
        uint8_t byte = USART_ReceiveData(CDC_USART);
        if (rx_idx < sizeof(rx_buf))
            rx_buf[rx_idx++] = byte;
        USART_ClearITPendingBit(CDC_USART, USART_IT_RXNE);
    }
}
