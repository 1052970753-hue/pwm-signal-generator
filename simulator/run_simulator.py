#!/usr/bin/env python3
"""PWM Signal Generator — Simulator (PyQt6)"""
import sys, math
from PyQt6.QtWidgets import (
    QApplication, QMainWindow, QWidget, QHBoxLayout, QVBoxLayout,
    QLabel, QFrame
)
from PyQt6.QtGui import QPainter, QColor, QImage, QFont, QPen, QBrush
from PyQt6.QtCore import Qt, QPoint, QRect, QTimer

# ── Constants ──
OLED_W, OLED_H = 128, 64
SCALE = 4

# Bold pixel font — 5x7, thicker strokes, industrial instrument style
FONT = [
    #sp   !     "     #     $
    0x00, 0x00, 0x00, 0x00, 0x00,  0x00, 0x17, 0x17, 0x00, 0x00,
    0x03, 0x03, 0x00, 0x03, 0x03,  0x14, 0x7F, 0x14, 0x7F, 0x14,
    0x12, 0x2A, 0x7F, 0x2A, 0x24,
    # %     &     '     (     )
    0x23, 0x33, 0x08, 0x66, 0x62,  0x36, 0x49, 0x55, 0x22, 0x50,
    0x00, 0x03, 0x03, 0x00, 0x00,  0x00, 0x1C, 0x22, 0x41, 0x00,
    0x00, 0x41, 0x22, 0x1C, 0x00,
    # *     +     ,     -     .
    0x08, 0x2A, 0x1C, 0x2A, 0x08,  0x08, 0x08, 0x3E, 0x08, 0x08,
    0x00, 0x58, 0x38, 0x00, 0x00,  0x08, 0x08, 0x08, 0x08, 0x08,
    0x00, 0x60, 0x60, 0x00, 0x00,
    # /     0     1     2     3
    0x20, 0x10, 0x08, 0x04, 0x02,  0x3E, 0x51, 0x49, 0x45, 0x3E,
    0x00, 0x42, 0x7F, 0x40, 0x00,  0x42, 0x61, 0x51, 0x49, 0x46,
    0x21, 0x41, 0x45, 0x4B, 0x31,
    # 4     5     6     7     8
    0x18, 0x14, 0x12, 0x7F, 0x10,  0x27, 0x45, 0x45, 0x45, 0x39,
    0x3C, 0x4A, 0x49, 0x49, 0x30,  0x01, 0x71, 0x09, 0x05, 0x03,
    0x36, 0x49, 0x49, 0x49, 0x36,
    # 9     :     ;     <     =
    0x06, 0x49, 0x49, 0x29, 0x1E,  0x00, 0x36, 0x36, 0x00, 0x00,
    0x00, 0x56, 0x36, 0x00, 0x00,  0x08, 0x14, 0x22, 0x41, 0x00,
    0x14, 0x14, 0x14, 0x14, 0x14,
    # >     ?     @     A     B
    0x00, 0x41, 0x22, 0x14, 0x08,  0x02, 0x01, 0x51, 0x09, 0x06,
    0x3E, 0x41, 0x5D, 0x55, 0x1E,  0x7E, 0x11, 0x11, 0x11, 0x7E,
    0x7F, 0x49, 0x49, 0x49, 0x36,
    # C     D     E     F     G
    0x3E, 0x41, 0x41, 0x41, 0x22,  0x7F, 0x41, 0x41, 0x22, 0x1C,
    0x7F, 0x49, 0x49, 0x49, 0x41,  0x7F, 0x09, 0x09, 0x09, 0x01,
    0x3E, 0x41, 0x41, 0x51, 0x72,
    # H     I     J     K     L
    0x7F, 0x08, 0x08, 0x08, 0x7F,  0x00, 0x41, 0x7F, 0x41, 0x00,
    0x20, 0x40, 0x41, 0x3F, 0x01,  0x7F, 0x08, 0x14, 0x22, 0x41,
    0x7F, 0x40, 0x40, 0x40, 0x40,
    # M     N     O     P     Q
    0x7F, 0x06, 0x08, 0x06, 0x7F,  0x7F, 0x06, 0x08, 0x30, 0x7F,
    0x3E, 0x41, 0x41, 0x41, 0x3E,  0x7F, 0x09, 0x09, 0x09, 0x06,
    0x3E, 0x41, 0x51, 0x21, 0x5E,
    # R     S     T     U     V
    0x7F, 0x09, 0x19, 0x29, 0x46,  0x26, 0x49, 0x49, 0x49, 0x32,
    0x01, 0x01, 0x7F, 0x01, 0x01,  0x3F, 0x40, 0x40, 0x40, 0x3F,
    0x1F, 0x20, 0x40, 0x20, 0x1F,
    # W     X     Y     Z     [
    0x7F, 0x30, 0x0C, 0x30, 0x7F,  0x63, 0x14, 0x08, 0x14, 0x63,
    0x07, 0x08, 0x70, 0x08, 0x07,  0x61, 0x51, 0x49, 0x45, 0x43,
    0x00, 0x00, 0x7F, 0x41, 0x41,
    # \     ]     ^     _     `
    0x02, 0x04, 0x08, 0x10, 0x20,  0x41, 0x41, 0x7F, 0x00, 0x00,
    0x04, 0x02, 0x01, 0x02, 0x04,  0x40, 0x40, 0x40, 0x40, 0x40,
    0x00, 0x01, 0x02, 0x04, 0x00,
    # a     b     c     d     e
    0x38, 0x44, 0x44, 0x44, 0x7C,  0x7F, 0x44, 0x44, 0x44, 0x38,
    0x38, 0x44, 0x44, 0x44, 0x28,  0x38, 0x44, 0x44, 0x48, 0x7F,
    0x38, 0x54, 0x54, 0x54, 0x08,
    # f     g     h     i     j
    0x08, 0x7E, 0x09, 0x09, 0x02,  0x0C, 0x52, 0x52, 0x52, 0x3E,
    0x7F, 0x04, 0x04, 0x04, 0x78,  0x00, 0x44, 0x7D, 0x40, 0x00,
    0x20, 0x40, 0x44, 0x3D, 0x00,
    # k     l     m     n     o
    0x7F, 0x10, 0x28, 0x44, 0x00,  0x00, 0x41, 0x7F, 0x40, 0x00,
    0x7C, 0x04, 0x78, 0x04, 0x78,  0x7C, 0x04, 0x04, 0x04, 0x78,
    0x38, 0x44, 0x44, 0x44, 0x38,
    # p     q     r     s     t
    0x7C, 0x14, 0x14, 0x14, 0x08,  0x08, 0x14, 0x14, 0x14, 0x7C,
    0x7C, 0x08, 0x04, 0x04, 0x08,  0x48, 0x54, 0x54, 0x54, 0x20,
    0x04, 0x3F, 0x44, 0x44, 0x20,
    # u     v     w     x     y
    0x3C, 0x40, 0x40, 0x20, 0x7C,  0x1C, 0x20, 0x40, 0x20, 0x1C,
    0x3C, 0x40, 0x30, 0x40, 0x3C,  0x44, 0x28, 0x10, 0x28, 0x44,
    0x0C, 0x50, 0x50, 0x50, 0x3C,
    # z     {     |     }     ~
    0x44, 0x64, 0x54, 0x4C, 0x44,  0x00, 0x08, 0x36, 0x41, 0x00,
    0x00, 0x00, 0x7F, 0x00, 0x00,  0x00, 0x41, 0x36, 0x08, 0x00,
    0x08, 0x08, 0x2A, 0x1C, 0x08,
]

