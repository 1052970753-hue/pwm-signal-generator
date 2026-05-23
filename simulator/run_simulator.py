#!/usr/bin/env python3
"""PWM Signal Generator — Simulator (PyQt6) with Multi-Mode & Test Support"""
import sys, math, struct, os, time
from PyQt6.QtWidgets import (
    QApplication, QMainWindow, QWidget, QHBoxLayout, QVBoxLayout,
    QLabel, QFrame, QComboBox, QPushButton, QFileDialog, QGroupBox
)
from PyQt6.QtGui import QPainter, QColor, QImage, QFont, QPen, QBrush
from PyQt6.QtCore import Qt, QPoint, QRect, QTimer

try:
    import serial
    import serial.tools.list_ports
    HAS_SERIAL = True
except ImportError:
    HAS_SERIAL = False

# ── Constants ──
OLED_W, OLED_H = 128, 64
SCALE = 4

# ── Protocol constants ──
FRAME_HEADER_PC2MCU = 0xAA
FRAME_HEADER_MCU2PC = 0xBB
CMD_READ_STATUS   = 0x10
CMD_WRITE_PWM     = 0x20
CMD_WRITE_FG_DIV  = 0x30
CMD_KEY_EVENT     = 0x41
CMD_SET_TEST      = 0x42
CMD_START_TEST    = 0x43
CMD_STOP_TEST     = 0x44
CMD_EXPORT_DATA   = 0x50
CMD_EXPORT_CHUNK  = 0x51
CMD_EXPORT_DONE   = 0x52

# ── CRC8 ──
def crc8(data: bytes) -> int:
    crc = 0x00
    for b in data:
        crc ^= b
        for _ in range(8):
            if crc & 0x80:
                crc = ((crc << 1) ^ 0x07) & 0xFF
            else:
                crc = (crc << 1) & 0xFF
    return crc

def build_frame(cmd: int, data: bytes = b'') -> bytes:
    header = bytes([FRAME_HEADER_PC2MCU, cmd, len(data)]) + data
    return header + bytes([crc8(header)])

# ── Events ──
EVENT_NONE, EVENT_CW, EVENT_CCW, EVENT_CLICK, EVENT_LONG_PRESS, EVENT_DOUBLE_CLICK = range(6)

# ── Modes ──
MODE_PWM_FG, MODE_FG, MODE_CH1, MODE_CH2, MODE_TEST = range(5)
NUM_MODES = 5
MODE_NAMES = ["PWM-FG", "FG模式", "CH1", "CH2", "测试模式"]

# ── Items ──
ITEM_CH1_FREQ, ITEM_CH1_DUTY, ITEM_CH2_FREQ, ITEM_CH2_DUTY, ITEM_FG_DIV = range(5)
NUM_ITEMS = 5

# Test items
TEST_ITEM_CHANNEL, TEST_ITEM_FREQ, TEST_ITEM_DUTY, TEST_ITEM_CYCLES = 0, 1, 2, 3
TEST_ITEM_ON_TIME, TEST_ITEM_OFF_TIME, TEST_ITEM_START = 4, 5, 6
NUM_TEST_ITEMS = 7


class Engine:
    """Multi-mode engine with test support."""

    def __init__(self):
        # PWM params
        self.ch1_freq = 1000
        self.ch1_duty = 50
        self.ch1_on = False
        self.ch2_freq = 1000
        self.ch2_duty = 50
        self.ch2_on = False
        self.fg_div = 2
        self.fg_rpm = 0

        # Mode & navigation
        self.mode = MODE_PWM_FG
        self.cursor = 1
        self.selected = False

        # Test config
        self.test_channel = 1
        self.test_freq = 1000
        self.test_duty = 50
        self.test_cycles = 10
        self.test_on_sec = 5
        self.test_off_sec = 3
        self.test_running = False
        self.test_record_count = 0
        self.test_current_cycle = 0

        # Export
        self.export_csv_lines = []
        self.export_active = False

    def _get_max_items(self):
        if self.mode == MODE_TEST:
            return NUM_TEST_ITEMS
        elif self.mode == MODE_PWM_FG:
            return NUM_ITEMS
        else:
            return 3  # Freq, Duty, Enable

    # ── Value get/set per mode ──

    def _get_value(self):
        if self.mode == MODE_PWM_FG:
            return [self.ch1_freq, self.ch1_duty,
                    self.ch2_freq, self.ch2_duty,
                    self.fg_div][self.cursor]
        elif self.mode == MODE_CH1:
            return [self.ch1_freq, self.ch1_duty, 0][self.cursor]
        elif self.mode == MODE_CH2:
            return [self.ch2_freq, self.ch2_duty, 0][self.cursor]
        elif self.mode == MODE_FG:
            return self.fg_div
        return 0

    def _set_value(self, v):
        if self.mode == MODE_PWM_FG:
            limits = [(1, 100000), (0, 100), (1, 100000), (0, 100), (1, 99)]
            lo, hi = limits[self.cursor]
            v = max(lo, min(hi, v))
            if self.cursor == ITEM_CH1_FREQ: self.ch1_freq = v
            elif self.cursor == ITEM_CH1_DUTY: self.ch1_duty = v
            elif self.cursor == ITEM_CH2_FREQ: self.ch2_freq = v
            elif self.cursor == ITEM_CH2_DUTY: self.ch2_duty = v
            elif self.cursor == ITEM_FG_DIV: self.fg_div = v
        elif self.mode == MODE_CH1:
            if self.cursor == 0: self.ch1_freq = max(1, min(100000, v))
            elif self.cursor == 1: self.ch1_duty = max(0, min(100, v))
        elif self.mode == MODE_CH2:
            if self.cursor == 0: self.ch2_freq = max(1, min(100000, v))
            elif self.cursor == 1: self.ch2_duty = max(0, min(100, v))
        elif self.mode == MODE_FG:
            self.fg_div = max(1, min(99, v))

    def _get_test_value(self):
        return [self.test_channel, self.test_freq, self.test_duty,
                self.test_cycles, self.test_on_sec, self.test_off_sec, 0][self.cursor]

    def _set_test_value(self, delta):
        if self.cursor == TEST_ITEM_CHANNEL:
            self.test_channel = 2 if self.test_channel == 1 else 1
        elif self.cursor == TEST_ITEM_FREQ:
            self.test_freq = max(1, min(100000, self.test_freq + delta))
        elif self.cursor == TEST_ITEM_DUTY:
            self.test_duty = max(0, min(100, self.test_duty + delta))
        elif self.cursor == TEST_ITEM_CYCLES:
            self.test_cycles = max(1, min(999, self.test_cycles + delta))
        elif self.cursor == TEST_ITEM_ON_TIME:
            self.test_on_sec = max(1, min(60, self.test_on_sec + delta))
        elif self.cursor == TEST_ITEM_OFF_TIME:
            self.test_off_sec = max(1, min(60, self.test_off_sec + delta))

    def process(self, ev):
        if ev == EVENT_NONE:
            return

        # Double-click: switch mode (always active)
        if ev == EVENT_DOUBLE_CLICK:
            self.mode = (self.mode + 1) % NUM_MODES
            self.cursor = 0
            self.selected = False
            return

        # Test running: only click stops
        if self.mode == MODE_TEST and self.test_running:
            if ev == EVENT_CLICK:
                self.test_running = False
            return

        # Select mode: encoder moves cursor
        if self.selected:
            mi = self._get_max_items()
            if ev == EVENT_CW:
                self.cursor = (self.cursor + 1) % mi
            elif ev == EVENT_CCW:
                self.cursor = (self.cursor - 1) % mi
            elif ev == EVENT_CLICK:
                self.selected = False
            return

        # Normal mode
        if self.mode == MODE_TEST:
            if ev == EVENT_CW:
                self._set_test_value(1)
            elif ev == EVENT_CCW:
                self._set_test_value(-1)
            elif ev == EVENT_CLICK:
                if self.cursor == TEST_ITEM_START:
                    self.test_running = True
                    self.test_record_count = 0
                    self.test_current_cycle = 0
            elif ev == EVENT_LONG_PRESS:
                self.selected = True
        elif self.mode == MODE_PWM_FG:
            if ev == EVENT_CW:
                self._set_value(self._get_value() + 1)
            elif ev == EVENT_CCW:
                self._set_value(self._get_value() - 1)
            elif ev == EVENT_CLICK:
                if self.cursor <= ITEM_CH1_DUTY:
                    self.ch1_on = not self.ch1_on
                elif self.cursor <= ITEM_CH2_DUTY:
                    self.ch2_on = not self.ch2_on
            elif ev == EVENT_LONG_PRESS:
                self.selected = True
        elif self.mode == MODE_FG:
            if ev == EVENT_CW:
                self.fg_div = min(99, self.fg_div + 1)
            elif ev == EVENT_CCW:
                self.fg_div = max(1, self.fg_div - 1)
            elif ev == EVENT_LONG_PRESS:
                self.selected = True
        elif self.mode in (MODE_CH1, MODE_CH2):
            ch = 1 if self.mode == MODE_CH1 else 2
            if ev == EVENT_CW:
                self._set_value(self._get_value() + 1)
            elif ev == EVENT_CCW:
                self._set_value(self._get_value() - 1)
            elif ev == EVENT_CLICK:
                if ch == 1:
                    self.ch1_on = not self.ch1_on
                else:
                    self.ch2_on = not self.ch2_on
            elif ev == EVENT_LONG_PRESS:
                self.selected = True


