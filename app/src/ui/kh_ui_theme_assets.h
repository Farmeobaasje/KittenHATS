/*
 * Copyright (c) 2025 KittenHATS Contributors
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * KittenHATS — Refactor D Gate 1: Runtime path migration to canonical
 * theme tree. Paths are constructed dynamically via s_printf() using
 * the active theme + language. No more hardcoded #define paths.
 *
 * 13 theme/language-dependent assets — loaded per theme+language profile.
 * Semantic IDs allow screens to reference assets by purpose rather than
 * by path or struct field name. The theme asset set is loaded once at
 * boot and never reloaded. All 13 must load successfully.
 */

#ifndef _KH_UI_THEME_ASSETS_H_
#define _KH_UI_THEME_ASSETS_H_

#include <utils/types.h>
#include <libs/lvgl/lvgl.h>

/* ------------------------------------------------------------------ */
/* Theme enum — visual theme profiles                                 */
/* ------------------------------------------------------------------ */

typedef enum _kh_theme_t
{
	KH_THEME_ROYAL  = 0,   /* Default theme */
	KH_THEME_AQUA   = 1,
	KH_THEME_CANDY  = 2,
	KH_THEME_SUNSET = 3,
	KH_THEME_COUNT  = 4
} kh_theme_t;

/* ------------------------------------------------------------------ */
/* Language enum — UI language profiles                                */
/* ------------------------------------------------------------------ */

typedef enum _kh_language_t
{
	KH_LANG_NL = 0,   /* Dutch */
	KH_LANG_EN = 1,   /* English */
	KH_LANG_DE = 2,   /* German */
	KH_LANG_ES = 3,   /* Spanish */
	KH_LANG_FR = 4,   /* French */
	KH_LANG_COUNT = 5
} kh_language_t;

/* ------------------------------------------------------------------ */
/* Semantic asset IDs — 26 theme/language-dependent assets             */
/* ------------------------------------------------------------------ */

typedef enum _kh_theme_asset_id_t
{
	/* Home screen (4) */
	KH_THEME_ASSET_GEAR_NORMAL      = 0,   /* Gear icon normal */
	KH_THEME_ASSET_GEAR_PRESSED     = 1,   /* Gear icon pressed */
	KH_THEME_ASSET_START_NORMAL     = 2,   /* Start button normal */
	KH_THEME_ASSET_START_PRESSED    = 3,   /* Start button pressed */

	/* PIN screen (3) */
	KH_THEME_ASSET_PIN_TITLE        = 4,   /* PIN title text */
	KH_THEME_ASSET_PIN_BACK_NORMAL  = 5,   /* PIN back button normal */
	KH_THEME_ASSET_PIN_BACK_PRESSED = 6,   /* PIN back button pressed */

	/* Settings screen (6) */
	KH_THEME_ASSET_SETTINGS_TITLE       = 7,   /* Settings title text */
	KH_THEME_ASSET_SETTINGS_LANGUAGE    = 8,   /* Language button normal (disabled) */
	KH_THEME_ASSET_SETTINGS_PIN_NORMAL  = 9,   /* PIN button normal */
	KH_THEME_ASSET_SETTINGS_PIN_PRESSED = 10,  /* PIN button pressed */
	KH_THEME_ASSET_SETTINGS_BACK_NORMAL = 11,  /* Settings back normal */
	KH_THEME_ASSET_SETTINGS_BACK_PRESSED= 12,  /* Settings back pressed */

	/* Language selection screen (13) */
	KH_THEME_ASSET_LANG_TITLE           = 13,  /* Language title text */
	KH_THEME_ASSET_LANG_BACK_NORMAL     = 14,  /* Language back button normal */
	KH_THEME_ASSET_LANG_BACK_PRESSED    = 15,  /* Language back button pressed */
	KH_THEME_ASSET_LANG_NL_NORMAL       = 16,  /* NL button normal */
	KH_THEME_ASSET_LANG_NL_PRESSED      = 17,  /* NL button pressed */
	KH_THEME_ASSET_LANG_EN_NORMAL       = 18,  /* EN button normal */
	KH_THEME_ASSET_LANG_EN_PRESSED      = 19,  /* EN button pressed */
	KH_THEME_ASSET_LANG_DE_NORMAL       = 20,  /* DE button normal */
	KH_THEME_ASSET_LANG_DE_PRESSED      = 21,  /* DE button pressed */
	KH_THEME_ASSET_LANG_ES_NORMAL       = 22,  /* ES button normal */
	KH_THEME_ASSET_LANG_ES_PRESSED      = 23,  /* ES button pressed */
	KH_THEME_ASSET_LANG_FR_NORMAL       = 24,  /* FR button normal */
	KH_THEME_ASSET_LANG_FR_PRESSED      = 25,  /* FR button pressed */

	KH_THEME_ASSET_COUNT = 26
} kh_theme_asset_id_t;


