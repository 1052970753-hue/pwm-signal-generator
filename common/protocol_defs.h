// common/protocol_defs.h
#ifndef PROTOCOL_DEFS_H
#define PROTOCOL_DEFS_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

#pragma pack(push, 1)

#define FRAME_HEADER_PC2MCU  0xAA
#define FRAME_HEADER_MCU2PC  0xBB

typedef enum {
    CMD_READ_STATUS  = 0x10,
    CMD_WRITE_PWM    = 0x20,
    CMD_WRITE_FG_DIV = 0x30,
    CMD_OLED_BUFFER  = 0x40,
    CMD_KEY_EVENT    = 0x41
} ProtocolCmd;

typedef struct {
    uint32_t ch1_freq_hz;
    uint8_t  ch1_duty_pct;
    uint8_t  ch1_enabled;
    uint32_t ch2_freq_hz;
    uint8_t  ch2_duty_pct;
    uint8_t  ch2_enabled;
    uint32_t fg_freq_mhz;
    uint8_t  fg_div;
    uint16_t rpm;
} StatusData;

typedef struct {
    uint8_t  channel;
    uint32_t freq_hz;
    uint8_t  duty_pct;
    uint8_t  enable;
} PwmWriteReq;

typedef struct {
    uint8_t div;
} FgDivReq;

typedef struct {
    uint8_t event;
} KeyEventReq;

// System parameters (runtime config)
typedef struct {
    uint32_t ch1_freq_hz;
    uint8_t  ch1_duty_pct;
    uint8_t  ch1_enabled;
    uint32_t ch2_freq_hz;
    uint8_t  ch2_duty_pct;
    uint8_t  ch2_enabled;
    uint8_t  fg_div;
    uint16_t fg_pulses_per_rev;
} SystemParams;

uint8_t crc8(const uint8_t *data, uint8_t len);

#pragma pack(pop)

#ifdef __cplusplus
}
#endif

#endif
