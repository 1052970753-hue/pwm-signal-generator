#include "oledwidget.h"
#include <QPainter>

OLEDWidget::OLEDWidget(QWidget *parent)
    : QWidget(parent),
      m_buffer(OLED_WIDTH, OLED_HEIGHT, QImage::Format_Mono)
{
    m_buffer.fill(0);
    setFocusPolicy(Qt::StrongFocus);
}

void OLEDWidget::paintEvent(QPaintEvent *) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, false);

    // Dark border
    p.fillRect(rect(), QColor(20, 20, 20));

    // OLED area (2x scale, centered)
    int ox = (width() - OLED_WIDTH * 2) / 2;
    int oy = (height() - OLED_HEIGHT * 2) / 2;

    // Background fill
    p.fillRect(ox, oy, OLED_WIDTH * 2, OLED_HEIGHT * 2, Qt::black);

    QImage scaled = m_buffer.scaled(OLED_WIDTH * 2, OLED_HEIGHT * 2,
        Qt::IgnoreAspectRatio, Qt::FastTransformation);

    // Colorize: black -> OLED dark, white -> OLED yellow
    for (int y = 0; y < scaled.height(); y++) {
        for (int x = 0; x < scaled.width(); x++) {
            QRgb px = scaled.pixel(x, y);
            if (qRed(px) > 128) {
                p.setPen(QColor(255, 220, 50));
                p.drawPoint(ox + x, oy + y);
            }
        }
    }
}

void OLEDWidget::setPixel(int x, int y, bool on) {
    if (x >= 0 && x < OLED_WIDTH && y >= 0 && y < OLED_HEIGHT)
        m_buffer.setPixel(x, y, on ? 1 : 0);
}

