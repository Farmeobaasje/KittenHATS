/*
 * Copyright (c) 2025 KittenHATS Contributors
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * KittenHATS — Settings screen.
 *
 * Layout (1280x720 logical LVGL space):
 *
 *   Container: 700x700 at (290, 10) — taller container for 420x160 controls
 *   ┌──────────────────────────────────────────────────────────────┐
 *   │                    ┌─────────────────┐                       │
 *   │                    │ settings_title   │  (500x120, y=20)     │
 *   │                    └─────────────────┘                       │
 *   │                                                              │
 *   │          ┌──────────────────────────────┐                    │
 *   │          │  TAAL (disabled, lv_img)      │  (420x160, y=160) │
 *   │          └──────────────────────────────┘                    │
 *   │                                                              │
 *   │          ┌──────────────────────────────┐                    │
 *   │          │  PIN (420x160, lv_imgbtn)     │  (y=340)          │
 *   │          └──────────────────────────────┘                    │
 *   │                                                              │
 *   │          ┌──────────────────────────────┐                    │
 *   │          │  TERUG (420x160, lv_imgbtn)   │  (y=520)          │
 *   │          └──────────────────────────────┘                    │
 *   └──────────────────────────────────────────────────────────────┘
 *
 * Button positions (x offsets within 700px container):
 *   All controls centered: x = (700 - 420) / 2 = 140
 *   20px vertical gap between controls (y=160, 340, 520)
 *
 * Ownership model:
 *   settings_container — root container for all Settings screen objects.
 *   Created once in kh_settings_screen_create(), hidden by default.
 *   Shown/hidden via kh_settings_show()/kh_settings_hide().
 *   No teardown — chainload reclaims all memory.
 *
 * Navigation:
 *   TAAL button: disabled (lv_img, no callback, no touch) — placeholder for P5-D
 *   PIN button:  requests TO_PIN transition
 *   TERUG:       requests TO_HOME transition
 *
 * Text rendering: all text via BMP images (lv_label font rendering broken).
 */

#include <string.h>
#include <stdlib.h>

#include <bdk.h>
#include <libs/lvgl/lvgl.h>

#include "screen_settings.h"
#include "screen_pin.h"
#include "ui_runtime.h"
#include "kh_ui_asset_context.h"

/* ------------------------------------------------------------------ */
/* Constants                                                          */
/* ------------------------------------------------------------------ */

/* Container geometry (700x700, taller for 420x160 controls) */
#define KH_SETTINGS_CONTAINER_W   700
#define KH_SETTINGS_CONTAINER_H   700
#define KH_SETTINGS_CONTAINER_X   290
#define KH_SETTINGS_CONTAINER_Y   10

/* Settings title: 500x120 */
#define KH_SETTINGS_TITLE_X       (KH_SETTINGS_CONTAINER_W - 500) / 2  /* 100 */
#define KH_SETTINGS_TITLE_Y       20

/* Control size: 420x160 (shared by TAAL, PIN, TERUG) */
#define KH_SETTINGS_CTRL_W         420
#define KH_SETTINGS_CTRL_H         160

/* Control x offset (centered in 700px container) */
#define KH_SETTINGS_CTRL_X         (KH_SETTINGS_CONTAINER_W - KH_SETTINGS_CTRL_W) / 2  /* 140 */

/* Control y positions (20px gap between each) */
#define KH_SETTINGS_TAAL_Y        160
#define KH_SETTINGS_PIN_Y         340
#define KH_SETTINGS_BACK_Y        520

/* ------------------------------------------------------------------ */
/* Static styles — persistent copies, never stack-allocated           */
/* ------------------------------------------------------------------ */

static lv_style_t kh_settings_bg_style;
static bool _kh_settings_styles_init = false;

/* ------------------------------------------------------------------ */
/* Static state                                                       */
/* ------------------------------------------------------------------ */

static lv_obj_t *_kh_settings_container = NULL;

/* ------------------------------------------------------------------ */
/* Button callbacks                                                    */
/* ------------------------------------------------------------------ */

/* TAAL button callback — request TO_LANG_SELECT transition */
static lv_res_t _kh_on_settings_taal_click(lv_obj_t *btn)
{
	(void)btn;
	kh_ui_request_transition(KH_UI_TRANSITION_TO_LANG_SELECT);
	return LV_RES_OK;
}

/* PIN button callback — request TO_PIN transition */
static lv_res_t _kh_on_settings_pin_click(lv_obj_t *btn)
{
	(void)btn;
	kh_ui_request_transition(KH_UI_TRANSITION_TO_PIN);
	return LV_RES_OK;
}

