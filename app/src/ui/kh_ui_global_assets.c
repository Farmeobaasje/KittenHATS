/*
 * Copyright (c) 2025 KittenHATS Contributors
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * KittenHATS — Global asset loading implementation.
 *
 * Loads 32 global BMP assets from SD using kh_asset_load_bmp().
 * All-or-nothing: returns true only if all 32 load successfully.
 * On failure, frees any partial set and returns false.
 *
 * Global assets are independent of theme and language (under common/).
 * Loaded once at boot, never reloaded.
 *
 * v0.1.17: Added explicit descriptor table with _Static_assert(32) and
 * per-asset diagnostic instrumentation via kh_asset_load_bmp_diagnostic().
 */

#include <string.h>
#include <stdlib.h>

#include <bdk.h>
#include <libs/fatfs/ff.h>
#include <storage/sd.h>

#include "kh_ui_global_assets.h"
#include "kh_ui_asset_loader.h"
#include "kh_ui_diagnostic.h"

/* ------------------------------------------------------------------ */
/* Global asset descriptor table — exactly 32 entries                 */
/* ------------------------------------------------------------------ */
/* Gear icons are NOT global assets — they are theme assets loaded by
 * kh_ui_theme_assets_load(). The global loader must NOT attempt to load
 * gear_icon_normal.bmp or gear_icon_pressed.bmp from assets/common/. */

#define KH_GLOBAL_ASSET_COUNT 32

typedef struct _kh_global_asset_desc_t
{
	const char *name;       /* Semantic name for diagnostics */
	const char *path;       /* Full SD path */
	lv_img_dsc_t **target;  /* Pointer to struct field */
} kh_global_asset_desc_t;

/* Helper macro: offsetof for a named struct field */
#define KH_GLOBAL_FIELD(assets, field) \
	&((assets)->field)

static kh_global_asset_desc_t _kh_global_asset_table[KH_GLOBAL_ASSET_COUNT] = {

	/* Home screen (2) */
	{ "background",       KH_GLOBAL_PATH_BACKGROUND,     0 },  /* filled at runtime */
	{ "logo",             KH_GLOBAL_PATH_LOGO,           0 },

	/* PIN digit keys 0-9 normal/pressed (20) */
	{ "pin_0_n",          KH_GLOBAL_PATH_PIN_0_N,        0 },
	{ "pin_0_p",          KH_GLOBAL_PATH_PIN_0_P,        0 },
	{ "pin_1_n",          KH_GLOBAL_PATH_PIN_1_N,        0 },
	{ "pin_1_p",          KH_GLOBAL_PATH_PIN_1_P,        0 },
	{ "pin_2_n",          KH_GLOBAL_PATH_PIN_2_N,        0 },
	{ "pin_2_p",          KH_GLOBAL_PATH_PIN_2_P,        0 },
	{ "pin_3_n",          KH_GLOBAL_PATH_PIN_3_N,        0 },
	{ "pin_3_p",          KH_GLOBAL_PATH_PIN_3_P,        0 },
	{ "pin_4_n",          KH_GLOBAL_PATH_PIN_4_N,        0 },
	{ "pin_4_p",          KH_GLOBAL_PATH_PIN_4_P,        0 },
	{ "pin_5_n",          KH_GLOBAL_PATH_PIN_5_N,        0 },
	{ "pin_5_p",          KH_GLOBAL_PATH_PIN_5_P,        0 },
	{ "pin_6_n",          KH_GLOBAL_PATH_PIN_6_N,        0 },
	{ "pin_6_p",          KH_GLOBAL_PATH_PIN_6_P,        0 },
	{ "pin_7_n",          KH_GLOBAL_PATH_PIN_7_N,        0 },
	{ "pin_7_p",          KH_GLOBAL_PATH_PIN_7_P,        0 },
	{ "pin_8_n",          KH_GLOBAL_PATH_PIN_8_N,        0 },
	{ "pin_8_p",          KH_GLOBAL_PATH_PIN_8_P,        0 },
	{ "pin_9_n",          KH_GLOBAL_PATH_PIN_9_N,        0 },
	{ "pin_9_p",          KH_GLOBAL_PATH_PIN_9_P,        0 },

	/* PIN clear and OK buttons normal/pressed (4) */
	{ "pin_clear_n",      KH_GLOBAL_PATH_PIN_CLR_N,      0 },
	{ "pin_clear_p",      KH_GLOBAL_PATH_PIN_CLR_P,      0 },
	{ "pin_ok_n",         KH_GLOBAL_PATH_PIN_OK_N,       0 },
	{ "pin_ok_p",         KH_GLOBAL_PATH_PIN_OK_P,       0 },

	/* PIN display variants 0-4 (5) */
	{ "pin_display_0",    KH_GLOBAL_PATH_PIN_DISP_0,     0 },
	{ "pin_display_1",    KH_GLOBAL_PATH_PIN_DISP_1,     0 },
	{ "pin_display_2",    KH_GLOBAL_PATH_PIN_DISP_2,     0 },
	{ "pin_display_3",    KH_GLOBAL_PATH_PIN_DISP_3,     0 },
	{ "pin_display_4",    KH_GLOBAL_PATH_PIN_DISP_4,     0 },

	/* PIN diagnostic OK image (1) */
	{ "pin_diag_ok",      KH_GLOBAL_PATH_PIN_DIAG_OK,    0 },
};

