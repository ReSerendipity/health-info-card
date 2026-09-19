<p align="right">
  <a href="health-info-card-guide.zh_CN.md">简体中文</a> · <strong>English</strong>
</p>

# Health Info Card · FoloToy AI Passport — Delivery Guide

Built on the official `feat/senior-safety-card` branch, this turns the
FoloToy AI Passport (ESP32-C3 / 240×320 ST7789 / no PSRAM) into an
"health info card" that other people can read quickly in an accident or when
the wearer is lost.

## 1. Feature: 5 pages (navigate with UP/DOWN keys)

| Page | Content | Notes |
| --- | --- | --- |
| 1/5 | Name · Age · Blood type · Help request | Core page: `Li Xiaoming / 68 / Type A`, "Hello, I may be lost. Please help me contact my family" |
| 2/5 | Contact family | Family name/relation, primary and backup phone numbers (official masking supported) |
| 3/5 | Take me home | Home address (Nanchang, Jiangxi) |
| 4/5 | Health reminders | Allergies, chronic conditions, daily medication (penicillin allergy; hypertension, daily medication) |
| 5/5 | WeChat contact | WeChat QR code (shows a hint when not uploaded) |

- Long-press OK = re-enter the configuration portal (official behavior).
- Deep sleep after 60 s of inactivity (official behavior, saves power); press any key to wake.
- **All data is placeholder sample data and must be replaced before real use** (see section 2).

## 2. Placeholder data and replacement (required)

The profile below is baked into the firmware (`safety_profile_defaults()` in
`main/safety_profile.c`). Every value is an example — **replace with real
information and re-flash**:

| Field | Current placeholder | Replace with |
| --- | --- | --- |
| Name | Li Xiaoming | real name |
| Age | 68 | real age |
| Blood type | Type A | real blood type (A/B/AB/O ± RH) |
| Address | No.1 Shili Rd, Honggutan District, Nanchang, Jiangxi | real home address |
| Family | Li Jianguo (son) | real family member and relation |
| Family phone | 13800000000 | real number |
| Backup phone | 13900000000 | real backup number |
| Medical | penicillin allergy; hypertension, daily medication | real allergies / chronic conditions / medication (important for emergencies) |

Field definitions live in `main/safety_profile.h` (profile struct version=2,
adds `age[]` / `blood_type[]`). After editing, rebuild and flash with the
commands in section 3; flashing erases the old NVS, and the new firmware uses
the new placeholder profile directly.

## 3. Build and flash

```powershell
# Activate ESP-IDF 5.5.3 (IDF_PATH must use forward slashes; the official
# recovery_boot_hook CMake file breaks on backslashes)
& C:\Users\Doro\esp\esp-idf-v5.5.3\export.ps1
$env:IDF_PATH = "C:/Users/Doro/esp/esp-idf-v5.5.3"
$env:IDF_COMPONENT_STORAGE_URL = "https://components-file.espressif.cn"

# Build (from the repo root)
idf.py build

# Merge the full image and flash (repo requires the full image at 0x0; adjust COM port)
cd build
python -m esptool --chip esp32c3 merge_bin -o FoloToy-AI-Passport-full.bin "@..\build\flash_args"
python -m esptool --chip esp32c3 -p COM3 -b 460800 write_flash 0x0 FoloToy-AI-Passport-full.bin
```

Python runtime: `C:\Users\Doro\.espressif\python_env\idf5.5_py3.14_env\Scripts\python.exe`

## 4. On-device acceptance over serial (screenshot + key injection)

The firmware embeds a read-only screenshot/key-injection protocol
(FAP_SCREENSHOT_V1 / FAP_KEY_V1), so acceptance runs automatically without
touching the keys:

```powershell
# Single frame (L8 grayscale, 240×320 → 24-bit BMP)
python tools\fap_device.py -p COM3 shot _shot.bmp

# Key injection (btn: 0=up 1=down 2=OK; ev: 1=click 3=long-press)
python tools\fap_device.py -p COM3 key 1 1

# Full 5-page acceptance (capture → page → capture, writes to the given dir)
python tools\fap_verify.py COM3 _fap_shots
python tools\fap_verify2.py COM3 _fap_shots   # standalone variant (does not go through fap_device.py)
```

Key points (pitfalls):
- **Do not reset the device via DTR/RTS**: the USB CDC line-state change makes
  the device's USB re-enumerate and breaks the protocol (measured: all captures
  time out after a reset). Wake the device with an esptool `hard_reset` or a
  key press; capture inside the 60 s awake window (or temporarily raise
  `NORMAL_IDLE_MS` in `main/main.c` and restore 60 s afterwards).
- The device sends via **register-level TX FIFO** (`usb_serial_jtag_ll_*`):
  the driver's `write_bytes` (ring+ISR) never lands in this firmware (driver
  only, no VFS).
- Snapshot format is **L8 grayscale** (1 B/px, 76,800 B): the SW renderer
  does not support ARGB2222 as a target (output is noise); RGB565 needs
  153,600 B contiguous heap, which this board cannot provide.
- Host capture must **read raw bytes and then locate the header line**:
  per-line `readline()` misses the header under a short timeout (this was the
  root cause of the earlier full-timeout captures).

See `docs/development/esp32c3-usb-serial-jtag-pitfalls.md` for the complete
pitfall log.

## 5. On-device acceptance result (2026-09-19)

| Check | Result |
| --- | --- |
| Boot stability | PASS (clean log, no panic / reboot loop) |
| Page 1/5 name·age·blood type | PASS (Li Xiaoming / 68 / Type A) |
| Page 2/5 contact family | PASS (Li Jianguo/son, primary/backup numbers) |
| Page 3/5 take me home | PASS (Nanchang, Jiangxi) |
| Page 4/5 health reminders | PASS (penicillin allergy / hypertension / daily medication) |
| Page 5/5 WeChat contact | PASS (QR not uploaded hint) |
| Key-injection paging | PASS (1→2→3→4→5 full chain) |
| 60 s deep sleep restored | PASS (re-flashed with official 60 s; 5-page capture succeeded inside the window) |

Capture files: `_fap_shots/fap_page_0X_*.bmp` (5 files, delivered alongside).

## 6. Main changes (vs. official `feat/senior-safety-card`)

| File | Change |
| --- | --- |
| `main/safety_profile.h` | Profile v2: adds `age[12]`, `blood_type[12]` |
| `main/safety_profile.c` | defaults fill the placeholder profile and seal it (configured=1) |
| `main/safety_store.c` | load returns the placeholder profile when NVS is empty (no forced config portal) |
| `main/ui_safety.c` | Page 0 becomes name + age + blood type + help request |
| `main/main.c` | Wires up the FAP protocol; `NORMAL_IDLE_MS` stays official 60 s |
| `main/fap_screenshot.{c,h}` | New: FAP_SCREENSHOT_V1 (L8 snapshot, register-level TX) + FAP_KEY_V1 |
| `main/CMakeLists.txt` | Adds `fap_screenshot.c` |
| `sdkconfig.defaults` | LV_USE_CLIB_MALLOC / LV_USE_SNAPSHOT / main & timer task stacks 8192 |
| `tools/fap_device.py` | Host shot/key tool (L8/ARGB2222/RGB565LE → BMP) |
| `tools/fap_verify.py` / `tools/fap_verify2.py` | 5-page automatic acceptance scripts |

> Note: changes are not committed yet (repo convention: commit only on
> request); the earlier self-built approach (`health-info-card` branch
> workspace, stash "fap-work-before-official-branch") is abandoned.