# ── Serial communication ──

class SerialComm:
    def __init__(self):
        self.ser = None
        self.connected = False
        self._rx_buf = bytearray()

    def list_ports(self):
        if not HAS_SERIAL:
            return []
        return [(p.device, p.description) for p in serial.tools.list_ports.comports()]

    def connect(self, port, baud=115200):
        try:
            self.ser = serial.Serial(port, baud, timeout=0.05)
            self.connected = True
            return True
        except Exception:
            self.connected = False
            self.ser = None
            return False

    def disconnect(self):
        if self.ser:
            try:
                self.ser.close()
            except Exception:
                pass
        self.connected = False
        self.ser = None

    def _send(self, frame):
        if self.connected and self.ser:
            try:
                self.ser.write(frame)
            except Exception:
                self.disconnect()

    def send_key_event(self, event_code):
        self._send(build_frame(CMD_KEY_EVENT, struct.pack('B', event_code)))

    def send_write_pwm(self, channel, freq, duty, enable):
        self._send(build_frame(CMD_WRITE_PWM, struct.pack('<BIBB', channel, freq, duty, enable)))

    def send_write_fg_div(self, div):
        self._send(build_frame(CMD_WRITE_FG_DIV, struct.pack('B', div)))

    def send_read_status(self):
        self._send(build_frame(CMD_READ_STATUS))

    def send_start_test(self):
        self._send(build_frame(CMD_START_TEST))

    def send_stop_test(self):
        self._send(build_frame(CMD_STOP_TEST))

    def send_export_data(self):
        self._send(build_frame(CMD_EXPORT_DATA))

    def poll(self):
        """Read available bytes, parse frames, return (cmd, data) or None."""
        if not self.connected or not self.ser:
            return None
        try:
            n = self.ser.in_waiting
            if n > 0:
                self._rx_buf.extend(self.ser.read(n))
        except Exception:
            self.disconnect()
            return None

        while len(self._rx_buf) >= 5:
            if self._rx_buf[0] != FRAME_HEADER_MCU2PC:
                self._rx_buf.pop(0)
                continue
            cmd = self._rx_buf[1]
            length = self._rx_buf[2]
            total = 3 + length + 1
            if len(self._rx_buf) < total:
                break
            frame = bytes(self._rx_buf[:total])
            calc_crc = crc8(frame[:-1])
            if calc_crc == frame[-1]:
                data = frame[3:3 + length]
                self._rx_buf = self._rx_buf[total:]
                return (cmd, data)
            else:
                self._rx_buf.pop(0)
        return None

    def read_status_data(self, data):
        """Parse StatusData from frame data (25 bytes)."""
        if len(data) < 25:
            return None
        return {
            'ch1_freq':    struct.unpack_from('<I', data, 0)[0],
            'ch1_duty':    data[4],
            'ch1_on':      bool(data[5]),
            'ch2_freq':    struct.unpack_from('<I', data, 6)[0],
            'ch2_duty':    data[10],
            'ch2_on':      bool(data[11]),
            'fg_freq_mhz': struct.unpack_from('<I', data, 12)[0],
            'fg_div':      data[16],
            'rpm':         struct.unpack_from('<H', data, 17)[0],
            'mode':        data[19],
            'test_state':  data[20],
            'test_cycle':  struct.unpack_from('<H', data, 21)[0],
            'test_total':  struct.unpack_from('<H', data, 23)[0],
        }


