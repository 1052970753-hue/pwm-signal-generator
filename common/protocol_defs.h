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
    CMD_READ_STATUS   = 0x10,
    CMD_WRITE_PWM     = 0x20,
    CMD_WRITE_FG_DIV  = 0x30,
    CMD_OLED_BUFFER   = 0x40,
    CMD_KEY_EVENT     = 0x41,
    CMD_SET_TEST      = 0x42,  // Set test parameters
    CMD_START_TEST    = 0x43,  // Start test
    CMD_STOP_TEST     = 0x44,  // Stop test
    CMD_EXPORT_DATA   = 0x50,  // Request test data export
    CMD_EXPORT_CHUNK  = 0x51,  // MCU sends CSV data chunk
    CMD_EXPORT_DONE   = 0x52,  // MCU signals export complete
    CMD_WRITE_VSP     = 0x60   // Write VSP parameters
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
    uint8_t  mode;         // current AppMode
    uint8_t  test_state;   // TestState (0=idle,1=running,2=done)
    uint16_t test_cycle;   // current cycle number
    uint16_t test_total;   // total cycles configured
    uint8_t  vsp_voltage_x10;  // VSP voltage *10 (0~50 = 0.0~5.0V)
    uint8_t  vsp_enabled;      // VSP enable (0=off, 1=on)
    uint8_t  test_on_method;   // Test ON method (0=PWM, 1=relay, 2=both)
} StatusData;  // 28 bytes

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

typedef struct {
    uint8_t  channel;       // 1 or 2
    uint32_t freq_hz;
    uint8_t  duty_pct;
    uint16_t cycles;        // 1-999
    uint16_t on_time_sec;   // 1-60
    uint16_t off_time_sec;  // 1-60
    uint8_t  on_method;     // 0=PWM, 1=relay, 2=both
} TestConfig;  // 13 bytes

typedef struct {
    uint8_t voltage_x10;    // VSP voltage *10 (0~50 = 0.0~5.0V)
    uint8_t enabled;        // VSP enable (0=off, 1=on)
} VspWriteReq;

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
    uint8_t  vsp_voltage_x10;
    uint8_t  vsp_enabled;
    uint8_t  test_on_method;
} SystemParams;

uint8_t crc8(const uint8_t *data, uint8_t len);

#pragma pack(pop)

#ifdef __cplusplus
}
#endif

#endif