# ── Bold digit bitmaps for prominent value display ──
FONT_DIGITS = {
    '0': [0x3E, 0x51, 0x49, 0x45, 0x3E],
    '1': [0x42, 0x7F, 0x40, 0x00, 0x00],
    '2': [0x62, 0x51, 0x49, 0x49, 0x46],
    '3': [0x22, 0x41, 0x49, 0x49, 0x36],
    '4': [0x18, 0x14, 0x12, 0x7F, 0x10],
    '5': [0x2F, 0x49, 0x49, 0x49, 0x31],
    '6': [0x3E, 0x49, 0x49, 0x49, 0x30],
    '7': [0x01, 0x71, 0x0D, 0x03, 0x01],
    '8': [0x36, 0x49, 0x49, 0x49, 0x36],
    '9': [0x06, 0x49, 0x49, 0x29, 0x1E],
}

# ── Events ──
EVENT_NONE, EVENT_CW, EVENT_CCW, EVENT_CLICK, EVENT_LONG_PRESS, EVENT_BACK = range(6)

# ── Items (cursor cycles Freq/Duty/FG div) ──
ITEM_CH1_FREQ, ITEM_CH1_DUTY = 0, 1
ITEM_CH2_FREQ, ITEM_CH2_DUTY = 2, 3
ITEM_FG_DIV = 4
NUM_ITEMS = 5


