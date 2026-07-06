/*
 * Copyright (c) 2025 KittenHATS Contributors
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * KittenHATS — Language selection screen.
 *
 * Layout (1280x720 logical LVGL space):
 *
 *   Container: 960x700 at (160, 10) — wider for 2-column layout
 *   ┌──────────────────────────────────────────────────────────────┐
 *   │  ┌──────────────────────────────────────────────────────┐    │
 *   │  │  language_title (500x120, centered, y=20)            │    │
 *   │  └──────────────────────────────────────────────────────┘    │
 *   │                                                              │
 *   │  ┌──────────────┐    ┌──────────────┐                       │
 *   │  │  NL (444x184) │    │  EN (444x184) │                       │
 *   │  │  x=24, y=110  │    │  x=492, y=110 │                       │
 *   │  └──────────────┘    └──────────────┘                       │
 *   │                                                              │
 *   │  ┌──────────────┐    ┌──────────────┐                       │
 *   │  │  DE (444x184) │    │  ES (444x184) │                       │
 *   │  │  x=24, y=300  │    │  x=492, y=300 │                       │
 *   │  └──────────────┘    └──────────────┘                       │
 *   │                                                              │
 *   │  ┌──────────────┐    ┌──────────────┐                       │
 *   │  │  FR (444x184) │    │  TERUG        │                       │
 *   │  │  x=24, y=490  │    │  x=492, y=490 │                       │
 *   │  └──────────────┘    └──────────────┘                       │
 *   └──────────────────────────────────────────────────────────────┘
 *
 * Button positions (x offsets within 960px container):
 *   Left column:  x = 24
 *   Right column: x = 492  (24 + 444 + 24 = 492)
 *   Row 1: y=110  (NL left, EN right)
 *   Row 2: y=300  (DE left, ES right)
 *   Row 3: y=490  (FR left, TERUG right)
 *   6px gap between rows (110+184=294, 300-294=6)
 *   26px bottom margin (490+184=674, 700-674=26)
 *
 * Ownership model:
 *   lang_container — root container for all Language screen objects.
 *   Created once in kh_lang_screen_create(), hidden by default.
 *   Shown/hidden via kh_lang_show()/kh_lang_hide().
 *   No teardown — chainload reclaims all memory.
 *
 * Navigation:
 *   Language buttons (NL/EN/DE/ES/FR): request LANG_CHANGED transition
 *     with the selected language. The main loop handles the reload.
 *   TERUG: requests TO_SETTINGS transition.
 *
 * Text rendering: all text via BMP images (lv_label font rendering broken).
 * Language buttons (NL/EN/DE/ES/FR) are language-independent — they show
 * the same labels regardless of active language.
 *
 * BMP dimensions (all language buttons): 444x184 — verified against assets.
 * lv_imgbtn.c refr_img() ALWAYS overrides object size to BMP dimensions
 * (line 380: lv_obj_set_size(imgbtn, header.w, header.h)), so the declared
 * size constants are informational only — the actual rendered size is 444x184.
 */

#include <string.h>
#include <stdlib.h>

#include <bdk.h>
#include <libs/lvgl/lvgl.h>

#include "screen_lang.h"
#include "ui_runtime.h"
#include "kh_ui_asset_context.h"

/* ------------------------------------------------------------------ */
/* Constants                                                          */
/* ------------------------------------------------------------------ */

/*
 * Container geometry: 960x700 at (160, 10)
 * Centered on 1280px screen: (1280 - 960) / 2 = 160
 * 700px height matches Settings screen container.
 */
#define KH_LANG_CONTAINER_W   960
#define KH_LANG_CONTAINER_H   700
#define KH_LANG_CONTAINER_X   160
#define KH_LANG_CONTAINER_Y   10

/*
 * Language title: 500x120, centered in 960px container
 * x = (960 - 500) / 2 = 230
 */
#define KH_LANG_TITLE_X       230
#define KH_LANG_TITLE_Y       20

