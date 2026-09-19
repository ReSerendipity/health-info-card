# tools/fap_verify2.py —— 独立真机验收:一次会话完成 5 页截图+按键翻页。
# 读取逻辑与手测一致(打开端口->发命令->直接读原始字节),不经 fap_device.py。
# 用法: python tools/fap_verify2.py COM3 <out_dir>
import struct
import sys
import time

import serial

PORT = sys.argv[1]
OUT_DIR = sys.argv[2]


def shot(ser, out, timeout=6):
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
        return None, "no header"
    # 头行结束 = 第一个 \n 之后
    j = buf.find(b"\n", i)
    if j < 0:
        return None, "no newline after header"
    header = buf[i:j].decode("ascii", "replace").strip()
    parts = header.split()
    w, h, fmt, nbytes = int(parts[1]), int(parts[2]), parts[3], int(parts[4])
    payload = buf[j + 1:]
    while len(payload) < nbytes:
        d = ser.read(nbytes - len(payload))
        if not d:
            break
        payload += d
    if len(payload) != nbytes:
        return None, f"short read {len(payload)}/{nbytes}"
    # 解码 L8(灰度)或 ARGB2222 -> BMP
    row_bytes = w * 3
    padded = (row_bytes + 3) & ~3
    img = bytearray()
    for y in range(h - 1, -1, -1):
        row = bytearray(padded)
        for x in range(w):
            v = payload[(y * w + x)]
            if fmt == "L8":
                R = G = B = v
            else:  # ARGB2222
                r = (v >> 4) & 0x3
                g = (v >> 2) & 0x3
                b = v & 0x3
                R = (r << 6) | (r << 4) | (r << 2) | r
                G = (g << 6) | (g << 4) | (g << 2) | g
                B = (b << 6) | (b << 4) | (b << 2) | b
            row[x * 3 + 0] = B
            row[x * 3 + 1] = G
            row[x * 3 + 2] = R
        img += row
    file_size = 14 + 40 + len(img)
    bmp = bytearray(b"BM")
    bmp += struct.pack("<IHHI", file_size, 0, 0, 54)
    bmp += struct.pack("<IiiHHIIiiII", 40, w, h, 1, 24, 0, len(img), 2835, 2835, 0, 0)
    bmp += img
    with open(out, "wb") as f:
        f.write(bmp)
    return (w, h, fmt, nbytes), "ok"


def key(ser, btn, ev):
    ser.reset_input_buffer()
    ser.write(f"FAP_KEY_V1 {btn} {ev}\n".encode("ascii"))


def main():
    ser = serial.Serial(PORT, 115200, timeout=0.5, write_timeout=2, dsrdtr=False, rtscts=False)
    try:
        ser.dtr = False
        ser.rts = False
    except Exception:
        pass

    names = ["01_name_age_blood", "02_family_phone", "03_home",
             "04_medical", "05_wechat"]
    fail = 0
    for i in range(5):
        out = f"{OUT_DIR}\\fap_page_{names[i]}.bmp"
        meta, status = shot(ser, out)
        print(f"[{i + 1}/5] {names[i]}: {status} {meta}")
        if status != "ok":
            fail += 1
        if i < 4:
            key(ser, 1, 1)
            time.sleep(0.5)
    ser.close()
    print("FAILURES:", fail)
    sys.exit(1 if fail else 0)


if __name__ == "__main__":
    main()