class Engine:
    """Two modes: normal (encoder changes value) and select (long-press OK, encoder moves cursor)."""

    def __init__(self):
        self.ch1_freq = 1000
        self.ch1_duty = 50
        self.ch1_on = False
        self.ch2_freq = 1000
        self.ch2_duty = 50
        self.ch2_on = False
        self.fg_div = 2
        self.cursor = 1
        self.selected = False  # True after long press — encoder moves cursor

    def _get(self):
        return [self.ch1_freq, self.ch1_duty,
                self.ch2_freq, self.ch2_duty,
                self.fg_div][self.cursor]

    def _set(self, v):
        if self.cursor == ITEM_CH1_FREQ:
            self.ch1_freq = max(1, min(100000, v))
        elif self.cursor == ITEM_CH1_DUTY:
            self.ch1_duty = max(0, min(100, v))
        elif self.cursor == ITEM_CH2_FREQ:
            self.ch2_freq = max(1, min(100000, v))
        elif self.cursor == ITEM_CH2_DUTY:
            self.ch2_duty = max(0, min(100, v))
        elif self.cursor == ITEM_FG_DIV:
            self.fg_div = max(1, min(99, v))

    def process(self, ev):
        if ev == EVENT_NONE:
            return

        if self.selected:
            if ev == EVENT_CW:
                self.cursor = (self.cursor + 1) % NUM_ITEMS
            elif ev == EVENT_CCW:
                self.cursor = (self.cursor - 1) % NUM_ITEMS
            elif ev == EVENT_CLICK:
                self.selected = False  # exit selection, stay on item
            elif ev == EVENT_BACK:
                self.selected = False
            return

        # Normal mode — encoder directly changes value
        if ev == EVENT_CW:
            self._set(self._get() + 1)
        elif ev == EVENT_CCW:
            self._set(self._get() - 1)
        elif ev == EVENT_LONG_PRESS:
            self.selected = True
        elif ev == EVENT_CLICK:
            # Short press OK: toggle channel based on cursor position
            if self.cursor <= ITEM_CH1_DUTY:
                self.ch1_on = not self.ch1_on
            elif self.cursor <= ITEM_CH2_DUTY:
                self.ch2_on = not self.ch2_on


# ── OLED drawing helpers ──