# ── OLED drawing ──

FONT = [
    0x00, 0x00, 0x00, 0x00, 0x00,  0x00, 0x17, 0x17, 0x00, 0x00,
    0x03, 0x03, 0x00, 0x03, 0x03,  0x14, 0x7F, 0x14, 0x7F, 0x14,
    0x12, 0x2A, 0x7F, 0x2A, 0x24,
    0x23, 0x33, 0x08, 0x66, 0x62,  0x36, 0x49, 0x55, 0x22, 0x50,
    0x00, 0x03, 0x03, 0x00, 0x00,  0x00, 0x1C, 0x22, 0x41, 0x00,
    0x00, 0x41, 0x22, 0x1C, 0x00,
    0x08, 0x2A, 0x1C, 0x2A, 0x08,  0x08, 0x08, 0x3E, 0x08, 0x08,
    0x00, 0x58, 0x38, 0x00, 0x00,  0x08, 0x08, 0x08, 0x08, 0x08,
    0x00, 0x60, 0x60, 0x00, 0x00,
    0x20, 0x10, 0x08, 0x04, 0x02,  0x3E, 0x51, 0x49, 0x45, 0x3E,
    0x00, 0x42, 0x7F, 0x40, 0x00,  0x42, 0x61, 0x51, 0x49, 0x46,
    0x21, 0x41, 0x45, 0x4B, 0x31,
    0x18, 0x14, 0x12, 0x7F, 0x10,  0x27, 0x45, 0x45, 0x45, 0x39,
    0x3C, 0x4A, 0x49, 0x49, 0x30,  0x01, 0x71, 0x09, 0x05, 0x03,
    0x36, 0x49, 0x49, 0x49, 0x36,
    0x06, 0x49, 0x49, 0x29, 0x1E,  0x00, 0x36, 0x36, 0x00, 0x00,
    0x00, 0x56, 0x36, 0x00, 0x00,  0x08, 0x14, 0x22, 0x41, 0x00,
    0x14, 0x14, 0x14, 0x14, 0x14,
    0x00, 0x41, 0x22, 0x14, 0x08,  0x02, 0x01, 0x51, 0x09, 0x06,
    0x3E, 0x41, 0x5D, 0x55, 0x1E,  0x7E, 0x11, 0x11, 0x11, 0x7E,
    0x7F, 0x49, 0x49, 0x49, 0x36,
    0x3E, 0x41, 0x41, 0x41, 0x22,  0x7F, 0x41, 0x41, 0x22, 0x1C,
    0x7F, 0x49, 0x49, 0x49, 0x41,  0x7F, 0x09, 0x09, 0x09, 0x01,
    0x3E, 0x41, 0x41, 0x51, 0x72,
    0x7F, 0x08, 0x08, 0x08, 0x7F,  0x00, 0x41, 0x7F, 0x41, 0x00,
    0x20, 0x40, 0x41, 0x3F, 0x01,  0x7F, 0x08, 0x14, 0x22, 0x41,
    0x7F, 0x40, 0x40, 0x40, 0x40,
    0x7F, 0x06, 0x08, 0x06, 0x7F,  0x7F, 0x06, 0x08, 0x30, 0x7F,
    0x3E, 0x41, 0x41, 0x41, 0x3E,  0x7F, 0x09, 0x09, 0x09, 0x06,
    0x3E, 0x41, 0x51, 0x21, 0x5E,
    0x7F, 0x09, 0x19, 0x29, 0x46,  0x26, 0x49, 0x49, 0x49, 0x32,
    0x01, 0x01, 0x7F, 0x01, 0x01,  0x3F, 0x40, 0x40, 0x40, 0x3F,
    0x1F, 0x20, 0x40, 0x20, 0x1F,
    0x7F, 0x30, 0x0C, 0x30, 0x7F,  0x63, 0x14, 0x08, 0x14, 0x63,
    0x07, 0x08, 0x70, 0x08, 0x07,  0x61, 0x51, 0x49, 0x45, 0x43,
    0x00, 0x00, 0x7F, 0x41, 0x41,
    0x02, 0x04, 0x08, 0x10, 0x20,  0x41, 0x41, 0x7F, 0x00, 0x00,
    0x04, 0x02, 0x01, 0x02, 0x04,  0x40, 0x40, 0x40, 0x40, 0x40,
    0x00, 0x01, 0x02, 0x04, 0x00,
    0x38, 0x44, 0x44, 0x44, 0x7C,  0x7F, 0x44, 0x44, 0x44, 0x38,
    0x38, 0x44, 0x44, 0x44, 0x28,  0x38, 0x44, 0x44, 0x48, 0x7F,
    0x38, 0x54, 0x54, 0x54, 0x08,
    0x08, 0x7E, 0x09, 0x09, 0x02,  0x0C, 0x52, 0x52, 0x52, 0x3E,
    0x7F, 0x04, 0x04, 0x04, 0x78,  0x00, 0x44, 0x7D, 0x40, 0x00,
    0x20, 0x40, 0x44, 0x3D, 0x00,
    0x7F, 0x10, 0x28, 0x44, 0x00,  0x00, 0x41, 0x7F, 0x40, 0x00,
    0x7C, 0x04, 0x78, 0x04, 0x78,  0x7C, 0x04, 0x04, 0x04, 0x78,
    0x38, 0x44, 0x44, 0x44, 0x38,
    0x7C, 0x14, 0x14, 0x14, 0x08,  0x08, 0x14, 0x14, 0x14, 0x7C,
    0x7C, 0x08, 0x04, 0x04, 0x08,  0x48, 0x54, 0x54, 0x54, 0x20,
    0x04, 0x3F, 0x44, 0x44, 0x20,
    0x3C, 0x40, 0x40, 0x20, 0x7C,  0x1C, 0x20, 0x40, 0x20, 0x1C,
    0x3C, 0x40, 0x30, 0x40, 0x3C,  0x44, 0x28, 0x10, 0x28, 0x44,
    0x0C, 0x50, 0x50, 0x50, 0x3C,
    0x44, 0x64, 0x54, 0x4C, 0x44,  0x00, 0x08, 0x36, 0x41, 0x00,
    0x00, 0x00, 0x7F, 0x00, 0x00,  0x00, 0x41, 0x36, 0x08, 0x00,
    0x08, 0x08, 0x2A, 0x1C, 0x08,
]


