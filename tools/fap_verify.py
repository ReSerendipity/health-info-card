# -*- coding: utf-8 -*-
"""Rewrite fap_verify.py: single persistent connection for capture+key.

Root cause of the earlier all-page-1 captures: each fap_device.py subprocess
opens and closes the COM port; closing restores DTR/RTS line state, which
re-enumerates the device USB and resets the running app (measured). Keep one
connection open for the whole 5-page run.
"""
import struct
import sys
import time

import serial

PORT = sys.argv[1]
OUT_DIR = sys.argv[2]


def open_port(port):
    ser = serial.Serial(port, 115200, timeout=1, write_timeout=2)
    try:
        ser.dtr = False
        ser.rts = False
    except Exception:
        pass
    return ser


def capture(ser, out, timeout=10):
    ser.reset_input_buffer()
    ser.write(b"FAP_SCREENSHOT_V1\n")
    end = time.time() + timeout
    buf = b""
    while time.time() < end:
        d = ser.read(65536)
        if d:
            buf += d
        if b"FAP_SCREENSHOT_V1" in buf:
            break
    i = buf.find(b"FAP_SCREENSHOT_V1")
    if i < 0:
        return False, "no header"
    j = buf.find(b"\n", i)
    if j < 0:
        return False, "unterminated header"
    header = buf[i:j]
    parts = header.decode("ascii", "replace").strip().split()
    if len(parts) < 5:
        return False, "bad header"
    w, h, fmt, nbytes = int(parts[1]), int(parts[2]), parts[3], int(parts[4])
    if fmt != "L8":
        return False, "unexpected format " + fmt
    data = buf[j + 1:]
    while len(data) < nbytes:
        chunk = ser.read(nbytes - len(data))
        if not chunk:
            break
        data += chunk
    if len(data) != nbytes:
        return False, "short read %d/%d" % (len(data), nbytes)
    row_bytes = w * 3
    padded = (row_bytes + 3) & ~3
    img = bytearray()
    for y in range(h - 1, -1, -1):
        row = bytearray(padded)
        for x in range(w):
            v = data[y * w + x]
            row[x * 3 + 0] = v
            row[x * 3 + 1] = v
            row[x * 3 + 2] = v
        img += row
    file_size = 14 + 40 + len(img)
    bmp = bytearray()
    bmp += b"BM"
    bmp += struct.pack("<IHHI", file_size, 0, 0, 54)
    bmp += struct.pack("<IiiHHIIiiII", 40, w, h, 1, 24, 0, len(img), 2835, 2835, 0, 0)
    bmp += img
    with open(out, "wb") as f:
        f.write(bmp)
    return True, "saved %dx%d %s" % (w, h, fmt)


names = ["01_name_age_blood", "02_family_phone", "03_home",
         "04_medical", "05_wechat"]
fail = 0
ser = open_port(PORT)
try:
    for i in range(5):
        out = "%s\\fap_page_%s.bmp" % (OUT_DIR, names[i])
        ok, msg = capture(ser, out)
        print("[%d/5] shot %s: %s" % (i + 1, names[i], msg))
        if not ok:
            fail += 1
        if i < 4:
            ser.write(b"FAP_KEY_V1 1 1\n")
            ser.flush()
            time.sleep(0.4)
            print("   key DOWN sent")
finally:
    try:
        ser.close()
    except Exception:
        pass

print("FAILURES:", fail)
sys.exit(1 if fail else 0)
