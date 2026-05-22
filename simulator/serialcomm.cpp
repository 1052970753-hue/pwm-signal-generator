#include "serialcomm.h"
#include <QDebug>

SerialComm::SerialComm(QObject *parent)
    : QObject(parent), m_port(new QSerialPort(this))
{
    connect(m_port, &QSerialPort::readyRead,
            this, &SerialComm::onReadyRead);
}

SerialComm::~SerialComm() {
    if (m_port->isOpen())
        m_port->close();
}

void SerialComm::connectToPort(const QString &name, int baudRate) {
    m_port->setPortName(name);
    m_port->setBaudRate(baudRate);
    m_port->setDataBits(QSerialPort::Data8);
    m_port->setParity(QSerialPort::NoParity);
    m_port->setStopBits(QSerialPort::OneStop);
    if (!m_port->open(QIODevice::ReadWrite)) {
        qWarning() << "Failed to open serial port:" << m_port->portName()
                   << m_port->errorString();
        return;
    }
    emit connected();
}

void SerialComm::disconnect() {
    if (m_port->isOpen()) {
        m_port->close();
        emit disconnected();
    }
}

bool SerialComm::isConnected() const {
    return m_port->isOpen();
}

void SerialComm::sendFrame(uint8_t cmd, const uint8_t *data, uint8_t len) {
    if (len > 252) return; // max payload: 256 - header(3) - crc(1)
    if (!m_port->isOpen()) return;
    uint8_t buf[256];
    buf[0] = FRAME_HEADER_PC2MCU;
    buf[1] = cmd;
    buf[2] = len;
    if (len > 0 && data)
        memcpy(buf + 3, data, len);
    buf[3 + len] = crc8(buf, 3 + len);
    m_port->write(reinterpret_cast<const char *>(buf), 4 + len);
}

void SerialComm::sendPwmWrite(const PwmWriteReq &req) {
    sendFrame(CMD_WRITE_PWM,
        reinterpret_cast<const uint8_t *>(&req), sizeof(req));
}

void SerialComm::sendFgDiv(const FgDivReq &req) {
    sendFrame(CMD_WRITE_FG_DIV,
        reinterpret_cast<const uint8_t *>(&req), sizeof(req));
}

void SerialComm::sendKeyEvent(InputEvent ev) {
    KeyEventReq req;
    req.event = static_cast<uint8_t>(ev);
    sendFrame(CMD_KEY_EVENT,
        reinterpret_cast<const uint8_t *>(&req), sizeof(req));
}

void SerialComm::onReadyRead() {
    emit dataReceived(m_port->readAll());
}