class OLEDWidget(QWidget):
    # 分子重组动画参数
    MOL_FRAMES = 12       # 动画总帧数
    MOL_INTERVAL = 40     # 每帧间隔 (ms)

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
        # 分子重组动画状态
        self._prev_mode = None           # 上次渲染的模式
        self._mol_active = False         # 动画是否进行中
        self._mol_old = []               # 旧帧像素 [(x,y), ...]
        self._mol_new = []               # 新帧目标像素 [(x,y), ...]
        self._mol_frame = 0              # 当前帧号
        self._mol_timer = QTimer(self)
        self._mol_timer.timeout.connect(self._molecular_tick)

    def _toggle_blink(self):
        self._blink = not self._blink
        if self._eng and not self._mol_active:
            self.render(self._eng)

    # ── 分子重组动画 ──
    # 像素从旧位置平滑移动到新位置，模拟分子解散重组效果

    def _get_pixels(self):
        """收集当前缓冲区中所有亮像素的坐标"""
        px = []
        for y in range(OLED_H):
            for x in range(OLED_W):
                if self.buf.pixel(x, y) & 1:
                    px.append((x, y))
        return px

    def _start_molecular_transition(self, old_pixels, new_pixels):
        """启动分子重组动画: 旧像素 → 新位置"""
        self._mol_old = old_pixels
        self._mol_new = new_pixels
        self._mol_frame = 0
        self._mol_active = True
        self._mol_timer.start(self.MOL_INTERVAL)

    def _molecular_tick(self):
        """动画单帧推进 — 列扫溶解/重组效果

        效果: 旧像素从左到右逐列溶解向上飘散,
              新像素从左到右逐列从上方沉降重组
        """
        self._mol_frame += 1
        t = self._mol_frame / self.MOL_FRAMES  # 进度 0→1

        if t >= 1.0:
            self._mol_timer.stop()
            self._mol_active = False
            if self._eng:
                self.render(self._eng)
            return

        ease = 1.0 - (1.0 - t) ** 2  # ease-out

        old = self._mol_old
        new = self._mol_new

        self.buf.fill(0)

        # ── 旧像素: 逐列从左到右溶解，向上飘散 ──
        # 列归一化因子: 列越靠左越先溶解
        for i, (ox, oy) in enumerate(old):
            # 列进度: 左边先 (col_phase 小), 右边后 (col_phase 大)
            col_phase = ox / max(OLED_W - 1, 1)  # 0~1
            # 该像素的溶解进度, 延迟 col_phase * 0.4
            px_t = max(0.0, min(1.0, (t - col_phase * 0.4) / 0.6))
            px_ease = 1.0 - (1.0 - px_t) ** 2 if px_t > 0 else 0.0

            if px_ease <= 0.001:
                # 尚未溶解，留在原位
                self._px(ox, oy, True)
            elif px_ease < 0.999:
                # 溶解中: 向上飘散 + 渐淡 (每帧只画部分像素实现渐淡)
                drift_y = int(-12 * px_ease)  # 向上漂移 12px
                y = oy + drift_y
                # 渐淡效果: 根据进度决定是否绘制
                # 用像素的哈希值做阈值比较，产生均匀渐淡
                hash_val = (ox * 7 + oy * 13) & 0xFF
                if hash_val < int((1.0 - px_ease) * 255):
                    self._px(ox, y, True)
            # px_ease >= 0.999: 完全溶解，不绘制

        # ── 新像素: 逐列从左到右重组，从上方沉降 ──
        for j, (tx, ty) in enumerate(new):
            col_phase = tx / max(OLED_W - 1, 1)
            # 新像素重组比旧像素溶解延迟 0.2
            px_t = max(0.0, min(1.0, (t - 0.2 - col_phase * 0.4) / 0.6))
            px_ease = 1.0 - (1.0 - px_t) ** 2 if px_t > 0 else 0.0

            if px_ease <= 0.001:
                pass  # 尚未出现
            elif px_ease < 0.999:
                # 从上方沉降到目标位置
                start_y = ty - 10  # 起始位置在目标上方 10px
                y = int(start_y + (ty - start_y) * px_ease)
                # 渐现效果
                hash_val = (tx * 11 + ty * 17) & 0xFF
                if hash_val < int(px_ease * 255):
                    self._px(tx, y, True)
            else:
                # 到达目标位置
                self._px(tx, ty, True)

        self.update()

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
        p.setPen(Qt.PenStyle.NoPen)
        p.setBrush(QColor(0xd0, 0xd2, 0xd8))
        p.drawRoundedRect(ox - 6, oy - 6, bw, bh, 8, 8)
        p.setBrush(QColor(0x08, 0x08, 0x10))
        p.drawRoundedRect(ox - 2, oy - 2, bw - 8, bh - 8, 5, 5)
        for yy in range(OLED_H):
            for xx in range(OLED_W):
                if self.buf.pixel(xx, yy) & 1:
                    p.fillRect(ox + xx * SCALE, oy + yy * SCALE, SCALE, SCALE, QColor(0xFF, 0xD8, 0x30))

    # ── Render dispatch ──

    def render(self, eng: Engine):
        # 如果动画正在进行中，直接渲染最终帧（不重复动画）
        if self._mol_active:
            self._mol_timer.stop()
            self._mol_active = False

        # 检测模式切换 → 触发分子重组动画
        animate = (self._prev_mode is not None and self._prev_mode != eng.mode)
        old_pixels = self._get_pixels() if animate else []

        # 渲染新帧到缓冲区
        self.buf.fill(0)
        if eng.mode == MODE_PWM_FG:
            self._render_pwm_fg(eng)
        elif eng.mode == MODE_FG:
            self._render_fg(eng)
        elif eng.mode == MODE_CH1:
            self._render_ch(eng, 1)
        elif eng.mode == MODE_CH2:
            self._render_ch(eng, 2)
        elif eng.mode == MODE_TEST:
            self._render_test(eng)

        if animate:
            new_pixels = self._get_pixels()
            # 启动动画并立即绘制第一帧（像素在旧位置）
            self._start_molecular_transition(old_pixels, new_pixels)
        else:
            self.update()

        self._prev_mode = eng.mode

    def _marker(self, eng, idx):
        if eng.cursor == idx:
            if eng.selected:
                return ">" if self._blink else " "
            return ">"
        return " "

    def _row(self, eng, x, y, xvr, idx, label, val_str):
        self._text(x, y, self._marker(eng, idx))
        self._text(x + 6, y, label)
        self._text_r(xvr, y, val_str)

    # ── Mode 0: PWM-FG ──
    def _render_pwm_fg(self, eng):
        self._rect(0, 0, OLED_W, OLED_H)
        self._hline(1, 126, 11)
        self._text(40, 2, "PWM_TOOL")

        self._circle(3, 14, eng.ch1_on)
        self._text(12, 14, "CH1")
        self._text_r(59, 14, "ON" if eng.ch1_on else "OFF")
        self._row(eng, 3, 26, 59, ITEM_CH1_FREQ, "Fr", f"{eng.ch1_freq}Hz")
        self._row(eng, 3, 38, 59, ITEM_CH1_DUTY, "Duty", f"{eng.ch1_duty}%")

        self._vline(65, 12, 48)
        self._circle(68, 14, eng.ch2_on)
        self._text(77, 14, "CH2")
        self._text_r(125, 14, "ON" if eng.ch2_on else "OFF")
        self._row(eng, 68, 26, 125, ITEM_CH2_FREQ, "Fr", f"{eng.ch2_freq}Hz")
        self._row(eng, 68, 38, 125, ITEM_CH2_DUTY, "Duty", f"{eng.ch2_duty}%")

        self._hline(1, 126, 51)
        self._text(3, 54, "FG")
        self._text_r(62, 54, str(eng.fg_rpm))
        self._text(64, 54, "RPM")
        self._text(98, 54, self._marker(eng, ITEM_FG_DIV))
        self._text_r(125, 54, f"/{eng.fg_div}")

    # ── Mode 1: FG only ──
    def _render_fg(self, eng):
        self._rect(0, 0, OLED_W, OLED_H)
        self._hline(1, 126, 11)
        self._text(38, 2, "FG MODE")

        rpm_str = str(eng.fg_rpm)
        self._text(3, 16, "RPM:")

        # Bold RPM display
        start_x = (128 - len(rpm_str) * 12) // 2
        for i, ch in enumerate(rpm_str):
            x = start_x + i * 12
            self._char(x, 20, ch)
            self._char(x + 1, 20, ch)
            self._char(x, 28, ch)
            self._char(x + 1, 28, ch)

        self._hline(1, 126, 44)
        self._text(3, 50, "FG")
        self._text(20, 50, f"Div:{eng.fg_div}")

    # ── Mode 2 & 3: CH1 / CH2 ──
    def _render_ch(self, eng, ch):
        self._rect(0, 0, OLED_W, OLED_H)
        self._hline(1, 126, 11)

        if ch == 1:
            freq, duty, on = eng.ch1_freq, eng.ch1_duty, eng.ch1_on
            self._text(34, 2, "CH1 PWM")
        else:
            freq, duty, on = eng.ch2_freq, eng.ch2_duty, eng.ch2_on
            self._text(34, 2, "CH2 PWM")

        self._circle(3, 14, on)
        self._text(12, 14, "ON" if on else "OFF")
        self._row(eng, 3, 28, 125, 0, "Freq", f"{freq}Hz")
        self._row(eng, 3, 40, 125, 1, "Duty", f"{duty}%")
        self._row(eng, 3, 52, 125, 2, "EN", "ON" if on else "OFF")

        self._hline(80, 126, 11)
        self._text_r(125, 2, str(eng.fg_rpm))

    # ── Mode 4: TEST ──
    def _render_test(self, eng):
        self._rect(0, 0, OLED_W, OLED_H)
        self._hline(1, 126, 11)

        if eng.test_running:
            self._text(34, 2, "TEST RUN")
            self._text(3, 16, f"RPM:{eng.fg_rpm}")
            self._text(3, 28, f"CH{eng.test_channel} {eng.test_freq}Hz {eng.test_duty}%")
            self._text(3, 40, f"Cycle:{eng.test_current_cycle}/{eng.test_cycles}")
            if self._blink:
                self._text(100, 40, ">>>")
            self._text(3, 54, "Click=Stop")
        else:
            self._text(31, 2, "TEST MODE")

            def trow(x, y, xvr, idx, label, val_str):
                self._text(x, y, self._marker(eng, idx))
                self._text(x + 6, y, label)
                self._text_r(xvr, y, val_str)

            trow(3, 16, 62, TEST_ITEM_CHANNEL, "Ch", f"CH{eng.test_channel}")
            trow(66, 16, 125, TEST_ITEM_FREQ, "Fr", f"{eng.test_freq}Hz")
            trow(3, 28, 62, TEST_ITEM_DUTY, "Duty", f"{eng.test_duty}%")
            trow(66, 28, 125, TEST_ITEM_CYCLES, "N", str(eng.test_cycles))
            trow(3, 40, 62, TEST_ITEM_ON_TIME, "ON", f"{eng.test_on_sec}s")
            trow(66, 40, 125, TEST_ITEM_OFF_TIME, "OFF", f"{eng.test_off_sec}s")

            self._hline(1, 126, 51)
            self._text(40, 54, self._marker(eng, TEST_ITEM_START))
            self._text(48, 54, "START")

            if eng.test_record_count > 0:
                self._text(90, 54, f"Rec:{eng.test_record_count}")


