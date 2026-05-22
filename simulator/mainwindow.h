#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QComboBox>
#include <QLabel>
#include <QPushButton>
#include "oledwidget.h"
#include "menuengine.h"
#include "serialcomm.h"

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);

protected:
    void keyPressEvent(QKeyEvent *event) override;

private slots:
    void onModeChanged(int index);
    void onSerialConnectToggled();
    void onSerialConnected();
    void onSerialDisconnected();
    void onSerialDataReceived(const QByteArray &data);

private:
    void setupUI();
    void processKeyInput(InputEvent ev);

    OLEDWidget  *m_oled;
    MenuEngine  *m_menu;
    SerialComm  *m_serial;
    QComboBox   *m_modeCombo;
    QComboBox   *m_portCombo;
    QPushButton *m_connectBtn;
    QLabel      *m_statusLabel;
    bool         m_hardwareMode = false;
};

#endif
