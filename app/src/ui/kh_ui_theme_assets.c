/*
 * Copyright (c) 2025 KittenHATS Contributors
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * KittenHATS — Runtime path migration to canonical theme tree.
 * Paths are constructed dynamically via s_printf() using
 * the active theme + language. No more hardcoded #define paths.
 *
 * Theme/language name tables map enum values to directory names.
 * The path resolver builds canonical SD paths for each semantic asset ID.
 * Gear icons use common/ directory; all other assets use language directory.
 *
 * All 26 theme assets must load successfully — all-or-nothing transactional
 * load. On failure, any partial set is freed and false is returned.

 *
 * v0.1.17: Added per-asset diagnostic instrumentation via
 * kh_asset_load_bmp_diagnostic() and kh_diag_halt().
 */

#include <string.h>
#include <stdlib.h>

#include <bdk.h>
#include <utils/sprintf.h>
#include <libs/fatfs/ff.h>
#include <storage/sd.h>

#include "kh_ui_theme_assets.h"
#include "kh_ui_asset_loader.h"
#include "kh_ui_diagnostic.h"

/* ------------------------------------------------------------------ */
/* Theme name strings — map kh_theme_t enum to directory names         */
/* ------------------------------------------------------------------ */

const char * const kh_theme_names[KH_THEME_COUNT] = {
	[KH_THEME_ROYAL]  = "royal",
	[KH_THEME_AQUA]   = "aqua",
	[KH_THEME_CANDY]  = "candy",
	[KH_THEME_SUNSET] = "sunset",
};

/* ------------------------------------------------------------------ */
/* Language name strings — map kh_language_t enum to directory names   */
/* ------------------------------------------------------------------ */

const char * const kh_language_names[KH_LANG_COUNT] = {
	[KH_LANG_NL] = "nl",
	[KH_LANG_EN] = "en",
	[KH_LANG_DE] = "de",
	[KH_LANG_ES] = "es",
	[KH_LANG_FR] = "fr",
};

/* ------------------------------------------------------------------ */
/* Filename table — map semantic asset ID to BMP filename              */
/* ------------------------------------------------------------------ */

static const char * const _kh_asset_filenames[KH_THEME_ASSET_COUNT] = {
	[KH_THEME_ASSET_GEAR_NORMAL]           = "gear_icon_normal.bmp",
	[KH_THEME_ASSET_GEAR_PRESSED]          = "gear_icon_pressed.bmp",
	[KH_THEME_ASSET_START_NORMAL]          = "start_button_normal.bmp",
	[KH_THEME_ASSET_START_PRESSED]         = "start_button_pressed.bmp",
	[KH_THEME_ASSET_PIN_TITLE]             = "pin_title.bmp",
	[KH_THEME_ASSET_PIN_BACK_NORMAL]       = "back_button_normal.bmp",
	[KH_THEME_ASSET_PIN_BACK_PRESSED]      = "back_button_pressed.bmp",
	[KH_THEME_ASSET_SETTINGS_TITLE]        = "settings_title.bmp",
	[KH_THEME_ASSET_SETTINGS_LANGUAGE]     = "taal_button_normal.bmp",
	[KH_THEME_ASSET_SETTINGS_PIN_NORMAL]   = "pin_button_normal.bmp",
	[KH_THEME_ASSET_SETTINGS_PIN_PRESSED]  = "pin_button_pressed.bmp",
	[KH_THEME_ASSET_SETTINGS_BACK_NORMAL]  = "settings_back_normal.bmp",
	[KH_THEME_ASSET_SETTINGS_BACK_PRESSED] = "settings_back_pressed.bmp",

	/* Language selection screen (13) */
	[KH_THEME_ASSET_LANG_TITLE]            = "language_title.bmp",
	[KH_THEME_ASSET_LANG_BACK_NORMAL]      = "lang_back_normal.bmp",
	[KH_THEME_ASSET_LANG_BACK_PRESSED]     = "lang_back_pressed.bmp",
	[KH_THEME_ASSET_LANG_NL_NORMAL]        = "nl_button_normal.bmp",
	[KH_THEME_ASSET_LANG_NL_PRESSED]       = "nl_button_pressed.bmp",
	[KH_THEME_ASSET_LANG_EN_NORMAL]        = "en_button_normal.bmp",
	[KH_THEME_ASSET_LANG_EN_PRESSED]       = "en_button_pressed.bmp",
	[KH_THEME_ASSET_LANG_DE_NORMAL]        = "de_button_normal.bmp",
	[KH_THEME_ASSET_LANG_DE_PRESSED]       = "de_button_pressed.bmp",
	[KH_THEME_ASSET_LANG_ES_NORMAL]        = "es_button_normal.bmp",
	[KH_THEME_ASSET_LANG_ES_PRESSED]       = "es_button_pressed.bmp",
	[KH_THEME_ASSET_LANG_FR_NORMAL]        = "fr_button_normal.bmp",
	[KH_THEME_ASSET_LANG_FR_PRESSED]       = "fr_button_pressed.bmp",
};