void OLEDWidget::drawChar(int x, int y, char c) {
    static const uint8_t font[96][5] = {
        {0x00,0x00,0x00,0x00,0x00}, // space
        {0x00,0x00,0x5F,0x00,0x00},
        {0x00,0x07,0x00,0x07,0x00},
        {0x14,0x7F,0x14,0x7F,0x14},
        {0x24,0x2A,0x7F,0x2A,0x12},
        {0x23,0x13,0x08,0x64,0x62},
        {0x36,0x49,0x55,0x22,0x50},
        {0x00,0x05,0x03,0x00,0x00},
        {0x00,0x1C,0x22,0x41,0x00},
        {0x00,0x41,0x22,0x1C,0x00},
        {0x08,0x2A,0x1C,0x2A,0x08},
        {0x08,0x08,0x3E,0x08,0x08},
        {0x00,0x50,0x30,0x00,0x00},
        {0x08,0x08,0x08,0x08,0x08},
        {0x00,0x60,0x60,0x00,0x00},
        {0x20,0x10,0x08,0x04,0x02},
        {0x3E,0x51,0x49,0x45,0x3E}, // 0
        {0x00,0x42,0x7F,0x40,0x00},
        {0x42,0x61,0x51,0x49,0x46},
        {0x21,0x41,0x45,0x4B,0x31},
        {0x18,0x14,0x12,0x7F,0x10},
        {0x27,0x45,0x45,0x45,0x39},
        {0x3C,0x4A,0x49,0x49,0x30},
        {0x01,0x71,0x09,0x05,0x03},
        {0x36,0x49,0x49,0x49,0x36},
        {0x06,0x49,0x49,0x29,0x1E}, // 9
        {0x00,0x36,0x36,0x00,0x00},
        {0x00,0x56,0x36,0x00,0x00},
        {0x00,0x08,0x14,0x22,0x41},
        {0x14,0x14,0x14,0x14,0x14},
        {0x41,0x22,0x14,0x08,0x00},
        {0x02,0x01,0x51,0x09,0x06},
        {0x32,0x49,0x79,0x41,0x3E}, // @
        {0x7E,0x11,0x11,0x11,0x7E}, // A
        {0x7F,0x49,0x49,0x49,0x36},
        {0x3E,0x41,0x41,0x41,0x22},
        {0x7F,0x41,0x41,0x22,0x1C},
        {0x7F,0x49,0x49,0x49,0x41},
        {0x7F,0x09,0x09,0x01,0x01},
        {0x3E,0x41,0x41,0x51,0x32},
        {0x7F,0x08,0x08,0x08,0x7F},
        {0x00,0x41,0x7F,0x41,0x00},
        {0x20,0x40,0x41,0x3F,0x01},
        {0x7F,0x08,0x14,0x22,0x41},
        {0x7F,0x40,0x40,0x40,0x40},
        {0x7F,0x02,0x04,0x02,0x7F},
        {0x7F,0x04,0x08,0x10,0x7F},
        {0x3E,0x41,0x41,0x41,0x3E}, // O
        {0x7F,0x09,0x09,0x09,0x06},
        {0x3E,0x41,0x51,0x21,0x5E},
        {0x7F,0x09,0x19,0x29,0x46},
        {0x46,0x49,0x49,0x49,0x31},
        {0x01,0x01,0x7F,0x01,0x01},
        {0x3F,0x40,0x40,0x40,0x3F},
        {0x1F,0x20,0x40,0x20,0x1F},
        {0x7F,0x20,0x18,0x20,0x7F},
        {0x63,0x14,0x08,0x14,0x63},
        {0x03,0x04,0x78,0x04,0x03},
        {0x61,0x51,0x49,0x45,0x43}, // Z
        {0x00,0x00,0x7F,0x41,0x41}, // [
        {0x02,0x04,0x08,0x10,0x20}, // backslash
        {0x41,0x41,0x7F,0x00,0x00}, // ]
        {0x04,0x02,0x01,0x02,0x04}, // ^
        {0x40,0x40,0x40,0x40,0x40}, // _
        {0x00,0x01,0x02,0x04,0x00}, // `
        {0x20,0x54,0x54,0x54,0x78}, // a
        {0x7F,0x48,0x44,0x44,0x38}, // b
        {0x38,0x44,0x44,0x44,0x20}, // c
        {0x38,0x44,0x44,0x48,0x7F}, // d
        {0x38,0x54,0x54,0x54,0x18}, // e
        {0x08,0x7E,0x09,0x01,0x02}, // f
        {0x0C,0x52,0x52,0x52,0x3E}, // g
        {0x7F,0x08,0x04,0x04,0x78}, // h
        {0x00,0x44,0x7D,0x40,0x00}, // i
        {0x20,0x40,0x44,0x3D,0x00}, // j
        {0x7F,0x10,0x28,0x44,0x00}, // k
        {0x00,0x41,0x7F,0x40,0x00}, // l
        {0x7C,0x04,0x18,0x04,0x78}, // m
        {0x7C,0x08,0x04,0x04,0x78}, // n
        {0x38,0x44,0x44,0x44,0x38}, // o
        {0x7C,0x14,0x14,0x14,0x08}, // p
        {0x08,0x14,0x14,0x18,0x7C}, // q
        {0x7C,0x08,0x04,0x04,0x08}, // r
        {0x48,0x54,0x54,0x54,0x20}, // s
        {0x04,0x3F,0x44,0x40,0x20}, // t
        {0x3C,0x40,0x40,0x20,0x7C}, // u
        {0x1C,0x20,0x40,0x20,0x1C}, // v
        {0x3C,0x40,0x30,0x40,0x3C}, // w
        {0x44,0x28,0x10,0x28,0x44}, // x
        {0x0C,0x50,0x50,0x50,0x3C}, // y
        {0x44,0x64,0x54,0x4C,0x44}, // z
        {0x00,0x08,0x36,0x41,0x00}, // {
        {0x00,0x00,0x7F,0x00,0x00}, // |
        {0x00,0x41,0x36,0x08,0x00}, // }
        {0x08,0x08,0x2A,0x1C,0x08}, // ~
    };
    if (c < ' ' || c > '~') c = ' ';
    for (int i = 0; i < 5; i++) {
        uint8_t line = font[c - ' '][i];
        for (int j = 0; j < 8; j++) {
            if (line & (1 << j))
                setPixel(x + i, y + j, true);
        }
    }
}

void OLEDWidget::drawString(int x, int y, const QString &s) {
    for (const QChar &ch : s) {
        drawChar(x, y, ch.toLatin1());
        x += 6;
    }
}