/*
 * BMP dimensions (all language buttons): 444x184
 * lv_imgbtn.c refr_img() ALWAYS overrides object size to BMP dimensions,
 * so these constants document the actual rendered size.
 * The old 420x160 values were incorrect — they were overwritten by refr_img().
 */
#define KH_LANG_BTN_W         444
#define KH_LANG_BTN_H         184

/*
 * 2-column layout within 960px container:
 *   Left column:  x = 24
 *   Right column: x = 492  (24 + 444 + 24 = 492)
 *   24px left margin, 24px between columns, 24px right margin
 *   Total: 24 + 444 + 24 + 444 + 24 = 960 ✓
 */
#define KH_LANG_COL_LEFT      24
#define KH_LANG_COL_RIGHT     492

/*
 * Row positions (3 rows, 6px gap between rows):
 *   Row 1: y=110  (110 + 184 = 294)
 *   Row 2: y=300  (300 + 184 = 484)
 *   Row 3: y=490  (490 + 184 = 674)
 *   6px gap: 300 - 294 = 6, 490 - 484 = 6
 *   26px bottom margin: 700 - 674 = 26
 */
#define KH_LANG_ROW1_Y        110
#define KH_LANG_ROW2_Y        300
#define KH_LANG_ROW3_Y        490

/* ------------------------------------------------------------------ */
/* Static styles — persistent copies, never stack-allocated           */
/* ------------------------------------------------------------------ */

static lv_style_t kh_lang_bg_style;
static bool _kh_lang_styles_init = false;

/* ------------------------------------------------------------------ */
/* Static state                                                       */
/* ------------------------------------------------------------------ */

static lv_obj_t *_kh_lang_container = NULL;

/* ------------------------------------------------------------------ */
/* Button callbacks                                                    */
/* ------------------------------------------------------------------ */

/* Language button callback — request language change via runtime */
static lv_res_t _kh_on_lang_nl_click(lv_obj_t *btn)
{
	(void)btn;
	kh_ui_request_language_change(KH_LANG_NL);
	return LV_RES_OK;
}

static lv_res_t _kh_on_lang_en_click(lv_obj_t *btn)
{
	(void)btn;
	kh_ui_request_language_change(KH_LANG_EN);
	return LV_RES_OK;
}

static lv_res_t _kh_on_lang_de_click(lv_obj_t *btn)
{
	(void)btn;
	kh_ui_request_language_change(KH_LANG_DE);
	return LV_RES_OK;
}

static lv_res_t _kh_on_lang_es_click(lv_obj_t *btn)
{
	(void)btn;
	kh_ui_request_language_change(KH_LANG_ES);
	return LV_RES_OK;
}

static lv_res_t _kh_on_lang_fr_click(lv_obj_t *btn)
{
	(void)btn;
	kh_ui_request_language_change(KH_LANG_FR);
	return LV_RES_OK;
}


/* TERUG button callback — request TO_SETTINGS transition */
static lv_res_t _kh_on_lang_back_click(lv_obj_t *btn)
{
	(void)btn;
	kh_ui_request_transition(KH_UI_TRANSITION_TO_SETTINGS);
	return LV_RES_OK;
}

/* ------------------------------------------------------------------ */
/* Style initialisation — called once from kh_lang_screen_create()    */
/* ------------------------------------------------------------------ */

static void _kh_lang_init_styles(void)
{
	if (_kh_lang_styles_init)
		return;

	/* Background container style (dark charcoal, opaque, rounded) */
	lv_style_copy(&kh_lang_bg_style, &lv_style_plain);
	kh_lang_bg_style.body.main_color = LV_COLOR_HEX(0x111111);
	kh_lang_bg_style.body.grad_color = LV_COLOR_HEX(0x111111);
	kh_lang_bg_style.body.radius = 8;
	kh_lang_bg_style.body.border.color = LV_COLOR_HEX(0x333333);
	kh_lang_bg_style.body.border.width = 2;
	kh_lang_bg_style.body.border.part = LV_BORDER_FULL;
	kh_lang_bg_style.body.empty = 0;  /* Not transparent */

	_kh_lang_styles_init = true;
}

