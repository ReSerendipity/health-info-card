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
- Placeholder profiles show a red **"sample data" banner on every page**; the banner disappears after a real profile is saved from the portal.
- Phone numbers are **masked by default** on display pages; enable full display explicitly in the portal.
- Portal PIN attempts are **throttled** (5 failures lock for 60 s) against online brute force.

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

Field definitions live in `main/safety_profile.h` (profile struct version=3,
adds the `demo` flag; `age[]` / `blood_type[]` are also editable from the
portal form). After editing the placeholder, rebuild and flash with the
commands in section 3; flashing erases the old NVS, and the new firmware uses
the new placeholder profile directly. Saving a real profile from the portal
clears the `demo` flag and the sample-data banner.

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

## 5. On-device acceptance result (2026-09-20, v3)

| Check | Result |
| --- | --- |
| Boot stability | PASS (clean log, no panic / reboot loop) |
| Page 1/5 name·age·blood type | PASS (Li Xiaoming / 68 / Type A) |
| Sample-data banner | PASS (red banner on every page until a real profile is saved) |
| Page 2/5 contact family | PASS (Li Jianguo/son; **primary masked `138****0000`, backup masked `139****0000`**) |
| Page 3/5 take me home | PASS (Nanchang, Jiangxi) |
| Page 4/5 health reminders | PASS (penicillin allergy / hypertension / daily medication) |
| Page 5/5 WeChat contact | PASS (QR not uploaded hint) |
| Key-injection paging | PASS (1→2→3→4→5 full chain; single-connection capture) |
| Long-press OK → portal | PASS (RESET confirm → LOCAL SETUP with QR, hotspot `AnXin-2041`; hint now reads “WiFi scan to join hotspot / open 192.168.4.1 in browser / password valid once, avoid photos”) |
| Portal exit | PASS (long-press OK returns to profile page) |
| 60 s deep sleep | PASS (USB activity wakes the device; page resets to 1/5 on non-button wake as designed) |
| GPIO-button wake returns to previous page | NOT RUN (needs physical key press; logic covered by review) |

Capture files: `_fap_shots/fap_page_0X_*.bmp` (5 files, delivered alongside).

## 6. Main changes (vs. official `feat/senior-safety-card`)

| File | Change |
| --- | --- |
| `main/safety_profile.h` | Profile v3: adds `demo` flag; `age[12]`, `blood_type[12]` |
| `main/safety_profile.c` | defaults fill the placeholder profile, seal it, set `demo`; phone masked by default; new form content validators (phone digits, age 1–150, blood-type whitelist) |
| `main/safety_store.c` | load returns the placeholder profile when NVS is empty; corrupted blobs degrade to setup portal |
| `main/ui_safety.c` | Page 0 shows name + age + blood type + help request; sample-data banner on every page |
| `main/main.c` | Wires up the FAP protocol; deep-sleep wake restores the previous page via RTC memory |
| `main/fap_screenshot.{c,h}` | New: FAP_SCREENSHOT_V1 (L8 snapshot, register-level TX) + FAP_KEY_V1 |
| `main/pin_throttle.{c,h}` | New: pure-logic PIN attempt throttle (5 failures → 60 s lock) |
| `main/safety_portal.c` | PIN throttle integration; age/blood_type editable; saving clears `demo` |
| `main/CMakeLists.txt` | Adds `fap_screenshot.c`, `pin_throttle.c` |
| `sdkconfig.defaults` | LV_USE_CLIB_MALLOC / LV_USE_SNAPSHOT / main & timer task stacks 8192 |
| `tools/fap_device.py` | Host shot/key tool (L8/ARGB2222/RGB565LE → BMP) |
| `tools/fap_verify.py` / `tools/fap_verify2.py` | 5-page automatic acceptance scripts |

## 7. Storage budget and image limits

- The WeChat QR code lives in the dedicated `imgstore` data partition: **128 KB**,
  with a 4 KB file header leaving about **124 KB** of usable data. The current
  design is a single QR-code slot.
- The per-image upload cap is **110 KB** (`QR_UPLOAD_MAX`). It sits at about
  **89% (~90% budget)** of the data area, deliberately not filling the space:
  the headroom prevents write exhaustion, fragmentation, or degradation of NVS
  and other storage; 110 KB is already far more than a 240x240 JPEG
  (~10-20 KB) needs.
- The front-end downscales phone images to **240x240 JPEG** (screen width);
  the decoder rejects anything wider than 240 or taller than 320.
- **Screen brightness is fixed**: 82% on boot, 0 (off) in deep sleep. The app
  exposes **no brightness control**; the backlight is GPIO21 PWM (software-tunable
  in the BSP but not exposed as a product feature).
