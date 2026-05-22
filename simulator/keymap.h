#ifndef KEYMAP_H
#define KEYMAP_H

#include <QKeyEvent>
#include "../common/menu_fsm.h"

class KeyMap {
public:
    static InputEvent toInputEvent(QKeyEvent *event);
};

#endif