# ── Encoder dial widget ──

class EncoderDial(QWidget):
    LONG_PRESS_MS = 800
    DOUBLE_CLICK_MS = 400
    SZ = 260
    C = 130

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
        # 延迟单击定时器: 第一次点击后等待 DOUBLE_CLICK_MS, 期间无第二次点击才发单击
        self._click_timer = QTimer(self)
        self._click_timer.setSingleShot(True)
        self._click_timer.timeout.connect(self._fire_click)
        self._pending_click = False
        self.setMouseTracking(True)

    def set_callback(self, cb):
        self._callback = cb

    def _on_long_press(self):
        self._lp_fired = True
        if self._callback and self._pressed == 'ok':
            self._callback(EVENT_LONG_PRESS)

    def _fire_click(self):
        """延迟单击定时器到期 → 发送单击事件"""
        if self._pending_click:
            self._pending_click = False
            if self._callback:
                self._callback(EVENT_CLICK)

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
            if h == 'ok':
                if self._pending_click:
                    # 第二次点击: 取消延迟单击, 发双击
                    self._click_timer.stop()
                    self._pending_click = False
                    if self._callback:
                        self._callback(EVENT_DOUBLE_CLICK)
                else:
                    # 第一次点击: 启动延迟定时器, 等待可能的第二次点击
                    self._pending_click = True
                    self._click_timer.start(int(self.DOUBLE_CLICK_MS))
            else:
                mapping = {'cw': EVENT_CW, 'ccw': EVENT_CCW}
                if self._callback and h in mapping:
                    self._callback(mapping[h])
        # 清理: 长按触发或点击无效区域时, 取消待处理的单击
        if self._lp_fired or not h or h != self._pressed:
            self._click_timer.stop()
            self._pending_click = False
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
        c = self.C

        p.setPen(Qt.PenStyle.NoPen)
        p.setBrush(QColor(0xd0, 0xd2, 0xdc))
        p.drawEllipse(QPoint(c, c + 2), 126, 126)
        p.setBrush(QColor(0xea, 0xec, 0xf2))
        p.drawEllipse(QPoint(c, c), 124, 124)
        p.setBrush(QColor(0xde, 0xe0, 0xe8))
        p.drawEllipse(QPoint(c, c), 112, 112)
        p.setBrush(QColor(0xf0, 0xf1, 0xf6))
        p.drawEllipse(QPoint(c, c), 102, 102)

        for i in range(36):
            a = math.radians(i * 10)
            bright = 0xc0 + (0x18 if i % 9 == 0 else 0)
            p.setPen(QPen(QColor(bright, bright, bright + 6), 1))
            inner = 104 if i % 3 == 0 else 108
            p.drawLine(
                int(c + inner * math.cos(a)), int(c - inner * math.sin(a)),
                int(c + 110 * math.cos(a)), int(c - 110 * math.sin(a))
            )

        # CCW button
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

        # CW button
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

        # OK button
        ok_p = self._pressed == 'ok'
        ok_h = self._hover == 'ok'
        if ok_p:
            bc, bg, tc = QColor(0x00, 0x7a, 0xc6), QColor(0x00, 0x90, 0xe0), QColor(0xff, 0xff, 0xff)
        elif ok_h:
            bc, bg, tc = QColor(0x00, 0x68, 0xb0), QColor(0x00, 0x80, 0xd0), QColor(0xff, 0xff, 0xff)
        else:
            bc, bg, tc = QColor(0x00, 0x58, 0x98), QColor(0x00, 0x70, 0xbc), QColor(0xe0, 0xf0, 0xf8)
        p.setPen(Qt.PenStyle.NoPen)
        p.setBrush(QColor(bg.red(), bg.green(), bg.blue(), 60))
        p.drawEllipse(QPoint(c, c), 52, 52)
        p.setBrush(bc)
        p.drawEllipse(QPoint(c, c), 46, 46)
        p.setBrush(QColor(bc.red() + 20, bc.green() + 20, bc.blue() + 20))
        p.drawEllipse(QPoint(c, c - 6), 34, 28)
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
        self.setWindowTitle("PWM 信号发生器仿真器")
        self.setFixedSize(600, 900)
        self.eng = Engine()
        self.serial = SerialComm()
        self.setStyleSheet(APP_STYLE)

        central = QWidget()
        root = QVBoxLayout(central)
        root.setContentsMargins(24, 20, 24, 20)
        root.setSpacing(10)

        # ── Title ──
        title = QLabel("PWM 信号发生器")
        title.setAlignment(Qt.AlignmentFlag.AlignCenter)
        title.setStyleSheet("color:#2a3050; font-size:18px; font-weight:bold; letter-spacing:4px;")
        root.addWidget(title)

        # ── Mode indicator ──
        self.mode_lbl = QLabel("PWM-FG")
        self.mode_lbl.setAlignment(Qt.AlignmentFlag.AlignCenter)
        self.mode_lbl.setStyleSheet("""
            color:#005898; font-size:13px; font-weight:bold;
            background:#e0f0ff; border:1px solid #b0d8f0;
            border-radius:6px; padding:4px 16px;
            font-family:Consolas,monospace;
        """)
        root.addWidget(self.mode_lbl)

        # ── OLED ──
        self.oled = OLEDWidget()
        self.oled._eng = self.eng
        root.addWidget(self.oled, 0, Qt.AlignmentFlag.AlignCenter)

        # ── Serial port controls ──
        serial_row = QHBoxLayout()
        serial_row.setSpacing(8)

        self.port_combo = QComboBox()
        self.port_combo.setMinimumWidth(180)
        self.port_combo.setStyleSheet("""
            QComboBox { color:#505870; font-size:12px; background:#fff;
                        border:1px solid #d8dae2; border-radius:6px; padding:5px 10px; }
            QComboBox::drop-down { border:none; }
        """)
        serial_row.addWidget(self.port_combo)

        self.refresh_btn = QPushButton("刷新")
        self.refresh_btn.setFixedWidth(72)
        self.refresh_btn.setStyleSheet("""
            QPushButton { color:#505870; font-size:11px; background:#fff;
                          border:1px solid #d8dae2; border-radius:6px; padding:5px 10px; }
            QPushButton:hover { background:#e8eaf0; }
        """)
        self.refresh_btn.clicked.connect(self._refresh_ports)
        serial_row.addWidget(self.refresh_btn)

        self.connect_btn = QPushButton("连接")
        self.connect_btn.setFixedWidth(90)
        self.connect_btn.setStyleSheet("""
            QPushButton { color:#fff; font-size:11px; font-weight:bold;
                          background:#0070bc; border:none; border-radius:6px; padding:5px 12px; }
            QPushButton:hover { background:#0088dd; }
            QPushButton:pressed { background:#005898; }
        """)
        self.connect_btn.clicked.connect(self._toggle_connect)
        serial_row.addWidget(self.connect_btn)

        self.serial_status = QLabel("未连接")
        self.serial_status.setStyleSheet("color:#998888; font-size:10px;")
        serial_row.addWidget(self.serial_status)
        serial_row.addStretch()

        root.addLayout(serial_row)

        # ── State label ──
        self.state_lbl = QLabel("CH1 占空比")
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

        # ── Test status bar ──
        self.test_lbl = QLabel("")
        self.test_lbl.setAlignment(Qt.AlignmentFlag.AlignCenter)
        self.test_lbl.setStyleSheet("""
            color:#cc6600; font-size:12px; font-weight:bold;
            background:#fff8f0; border:1px solid #f0d8a0;
            border-radius:6px; padding:4px 12px;
            font-family:Consolas,monospace;
        """)
        self.test_lbl.setVisible(False)
        root.addWidget(self.test_lbl)

        # ── Encoder ──
        el = QLabel("旋钮编码器")
        el.setAlignment(Qt.AlignmentFlag.AlignCenter)
        el.setStyleSheet("color:#8090a0; font-size:11px; letter-spacing:2px;")
        root.addWidget(el)

        self.encoder = EncoderDial()
        self.encoder.set_callback(self._on)
        root.addWidget(self.encoder, 0, Qt.AlignmentFlag.AlignCenter)

        # ── Help panel (collapsible) ──
        self.help_group = QGroupBox("  使用说明 ▼")
        self.help_group.setCheckable(True)
        self.help_group.setChecked(False)
        self.help_group.setStyleSheet("""
            QGroupBox {
                font-size:11px; font-weight:bold; color:#506080;
                border:1px solid #c0c8d8; border-radius:8px;
                margin-top:6px; padding:12px 8px 8px 8px;
            }
            QGroupBox::title {
                subcontrol-origin: margin; left:12px; padding:0 4px;
            }
        """)
        help_layout = QVBoxLayout(self.help_group)
        help_layout.setSpacing(4)
        help_layout.setContentsMargins(8, 4, 8, 4)

        help_text = QLabel(
            "【操作】旋转旋钮调参  |  点击OK启停通道  |  长按OK选择项目  |  双击OK切换模式\n"
            "【模式】PWM-FG:双通道输出  FG:频率计  CH1/CH2:单通道调节  TEST:自动测试循环\n"
            "【串口】选择COM口 → 点击「连接」，参数自动同步，测试数据可导出CSV"
        )
        help_text.setWordWrap(True)
        help_text.setStyleSheet("color:#606878; font-size:10px; font-weight:normal; line-height:1.4;")
        help_layout.addWidget(help_text)
        root.addWidget(self.help_group)

        # ── Tip ──
        tip = QLabel("单击=启停 | 长按=选择 | 双击=切模式")
        tip.setAlignment(Qt.AlignmentFlag.AlignCenter)
        tip.setStyleSheet("color:#a0a4b0; font-size:10px; letter-spacing:1px;")
        root.addWidget(tip)

        root.addStretch()
        self.setCentralWidget(central)

        # ── Timers ──
        self.serial_timer = QTimer(self)
        self.serial_timer.timeout.connect(self._poll_serial)
        self.serial_timer.start(50)

        self.status_timer = QTimer(self)
        self.status_timer.timeout.connect(self._request_status)
        self.status_timer.start(500)

        if HAS_SERIAL:
            self._refresh_ports()
        else:
            self.port_combo.addItem("未安装pyserial")
            self.connect_btn.setEnabled(False)

        self._refresh()

    def _refresh_ports(self):
        self.port_combo.clear()
        ports = self.serial.list_ports()
        for dev, desc in ports:
            self.port_combo.addItem(f"{dev}  —  {desc}", dev)
        if not ports:
            self.port_combo.addItem("未找到串口")

    def _toggle_connect(self):
        if self.serial.connected:
            self.serial.disconnect()
            self.connect_btn.setText("连接")
            self.connect_btn.setStyleSheet("""
                QPushButton { color:#fff; font-size:11px; font-weight:bold;
                              background:#0070bc; border:none; border-radius:6px; padding:5px 12px; }
                QPushButton:hover { background:#0088dd; }
            """)
            self.serial_status.setText("已断开")
            self.serial_status.setStyleSheet("color:#998888; font-size:10px;")
        else:
            dev = self.port_combo.currentData()
            if dev and self.serial.connect(dev):
                self.connect_btn.setText("断开")
                self.connect_btn.setStyleSheet("""
                    QPushButton { color:#fff; font-size:11px; font-weight:bold;
                                  background:#cc4444; border:none; border-radius:6px; padding:5px 12px; }
                    QPushButton:hover { background:#dd5555; }
                """)
                self.serial_status.setText(f"已连接 {dev}")
                self.serial_status.setStyleSheet("color:#008844; font-size:10px;")
            else:
                self.serial_status.setText("连接失败！")
                self.serial_status.setStyleSheet("color:#cc4444; font-size:10px;")

    def _request_status(self):
        if self.serial.connected:
            self.serial.send_read_status()

    def _poll_serial(self):
        if not self.serial.connected:
            return
        result = self.serial.poll()
        while result:
            cmd, data = result
            if cmd == CMD_READ_STATUS:
                status = self.serial.read_status_data(data)
                if status:
                    self.eng.ch1_freq = status['ch1_freq']
                    self.eng.ch1_duty = status['ch1_duty']
                    self.eng.ch1_on = status['ch1_on']
                    self.eng.ch2_freq = status['ch2_freq']
                    self.eng.ch2_duty = status['ch2_duty']
                    self.eng.ch2_on = status['ch2_on']
                    self.eng.fg_div = status['fg_div']
                    self.eng.fg_rpm = status['rpm']
                    self.eng.mode = status['mode']
                    self.eng.test_running = (status['test_state'] == 1)
                    self.eng.test_current_cycle = status['test_cycle']
                    self._refresh()
            elif cmd == CMD_EXPORT_CHUNK:
                # Receive CSV line
                try:
                    line = data.decode('ascii', errors='replace')
                    self.eng.export_csv_lines.append(line)
                except Exception:
                    pass
            elif cmd == CMD_EXPORT_DONE:
                # Export complete — save to file
                self._save_export_csv()
            result = self.serial.poll()

    def _save_export_csv(self):
        if not self.eng.export_csv_lines:
            return
        path, _ = QFileDialog.getSaveFileName(
            self, "保存测试数据", "test_data.csv", "CSV文件 (*.csv)")
        if path:
            try:
                with open(path, 'w', newline='') as f:
                    for line in self.eng.export_csv_lines:
                        f.write(line)
                self.test_lbl.setText(f"已导出 {len(self.eng.export_csv_lines)} 行")
                self.test_lbl.setStyleSheet("""
                    color:#008844; font-size:12px; font-weight:bold;
                    background:#e8f8f0; border:1px solid #b0e8cc;
                    border-radius:6px; padding:4px 12px;
                    font-family:Consolas,monospace;
                """)
                self.test_lbl.setVisible(True)
            except Exception as e:
                self.test_lbl.setText(f"导出失败：{e}")
        self.eng.export_csv_lines = []

    def _on(self, ev):
        self.eng.process(ev)
        self._refresh()

        if self.serial.connected:
            ev_map = {
                EVENT_CW: 1, EVENT_CCW: 2,
                EVENT_CLICK: 3, EVENT_LONG_PRESS: 4,
            }
            if ev in ev_map:
                self.serial.send_key_event(ev_map[ev])

            # Sync params to hardware
            if ev in (EVENT_CW, EVENT_CCW) and not self.eng.selected:
                if self.eng.mode == MODE_PWM_FG:
                    ch = 1 if self.eng.cursor <= ITEM_CH1_DUTY else 2
                    if ch == 1:
                        self.serial.send_write_pwm(1, self.eng.ch1_freq, self.eng.ch1_duty, self.eng.ch1_on)
                    else:
                        self.serial.send_write_pwm(2, self.eng.ch2_freq, self.eng.ch2_duty, self.eng.ch2_on)
                elif self.eng.mode == MODE_CH1:
                    self.serial.send_write_pwm(1, self.eng.ch1_freq, self.eng.ch1_duty, self.eng.ch1_on)
                elif self.eng.mode == MODE_CH2:
                    self.serial.send_write_pwm(2, self.eng.ch2_freq, self.eng.ch2_duty, self.eng.ch2_on)
                elif self.eng.mode == MODE_FG:
                    self.serial.send_write_fg_div(self.eng.fg_div)

            if ev == EVENT_CLICK and not self.eng.selected:
                if self.eng.mode == MODE_PWM_FG:
                    ch = 1 if self.eng.cursor <= ITEM_CH1_DUTY else 2
                    if ch == 1:
                        self.serial.send_write_pwm(1, self.eng.ch1_freq, self.eng.ch1_duty, self.eng.ch1_on)
                    else:
                        self.serial.send_write_pwm(2, self.eng.ch2_freq, self.eng.ch2_duty, self.eng.ch2_on)
                elif self.eng.mode in (MODE_CH1, MODE_CH2):
                    ch = 1 if self.eng.mode == MODE_CH1 else 2
                    if ch == 1:
                        self.serial.send_write_pwm(1, self.eng.ch1_freq, self.eng.ch1_duty, self.eng.ch1_on)
                    else:
                        self.serial.send_write_pwm(2, self.eng.ch2_freq, self.eng.ch2_duty, self.eng.ch2_on)

    def _refresh(self):
        self.oled.render(self.eng)

        # Mode indicator
        self.mode_lbl.setText(MODE_NAMES[self.eng.mode])

        # State label
        mode = self.eng.mode
        if mode == MODE_TEST:
            if self.eng.test_running:
                lbl = f"测试运行中  第{self.eng.test_current_cycle}/{self.eng.test_cycles}轮"
            else:
                names = {0: "通道", 1: "频率", 2: "占空比", 3: "循环数",
                         4: "ON时间", 5: "OFF时间", 6: "开始"}
                lbl = f"测试: {names.get(self.eng.cursor, '')}"
                if self.eng.selected:
                    lbl += "  [选择]"
        elif mode == MODE_FG:
            lbl = "FG分频"
            if self.eng.selected:
                lbl += "  [选择]"
        elif mode == MODE_CH1:
            names = {0: "CH1频率", 1: "CH1占空比", 2: "CH1使能"}
            lbl = names.get(self.eng.cursor, "")
            if self.eng.selected:
                lbl += "  [选择]"
        elif mode == MODE_CH2:
            names = {0: "CH2频率", 1: "CH2占空比", 2: "CH2使能"}
            lbl = names.get(self.eng.cursor, "")
            if self.eng.selected:
                lbl += "  [选择]"
        else:
            names = {0: "CH1频率", 1: "CH1占空比", 2: "CH2频率", 3: "CH2占空比", 4: "FG分频"}
            lbl = names.get(self.eng.cursor, "")
            if self.eng.selected:
                lbl += "  [选择]"
        self.state_lbl.setText(lbl)

        # Channel cards
        if self.eng.ch1_on:
            self.ch1_card.setText(f"CH1  {self.eng.ch1_freq}Hz  {self.eng.ch1_duty}%")
            self.ch1_card.setStyleSheet("color:#008844; font-size:14px; font-weight:bold; font-family:Consolas,monospace; background:#e8f8f0; border:1px solid #b0e8cc; border-radius:8px; padding:8px 20px;")
        else:
            self.ch1_card.setText("CH1  OFF")
            self.ch1_card.setStyleSheet("color:#998888; font-size:14px; font-weight:bold; font-family:Consolas,monospace; background:#fff; border:1px solid #d8dae2; border-radius:8px; padding:8px 20px;")

        if self.eng.ch2_on:
            self.ch2_card.setText(f"CH2  {self.eng.ch2_freq}Hz  {self.eng.ch2_duty}%")
            self.ch2_card.setStyleSheet("color:#008844; font-size:14px; font-weight:bold; font-family:Consolas,monospace; background:#e8f8f0; border:1px solid #b0e8cc; border-radius:8px; padding:8px 20px;")
        else:
            self.ch2_card.setText("CH2  OFF")
            self.ch2_card.setStyleSheet("color:#998888; font-size:14px; font-weight:bold; font-family:Consolas,monospace; background:#fff; border:1px solid #d8dae2; border-radius:8px; padding:8px 20px;")

        # Test status bar
        if self.eng.test_running:
            self.test_lbl.setText(f"测试运行中  CH{self.eng.test_channel}  第{self.eng.test_current_cycle}/{self.eng.test_cycles}轮")
            self.test_lbl.setStyleSheet("""
                color:#cc6600; font-size:12px; font-weight:bold;
                background:#fff8f0; border:1px solid #f0d8a0;
                border-radius:6px; padding:4px 12px;
                font-family:Consolas,monospace;
            """)
            self.test_lbl.setVisible(True)
        elif mode == MODE_TEST:
            self.test_lbl.setText(f"测试就绪  记录: {self.eng.test_record_count}")
            self.test_lbl.setStyleSheet("""
                color:#4a60a0; font-size:12px; font-weight:bold;
                background:#f0f4ff; border:1px solid #c0d0e8;
                border-radius:6px; padding:4px 12px;
                font-family:Consolas,monospace;
            """)
            self.test_lbl.setVisible(True)
        else:
            self.test_lbl.setVisible(False)

    def closeEvent(self, ev):
        self.serial.disconnect()
        super().closeEvent(ev)


if __name__ == "__main__":
    app = QApplication(sys.argv)
    app.setStyle("Fusion")
    w = MainWindow()
    w.show()
    sys.exit(app.exec())
