#include "protocol.h"
#include <cstring>

uint8_t crc8(const uint8_t *data, uint8_t len) {
    uint8_t crc = 0x00;
    while (len--) {
        crc ^= *data++;
        for (int i = 0; i < 8; i++) {
            if (crc & 0x80)
                crc = (crc << 1) ^ 0x07;
            else
                crc <<= 1;
        }
    }
    return crc;
}

StatusData Protocol::parseStatusResponse(const QByteArray &data) {
    StatusData sd;
    memset(&sd, 0, sizeof(sd));

    if (data.size() < 4) return sd;
    if ((uint8_t)data[0] != FRAME_HEADER_MCU2PC) return sd;

    uint8_t len = (uint8_t)data[2];
    if (data.size() < 4 + len) return sd;

    // Verify CRC
    uint8_t expectedCrc = crc8((const uint8_t *)data.constData(), 3 + len);
    if (expectedCrc != (uint8_t)data[3 + len]) return sd;

    if (len >= sizeof(StatusData))
        memcpy(&sd, data.constData() + 3, sizeof(StatusData));

    return sd;
}

QByteArray Protocol::encodeWritePwm(const PwmWriteReq &req) {
    QByteArray buf;
    buf.append(FRAME_HEADER_PC2MCU);
    buf.append(CMD_WRITE_PWM);
    buf.append(sizeof(PwmWriteReq));
    buf.append(reinterpret_cast<const char *>(&req), sizeof(PwmWriteReq));
    uint8_t crc = crc8((const uint8_t *)buf.constData(), buf.size());
    buf.append((char)crc);
    return buf;
}

QByteArray Protocol::encodeWriteFgDiv(const FgDivReq &req) {
    QByteArray buf;
    buf.append(FRAME_HEADER_PC2MCU);
    buf.append(CMD_WRITE_FG_DIV);
    buf.append(sizeof(FgDivReq));
    buf.append(reinterpret_cast<const char *>(&req), sizeof(FgDivReq));
    uint8_t crc = crc8((const uint8_t *)buf.constData(), buf.size());
    buf.append((char)crc);
    return buf;
}
