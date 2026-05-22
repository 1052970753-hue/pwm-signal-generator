#include "keymap.h"

InputEvent KeyMap::toInputEvent(QKeyEvent *ev) {
    switch (ev->key()) {
        case Qt::Key_Up:         return EVENT_CW;
        case Qt::Key_Down:       return EVENT_CCW;
        case Qt::Key_Return:
        case Qt::Key_Enter:      return EVENT_CLICK;
        case Qt::Key_Backspace:  return EVENT_LONG_PRESS;
        case Qt::Key_Escape:     return EVENT_BACK;
        default:                 return EVENT_NONE;
    }
}
