<p align="right">
  <a href="esp32c3-usb-serial-jtag-pitfalls.zh_CN.md">简体中文</a> · <strong>English</strong>
</p>

# ESP32-C3 USB-Serial-JTAG: Serial Screenshot & Key-Injection Pitfalls

Field-tested pitfalls from adding a read-only serial screenshot protocol
(`FAP_SCREENSHOT_V1`) and key injection (`FAP_KEY_V1`) to the AI Passport
(ESP32-C3, no PSRAM, USB-Serial-JTAG console). Each entry: symptom → root
cause → correct approach. Read this before touching the FAP protocol, the
console driver, LVGL snapshot, or host-side serial capture scripts.

Reference implementation: official commit
`6acfd9db3b9e7b937571827bb78c40a5d414bf5c` ("feat: add read-only
FAP_SCREENSHOT_V1 serial screenshot protocol"); this fork's wiring lives in
`main/fap_screenshot.{c,h}` and `tools/fap_device.py`, `tools/fap_verify*.py`.

## 1. DTR/RTS reset breaks the protocol; deep sleep ignores DTR

- **Symptom**: host resets the device via DTR before capture; every screenshot
  then times out with "no screenshot header", even though a manual capture
  without the reset succeeds.
- **Root cause**: on USB-Serial-JTAG the CDC line-state change causes the USB
  endpoint to re-enumerate, which disturbs the driver RX inside the running
  firmware. Additionally, in deep sleep only GPIO wake-up sources are active —
  DTR/RTS cannot wake the device (it was verified that raising DTR alone does
  not wake it).
- **Correct approach**:
  - Never toggle DTR/RTS around protocol traffic. `serial.Serial(..., dsrdtr=False, rtscts=False)` and leave them low.
  - To wake/restart the device from the host, use `esptool` with
    `--after hard_reset` (it drives the ROM download/boot sequence), or press a
    key. Re-flashing is also a reliable hard reset.
  - Keep host captures inside the awake window (default 60 s no-operation
    deep sleep). For a long verification session, temporarily raise
    `NORMAL_IDLE_MS` in `main/main.c`, then restore 60 s and re-flash.

## 2. `usb_serial_jtag_write_bytes` TX never lands without the VFS

- **Symptom**: the device logs its "transfer complete" message but the
  host receives only the header/log bytes and almost no pixels.
- **Root cause**: with `usb_serial_jtag_driver_install()` only (no
  `usb_serial_jtag_vfs_use_driver()`), driver `write_bytes` pushes into the TX
  ring/ISR path that never actually drains to the FIFO in this configuration —
  the write call "succeeds" locally while nothing reaches the USB host.
- **Correct approach**: send with register-level polling:
  `usb_serial_jtag_ll_txfifo_writable()` +
  `usb_serial_jtag_ll_write_txfifo(p, n)`, then
  `usb_serial_jtag_ll_txfifo_flush()` at the end. This is the same path the
  boot log uses and is proven stable. RX can stay on the driver
  (`usb_serial_jtag_read_bytes`), which works.

## 3. The SW renderer cannot render into an ARGB2222 snapshot

- **Symptom**: `lv_snapshot_take(..., LV_COLOR_FORMAT_ARGB2222)` returns
  plausible size (76,800 B) but the decoded image is full of noise.
- **Root cause**: the software renderer supports these **target** formats:
  `RGB565`, `RGB888`, `XRGB8888`, `ARGB8888`, `L8`. `ARGB2222` is not a valid
  snapshot target, so the output is garbage.
- **Correct approach**: on this board (no PSRAM, ~113 KB free heap at boot)
  `RGB565` needs 153,600 B contiguous — too large. Use **`L8` grayscale**
  (76,800 B, 1 B/px, text stays sharp). Decode on the host as grayscale BMP.
  The protocol header carries the format name, so the host can branch on it.

## 4. Host `readline()` misses the protocol header

- **Symptom**: scripted captures consistently time out while a manual
  read-raw-bytes capture succeeds.
- **Root cause**: the device emits the header line immediately followed by the
  pixel block; per-line `readline()` under a short timeout can truncate or miss
  the boundary, so the header is never seen.
- **Correct approach**: read raw bytes
  (`ser.read(65536)` in a loop until the header marker appears in the buffer),
  locate `FAP_SCREENSHOT_V1` with `buf.find()`, take the pixel payload from the
  first `\n` after it. Do not `reset_input_buffer()` right before sending.

## 5. `IDF_PATH` backslashes break the build (CMake escape)

- **Symptom**: `idf.py build` fails with
  `Invalid character escape '\U'` in
  `bootloader_components/recovery_boot_hook/CMakeLists.txt`.
- **Root cause**: that CMake file interpolates `$ENV{IDF_PATH}`; on Windows a
  backslash path becomes an invalid escape in a CMake string.
- **Correct approach**: export `IDF_PATH` with forward slashes before building,
  e.g. `$env:IDF_PATH = "C:/Users/Doro/esp/esp-idf-v5.5.3"`.

## 6. Do not install the console VFS + big startup allocation + printf (crash)

- **Symptom**: an earlier self-written FAP used
  `usb_serial_jtag_vfs_use_driver()` plus a large startup frame-buffer
  allocation and `printf` from the main task. The device crashed in a loop:
  main-task Stack protection fault at `_vfprintf_r`, visible as a flashing
  screen on boot.
- **Root cause**: switching the console path to the driver route drastically
  increases stack usage of `printf` in the main task; combined with the large
  startup heap allocation the stack/heap budget was exceeded.
- **Correct approach**: keep the default (no-driver) console; install only the
  USB-Serial-JTAG driver for RX; do the snapshot work in a dedicated task with
  its own 8 KB stack; never allocate the whole frame buffer at startup; keep
  failure paths silent (log only, do not answer) so the host gets a clean
  timeout instead of corrupted streams.

## 7. A 64-byte-aligned final packet needs a ZLP terminator

- **Symptom**: 76,800 = 64 × 1200 exactly; without termination the last 64
  bytes may never reach the host.
- **Root cause**: a full 64-byte FIFO packet is treated as an incomplete USB
  transaction by the host CDC-ACM port.
- **Correct approach**: after the last byte, call
  `usb_serial_jtag_ll_txfifo_flush()`; when the FIFO frees again, flush once
  more to emit the zero-length packet (see the HAL header comment).

## Verification cheat-sheet

- Build: activate ESP-IDF, set `IDF_PATH` with forward slashes, `idf.py build`.
- Flash (repo requires a merged full image at `0x0`):
  `python -m esptool --chip esp32c3 merge_bin -o FoloToy-AI-Passport-full.bin "@build\flash_args"`
  then `python -m esptool --chip esp32c3 -p COM3 -b 460800 write_flash 0x0 FoloToy-AI-Passport-full.bin`.
  `--after hard_reset` restarts the device; capture within the awake window.
- Capture one frame: `python tools/fap_device.py -p COM3 shot out.bmp`.
- Full 5-page acceptance: `python tools/fap_verify.py COM3 <out_dir>`
  (or `tools/fap_verify2.py`, a self-contained variant).
- Protocol summary: header line
  `FAP_SCREENSHOT_V1 240 320 <FMT> <N>\n` then N bytes (L8 / ARGB2222: 1 B/px,
  RGB565LE: 2 B/px, row-major top-down). Key line: `FAP_KEY_V1 <btn> <ev>`
  (btn 0=up 1=down 2=OK; ev 1=click 3=long-press).