/* ------------------------------------------------------------------ */
/* Helper: create a language button with PR→REL fix                   */
/* ------------------------------------------------------------------ */

static lv_obj_t *_kh_lang_create_btn(lv_obj_t *parent,
	lv_img_dsc_t *normal, lv_img_dsc_t *pressed,
	s32 x, s32 y, lv_res_t (*callback)(lv_obj_t *))
{
	lv_obj_t *btn = lv_imgbtn_create(parent, NULL);
	if (!btn)
		return NULL;

	lv_imgbtn_set_src(btn, LV_BTN_STATE_REL, normal);
	lv_imgbtn_set_src(btn, LV_BTN_STATE_PR, pressed);
	lv_imgbtn_set_action(btn, LV_BTN_ACTION_CLICK, callback);
	lv_obj_set_pos(btn, x, y);
	/*
	 * Note: lv_obj_set_size() is immediately overwritten by refr_img()
	 * (lv_imgbtn.c line 380) which sets size to BMP dimensions (444x184).
	 * We still call it for documentation purposes — the actual rendered
	 * size is always the BMP dimensions.
	 */
	lv_obj_set_size(btn, KH_LANG_BTN_W, KH_LANG_BTN_H);

	/*
	 * Controlled Experiment 1 (v0.1.19): Force PR→REL state transition
	 * after final geometry is set. Same rationale as Settings screen.
	 */
	lv_imgbtn_set_state(btn, LV_BTN_STATE_PR);
	lv_imgbtn_set_state(btn, LV_BTN_STATE_REL);
	lv_obj_invalidate(btn);

	return btn;
}

/* ------------------------------------------------------------------ */
/* Public API                                                         */
/* ------------------------------------------------------------------ */

