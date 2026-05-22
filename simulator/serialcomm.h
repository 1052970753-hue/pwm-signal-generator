#ifndef SERIALCOMM_H
#define SERIALCOMM_H

#include <QObject>
#include <QSerialPort>
#include "../common/protocol_defs.h"
#include "../common/menu_fsm.h"

class SerialComm : public QObject {
    Q_OBJECT

public:
    explicit SerialComm(QObject *parent = nullptr);
    ~SerialComm();

    void connectToPort(const QString &name, int baudRate);
    void disconnect();
    bool isConnected() const;

    void sendPwmWrite(const PwmWriteReq &req);
    void sendFgDiv(const FgDivReq &req);
    void sendKeyEvent(InputEvent ev);

signals:
    void connected();
    void disconnected();
    void dataReceived(const QByteArray &data);

private slots:
    void onReadyRead();

private:
    QSerialPort *m_port;
    void sendFrame(uint8_t cmd, const uint8_t *data, uint8_t len);
};

#endif
