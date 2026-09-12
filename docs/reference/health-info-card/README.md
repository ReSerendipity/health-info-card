<p align="right">
  <a href="README.zh_CN.md">简体中文</a> · <strong>English</strong>
</p>

# Health Info Card

A four-page ICE (In Case of Emergency) / medical-info display for the FoloToy AI
Passport. When someone encounters an accident involving the owner (or finds the
device), the essential information — name, age, blood type, family phone
numbers, allergies, chronic conditions — is one press away.

## What each page shows

| Page | Title | Content |
| --- | --- | --- |
| 0 | Overview | Name (large), blood type badge, age / gender, "show this card in an accident" reminder |
| 1 | Emergency contacts | Up to 3 family contacts (relationship + phone) and a "contact family first" reminder |
| 2 | Medical info | Allergies, chronic conditions, regular medications, notes |
| 3 | First-aid notes | Auto-built action list for responders: call 120, contact family, allergy warning, donor status |

## Button mapping

- **UP / DOWN** — switch page (wraps around)
- **OK (short press)** — next page (handy one-hand navigation)
- **OK (long press)** — return to the main menu (handled by `main.c`)

## Editing your own data

All personal data lives in one place:

- `main/health_profile.h` — field descriptions and conventions
- `main/health_profile.c` — the actual `g_health_profile` values

Fill in the placeholder values (name, phones, allergies, …), rebuild, and flash.
Notes:

- Phones in `138-0000-0000` grouping read best on the small screen.
- Use `""` to hide a field, `"无"` for "none".
- `blood_type` should stay short (e.g. `A 型`, `AB 型 Rh-`) to fit the badge.
- Chinese display depends on the subset fonts. If a character (e.g. a rare name
  character) shows blank, add it to `tools/health_font_chars.txt` and rerun
  `tools/gen_health_font.ps1`, then rebuild.

## Build and flash

Activate ESP-IDF 5.5.3, then from the repository root:

```bash
./tools/validate.sh --firmware
```

Flash the verified merged image at offset `0x0`:

```bash
python -m esptool --chip esp32c3 -p <port> -b 460800 \
    write-flash 0x0 build/FoloToy-AI-Passport-full.bin
```

The device boots into the menu; select **Health**. The build also runs the
profile-data host test (`tests/test_health_profile.c`).

## Files

| File | Purpose |
| --- | --- |
| `main/demo_health.c` | Page UI, key handling, battery indicator |
| `main/health_profile.h` / `.c` | Editable personal data |
| `main/health_font_16.c` / `_20.c` | Noto Sans CJK SC subset fonts (generated) |
| `main/health_font.h` | Font declarations |
| `tools/health_font_chars.txt` | Font character set (add missing chars here) |
| `tools/gen_health_font.ps1` | Font regeneration script |
| `tests/test_health_profile.c` | Host-side profile data checks |

## On-device acceptance checklist

- [ ] Overview shows name, blood type, age, gender without overflow
- [ ] Contact page shows every entered phone correctly
- [ ] UP / DOWN / OK switch pages; OK long-press returns to menu
- [ ] Battery percentage appears top-right on every page
- [ ] All allergy / condition lines render (no blank boxes)