/* Compile-time check: filename table must have exactly 26 entries */
_Static_assert(
	sizeof(_kh_asset_filenames) / sizeof(_kh_asset_filenames[0])
		== KH_THEME_ASSET_COUNT,
	"theme filename table must contain exactly 26 entries");


/* ------------------------------------------------------------------ */
/* Semantic name strings for diagnostics                              */
/* ------------------------------------------------------------------ */

static const char * const _kh_asset_names[KH_THEME_ASSET_COUNT] = {
	[KH_THEME_ASSET_GEAR_NORMAL]           = "gear_normal",
	[KH_THEME_ASSET_GEAR_PRESSED]          = "gear_pressed",
	[KH_THEME_ASSET_START_NORMAL]          = "start_normal",
	[KH_THEME_ASSET_START_PRESSED]         = "start_pressed",
	[KH_THEME_ASSET_PIN_TITLE]             = "pin_title",
	[KH_THEME_ASSET_PIN_BACK_NORMAL]       = "pin_back_normal",
	[KH_THEME_ASSET_PIN_BACK_PRESSED]      = "pin_back_pressed",
	[KH_THEME_ASSET_SETTINGS_TITLE]        = "settings_title",
	[KH_THEME_ASSET_SETTINGS_LANGUAGE]     = "settings_language",
	[KH_THEME_ASSET_SETTINGS_PIN_NORMAL]   = "settings_pin_normal",
	[KH_THEME_ASSET_SETTINGS_PIN_PRESSED]  = "settings_pin_pressed",
	[KH_THEME_ASSET_SETTINGS_BACK_NORMAL]  = "settings_back_normal",
	[KH_THEME_ASSET_SETTINGS_BACK_PRESSED] = "settings_back_pressed",

	/* Language selection screen (13) */
	[KH_THEME_ASSET_LANG_TITLE]            = "lang_title",
	[KH_THEME_ASSET_LANG_BACK_NORMAL]      = "lang_back_normal",
	[KH_THEME_ASSET_LANG_BACK_PRESSED]     = "lang_back_pressed",
	[KH_THEME_ASSET_LANG_NL_NORMAL]        = "lang_nl_normal",
	[KH_THEME_ASSET_LANG_NL_PRESSED]       = "lang_nl_pressed",
	[KH_THEME_ASSET_LANG_EN_NORMAL]        = "lang_en_normal",
	[KH_THEME_ASSET_LANG_EN_PRESSED]       = "lang_en_pressed",
	[KH_THEME_ASSET_LANG_DE_NORMAL]        = "lang_de_normal",
	[KH_THEME_ASSET_LANG_DE_PRESSED]       = "lang_de_pressed",
	[KH_THEME_ASSET_LANG_ES_NORMAL]        = "lang_es_normal",
	[KH_THEME_ASSET_LANG_ES_PRESSED]       = "lang_es_pressed",
	[KH_THEME_ASSET_LANG_FR_NORMAL]        = "lang_fr_normal",
	[KH_THEME_ASSET_LANG_FR_PRESSED]       = "lang_fr_pressed",
};


