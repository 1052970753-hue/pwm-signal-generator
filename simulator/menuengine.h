#ifndef MENUENGINE_H
#define MENUENGINE_H

#include <QObject>
#include "../common/menu_fsm.h"
#include "../common/protocol_defs.h"

class MenuEngine : public QObject {
    Q_OBJECT

public:
    explicit MenuEngine(QObject *parent = nullptr);

    void processEvent(InputEvent ev);
    StatusData status() const;
    MenuState state() const { return m_state; }

private:
    SystemParams m_params;
    MenuState    m_state;
    uint32_t getEditValue() const;
    void setEditValue(uint32_t val);
};

#endif