void OLEDWidget::renderMain(const StatusData &sd) {
    m_buffer.fill(0);
    char buf[32];

    snprintf(buf, sizeof(buf), "CH1:%-6luHz %3u%% %s",
        (unsigned long)sd.ch1_freq_hz, sd.ch1_duty_pct,
        sd.ch1_enabled ? "ON " : "OFF");
    drawString(0, 0, QString::fromLatin1(buf));

    snprintf(buf, sizeof(buf), "CH2:%-6luHz %3u%% %s",
        (unsigned long)sd.ch2_freq_hz, sd.ch2_duty_pct,
        sd.ch2_enabled ? "ON " : "OFF");
    drawString(0, 14, QString::fromLatin1(buf));

    snprintf(buf, sizeof(buf), "FG:%5uRPM /%u",
        sd.rpm, sd.fg_div);
    drawString(0, 30, QString::fromLatin1(buf));

    snprintf(buf, sizeof(buf), "Freq:%luHz", (unsigned long)(sd.fg_freq_mhz / 1000));
    drawString(0, 44, QString::fromLatin1(buf));

    // Duty bar
    int barW = sd.ch1_duty_pct * 128 / 100;
    for (int x = 0; x < barW; x++)
        for (int y = 56; y < 60; y++)
            setPixel(x, y, true);

    update();
}

void OLEDWidget::renderMenu(const MenuState &state) {
    m_buffer.fill(0);
    drawString(0, 0, "* MAIN MENU *");
    const char *titles[] = {"CH1设置", "CH2设置", "FG设置", "系统设置"};
    for (int i = 0; i < 4; i++) {
        drawString(6, 14 + i * 12,
            (state.cursor == i) ? ">" : " ");
        drawString(16, 14 + i * 12, titles[i]);
    }
    update();
}

void OLEDWidget::updateFromProtocolData(const QByteArray &data) {
    StatusData sd = Protocol::parseStatusResponse(data);
    renderMain(sd);
}

void OLEDWidget::renderSubMenu(const MenuState &state, const StatusData &sd) {
    m_buffer.fill(0);
    char buf[32];
    const char *div_strs[] = {"---", "/1", "/2", "---", "/4", "---", "---", "---", "/8"};

    switch (state.screen) {
        case SCREEN_CH1_MENU:
            drawString(0, 0, "CH1设置");

            snprintf(buf, sizeof(buf), "%luHz", (unsigned long)sd.ch1_freq_hz);
            drawString(6, 14, (state.cursor == 0) ? ">" : " ");
            drawString(16, 14, "频率");
            drawString(80, 14, QString::fromLatin1(buf));

            snprintf(buf, sizeof(buf), "%u%%", sd.ch1_duty_pct);
            drawString(6, 28, (state.cursor == 1) ? ">" : " ");
            drawString(16, 28, "占空比");
            drawString(80, 28, QString::fromLatin1(buf));

            drawString(6, 42, (state.cursor == 2) ? ">" : " ");
            drawString(16, 42, "输出使能");
            drawString(80, 42, sd.ch1_enabled ? "ON" : "OFF");
            break;

        case SCREEN_CH2_MENU:
            drawString(0, 0, "CH2设置");

            snprintf(buf, sizeof(buf), "%luHz", (unsigned long)sd.ch2_freq_hz);
            drawString(6, 14, (state.cursor == 0) ? ">" : " ");
            drawString(16, 14, "频率");
            drawString(80, 14, QString::fromLatin1(buf));

            snprintf(buf, sizeof(buf), "%u%%", sd.ch2_duty_pct);
            drawString(6, 28, (state.cursor == 1) ? ">" : " ");
            drawString(16, 28, "占空比");
            drawString(80, 28, QString::fromLatin1(buf));

            drawString(6, 42, (state.cursor == 2) ? ">" : " ");
            drawString(16, 42, "输出使能");
            drawString(80, 42, sd.ch2_enabled ? "ON" : "OFF");
            break;

        case SCREEN_FG_MENU:
            drawString(0, 0, "FG设置");

            drawString(6, 14, (state.cursor == 0) ? ">" : " ");
            drawString(16, 14, "分频比");
            if (sd.fg_div > 0 && sd.fg_div <= 8)
                drawString(80, 14, div_strs[sd.fg_div]);
            else
                drawString(80, 14, "---");

            drawString(6, 28, (state.cursor == 1) ? ">" : " ");
            drawString(16, 28, "显示刷新率");
            drawString(80, 28, "---");
            break;

        case SCREEN_SYS_MENU:
            drawString(0, 0, "系统设置");

            drawString(6, 14, (state.cursor == 0) ? ">" : " ");
            drawString(16, 14, "OLED亮度");
            drawString(80, 14, "---");

            drawString(6, 28, (state.cursor == 1) ? ">" : " ");
            drawString(16, 28, "恢复出厂");
            drawString(80, 28, "---");
            break;

        default:
            break;
    }

    update();
}