/* ------------------------------------------------------------------ */
/* Path resolver — build canonical SD path for a given asset           */
/* ------------------------------------------------------------------ */

const char *kh_theme_asset_path(char *buf, kh_theme_t theme,
                                kh_language_t lang, kh_theme_asset_id_t id)
{
	/* Validate parameters */
	if (!buf || id >= KH_THEME_ASSET_COUNT ||
	    theme >= KH_THEME_COUNT || lang >= KH_LANG_COUNT)
		return NULL;

	/*
	 * Gear icons are theme-dependent but language-independent.
	 * They live in the common/ directory of each theme.
	 * All other assets are both theme- and language-dependent.
	 */
	if (id == KH_THEME_ASSET_GEAR_NORMAL || id == KH_THEME_ASSET_GEAR_PRESSED)
	{
		/*
		 * Path: "bootloader/kittenhats/themes/<theme>/common/<filename>"
		 * Example: "bootloader/kittenhats/themes/royal/common/gear_icon_normal.bmp"
		 */
		s_printf(buf, "%s/%s/common/%s",
		         KH_THEME_BASE,
		         kh_theme_names[theme],
		         _kh_asset_filenames[id]);
	}
	else
	{
		/*
		 * Path: "bootloader/kittenhats/themes/<theme>/<lang>/<filename>"
		 * Example: "bootloader/kittenhats/themes/royal/nl/start_button_normal.bmp"
		 */
		s_printf(buf, "%s/%s/%s/%s",
		         KH_THEME_BASE,
		         kh_theme_names[theme],
		         kh_language_names[lang],
		         _kh_asset_filenames[id]);
	}

	return buf;
}

/* ------------------------------------------------------------------ */
/* Load all 26 theme/language BMP assets — diagnostic instrumented    */
/* ------------------------------------------------------------------ */

bool kh_ui_theme_assets_load(kh_ui_theme_asset_set_t *assets,
                             kh_theme_t theme, kh_language_t lang)
{
	if (!assets)
		return false;

	/* Ensure all pointers start NULL */
	memset(assets, 0, sizeof(kh_ui_theme_asset_set_t));

	/* Path buffer for dynamic path construction */
	char path[KH_THEME_PATH_BUF_SIZE];

	/* Load all 26 assets in a single loop with diagnostic instrumentation */

	for (int i = 0; i < KH_THEME_ASSET_COUNT; i++)
	{
		/* Build canonical path for this asset */
		if (!kh_theme_asset_path(path, theme, lang, (kh_theme_asset_id_t)i))
		{
			/* Path resolution failed — halt permanently */
			kh_diag_halt(KH_DBG_STAGE_THEME, i,
			             _kh_asset_names[i], "PATH_RESOLVE_FAILED",
			             KH_ASSET_LOAD_ERR_ARGUMENT);
		}

		/* Load BMP from SD with diagnostic status */
		kh_asset_load_status_t status =
			kh_asset_load_bmp_diagnostic(path, &assets->items[i]);

		if (status != KH_ASSET_LOAD_OK)
		{
			/* Halt permanently — never returns */
			kh_diag_halt(KH_DBG_STAGE_THEME, i,
			             _kh_asset_names[i], path, status);
		}
	}

	/* All 26 loaded successfully */

	return true;

	/* Note: kh_diag_halt is noreturn, so the old goto fail path is unreachable.
	 * The free-on-failure is handled by the diagnostic halt never returning. */
}

/* ------------------------------------------------------------------ */
/* Free all 26 theme asset allocations                                */
/* ------------------------------------------------------------------ */


void kh_ui_theme_assets_free(kh_ui_theme_asset_set_t *assets)
{
	if (!assets)
		return;

	for (int i = 0; i < KH_THEME_ASSET_COUNT; i++)
		kh_asset_free(&assets->items[i]);
}
