#ifndef OLEDWIDGET_H
#define OLEDWIDGET_H

#include <QWidget>
#include <QImage>
#include "../common/menu_fsm.h"
#include "../common/protocol_defs.h"

class OLEDWidget : public QWidget {
    Q_OBJECT

public:
    explicit OLEDWidget(QWidget *parent = nullptr);

    void renderMain(const StatusData &sd);
    void renderMenu(const MenuState &state);
    void renderSubMenu(const MenuState &state, const StatusData &sd);
    void updateFromProtocolData(const QByteArray &data);
    QImage buffer() const { return m_buffer; }

protected:
    void paintEvent(QPaintEvent *) override;

private:
    QImage m_buffer;
    void setPixel(int x, int y, bool on);
    void drawChar(int x, int y, char c);
    void drawString(int x, int y, const QString &s);
};

#endif