/* TERUG button callback — request TO_HOME transition */
static lv_res_t _kh_on_settings_back_click(lv_obj_t *btn)
{
	(void)btn;
	kh_ui_request_transition(KH_UI_TRANSITION_TO_HOME);
	return LV_RES_OK;
}

/* ------------------------------------------------------------------ */
/* Style initialisation — called once from kh_settings_screen_create()*/
/* ------------------------------------------------------------------ */

static void _kh_settings_init_styles(void)
{
	if (_kh_settings_styles_init)
		return;

	/* Background container style (dark charcoal, opaque, rounded) */
	lv_style_copy(&kh_settings_bg_style, &lv_style_plain);
	kh_settings_bg_style.body.main_color = LV_COLOR_HEX(0x111111);
	kh_settings_bg_style.body.grad_color = LV_COLOR_HEX(0x111111);
	kh_settings_bg_style.body.radius = 8;
	kh_settings_bg_style.body.border.color = LV_COLOR_HEX(0x333333);
	kh_settings_bg_style.body.border.width = 2;
	kh_settings_bg_style.body.border.part = LV_BORDER_FULL;
	kh_settings_bg_style.body.empty = 0;  /* Not transparent */

	_kh_settings_styles_init = true;
}

/* ------------------------------------------------------------------ */
/* Public API                                                         */
/* ------------------------------------------------------------------ */