class OLEDWidget(QWidget):
    def __init__(self, parent=None):
        super().__init__(parent)
        self.buf = QImage(OLED_W, OLED_H, QImage.Format.Format_Mono)
        self.buf.fill(0)
        self.setFixedSize(OLED_W * SCALE + 16, OLED_H * SCALE + 16)
        self._eng = None
        self._blink = True
        self._blink_timer = QTimer(self)
        self._blink_timer.timeout.connect(self._toggle_blink)
        self._blink_timer.start(400)

    def _toggle_blink(self):
        self._blink = not self._blink
        if self._eng:
            self.render(self._eng)

    def _px(self, x, y, on=True):
        if 0 <= x < OLED_W and 0 <= y < OLED_H:
            self.buf.setPixel(x, y, 1 if on else 0)

    def _hline(self, x1, x2, y):
        for x in range(max(0, x1), min(OLED_W, x2 + 1)):
            self._px(x, y, True)

    def _vline(self, x, y1, y2):
        for y in range(max(0, y1), min(OLED_H, y2 + 1)):
            self._px(x, y, True)

    def _rect(self, x, y, w, h, fill=False):
        if fill:
            for yy in range(y, min(OLED_H, y + h)):
                for xx in range(x, min(OLED_W, x + w)):
                    self._px(xx, yy, True)
        else:
            self._hline(x, x + w - 1, y)
            self._hline(x, x + w - 1, y + h - 1)
            self._vline(x, y, y + h - 1)
            self._vline(x + w - 1, y, y + h - 1)

    def _char(self, x, y, c, inv=False):
        idx = ord(c) - 32
        if idx < 0 or idx > 94:
            idx = 0
        if inv:
            for dy in range(8):
                for dx in range(6):
                    self._px(x + dx, y + dy, True)
            for i in range(5):
                line = FONT[idx * 5 + i]
                for j in range(8):
                    if line & (1 << j):
                        self._px(x + i, y + j, False)
        else:
            for i in range(5):
                line = FONT[idx * 5 + i]
                for j in range(8):
                    if line & (1 << j):
                        self._px(x + i, y + j, True)

    def _text(self, x, y, s, inv=False):
        for ch in s:
            self._char(x, y, ch, inv)
            x += 6

    def _text_r(self, xr, y, s, inv=False):
        self._text(xr - len(s) * 6, y, s, inv)

    def _bar(self, x, y, w, h, pct):
        self._rect(x, y, w, h)
        fw = max(0, min(w - 2, int((w - 2) * pct / 100)))
        if fw > 0:
            self._rect(x + 1, y + 1, fw, h - 2, fill=True)

    def _circle(self, x, y, on):
        cx, cy, r = x + 3, y + 3, 3
        for dy in range(-r, r + 1):
            for dx in range(-r, r + 1):
                d2 = dx * dx + dy * dy
                if on:
                    if d2 <= r * r:
                        self._px(cx + dx, cy + dy, True)
                else:
                    if (r - 1) * (r - 1) < d2 <= r * r:
                        self._px(cx + dx, cy + dy, True)

    def paintEvent(self, _):
        p = QPainter(self)
        p.setRenderHint(QPainter.RenderHint.Antialiasing)
        p.fillRect(self.rect(), QColor(0xf0, 0xf1, 0xf5))
        ox = (self.width() - OLED_W * SCALE) // 2
        oy = (self.height() - OLED_H * SCALE) // 2
        bw, bh = OLED_W * SCALE + 12, OLED_H * SCALE + 12
        # Outer bezel (light gray plastic)
        p.setPen(Qt.PenStyle.NoPen)
        p.setBrush(QColor(0xd0, 0xd2, 0xd8))
        p.drawRoundedRect(ox - 6, oy - 6, bw, bh, 8, 8)
        # Inner bezel (dark recess)
        p.setBrush(QColor(0x08, 0x08, 0x10))
        p.drawRoundedRect(ox - 2, oy - 2, bw - 8, bh - 8, 5, 5)
        # OLED pixels
        for yy in range(OLED_H):
            for xx in range(OLED_W):
                if self.buf.pixel(xx, yy) & 1:
                    p.fillRect(ox + xx * SCALE, oy + yy * SCALE, SCALE, SCALE, QColor(0xFF, 0xD8, 0x30))

    # ── Render ──
    def render(self, eng: Engine):
        self.buf.fill(0)
        cur = eng.cursor
        sel = eng.selected

        # Frame border (1px, content starts at x=1,y=1)
        self._rect(0, 0, OLED_W, OLED_H)

        # Title bar — 20% taller: 8→10px. Content y: 1..10, separator at y=11
        self._hline(1, 126, 11)
        # "PWM_TOOL" centered: (128 - 8*6) / 2 = 40
        self._text(40, 2, "PWM_TOOL")

        def marker(idx):
            if cur == idx:
                if sel:
                    return ">" if self._blink else " "
                return ">"
            return " "

        def row(x, y, xvr, idx, label, val_str):
            self._text(x, y, marker(idx))
            self._text(x + 6, y, label)
            self._text_r(xvr, y, val_str)

        # ── CH1 (left col: x=2..61, inner width 60) ──
        self._circle(3, 14, eng.ch1_on)
        self._text(12, 14, "CH1")
        self._text_r(59, 14, "ON" if eng.ch1_on else "OFF")
        row(3, 26, 59, ITEM_CH1_FREQ, "Fr", f"{eng.ch1_freq}Hz")
        row(3, 38, 59, ITEM_CH1_DUTY, "Duty", f"{eng.ch1_duty}%")

        # ── CH2 (right col: x=67..126, inner width 60) ──
        self._vline(65, 12, 48)
        self._circle(68, 14, eng.ch2_on)
        self._text(77, 14, "CH2")
        self._text_r(125, 14, "ON" if eng.ch2_on else "OFF")
        row(68, 26, 125, ITEM_CH2_FREQ, "Fr", f"{eng.ch2_freq}Hz")
        row(68, 38, 125, ITEM_CH2_DUTY, "Duty", f"{eng.ch2_duty}%")

        # ── FG (y=52..62, divider at y=51) ──
        self._hline(1, 126, 51)
        self._text(3, 54, "FG")
        self._text_r(62, 54, "0")
        self._text(64, 54, "RPM")
        # /2 — selectable, right side
        self._text(98, 54, marker(ITEM_FG_DIV))
        self._text_r(125, 54, f"/{eng.fg_div}")

        self.update()


