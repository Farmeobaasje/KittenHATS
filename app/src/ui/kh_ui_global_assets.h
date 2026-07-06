/*
 * Copyright (c) 2025 KittenHATS Contributors
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * KittenHATS — Refactor C: Global asset definitions.
 *
 * 32 global assets — independent of theme and language.
 * These remain outside theme profiles (under common/).
 *
 * Loaded once at boot, never reloaded.
 * All 32 must load successfully for the UI to start.
 */

#ifndef _KH_UI_GLOBAL_ASSETS_H_
#define _KH_UI_GLOBAL_ASSETS_H_

#include <utils/types.h>
#include <libs/lvgl/lvgl.h>

/* ------------------------------------------------------------------ */
/* SD paths — legacy runtime paths (unchanged in Refactor C)          */
/* ------------------------------------------------------------------ */

#define KH_GLOBAL_PATH_BACKGROUND  "bootloader/kittenhats/assets/common/home_background.bmp"
#define KH_GLOBAL_PATH_LOGO        "bootloader/kittenhats/assets/common/kittenhats_logo.bmp"

#define KH_GLOBAL_PATH_PIN_0_N     "bootloader/kittenhats/assets/common/pin_0_normal.bmp"
#define KH_GLOBAL_PATH_PIN_0_P     "bootloader/kittenhats/assets/common/pin_0_pressed.bmp"
#define KH_GLOBAL_PATH_PIN_1_N     "bootloader/kittenhats/assets/common/pin_1_normal.bmp"
#define KH_GLOBAL_PATH_PIN_1_P     "bootloader/kittenhats/assets/common/pin_1_pressed.bmp"
#define KH_GLOBAL_PATH_PIN_2_N     "bootloader/kittenhats/assets/common/pin_2_normal.bmp"
#define KH_GLOBAL_PATH_PIN_2_P     "bootloader/kittenhats/assets/common/pin_2_pressed.bmp"
#define KH_GLOBAL_PATH_PIN_3_N     "bootloader/kittenhats/assets/common/pin_3_normal.bmp"
#define KH_GLOBAL_PATH_PIN_3_P     "bootloader/kittenhats/assets/common/pin_3_pressed.bmp"
#define KH_GLOBAL_PATH_PIN_4_N     "bootloader/kittenhats/assets/common/pin_4_normal.bmp"
#define KH_GLOBAL_PATH_PIN_4_P     "bootloader/kittenhats/assets/common/pin_4_pressed.bmp"
#define KH_GLOBAL_PATH_PIN_5_N     "bootloader/kittenhats/assets/common/pin_5_normal.bmp"
#define KH_GLOBAL_PATH_PIN_5_P     "bootloader/kittenhats/assets/common/pin_5_pressed.bmp"
#define KH_GLOBAL_PATH_PIN_6_N     "bootloader/kittenhats/assets/common/pin_6_normal.bmp"
#define KH_GLOBAL_PATH_PIN_6_P     "bootloader/kittenhats/assets/common/pin_6_pressed.bmp"
#define KH_GLOBAL_PATH_PIN_7_N     "bootloader/kittenhats/assets/common/pin_7_normal.bmp"
#define KH_GLOBAL_PATH_PIN_7_P     "bootloader/kittenhats/assets/common/pin_7_pressed.bmp"
#define KH_GLOBAL_PATH_PIN_8_N     "bootloader/kittenhats/assets/common/pin_8_normal.bmp"
#define KH_GLOBAL_PATH_PIN_8_P     "bootloader/kittenhats/assets/common/pin_8_pressed.bmp"
#define KH_GLOBAL_PATH_PIN_9_N     "bootloader/kittenhats/assets/common/pin_9_normal.bmp"
#define KH_GLOBAL_PATH_PIN_9_P     "bootloader/kittenhats/assets/common/pin_9_pressed.bmp"

#define KH_GLOBAL_PATH_PIN_CLR_N   "bootloader/kittenhats/assets/common/pin_clear_normal.bmp"
#define KH_GLOBAL_PATH_PIN_CLR_P   "bootloader/kittenhats/assets/common/pin_clear_pressed.bmp"
#define KH_GLOBAL_PATH_PIN_OK_N    "bootloader/kittenhats/assets/common/pin_ok_normal.bmp"
#define KH_GLOBAL_PATH_PIN_OK_P    "bootloader/kittenhats/assets/common/pin_ok_pressed.bmp"

#define KH_GLOBAL_PATH_PIN_DISP_0  "bootloader/kittenhats/assets/common/pin_display_0.bmp"
#define KH_GLOBAL_PATH_PIN_DISP_1  "bootloader/kittenhats/assets/common/pin_display_1.bmp"
#define KH_GLOBAL_PATH_PIN_DISP_2  "bootloader/kittenhats/assets/common/pin_display_2.bmp"
#define KH_GLOBAL_PATH_PIN_DISP_3  "bootloader/kittenhats/assets/common/pin_display_3.bmp"
#define KH_GLOBAL_PATH_PIN_DISP_4  "bootloader/kittenhats/assets/common/pin_display_4.bmp"

#define KH_GLOBAL_PATH_PIN_DIAG_OK "bootloader/kittenhats/assets/common/pin_diag_ok.bmp"

/* ------------------------------------------------------------------ */
/* Global asset struct — 32 descriptors                               */
/* ------------------------------------------------------------------ */

typedef struct _kh_ui_global_assets_t
{
	/* Home screen (2) */
	lv_img_dsc_t *background;   /* 1280x720 full-screen background */
	lv_img_dsc_t *logo;         /* 500x180 KittenHATS logo */

	/* PIN digit keys 0-9 normal/pressed (20) */
	lv_img_dsc_t *pin_digit_n[10];
	lv_img_dsc_t *pin_digit_p[10];

	/* PIN clear and OK buttons normal/pressed (4) */
	lv_img_dsc_t *pin_clear_n;
	lv_img_dsc_t *pin_clear_p;
	lv_img_dsc_t *pin_ok_n;
	lv_img_dsc_t *pin_ok_p;

	/* PIN display variants 0-4 (5) — 4-digit PIN */
	lv_img_dsc_t *pin_display[5];

	/* PIN diagnostic OK image (1) */
	lv_img_dsc_t *pin_diag_ok;
} kh_ui_global_assets_t;

/*
 * Load all 32 global BMP assets from SD.
 * All 32 must load successfully — returns true only on full success.
 * On failure, frees any partial set and returns false.
 *
 * @param assets  Pointer to zeroed global assets struct.
 * @return        true if all 32 loaded, false on any failure.
 */
bool kh_ui_global_assets_load(kh_ui_global_assets_t *assets);

/*
 * Free all 32 global asset allocations.
 * NULL-safe — each pointer is checked before free.
 * Sets all pointers to NULL after freeing.
 *
 * @param assets  Pointer to global assets struct.
 */
void kh_ui_global_assets_free(kh_ui_global_assets_t *assets);

#endif /* _KH_UI_GLOBAL_ASSETS_H_ */
