#include "mainwindow.h"
#include "keymap.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QKeyEvent>
#include <QSerialPortInfo>
#include <QLabel>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    m_oled   = new OLEDWidget(this);
    m_menu   = new MenuEngine(this);
    m_serial = new SerialComm(this);

    connect(m_serial, &SerialComm::connected,
            this, &MainWindow::onSerialConnected);
    connect(m_serial, &SerialComm::disconnected,
            this, &MainWindow::onSerialDisconnected);
    connect(m_serial, &SerialComm::dataReceived,
            this, &MainWindow::onSerialDataReceived);

    setupUI();

    m_oled->renderMain(m_menu->status());
}

void MainWindow::setupUI() {
    QWidget *central = new QWidget(this);
    QHBoxLayout *mainLayout = new QHBoxLayout(central);

    // Left: OLED display
    QGroupBox *oledGroup = new QGroupBox("OLED 显示 (128×64)");
    QVBoxLayout *oledLay = new QVBoxLayout(oledGroup);
    m_oled->setFixedSize(OLED_WIDTH * 2 + 4, OLED_HEIGHT * 2 + 4);
    oledLay->addWidget(m_oled);
    mainLayout->addWidget(oledGroup);

    // Right: Control panel
    QGroupBox *ctrlGroup = new QGroupBox("控制面板");
    QVBoxLayout *ctrlLay = new QVBoxLayout(ctrlGroup);

    ctrlLay->addWidget(new QLabel("工作模式:"));
    m_modeCombo = new QComboBox();
    m_modeCombo->addItems({"模拟器模式", "硬件模式 (USB CDC)"});
    connect(m_modeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &MainWindow::onModeChanged);
    ctrlLay->addWidget(m_modeCombo);

    ctrlLay->addWidget(new QLabel("串口:"));
    m_portCombo = new QComboBox();
    const auto ports = QSerialPortInfo::availablePorts();
    for (const auto &p : ports)
        m_portCombo->addItem(p.portName());
    if (ports.isEmpty())
        m_portCombo->addItem("(无可用串口)");
    m_portCombo->setEnabled(false);
    ctrlLay->addWidget(m_portCombo);

    m_connectBtn = new QPushButton("连接");
    m_connectBtn->setEnabled(false);
    connect(m_connectBtn, &QPushButton::clicked,
            this, &MainWindow::onSerialConnectToggled);
    ctrlLay->addWidget(m_connectBtn);

    m_statusLabel = new QLabel("状态: 模拟器模式运行中");
    ctrlLay->addWidget(m_statusLabel);

    ctrlLay->addSpacing(10);
    QLabel *hint = new QLabel(
        "══ 键盘映射 ══\n"
        "↑ ↓  = 编码器旋转\n"
        "Enter = 确认\n"
        "BS    = 长按返回\n"
        "Esc   = BACK 键\n"
        "Space = 切换 CH1 使能"
    );
    hint->setStyleSheet("color: #888; font-size: 11px;");
    ctrlLay->addWidget(hint);

    ctrlLay->addStretch();
    mainLayout->addWidget(ctrlGroup);

    setCentralWidget(central);
}

void MainWindow::keyPressEvent(QKeyEvent *ev) {
    InputEvent ie = KeyMap::toInputEvent(ev);
    if (ie != EVENT_NONE) {
        processKeyInput(ie);
        return;
    }
    if (ev->key() == Qt::Key_Space) {
        auto status = m_menu->status();
        status.ch1_enabled = !status.ch1_enabled;
        m_oled->renderMain(status);
        if (m_hardwareMode && m_serial->isConnected()) {
            PwmWriteReq req;
            req.channel = 1;
            req.freq_hz = status.ch1_freq_hz;
            req.duty_pct = status.ch1_duty_pct;
            req.enable = status.ch1_enabled;
            m_serial->sendPwmWrite(req);
        }
        return;
    }
    QMainWindow::keyPressEvent(ev);
}

void MainWindow::processKeyInput(InputEvent ev) {
    if (m_hardwareMode && m_serial->isConnected()) {
        m_serial->sendKeyEvent(ev);
    } else {
        m_menu->processEvent(ev);
        MenuState st = m_menu->state();
        if (st.screen == SCREEN_MAIN) {
            m_oled->renderMain(m_menu->status());
        } else if (st.screen == SCREEN_MAIN_MENU) {
            m_oled->renderMenu(st);
        } else {
            m_oled->renderSubMenu(st, m_menu->status());
        }
    }
}

void MainWindow::onModeChanged(int idx) {
    m_hardwareMode = (idx == 1);
    m_portCombo->setEnabled(m_hardwareMode);
    m_connectBtn->setEnabled(m_hardwareMode);
    m_statusLabel->setText(m_hardwareMode
        ? "状态: 硬件模式 — 未连接"
        : "状态: 模拟器模式运行中");
}

void MainWindow::onSerialConnectToggled() {
    if (m_serial->isConnected())
        m_serial->disconnect();
    else
        m_serial->connectToPort(m_portCombo->currentText(), 115200);
}

void MainWindow::onSerialConnected() {
    m_statusLabel->setText("状态: 已连接 — 硬件模式");
    m_connectBtn->setText("断开");
}

void MainWindow::onSerialDisconnected() {
    m_statusLabel->setText("状态: 硬件模式 — 已断开");
    m_connectBtn->setText("连接");
}

void MainWindow::onSerialDataReceived(const QByteArray &data) {
    auto status = Protocol::parseStatusResponse(data);
    m_oled->renderMain(status);
}