bool kh_lang_screen_create(const kh_ui_asset_context_t *ctx)
{
	/* Initialise styles once */
	_kh_lang_init_styles();

	/* Create root container (fixed position, fixed size) */
	_kh_lang_container = lv_cont_create(lv_scr_act(), NULL);
	if (!_kh_lang_container)
		return false;

	lv_obj_set_size(_kh_lang_container, KH_LANG_CONTAINER_W, KH_LANG_CONTAINER_H);
	lv_obj_set_pos(_kh_lang_container, KH_LANG_CONTAINER_X, KH_LANG_CONTAINER_Y);
	lv_cont_set_layout(_kh_lang_container, LV_LAYOUT_OFF);

	/* Set background style */
	lv_obj_set_style(_kh_lang_container, &kh_lang_bg_style);

	/*
	 * Language title — lv_img with language_title.bmp (500x120).
	 * No lv_label — font rendering is broken in this Hekate LVGL build.
	 */
	if (ctx && ctx->theme.items[KH_THEME_ASSET_LANG_TITLE])
	{
		lv_obj_t *title = lv_img_create(_kh_lang_container, NULL);
		if (title)
		{
			lv_img_set_src(title, ctx->theme.items[KH_THEME_ASSET_LANG_TITLE]);
			lv_obj_set_pos(title, KH_LANG_TITLE_X, KH_LANG_TITLE_Y);
		}
	}

	/*
	 * NL button — left column, row 1.
	 * lv_imgbtn with nl_button_normal/pressed.bmp (444x184).
	 */
	if (ctx && ctx->theme.items[KH_THEME_ASSET_LANG_NL_NORMAL]
		&& ctx->theme.items[KH_THEME_ASSET_LANG_NL_PRESSED])
	{
		_kh_lang_create_btn(_kh_lang_container,
			ctx->theme.items[KH_THEME_ASSET_LANG_NL_NORMAL],
			ctx->theme.items[KH_THEME_ASSET_LANG_NL_PRESSED],
			KH_LANG_COL_LEFT, KH_LANG_ROW1_Y,
			_kh_on_lang_nl_click);
	}

	/*
	 * EN button — right column, row 1.
	 * lv_imgbtn with en_button_normal/pressed.bmp (444x184).
	 */
	if (ctx && ctx->theme.items[KH_THEME_ASSET_LANG_EN_NORMAL]
		&& ctx->theme.items[KH_THEME_ASSET_LANG_EN_PRESSED])
	{
		_kh_lang_create_btn(_kh_lang_container,
			ctx->theme.items[KH_THEME_ASSET_LANG_EN_NORMAL],
			ctx->theme.items[KH_THEME_ASSET_LANG_EN_PRESSED],
			KH_LANG_COL_RIGHT, KH_LANG_ROW1_Y,
			_kh_on_lang_en_click);
	}

	/*
	 * DE button — left column, row 2.
	 * lv_imgbtn with de_button_normal/pressed.bmp (444x184).
	 */
	if (ctx && ctx->theme.items[KH_THEME_ASSET_LANG_DE_NORMAL]
		&& ctx->theme.items[KH_THEME_ASSET_LANG_DE_PRESSED])
	{
		_kh_lang_create_btn(_kh_lang_container,
			ctx->theme.items[KH_THEME_ASSET_LANG_DE_NORMAL],
			ctx->theme.items[KH_THEME_ASSET_LANG_DE_PRESSED],
			KH_LANG_COL_LEFT, KH_LANG_ROW2_Y,
			_kh_on_lang_de_click);
	}

	/*
	 * ES button — right column, row 2.
	 * lv_imgbtn with es_button_normal/pressed.bmp (444x184).
	 */
	if (ctx && ctx->theme.items[KH_THEME_ASSET_LANG_ES_NORMAL]
		&& ctx->theme.items[KH_THEME_ASSET_LANG_ES_PRESSED])
	{
		_kh_lang_create_btn(_kh_lang_container,
			ctx->theme.items[KH_THEME_ASSET_LANG_ES_NORMAL],
			ctx->theme.items[KH_THEME_ASSET_LANG_ES_PRESSED],
			KH_LANG_COL_RIGHT, KH_LANG_ROW2_Y,
			_kh_on_lang_es_click);
	}

	/*
	 * FR button — left column, row 3.
	 * lv_imgbtn with fr_button_normal/pressed.bmp (444x184).
	 */
	if (ctx && ctx->theme.items[KH_THEME_ASSET_LANG_FR_NORMAL]
		&& ctx->theme.items[KH_THEME_ASSET_LANG_FR_PRESSED])
	{
		_kh_lang_create_btn(_kh_lang_container,
			ctx->theme.items[KH_THEME_ASSET_LANG_FR_NORMAL],
			ctx->theme.items[KH_THEME_ASSET_LANG_FR_PRESSED],
			KH_LANG_COL_LEFT, KH_LANG_ROW3_Y,
			_kh_on_lang_fr_click);
	}

	/*
	 * TERUG button — right column, row 3.
	 * lv_imgbtn with lang_back_normal/pressed.bmp (444x184).
	 */
	if (ctx && ctx->theme.items[KH_THEME_ASSET_LANG_BACK_NORMAL]
		&& ctx->theme.items[KH_THEME_ASSET_LANG_BACK_PRESSED])
	{
		_kh_lang_create_btn(_kh_lang_container,
			ctx->theme.items[KH_THEME_ASSET_LANG_BACK_NORMAL],
			ctx->theme.items[KH_THEME_ASSET_LANG_BACK_PRESSED],
			KH_LANG_COL_RIGHT, KH_LANG_ROW3_Y,
			_kh_on_lang_back_click);
	}

	/* Start hidden — shown only on TO_LANG_SELECT transition */
	lv_obj_set_hidden(_kh_lang_container, true);

	return true;
}

void kh_lang_show(void)
{
	if (!_kh_lang_container)
		return;

	lv_obj_set_hidden(_kh_lang_container, false);
}

void kh_lang_hide(void)
{
	if (!_kh_lang_container)
		return;

	lv_obj_set_hidden(_kh_lang_container, true);
}

void kh_lang_destroy(void)
{
	if (_kh_lang_container)
	{
		lv_obj_del(_kh_lang_container);
		_kh_lang_container = NULL;
	}
}