/* Compile-time check: table must have exactly 32 entries */
_Static_assert(
	sizeof(_kh_global_asset_table) / sizeof(_kh_global_asset_table[0])
		== KH_GLOBAL_ASSET_COUNT,
	"global asset table must contain exactly 32 assets");

/* ------------------------------------------------------------------ */
/* Load all 32 global BMP assets — diagnostic instrumented            */
/* ------------------------------------------------------------------ */

bool kh_ui_global_assets_load(kh_ui_global_assets_t *assets)
{
	if (!assets)
		return false;

	/* Ensure all pointers start NULL */
	memset(assets, 0, sizeof(kh_ui_global_assets_t));

	/* Fill target pointers in the descriptor table */
	_kh_global_asset_table[0].target  = KH_GLOBAL_FIELD(assets, background);
	_kh_global_asset_table[1].target  = KH_GLOBAL_FIELD(assets, logo);
	/* PIN digits 0-9 normal (indices 2,4,6,8,10,12,14,16,18,20) */
	_kh_global_asset_table[2].target  = KH_GLOBAL_FIELD(assets, pin_digit_n[0]);
	_kh_global_asset_table[3].target  = KH_GLOBAL_FIELD(assets, pin_digit_p[0]);
	_kh_global_asset_table[4].target  = KH_GLOBAL_FIELD(assets, pin_digit_n[1]);
	_kh_global_asset_table[5].target  = KH_GLOBAL_FIELD(assets, pin_digit_p[1]);
	_kh_global_asset_table[6].target  = KH_GLOBAL_FIELD(assets, pin_digit_n[2]);
	_kh_global_asset_table[7].target  = KH_GLOBAL_FIELD(assets, pin_digit_p[2]);
	_kh_global_asset_table[8].target  = KH_GLOBAL_FIELD(assets, pin_digit_n[3]);
	_kh_global_asset_table[9].target  = KH_GLOBAL_FIELD(assets, pin_digit_p[3]);
	_kh_global_asset_table[10].target = KH_GLOBAL_FIELD(assets, pin_digit_n[4]);
	_kh_global_asset_table[11].target = KH_GLOBAL_FIELD(assets, pin_digit_p[4]);
	_kh_global_asset_table[12].target = KH_GLOBAL_FIELD(assets, pin_digit_n[5]);
	_kh_global_asset_table[13].target = KH_GLOBAL_FIELD(assets, pin_digit_p[5]);
	_kh_global_asset_table[14].target = KH_GLOBAL_FIELD(assets, pin_digit_n[6]);
	_kh_global_asset_table[15].target = KH_GLOBAL_FIELD(assets, pin_digit_p[6]);
	_kh_global_asset_table[16].target = KH_GLOBAL_FIELD(assets, pin_digit_n[7]);
	_kh_global_asset_table[17].target = KH_GLOBAL_FIELD(assets, pin_digit_p[7]);
	_kh_global_asset_table[18].target = KH_GLOBAL_FIELD(assets, pin_digit_n[8]);
	_kh_global_asset_table[19].target = KH_GLOBAL_FIELD(assets, pin_digit_p[8]);
	_kh_global_asset_table[20].target = KH_GLOBAL_FIELD(assets, pin_digit_n[9]);
	_kh_global_asset_table[21].target = KH_GLOBAL_FIELD(assets, pin_digit_p[9]);
	/* Clear/OK (4) */
	_kh_global_asset_table[22].target = KH_GLOBAL_FIELD(assets, pin_clear_n);
	_kh_global_asset_table[23].target = KH_GLOBAL_FIELD(assets, pin_clear_p);
	_kh_global_asset_table[24].target = KH_GLOBAL_FIELD(assets, pin_ok_n);
	_kh_global_asset_table[25].target = KH_GLOBAL_FIELD(assets, pin_ok_p);
	/* Display variants (5) */
	_kh_global_asset_table[26].target = KH_GLOBAL_FIELD(assets, pin_display[0]);
	_kh_global_asset_table[27].target = KH_GLOBAL_FIELD(assets, pin_display[1]);
	_kh_global_asset_table[28].target = KH_GLOBAL_FIELD(assets, pin_display[2]);
	_kh_global_asset_table[29].target = KH_GLOBAL_FIELD(assets, pin_display[3]);
	_kh_global_asset_table[30].target = KH_GLOBAL_FIELD(assets, pin_display[4]);
	/* Diagnostic */
	_kh_global_asset_table[31].target = KH_GLOBAL_FIELD(assets, pin_diag_ok);

	/* Load all 32 assets with diagnostic instrumentation */
	for (int i = 0; i < KH_GLOBAL_ASSET_COUNT; i++)
	{
		const kh_global_asset_desc_t *desc = &_kh_global_asset_table[i];

		kh_asset_load_status_t status =
			kh_asset_load_bmp_diagnostic(desc->path, desc->target);

		if (status != KH_ASSET_LOAD_OK)
		{
			/* Halt permanently — never returns */
			kh_diag_halt(KH_DBG_STAGE_GLOBAL, i,
			             desc->name, desc->path, status);
		}
	}

	/* All 32 loaded successfully */
	return true;
}

/* ------------------------------------------------------------------ */
/* Free all 32 global asset allocations                               */
/* ------------------------------------------------------------------ */

void kh_ui_global_assets_free(kh_ui_global_assets_t *assets)
{
	if (!assets)
		return;

	/* Home screen (2) */
	kh_asset_free(&assets->background);
	kh_asset_free(&assets->logo);

	/* PIN digit keys 0-9 normal/pressed (20) */
	for (int i = 0; i < 10; i++)
	{
		kh_asset_free(&assets->pin_digit_n[i]);
		kh_asset_free(&assets->pin_digit_p[i]);
	}

	/* PIN clear and OK buttons normal/pressed (4) */
	kh_asset_free(&assets->pin_clear_n);
	kh_asset_free(&assets->pin_clear_p);
	kh_asset_free(&assets->pin_ok_n);
	kh_asset_free(&assets->pin_ok_p);

	/* PIN display variants 0-4 (5) */
	for (int i = 0; i < 5; i++)
		kh_asset_free(&assets->pin_display[i]);

	/* PIN diagnostic OK image (1) */
	kh_asset_free(&assets->pin_diag_ok);
}