# ── Encoder dial widget ──

class EncoderDial(QWidget):
    LONG_PRESS_MS = 800
    SZ = 260
    C = 130  # center

    def __init__(self, parent=None):
        super().__init__(parent)
        self.setFixedSize(self.SZ, self.SZ)
        self._hover = None
        self._pressed = None
        self._callback = None
        self._lp_timer = QTimer(self)
        self._lp_timer.setSingleShot(True)
        self._lp_timer.timeout.connect(self._on_long_press)
        self._lp_fired = False
        self.setMouseTracking(True)

    def set_callback(self, cb):
        self._callback = cb

    def _on_long_press(self):
        self._lp_fired = True
        if self._callback and self._pressed == 'ok':
            self._callback(EVENT_LONG_PRESS)

    def _hit_test(self, pos):
        c = self.C
        dx, dy = pos.x() - c, pos.y() - c
        dist = (dx * dx + dy * dy) ** 0.5
        if dist < 55:
            return 'ok'
        if dist > 116:
            return None
        angle = math.atan2(-dy, dx)
        deg = math.degrees(angle)
        if -45 <= deg < 45:
            return 'cw'
        if 135 <= deg or deg < -135:
            return 'ccw'
        return None

    def mouseMoveEvent(self, ev):
        h = self._hit_test(ev.pos())
        if h != self._hover:
            self._hover = h
            self.update()

    def mousePressEvent(self, ev):
        h = self._hit_test(ev.pos())
        self._pressed = h
        self._lp_fired = False
        if h == 'ok':
            self._lp_timer.start(self.LONG_PRESS_MS)
        self.update()

    def mouseReleaseEvent(self, ev):
        self._lp_timer.stop()
        h = self._hit_test(ev.pos())
        if h and h == self._pressed and not self._lp_fired:
            mapping = {'cw': EVENT_CW, 'ccw': EVENT_CCW, 'ok': EVENT_CLICK}
            if self._callback and h in mapping:
                self._callback(mapping[h])
        self._pressed = None
        self._lp_fired = False
        self.update()

    def leaveEvent(self, ev):
        self._hover = None
        self._pressed = None
        self.update()

    def paintEvent(self, _):
        p = QPainter(self)
        p.setRenderHint(QPainter.RenderHint.Antialiasing)
        c = self.C  # 130

        # ── Base shadow ──
        p.setPen(Qt.PenStyle.NoPen)
        p.setBrush(QColor(0xd0, 0xd2, 0xdc))
        p.drawEllipse(QPoint(c, c + 2), 126, 126)

        # ── Outer ring ──
        p.setBrush(QColor(0xea, 0xec, 0xf2))
        p.drawEllipse(QPoint(c, c), 124, 124)

        # ── Ring groove ──
        p.setBrush(QColor(0xde, 0xe0, 0xe8))
        p.drawEllipse(QPoint(c, c), 112, 112)

        # ── Inner plate ──
        p.setBrush(QColor(0xf0, 0xf1, 0xf6))
        p.drawEllipse(QPoint(c, c), 102, 102)

        # ── Tick marks ──
        for i in range(36):
            a = math.radians(i * 10)
            bright = 0xc0 + (0x18 if i % 9 == 0 else 0)
            p.setPen(QPen(QColor(bright, bright, bright + 6), 1))
            inner = 104 if i % 3 == 0 else 108
            p.drawLine(
                int(c + inner * math.cos(a)), int(c - inner * math.sin(a)),
                int(c + 110 * math.cos(a)), int(c - 110 * math.sin(a))
            )

        # ── CCW (left) ──
        ccw_p = self._pressed == 'ccw'
        ccw_h = self._hover == 'ccw'
        if ccw_p:
            ring, fill = QColor(0x1a, 0x7a, 0xd0), QColor(0xd8, 0xec, 0xf8)
        elif ccw_h:
            ring, fill = QColor(0x30, 0x90, 0xe0), QColor(0xe4, 0xf0, 0xfa)
        else:
            ring, fill = QColor(0x80, 0x98, 0xb8), QColor(0xf4, 0xf5, 0xf8)
        p.setBrush(fill)
        p.setPen(QPen(ring, 2))
        p.drawEllipse(QPoint(c - 61, c), 30, 30)
        p.setPen(QPen(ring, 3))
        ax = c - 61
        p.drawLine(ax - 12, c, ax + 8, c - 12)
        p.drawLine(ax - 12, c, ax + 8, c + 12)

        # ── CW (right) ──
        cw_p = self._pressed == 'cw'
        cw_h = self._hover == 'cw'
        if cw_p:
            ring2, fill2 = QColor(0x1a, 0x7a, 0xd0), QColor(0xd8, 0xec, 0xf8)
        elif cw_h:
            ring2, fill2 = QColor(0x30, 0x90, 0xe0), QColor(0xe4, 0xf0, 0xfa)
        else:
            ring2, fill2 = QColor(0x80, 0x98, 0xb8), QColor(0xf4, 0xf5, 0xf8)
        p.setBrush(fill2)
        p.setPen(QPen(ring2, 2))
        p.drawEllipse(QPoint(c + 61, c), 30, 30)
        p.setPen(QPen(ring2, 3))
        ax = c + 61
        p.drawLine(ax + 12, c, ax - 8, c - 12)
        p.drawLine(ax + 12, c, ax - 8, c + 12)

        # ── Center OK button ──
        ok_p = self._pressed == 'ok'
        ok_h = self._hover == 'ok'
        if ok_p:
            bc, bg, tc = QColor(0x00, 0x7a, 0xc6), QColor(0x00, 0x90, 0xe0), QColor(0xff, 0xff, 0xff)
        elif ok_h:
            bc, bg, tc = QColor(0x00, 0x68, 0xb0), QColor(0x00, 0x80, 0xd0), QColor(0xff, 0xff, 0xff)
        else:
            bc, bg, tc = QColor(0x00, 0x58, 0x98), QColor(0x00, 0x70, 0xbc), QColor(0xe0, 0xf0, 0xf8)
        # Glow
        p.setPen(Qt.PenStyle.NoPen)
        p.setBrush(QColor(bg.red(), bg.green(), bg.blue(), 60))
        p.drawEllipse(QPoint(c, c), 52, 52)
        # Body
        p.setBrush(bc)
        p.drawEllipse(QPoint(c, c), 46, 46)
        # Highlight
        p.setBrush(QColor(bc.red() + 20, bc.green() + 20, bc.blue() + 20))
        p.drawEllipse(QPoint(c, c - 6), 34, 28)
        # Text
        p.setPen(tc)
        p.setFont(QFont("Segoe UI", 16, QFont.Weight.Bold))
        p.drawText(QRect(c - 24, c - 14, 48, 28), Qt.AlignmentFlag.AlignCenter, "OK")


