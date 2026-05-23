#include "protocol.h"
#include "menu.h"
#include "pwm_engine.h"
#include <string.h>

// ── TX buffer ──
static uint8_t tx_buf[64];

// ── RX ring buffer (filled by ISR, consumed by Protocol_Process) ──
#define RX_RING_SIZE 256
static volatile uint8_t rx_ring[RX_RING_SIZE];
static volatile uint16_t rx_head = 0;  // ISR writes here
static volatile uint16_t rx_tail = 0;  // main loop reads here

// ── Frame parser state machine ──
typedef enum {
    PS_WAIT_HEADER,
    PS_WAIT_CMD,
    PS_WAIT_LEN,
    PS_WAIT_DATA,
    PS_WAIT_CRC
} ParseState;

static ParseState ps = PS_WAIT_HEADER;
static uint8_t frame_cmd;
static uint8_t frame_len;
static uint8_t frame_data[32];
static uint8_t frame_idx;

static uint8_t rx_pop(void) {
    if (rx_head == rx_tail) return 0;
    uint8_t b = rx_ring[rx_tail];
    rx_tail = (rx_tail + 1) % RX_RING_SIZE;
    return b;
}

static uint8_t rx_available(void) {
    return (rx_head != rx_tail);
}

// ── Hardware init ──
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

// ── ISR: push received byte into ring buffer ──
void USART1_IRQHandler(void) {
    if (USART_GetITStatus(CDC_USART, USART_IT_RXNE) != RESET) {
        uint8_t byte = USART_ReceiveData(CDC_USART);
        uint16_t next = (rx_head + 1) % RX_RING_SIZE;
        if (next != rx_tail) {  // buffer not full
            rx_ring[rx_head] = byte;
            rx_head = next;
        }
        USART_ClearITPendingBit(CDC_USART, USART_IT_RXNE);
    }
}

// ── TX helper: send raw bytes ──
static void uart_send(const uint8_t *data, uint16_t len) {
    for (uint16_t i = 0; i < len; i++) {
        while (USART_GetFlagStatus(CDC_USART, USART_FLAG_TXE) == RESET);
        USART_SendData(CDC_USART, data[i]);
    }
}

// ── TX: send a framed response ──
static void send_frame(uint8_t cmd, const uint8_t *data, uint8_t len) {
    tx_buf[0] = FRAME_HEADER_MCU2PC;
    tx_buf[1] = cmd;
    tx_buf[2] = len;
    if (len > 0 && data != NULL)
        memcpy(tx_buf + 3, data, len);
    tx_buf[3 + len] = crc8(tx_buf, 3 + len);
    uart_send(tx_buf, 4 + len);
}

// ── TX: send status ──
void Protocol_SendStatus(StatusData *sd) {
    send_frame(CMD_READ_STATUS, (const uint8_t *)sd, sizeof(StatusData));
}

// ── TX: send ACK (empty data, just confirms receipt) ──
static void send_ack(uint8_t cmd) {
    send_frame(cmd, NULL, 0);
}

// ── Command handlers ──

static void handle_write_pwm(void) {
    if (frame_len != sizeof(PwmWriteReq)) return;
    PwmWriteReq *req = (PwmWriteReq *)frame_data;

    if (req->channel == 1) {
        g_params.ch1_freq_hz = req->freq_hz;
        g_params.ch1_duty_pct = req->duty_pct;
        g_params.ch1_enabled = req->enable;
    } else if (req->channel == 2) {
        g_params.ch2_freq_hz = req->freq_hz;
        g_params.ch2_duty_pct = req->duty_pct;
        g_params.ch2_enabled = req->enable;
    }

    PWM_UpdateFromParams(&g_params);
    g_menu.dirty = 1;
    send_ack(CMD_WRITE_PWM);
}

static void handle_write_fg_div(void) {
    if (frame_len != sizeof(FgDivReq)) return;
    FgDivReq *req = (FgDivReq *)frame_data;

    if (req->div == 1 || req->div == 2 || req->div == 4 || req->div == 8)
        g_params.fg_div = req->div;

    g_menu.dirty = 1;
    send_ack(CMD_WRITE_FG_DIV);
}

static void handle_key_event(void) {
    if (frame_len != sizeof(KeyEventReq)) return;
    KeyEventReq *req = (KeyEventReq *)frame_data;

    // Map protocol event code directly to InputEvent enum
    // EVENT_NONE=0, CW=1, CCW=2, CLICK=3, LONG_PRESS=4
    if (req->event >= EVENT_NONE && req->event <= EVENT_LONG_PRESS) {
        Menu_Process((InputEvent)req->event);
        if (g_menu.dirty) {
            PWM_UpdateFromParams(&g_params);
            g_menu.dirty = 0;
        }
    }
    send_ack(CMD_KEY_EVENT);
}

static void handle_read_status(void) {
    StatusData sd;
    sd.ch1_freq_hz   = g_params.ch1_freq_hz;
    sd.ch1_duty_pct  = g_params.ch1_duty_pct;
    sd.ch1_enabled   = g_params.ch1_enabled;
    sd.ch2_freq_hz   = g_params.ch2_freq_hz;
    sd.ch2_duty_pct  = g_params.ch2_duty_pct;
    sd.ch2_enabled   = g_params.ch2_enabled;
    sd.fg_freq_mhz   = 0;  // filled by caller if needed
    sd.fg_div        = g_params.fg_div;
    sd.rpm           = 0;
    send_frame(CMD_READ_STATUS, (const uint8_t *)&sd, sizeof(StatusData));
}

// ── Dispatch received frame (CRC already verified by parser) ──
static void dispatch_frame(void) {
    switch (frame_cmd) {
        case CMD_READ_STATUS:  handle_read_status();  break;
        case CMD_WRITE_PWM:    handle_write_pwm();    break;
        case CMD_WRITE_FG_DIV: handle_write_fg_div(); break;
        case CMD_KEY_EVENT:    handle_key_event();     break;
        default: break;  // unknown command, ignore
    }
}

// ── Frame parser: called each iteration of main loop ──
void Protocol_Process(void) {
    while (rx_available()) {
        uint8_t b = rx_pop();

        switch (ps) {
            case PS_WAIT_HEADER:
                if (b == FRAME_HEADER_PC2MCU) {
                    ps = PS_WAIT_CMD;
                }
                break;

            case PS_WAIT_CMD:
                frame_cmd = b;
                ps = PS_WAIT_LEN;
                break;

            case PS_WAIT_LEN:
                frame_len = b;
                if (frame_len > sizeof(frame_data)) {
                    ps = PS_WAIT_HEADER;  // invalid length, reset
                } else if (frame_len == 0) {
                    ps = PS_WAIT_CRC;
                } else {
                    frame_idx = 0;
                    ps = PS_WAIT_DATA;
                }
                break;

            case PS_WAIT_DATA:
                frame_data[frame_idx++] = b;
                if (frame_idx >= frame_len) {
                    ps = PS_WAIT_CRC;
                }
                break;

            case PS_WAIT_CRC: {
                // Verify CRC
                tx_buf[0] = FRAME_HEADER_PC2MCU;
                tx_buf[1] = frame_cmd;
                tx_buf[2] = frame_len;
                if (frame_len > 0)
                    memcpy(tx_buf + 3, frame_data, frame_len);
                uint8_t calc = crc8(tx_buf, 3 + frame_len);

                if (calc == b) {
                    dispatch_frame();
                }
                // Either way, go back to waiting for next frame
                ps = PS_WAIT_HEADER;
                break;
            }
        }
    }
}
