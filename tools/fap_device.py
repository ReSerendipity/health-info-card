#!/usr/bin/env python3
# tools/fap_device.py —— FAP_SCREENSHOT_V1 / FAP_KEY_V1 主机端工具。
#
#   python tools/fap_device.py -p COM3 shot <out.bmp>
#       发截图命令, 收头行 + RGB565LE 像素, 转 24 位 BMP 存盘。
#   python tools/fap_device.py -p COM3 key <btn> <ev>
#       发按键命令 (btn: 0=上 1=下 2=确定; ev: 1=单击 3=长按), 无应答。
#
# 协议细节见 docs/reference/y2lin/serial-screenshot-protocol.zh_CN.md。
import argparse
import struct
import sys
import time

import serial


def _open(port):
    ser = serial.Serial(port, 115200, timeout=1, write_timeout=2)
    try:
        ser.dtr = False
        ser.rts = False          # 不做 DTR/RTS 复位
    except Exception:
        pass
    return ser


def shot(port, out, timeout=12):
    ser = _open(port)
    ser.timeout = 0.5
    ser.reset_input_buffer()
    ser.write(b"FAP_SCREENSHOT_V1\n")

    # 读响应:直接收原始字节,在缓冲里定位头行(设备头行紧接像素,
    # 逐行 readline 在短超时下可能读不到/截断,这是此前脚本化截图全部超时的根因)
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
        sys.exit("ERROR: no screenshot header (device busy or protocol not built-in)")
    j = buf.find(b"\n", i)
    if j < 0:
        sys.exit("ERROR: header line not terminated")
    header = buf[i:j]

    parts = header.decode("ascii", "replace").strip().split()
    w, h, fmt, nbytes = int(parts[1]), int(parts[2]), parts[3], int(parts[4])

    data = buf[j + 1:]
    while len(data) < nbytes:
        chunk = ser.read(nbytes - len(data))
        if not chunk:
            break
        data += chunk
    if len(data) != nbytes:
        sys.exit(f"ERROR: short read {len(data)}/{nbytes}")

    # 像素解码:RGB565LE(2B/px, 大缓冲)或 ARGB2222(1B/px, 低精度保色)
    if fmt == "RGB565LE":
        pixel = _rgb565le
        bpp = 2
    elif fmt == "ARGB2222":
        pixel = _argb2222
        bpp = 1
    elif fmt == "L8":
        pixel = _l8
        bpp = 1
    else:
        sys.exit(f"ERROR: unsupported format {fmt}")

    # -> 24 位 BGR, BMP 行从底到顶
    row_bytes = w * 3
    padded = (row_bytes + 3) & ~3
    img = bytearray()
    for y in range(h - 1, -1, -1):
        row = bytearray(padded)
        for x in range(w):
            r, g, b = pixel(data, (y * w + x) * bpp)
            row[x * 3 + 0] = b
            row[x * 3 + 1] = g
            row[x * 3 + 2] = r
        img += row

    file_size = 14 + 40 + len(img)
    bmp = bytearray()
    bmp += b"BM"
    bmp += struct.pack("<IHHI", file_size, 0, 0, 54)
    bmp += struct.pack("<IiiHHIIiiII", 40, w, h, 1, 24, 0, len(img), 2835, 2835, 0, 0)
    bmp += img
    with open(out, "wb") as f:
        f.write(bmp)
    print(f"saved {w}x{h} {fmt} -> {out} ({len(data)} px bytes)")


def _rgb565le(data, off):
    px = struct.unpack_from("<H", data, off)[0]
    r = (px >> 11) & 0x1F
    g = (px >> 5) & 0x3F
    b = px & 0x1F
    return ((r << 3) | (r >> 2)), ((g << 2) | (g >> 4)), ((b << 3) | (b >> 2))


def _l8(data, off):
    v = data[off]
    return v, v, v


def _argb2222(data, off):
    # 字节布局: AA RR GG BB (高2位 alpha, 次2位 R, 次2位 G, 低2位 B)
    v = data[off]
    r = (v >> 4) & 0x3
    g = (v >> 2) & 0x3
    b = v & 0x3
    return ((r << 6) | (r << 4) | (r << 2) | r), \
           ((g << 6) | (g << 4) | (g << 2) | g), \
           ((b << 6) | (b << 4) | (b << 2) | b)


def key(port, btn, ev):
    ser = _open(port)
    ser.write(f"FAP_KEY_V1 {btn} {ev}\n".encode("ascii"))
    ser.flush()
    # 进程退出时连接关闭会丢弃 Windows 驱动缓冲中未发送的字节;
    # 保持连接一小段时间确保命令真正进入 USB(实测不冲刷时按键不生效)。
    time.sleep(0.2)
    print(f"sent FAP_KEY_V1 {btn} {ev}")


def main():
    ap = argparse.ArgumentParser(description="FAP device protocol client")
    ap.add_argument("-p", "--port", required=True, help="COM port, e.g. COM3")
    sub = ap.add_subparsers(dest="cmd", required=True)
    sp = sub.add_parser("shot", help="capture screen to BMP")
    sp.add_argument("out", help="output .bmp path")
    kp = sub.add_parser("key", help="inject key event")
    kp.add_argument("btn", type=int, choices=[0, 1, 2])
    kp.add_argument("ev", type=int, choices=[1, 3])
    args = ap.parse_args()

    if args.cmd == "shot":
        shot(args.port, args.out)
    elif args.cmd == "key":
        key(args.port, args.btn, args.ev)


if __name__ == "__main__":
    main()