# ── Main window ──

APP_STYLE = """
    QMainWindow { background: #f0f1f5; }
    QLabel { color: #505870; }
"""


class MainWindow(QMainWindow):
    def __init__(self):
        super().__init__()
        self.setWindowTitle("PWM Signal Generator Simulator")
        self.setFixedSize(600, 780)
        self.eng = Engine()
        self.setStyleSheet(APP_STYLE)

        central = QWidget()
        root = QVBoxLayout(central)
        root.setContentsMargins(24, 20, 24, 20)
        root.setSpacing(12)

        # ── Title ──
        title = QLabel("PWM TOOL")
        title.setAlignment(Qt.AlignmentFlag.AlignCenter)
        title.setStyleSheet("color:#2a3050; font-size:18px; font-weight:bold; letter-spacing:4px;")
        root.addWidget(title)

        # ── OLED ──
        self.oled = OLEDWidget()
        self.oled._eng = self.eng
        root.addWidget(self.oled, 0, Qt.AlignmentFlag.AlignCenter)

        # ── State label ──
        self.state_lbl = QLabel("CH1 DUTY")
        self.state_lbl.setAlignment(Qt.AlignmentFlag.AlignCenter)
        self.state_lbl.setStyleSheet("""
            color:#4a60a0; font-size:14px; font-weight:bold;
            background:#fff; border:1px solid #d8dae2;
            border-radius:6px; padding:6px 16px;
            font-family:Consolas,monospace;
        """)
        root.addWidget(self.state_lbl)

        # ── Channel status ──
        status_row = QHBoxLayout()
        status_row.setSpacing(12)

        self.ch1_card = QLabel("CH1  OFF")
        self.ch1_card.setAlignment(Qt.AlignmentFlag.AlignCenter)
        self.ch1_card.setStyleSheet("""
            color:#998888; font-size:14px; font-weight:bold;
            font-family:Consolas,monospace;
            background:#fff; border:1px solid #d8dae2;
            border-radius:8px; padding:8px 20px;
        """)
        status_row.addWidget(self.ch1_card)

        self.ch2_card = QLabel("CH2  OFF")
        self.ch2_card.setAlignment(Qt.AlignmentFlag.AlignCenter)
        self.ch2_card.setStyleSheet("""
            color:#998888; font-size:14px; font-weight:bold;
            font-family:Consolas,monospace;
            background:#fff; border:1px solid #d8dae2;
            border-radius:8px; padding:8px 20px;
        """)
        status_row.addWidget(self.ch2_card)
        root.addLayout(status_row)

        # ── Encoder ──
        el = QLabel("ENCODER")
        el.setAlignment(Qt.AlignmentFlag.AlignCenter)
        el.setStyleSheet("color:#8090a0; font-size:11px; letter-spacing:2px;")
        root.addWidget(el)

        self.encoder = EncoderDial()
        self.encoder.set_callback(self._on)
        root.addWidget(self.encoder, 0, Qt.AlignmentFlag.AlignCenter)

        # ── Tip ──
        tip = QLabel("OK  toggle channel  |  Long-press  select item")
        tip.setAlignment(Qt.AlignmentFlag.AlignCenter)
        tip.setStyleSheet("color:#a0a4b0; font-size:10px; letter-spacing:1px;")
        root.addWidget(tip)

        root.addStretch()
        self.setCentralWidget(central)
        self._refresh()

    def _on(self, ev):
        self.eng.process(ev)
        self._refresh()

    def _refresh(self):
        self.oled.render(self.eng)

        names = {0: "CH1 FREQ", 1: "CH1 DUTY",
                 2: "CH2 FREQ", 3: "CH2 DUTY",
                 4: "FG DIV"}
        lbl = names.get(self.eng.cursor, "")
        if self.eng.selected:
            lbl += "  [SELECT]"
        self.state_lbl.setText(lbl)

        if self.eng.ch1_on:
            self.ch1_card.setText("CH1   ON")
            self.ch1_card.setStyleSheet("color:#008844; font-size:14px; font-weight:bold; font-family:Consolas,monospace; background:#e8f8f0; border:1px solid #b0e8cc; border-radius:8px; padding:8px 20px;")
        else:
            self.ch1_card.setText("CH1  OFF")
            self.ch1_card.setStyleSheet("color:#998888; font-size:14px; font-weight:bold; font-family:Consolas,monospace; background:#fff; border:1px solid #d8dae2; border-radius:8px; padding:8px 20px;")

        if self.eng.ch2_on:
            self.ch2_card.setText("CH2   ON")
            self.ch2_card.setStyleSheet("color:#008844; font-size:14px; font-weight:bold; font-family:Consolas,monospace; background:#e8f8f0; border:1px solid #b0e8cc; border-radius:8px; padding:8px 20px;")
        else:
            self.ch2_card.setText("CH2  OFF")
            self.ch2_card.setStyleSheet("color:#998888; font-size:14px; font-weight:bold; font-family:Consolas,monospace; background:#fff; border:1px solid #d8dae2; border-radius:8px; padding:8px 20px;")


if __name__ == "__main__":
    app = QApplication(sys.argv)
    app.setStyle("Fusion")
    w = MainWindow()
    w.show()
    sys.exit(app.exec())
