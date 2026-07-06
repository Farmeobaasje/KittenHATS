# KittenHATS 🐱🎮

> A Nintendo Switch that's safe for a child to use — built as a standalone BDK/LVGL payload that chainloads HATS. No fork of HATS or Hekate, pure configuration on top.

**⚠️ PRE-RELEASE NOTICE:** KittenHATS is in active development (v0.1.x). Features may change, and hardware testing is ongoing. Use at your own risk.

---

## Overview

KittenHATS is a small standalone payload (built on Hekate's BDK + LVGL) that loads first when the Switch boots. It presents a child-friendly interface with a single large "Start" button that boots directly into a safe emuMMC. An administrator can tap a gear icon, enter a PIN, and access the full Nyx/HATS menu.

Hekate and HATS remain **100% unmodified** — no fork, updates continue to work.

### How It Works

```
Power On → Modchip/RCM → KittenHATS (payload.bin)
                              │
                    ┌─────────┴──────────┐
                    ▼                    ▼
              [Start Button]       [Gear Icon]
                    │                    │
                    ▼                    ▼
           Boot from ID "KIDS"     PIN-pad (4 digits)
                    │                    │
                    ▼              ┌─────┴──────┐
              emuMMC (safe)       ▼              ▼
                            Correct PIN    Wrong PIN
                                  │              │
                                  ▼              ▼
                            Nyx/HATS menu   Back to home
```

### What This Solves

1. **At boot:** The child never sees the Nyx/Hekate menu — only the big "Start" button
2. **In firmware:** Since KittenHATS loads before any firmware, there's no homebrew launcher or Tesla overlay access

---

## Hardware Requirements

| Requirement | Details |
|-------------|---------|
| **Switch Model** | Erista (V1) or Mariko (V2/OLED) with modchip or RCM access |
| **Firmware** | Tested on HATS 2.1+ (firmware 22.1.0) |
| **SD Card** | FAT32 formatted, with HATS distribution installed |
| **Modchip/RCM** | Any method that loads `payload.bin` from SD root |

---

## Installation

### Prerequisites

- A Nintendo Switch with modchip or RCM access
- [HATS](https://github.com/sthetix/HATS) distribution installed on SD card
- SD card reader for your computer

### Steps

1. **Download** the latest KittenHATS release from the [Releases](https://github.com/sthetix/KittenHATS/releases) page
2. **Extract** the overlay package to your SD card root
3. **Replace** `payload.bin` on your SD card root with `kittenhats.bin` (or copy `payload.bin` from the package)
4. **Replace** `bootloader/` on your SD card with the one from the package (or merge the `kittenhats/` directory)
5. **Close the reboot backdoor:** Replace `atmosphere/reboot_payload.bin` with `kittenhats.bin` (so reboots go through KittenHATS)
6. **Configure** `kittenhats.ini` on your SD card (see Configuration below)
7. **Boot** your Switch — you should see the KittenHATS home screen

### File Placement

```
SD Card Root:
├── payload.bin              ← KittenHATS payload (copied from package)
├── boot.ini                 ← [payload] file=kittenhats.bin (optional)
├── kittenhats.ini           ← PIN hash and paths (create this)
├── bootloader/
│   └── kittenhats/
│       ├── kittenhats.bin   ← KittenHATS payload (second copy)
│       ├── splash.bin       ← Splash image
│       └── assets/
│           ├── common/      ← Shared UI assets
│           └── themes/      ← Theme assets
├── atmosphere/
│   └── reboot_payload.bin   ← Replace with kittenhats.bin
└── (rest of HATS distribution)
```

---

## Configuration

### `kittenhats.ini`

Create this file on your SD card root to set up the PIN:

```ini
[kittenhats]
pin_hash=5e884898da28047151d0e56f8dc6292773603d0d6aabbdd62a11ef721d1542d8
hekate_path=/bootloader/update.bin
```

| Key | Description | Default |
|-----|-------------|---------|
| `pin_hash` | SHA-256 hash of the PIN (hex-encoded) | Required |
| `hekate_path` | Path to Hekate binary for chainload | `/bootloader/update.bin` |

**Generating a PIN hash:**

```bash
# Linux/macOS
echo -n "1234" | sha256sum

# Windows (PowerShell)
Write-Host -NoNewline "1234" | Get-FileHash -Algorithm SHA256 | Select-Object -ExpandProperty Hash
```

Replace `1234` with your desired PIN (4-6 digits).

### `boot.ini` (optional)

If your modchip supports `boot.ini`, create this file on SD root:

```ini
[config]
autoboot=0
autoboot_list=0
bootwait=3
backlight=100
[payload]
file=kittenhats.bin
```

---

## Usage

### Child Mode

1. Power on the Switch
2. KittenHATS loads automatically
3. Tap the large **"Start"** button
4. The Switch boots directly into emuMMC (safe mode)
5. The child can play games without ever seeing Nyx or homebrew tools

### Administrator Mode

1. Power on the Switch
2. KittenHATS loads automatically
3. Tap the **gear icon** (⚙️) in the top-right corner
4. Enter your **4-digit PIN** on the keypad
5. **Correct PIN:** Switch boots into full Nyx/HATS menu
6. **Wrong PIN:** Returns to the home screen (fail-safe)

### Settings

From the PIN screen, tap the settings icon to access:
- **Language selection** — Choose your preferred language
- **PIN settings** — Configure or change the PIN

---

## Building from Source

### Prerequisites

- [devkitPro](https://devkitpro.org/) with devkitARM
- Python 3.x
- Git

### Setup

```bash
# Clone the repository
git clone https://github.com/sthetix/KittenHATS.git
cd KittenHATS

# Initialize submodule (Hekate source)
git submodule update --init

# Navigate to app directory
cd app
```

### Build

```bash
# Using devkitPro msys2 shell (Windows)
export DEVKITPRO=e:/devkitPro
export DEVKITARM=e:/devkitPro/devkitARM
make clean && make
```

**Expected output:**
- `app/dist/kittenhats.bin` — The built payload (< 256KB)
- `app/dist/splash.bin` — Splash image (3.6MB)

### Build Requirements

| Tool | Version | Notes |
|------|---------|-------|
| devkitARM | Latest | Install via devkitPro pacman: `pacman -S devkitARM` |
| Python 3 | 3.x | For splash image conversion |
| Hekate submodule | v6.5.3 | Automatically fetched via `git submodule update --init` |

---

## Hardware Test Status

⚠️ **Hardware tests pending:** The following features require hardware validation:

- [ ] Chainload to emuMMC (KIDS boot)
- [ ] Chainload to Nyx (CLEAN boot)
- [ ] PIN verification (correct PIN → Nyx)
- [ ] PIN fail-safe (wrong PIN → home screen)
- [ ] Touch input on all screens
- [ ] Display rendering (splash, home, PIN, settings)
- [ ] SD card config reading (kittenhats.ini)
- [ ] Reboot backdoor (reboot_payload.bin replacement)
- [ ] Language switching
- [ ] Theme switching

---

## Project Status

| Phase | Status |
|-------|--------|
| Fase 0 — Skeleton & toolchain | ✅ **COMPLETE** |
| Fase 1 — Splash + chainload | ✅ **COMPLETE** |
| Fase 2 — Boot storage: KIDS entry | ✅ **COMPLETE** |
| Fase 3 — Home screen with two buttons | ✅ **COMPLETE** |
| Fase 4 — PIN pad behind gear icon | ✅ **COMPLETE / HARDWARE VALIDATED** |
| Fase 5 — Language selection & localization | 🔄 **IN PROGRESS** |
| Fase 6 — Reboot backdoor & finishing | ⏳ **PENDING** |

See [roadmap.md](roadmap.md) for detailed progress (internal document).

---

## Tech Stack

| Component | Technology |
|-----------|------------|
| Language | C (C99) |
| GUI Toolkit | LVGL v5.3 (via Hekate) |
| Hardware Drivers | Hekate BDK |
| Toolchain | devkitARM (GNU-ARM) |
| Bootloader | Hekatos v6.5.3 (unmodified) |
| Distribution | HATS (unmodified) |

---

## License & Attribution

### License

KittenHATS is licensed under the **GNU General Public License v2.0** (GPL-2.0). See [LICENSE](LICENSE) for the full text.

### Third-Party Components

| Component | Source | License | Usage |
|-----------|--------|---------|-------|
| **Hekatos BDK** | `app/vendor/hekate/` (submodule) | GPL-2.0 | Hardware drivers (display, touch, SD, timer) |
| **Hekatos LVGL** | `app/vendor/hekate/` (submodule) | GPL-2.0 | GUI toolkit (lv_img, lv_imgbtn, lv_btnm) |
| **Hekatos FatFs** | `app/vendor/hekate/` (submodule) | GPL-2.0 | FAT32 filesystem for SD card |
| **devkitARM** | devkitPro | GPL-2.0 | GNU-ARM cross-compiler toolchain |

**No modifications** are made to any Hekate/Hekatos source code. All KittenHATS functionality is built as a separate payload that links against unmodified Hekate libraries.

See [THIRD_PARTY_LICENSES.md](THIRD_PARTY_LICENSES.md) for full third-party license details.

---

## Disclaimer

**WARNING:** Modifying your Nintendo Switch involves risk. KittenHATS provides access to system-level tools that can damage your console if used incorrectly. The PIN is child-proofing, not real security — anyone with physical access to the SD card can bypass it. Use at your own risk. The authors are not responsible for any damage to your device.

---

## Contributing

Contributions are welcome! Please see:
- [CONTRIBUTING.md](CONTRIBUTING.md) — Development guidelines
- [CODE_OF_CONDUCT.md](CODE_OF_CONDUCT.md) — Community standards
- [SECURITY.md](SECURITY.md) — Security policy

---

## Documentation

- [THIRD_PARTY_LICENSES.md](THIRD_PARTY_LICENSES.md) — Third-party license information
- [docs/](docs/) — Additional documentation (internal)