bool kh_settings_screen_create(const kh_ui_asset_context_t *ctx)
{
	/* Initialise styles once */
	_kh_settings_init_styles();

	/* Create root container (fixed position, fixed size) */
	_kh_settings_container = lv_cont_create(lv_scr_act(), NULL);
	if (!_kh_settings_container)
		return false;

	lv_obj_set_size(_kh_settings_container, KH_SETTINGS_CONTAINER_W, KH_SETTINGS_CONTAINER_H);
	lv_obj_set_pos(_kh_settings_container, KH_SETTINGS_CONTAINER_X, KH_SETTINGS_CONTAINER_Y);
	lv_cont_set_layout(_kh_settings_container, LV_LAYOUT_OFF);

	/* Set background style */
	lv_obj_set_style(_kh_settings_container, &kh_settings_bg_style);

	/*
	 * Settings title — lv_img with settings_title.bmp (500x120).
	 * No lv_label — font rendering is broken in this Hekate LVGL build.
	 */
	if (ctx && ctx->theme.items[KH_THEME_ASSET_SETTINGS_TITLE])
	{
		lv_obj_t *title = lv_img_create(_kh_settings_container, NULL);
		if (title)
		{
			lv_img_set_src(title, ctx->theme.items[KH_THEME_ASSET_SETTINGS_TITLE]);
			lv_obj_set_pos(title, KH_SETTINGS_TITLE_X, KH_SETTINGS_TITLE_Y);
		}
	}

	/*
	 * TAAL button — lv_imgbtn with taal_button_normal/pressed.bmp (420x160).
	 * Requests TO_LANG_SELECT transition on click.
	 * P5-D: Changed from lv_img (disabled) to lv_imgbtn (interactive).
	 */
	if (ctx && ctx->theme.items[KH_THEME_ASSET_SETTINGS_LANGUAGE])
	{
		/*
		 * P5-D: TAAL button uses the same asset for both normal and pressed
		 * states (taal_button_normal.bmp) since there's no dedicated pressed
		 * variant. The button is still interactive — the pressed state shows
		 * the same image but the callback fires on click.
		 */
		lv_obj_t *taal = lv_imgbtn_create(_kh_settings_container, NULL);
		if (taal)
		{
			lv_imgbtn_set_src(taal, LV_BTN_STATE_REL, ctx->theme.items[KH_THEME_ASSET_SETTINGS_LANGUAGE]);
			lv_imgbtn_set_src(taal, LV_BTN_STATE_PR, ctx->theme.items[KH_THEME_ASSET_SETTINGS_LANGUAGE]);
			lv_imgbtn_set_action(taal, LV_BTN_ACTION_CLICK, _kh_on_settings_taal_click);
			lv_obj_set_pos(taal, KH_SETTINGS_CTRL_X, KH_SETTINGS_TAAL_Y);
			lv_obj_set_size(taal, KH_SETTINGS_CTRL_W, KH_SETTINGS_CTRL_H);
			/*
			 * Controlled Experiment 1 (v0.1.19): Force PR→REL state transition
			 * after final geometry is set. Same rationale as PIN button above.
			 */
			lv_imgbtn_set_state(taal, LV_BTN_STATE_PR);
			lv_imgbtn_set_state(taal, LV_BTN_STATE_REL);
			lv_obj_invalidate(taal);
		}
	}

	/*
	 * PIN button — lv_imgbtn with pin_button_normal/pressed.bmp (420x160).
	 * Requests TO_PIN transition on click.
	 */
	if (ctx && ctx->theme.items[KH_THEME_ASSET_SETTINGS_PIN_NORMAL]
		&& ctx->theme.items[KH_THEME_ASSET_SETTINGS_PIN_PRESSED])
	{
		lv_obj_t *pin = lv_imgbtn_create(_kh_settings_container, NULL);
		if (pin)
		{
			lv_imgbtn_set_src(pin, LV_BTN_STATE_REL, ctx->theme.items[KH_THEME_ASSET_SETTINGS_PIN_NORMAL]);
			lv_imgbtn_set_src(pin, LV_BTN_STATE_PR, ctx->theme.items[KH_THEME_ASSET_SETTINGS_PIN_PRESSED]);
			lv_imgbtn_set_action(pin, LV_BTN_ACTION_CLICK, _kh_on_settings_pin_click);
			lv_obj_set_pos(pin, KH_SETTINGS_CTRL_X, KH_SETTINGS_PIN_Y);
			lv_obj_set_size(pin, KH_SETTINGS_CTRL_W, KH_SETTINGS_CTRL_H);
			/*
			 * Controlled Experiment 1 (v0.1.19): Force PR→REL state transition
			 * after final geometry is set. refr_img() (called by lv_imgbtn_set_src)
			 * ran at position (0,0) with BMP dimensions. lv_obj_set_pos() and
			 * lv_obj_set_size() override the BMP size. The stale dirty area from
			 * refr_img() does not cover the final bounds. PR→REL forces refr_img()
			 * twice with actual state changes, leaving the object in its correct
			 * initial state with the dirty area covering the final position.
			 */
			lv_imgbtn_set_state(pin, LV_BTN_STATE_PR);
			lv_imgbtn_set_state(pin, LV_BTN_STATE_REL);
			lv_obj_invalidate(pin);
		}
	}

	/*
	 * TERUG button — lv_imgbtn with settings_back_normal/pressed.bmp (420x160).
	 * Uses dedicated Settings Back assets (not the shared back_button assets).
	 */
	if (ctx && ctx->theme.items[KH_THEME_ASSET_SETTINGS_BACK_NORMAL]
		&& ctx->theme.items[KH_THEME_ASSET_SETTINGS_BACK_PRESSED])
	{
		lv_obj_t *back = lv_imgbtn_create(_kh_settings_container, NULL);
		if (back)
		{
			lv_imgbtn_set_src(back, LV_BTN_STATE_REL, ctx->theme.items[KH_THEME_ASSET_SETTINGS_BACK_NORMAL]);
			lv_imgbtn_set_src(back, LV_BTN_STATE_PR, ctx->theme.items[KH_THEME_ASSET_SETTINGS_BACK_PRESSED]);
			lv_imgbtn_set_action(back, LV_BTN_ACTION_CLICK, _kh_on_settings_back_click);
			lv_obj_set_pos(back, KH_SETTINGS_CTRL_X, KH_SETTINGS_BACK_Y);
			lv_obj_set_size(back, KH_SETTINGS_CTRL_W, KH_SETTINGS_CTRL_H);
			/*
			 * Controlled Experiment 1 (v0.1.19): Force PR→REL state transition
			 * after final geometry is set. Same rationale as PIN button above.
			 */
			lv_imgbtn_set_state(back, LV_BTN_STATE_PR);
			lv_imgbtn_set_state(back, LV_BTN_STATE_REL);
			lv_obj_invalidate(back);
		}
	}

	/* Start hidden — shown only on TO_SETTINGS transition */
	lv_obj_set_hidden(_kh_settings_container, true);

	return true;
}

void kh_settings_show(void)
{
	if (!_kh_settings_container)
		return;

	lv_obj_set_hidden(_kh_settings_container, false);
}

void kh_settings_hide(void)
{
	if (!_kh_settings_container)
		return;

	lv_obj_set_hidden(_kh_settings_container, true);
}

/* ------------------------------------------------------------------ */
/* Destroy the Settings screen container                               */
/* ------------------------------------------------------------------ */

void kh_settings_destroy(void)
{
	if (_kh_settings_container)
	{
		lv_obj_del(_kh_settings_container);
		_kh_settings_container = NULL;
	}
}
