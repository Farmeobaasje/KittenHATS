# Third-Party Licenses

KittenHATS is built on several third-party components. This file documents their licenses and attributions.

## Hekate / Hekatos BDK

- **Source:** `app/vendor/hekate/` (Git submodule)
- **Repository:** https://github.com/sthetix/Hekatos.git
- **License:** GNU General Public License v2.0 (GPL-2.0)
- **Usage:** Hardware drivers (display, touch, SD card, timer, boot storage), bootloader integration
- **Modifications:** None — KittenHATS links against unmodified Hekate libraries

## LVGL (LittlevGL) v5.3

- **Source:** Included in `app/vendor/hekate/bdk/libs/lvgl/`
- **Repository:** https://github.com/lvgl/lvgl (v5.3)
- **License:** GNU General Public License v2.0 (GPL-2.0)
- **Usage:** GUI toolkit — `lv_img`, `lv_imgbtn`, `lv_btnm`, `lv_page`, `lv_cont`
- **Modifications:** None — used as distributed with Hekate

## FatFs (FatFS)

- **Source:** Included in `app/vendor/hekate/bdk/libs/fatfs/`
- **Repository:** http://elm-chan.org/fsw/ff/00index_e.html
- **License:** GNU General Public License v2.0 (GPL-2.0)
- **Usage:** FAT32 filesystem access for SD card
- **Modifications:** None — used as distributed with Hekate

## devkitARM (Toolchain)

- **Source:** https://devkitpro.org/
- **License:** GNU General Public License v2.0 (GPL-2.0) — toolchain, not distributed with KittenHATS
- **Usage:** Cross-compilation toolchain for ARM (Nintendo Switch homebrew)
- **Note:** devkitARM is a build-time dependency only. It is not included in the KittenHATS repository or binary distribution.

## License Compliance

KittenHATS itself is licensed under GPL-2.0 (see `LICENSE`). All third-party components used are also GPL-2.0 compatible.

### Source Code Availability

The complete source code for Hekate/Hekatos is available via the Git submodule:
```
git submodule update --init
```

The LVGL and FatFs source code is included within the Hekate submodule at:
- `app/vendor/hekate/bdk/libs/lvgl/`
- `app/vendor/hekate/bdk/libs/fatfs/`

### No Modifications

No modifications have been made to any Hekate, LVGL, or FatFs source code. All KittenHATS functionality is built as a separate payload that links against unmodified libraries.