/* ------------------------------------------------------------------ */
/* Canonical theme tree base path                                      */
/* ------------------------------------------------------------------ */

#define KH_THEME_BASE "bootloader/kittenhats/themes"

/* ------------------------------------------------------------------ */
/* Path buffer size — max path fits in 70 bytes including NUL          */
/* ------------------------------------------------------------------ */
/* Max path: "bootloader/kittenhats/themes/sunset/common/gear_icon_pressed.bmp"
 *           31 + 1 + 6 + 1 + 6 + 1 + 24 = 70 chars including NUL
 * Verified: 31 + "/" + "sunset" + "/" + "common" + "/" + "gear_icon_pressed.bmp" + NUL
 *           = 31 + 1 + 6 + 1 + 6 + 1 + 24 + 1 = 71... let's use 80 for safety.
 */
#define KH_THEME_PATH_BUF_SIZE 80

/* ------------------------------------------------------------------ */
/* Theme/language name strings (for s_printf path construction)        */
/* ------------------------------------------------------------------ */

extern const char * const kh_theme_names[KH_THEME_COUNT];
extern const char * const kh_language_names[KH_LANG_COUNT];

/* ------------------------------------------------------------------ */
/* Path resolver — build canonical path for a given asset              */
/* ------------------------------------------------------------------ */

/*
 * Build the canonical SD path for a theme/language asset.
 * Gear icons (KH_THEME_ASSET_GEAR_NORMAL/PRESSED) use common/ directory.
 * All other assets use the language directory.
 *
 * @param buf     Output buffer (KH_THEME_PATH_BUF_SIZE bytes).
 * @param theme   Active theme.
 * @param lang    Active language.
 * @param id      Semantic asset ID.
 * @return        Pointer to buf (for convenience), or NULL on invalid id.
 */
const char *kh_theme_asset_path(char *buf, kh_theme_t theme,
                                kh_language_t lang, kh_theme_asset_id_t id);

/* ------------------------------------------------------------------ */
/* Theme asset set struct — 13 descriptors indexed by semantic ID     */
/* ------------------------------------------------------------------ */

typedef struct _kh_ui_theme_asset_set_t
{
	/* Array indexed by kh_theme_asset_id_t */
	lv_img_dsc_t *items[KH_THEME_ASSET_COUNT];
} kh_ui_theme_asset_set_t;

/*
 * Load all 13 theme/language BMP assets from SD.
 * Uses kh_theme_asset_path() to construct canonical paths.
 * All 13 must load successfully — returns true only on full success.
 * On failure, frees any partial set and returns false.
 *
 * @param assets  Pointer to zeroed theme asset set struct.
 * @param theme   Active theme.
 * @param lang    Active language.
 * @return        true if all 13 loaded, false on any failure.
 */
bool kh_ui_theme_assets_load(kh_ui_theme_asset_set_t *assets,
                             kh_theme_t theme, kh_language_t lang);

/*
 * Free all 13 theme asset allocations.
 * NULL-safe — each pointer is checked before free.
 * Sets all pointers to NULL after freeing.
 *
 * @param assets  Pointer to theme asset set struct.
 */
void kh_ui_theme_assets_free(kh_ui_theme_asset_set_t *assets);

#endif /* _KH_UI_THEME_ASSETS_H_ */
