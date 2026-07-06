/*
 * Copyright (c) 2025 KittenHATS Contributors
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * KittenHATS — Home screen UI.
 *
 * Layout (1280x720 logical LVGL space):
 *
 *   ┌──────────────────────────────────────────────────────────────┐
 *   │                                              ┌─────────┐    │
 *   │  24px marge                                 │ Gear    │    │
 *   │                                              │ 128x128 │    │
 *   │                                              │ y=24    │    │
 *   │                                              │ x=1128  │    │
 *   │  ┌─────────────────────────────────┐         └─────────┘    │
 *   │  │  Logo 500x180 at (390, 80)      │                        │
 *   │  └─────────────────────────────────┘                        │
 *   │                                                              │
 *   │  ┌─────────────────────────────────┐                        │
 *   │  │  START 500x260 at (390, 340)    │                        │
 *   │  └─────────────────────────────────┘                        │
 *   │                                                              │
 *   │                         24px ondermarge                      │
 *   └──────────────────────────────────────────────────────────────┘
 *
 * Ownership model:
 *   kh_home_view_t — root container + all LVGL objects (children of root).
 *   Cleanup: delete root once → all children auto-deleted.
 *   kh_ui_asset_context_t — global + theme assets (separate allocations).
 *   Cleanup: free each set individually via kh_ui_global_assets_free()
 *   and kh_ui_theme_assets_free().
 *
 * Refactor C: Transactional asset loading. All 32 global + 13 theme
 * assets must load before any screen is created. On failure, all
 * partial allocations are freed and the function returns NONE.
 *
 * No chainload.h include — UI returns kh_ui_action_t, main.c translates.
 */

#include <string.h>
#include <stdlib.h>

#include <bdk.h>
#include <display/di.h>
#include <libs/lvgl/lvgl.h>
#include <storage/sd.h>

#include "screen_home.h"

#include "screen_pin.h"
#include "screen_settings.h"
#include "screen_lang.h"
#include "ui_runtime.h"
#include "kh_ui_asset_context.h"
#include "config/kh_config.h"
#include "config/kh_ui_config.h"

/* ------------------------------------------------------------------ */
/* DEBUG: Stage-specific error colors for hardware diagnostics        */
/* ------------------------------------------------------------------ */
/* These are shown via display_color_screen() before returning NONE.  */
/* A8B8G8R8 format (Tegra X1 native).                                */
/*                                                                    */
/* RED    = global asset load failed (kh_ui_global_assets_load)       */
/* ORANGE = theme path resolver or theme asset load failed            */
/* PURPLE = Home UI creation failed (_kh_create_ui)                   */
/* BLUE   = PIN screen creation failed (kh_pin_screen_create)         */
/* CYAN   = Settings screen creation failed (kh_settings_screen_create)*/
/* GREEN  = all assets and screens successful (never shown, success)  */
/*                                                                    */
/* These are DEBUG ONLY — remove when root cause is found.           */
/* ------------------------------------------------------------------ */
/* NOTE: These colors must be DISTINCT from main.c's error colors:
 *   main.c red    = 0xFF0000FF
 *   main.c purple = 0xFFFF00FF
 *   main.c yellow = 0xFF00FFFF
 *   main.c cyan   = 0xFFFFFF00
 *   main.c blue   = 0xFFFF0000
 *   main.c orange = 0xFF2288FF
 *
 * Our diagnostic colors use different values to avoid ambiguity.
 */
#define KH_DBG_RED    0xFF002288  /* Distinct from main.c red (0xFF0000FF) — dark red */

#define KH_DBG_ORANGE 0xFF8822FF  /* Distinct from main.c orange (0xFF2288FF) */
#define KH_DBG_PURPLE 0xFFFF00FF  /* Same as main.c purple — OK, third stage */
#define KH_DBG_BLUE   0xFF0000AA  /* Distinct from main.c blue (0xFFFF0000) */
#define KH_DBG_CYAN   0xFF00AAAA  /* Distinct from main.c cyan (0xFFFFFF00) */
#define KH_DBG_GREEN  0xFF00FF00



/* ------------------------------------------------------------------ */
/* Constants                                                          */
/* ------------------------------------------------------------------ */

/* Layout positions (LVGL logical 1280x720) */
#define KH_LOGO_X       390
#define KH_LOGO_Y       80
#define KH_LOGO_W       500
#define KH_LOGO_H       180

#define KH_START_X      390
#define KH_START_Y      340
#define KH_START_W      500
#define KH_START_H      260

#define KH_GEAR_X       1128
#define KH_GEAR_Y       24
#define KH_GEAR_W       128
#define KH_GEAR_H       128

#define KH_MARGIN       24

/*
 * Focus indicator: 4px subtle border around the focused button.
 * Color: semi-transparent white (A8B8G8R8 format for Tegra X1).
 * 0x80FFFFFF = 50% alpha white — subtle, visible on any background.
 * The alpha byte (MSB) controls opacity: 0x00 = fully transparent,
 * 0xFF = fully opaque. 0x80 = ~50% opacity.
 */
#define KH_FOCUS_WIDTH  4
#define KH_FOCUS_COLOR  0x80FFFFFF  /* Semi-transparent white */

/*
 * Build A — display orientation diagnostic labels.
 * Corner labels at LVGL logical positions (1280x720):
 *   TL = (0, 0)     TR = (1240, 0)
 *   BL = (0, 680)   BR = (1240, 680)
 * Arrow at top/bottom center:
 *   TOP    = (640, 10)
 *   BOTTOM = (640, 690)
 */
#define KH_DIAG_LABEL_W    40
#define KH_DIAG_LABEL_H    40
#define KH_DIAG_ARROW_W    80
#define KH_DIAG_ARROW_H    30

/* ------------------------------------------------------------------ */
/* Home view — all LVGL objects for the home screen                   */
/* ------------------------------------------------------------------ */
typedef struct _kh_home_view_t
{
	lv_obj_t *root;          /* Container for all children */

	/* Image-based objects (may be NULL if asset failed to load) */
	lv_obj_t *background;    /* Full-screen background image */
	lv_obj_t *logo;          /* KittenHATS logo image */

	/* Start button: imgbtn if assets available, else plain btn + label */
	lv_obj_t *btn_start;     /* Start button (imgbtn or plain btn) */
	lv_obj_t *label_start;   /* Fallback label (NULL if imgbtn used) */

	/* Gear button: imgbtn if assets available, else plain btn + label */
	lv_obj_t *btn_gear;      /* Gear button (imgbtn or plain btn) */
	lv_obj_t *label_gear;    /* Fallback label (NULL if imgbtn used) */

	/* Focus indicator: colored rectangle behind the focused button */
	lv_obj_t *focus_frame;   /* Focus highlight rectangle */

#ifdef KH_DEBUG
	/* Diagnostic corner labels and arrows (only when KH_DEBUG defined) */
	lv_obj_t *label_tl;      /* Top-left: "TL" */
	lv_obj_t *label_tr;      /* Top-right: "TR" */
	lv_obj_t *label_bl;      /* Bottom-left: "BL" */
	lv_obj_t *label_br;      /* Bottom-right: "BR" */
	lv_obj_t *label_top;     /* Top center: "TOP" */
	lv_obj_t *label_bottom;  /* Bottom center: "BOTTOM" */
#endif
} kh_home_view_t;

/* ------------------------------------------------------------------ */
/* Static state                                                       */
/* ------------------------------------------------------------------ */

static kh_home_view_t _kh_view;
static kh_ui_asset_context_t _kh_ctx;

/* ------------------------------------------------------------------ */
/* Forward declarations                                               */
/* ------------------------------------------------------------------ */

static bool _kh_create_ui(void);
static lv_obj_t *_kh_create_imgbtn(lv_obj_t *parent, lv_img_dsc_t *normal,
	lv_img_dsc_t *pressed, s32 x, s32 y, kh_ui_action_t action);
static lv_obj_t *_kh_create_plain_btn(lv_obj_t *parent, const char *label,
	s32 x, s32 y, s32 w, s32 h, kh_ui_action_t action);

#ifdef KH_DEBUG
/* ------------------------------------------------------------------ */
/* Diagnostic label helper — creates a colored label at (x,y)         */
/* ------------------------------------------------------------------ */

static lv_obj_t *_kh_create_diag_label(lv_obj_t *parent, const char *text,
	s32 x, s32 y, lv_color_t color)
{
	lv_obj_t *label = lv_label_create(parent, NULL);
	if (!label)
		return NULL;

	lv_label_set_text(label, text);
	lv_obj_set_pos(label, x, y);

	/* Set text color via style */
	lv_style_t *style = lv_obj_get_style(label);
	if (style)
	{
		style->text.color = color;
	}

	return label;
}
#endif

/* ------------------------------------------------------------------ */
/* Button callbacks — request action via first-action-wins mechanism  */
/* ------------------------------------------------------------------ */

static lv_res_t _kh_on_start_click(lv_obj_t *btn)
{
	(void)btn;
	kh_ui_request_action(KH_UI_ACTION_KIDS);
	return LV_RES_OK;
}

static lv_res_t _kh_on_gear_click(lv_obj_t *btn)
{
	(void)btn;
	kh_ui_request_transition(KH_UI_TRANSITION_TO_SETTINGS);
	return LV_RES_OK;
}


/* ------------------------------------------------------------------ */
/* Public API: focus indicator control                                 */
/* ------------------------------------------------------------------ */

void kh_home_set_focus_visible(bool visible)
{
	if (!_kh_view.focus_frame)
		return;

	lv_obj_set_hidden(_kh_view.focus_frame, !visible);
}

void kh_home_set_focus(kh_jc_focus_t focus)
{
	if (!_kh_view.root)
		return;

	/* Determine which button to highlight */
	lv_obj_t *target = (focus == KH_JC_FOCUS_START)
		? _kh_view.btn_start : _kh_view.btn_gear;

	if (!target || !_kh_view.focus_frame)
		return;

	/* Position focus frame behind the target button */
	lv_area_t area;
	area.x1 = target->coords.x1 - KH_FOCUS_WIDTH;
	area.y1 = target->coords.y1 - KH_FOCUS_WIDTH;
	area.x2 = target->coords.x2 + KH_FOCUS_WIDTH;
	area.y2 = target->coords.y2 + KH_FOCUS_WIDTH;

	lv_obj_set_pos(_kh_view.focus_frame, area.x1, area.y1);
	lv_obj_set_size(_kh_view.focus_frame,
		area.x2 - area.x1 + 1, area.y2 - area.y1 + 1);

	/* Move focus frame to just behind the target in z-order */
	lv_obj_set_top(_kh_view.focus_frame, true);
	lv_obj_set_top(target, true);  /* Put target on top of frame */
}

/* ------------------------------------------------------------------ */
/* UI creation                                                        */
/* ------------------------------------------------------------------ */

static bool _kh_create_ui(void)
{
	memset(&_kh_view, 0, sizeof(kh_home_view_t));

	/* Create root container (full screen) */
	_kh_view.root = lv_cont_create(lv_scr_act(), NULL);
	if (!_kh_view.root)
		return false;

	lv_obj_set_size(_kh_view.root, LV_HOR_RES, LV_VER_RES);
	lv_obj_set_pos(_kh_view.root, 0, 0);
	lv_cont_set_layout(_kh_view.root, LV_LAYOUT_OFF);

	/* 1. Background image (full screen) */
	if (_kh_ctx.global.background)
	{
		_kh_view.background = lv_img_create(_kh_view.root, NULL);
		if (_kh_view.background)
		{
			lv_img_set_src(_kh_view.background, _kh_ctx.global.background);
			lv_obj_set_pos(_kh_view.background, 0, 0);
		}
	}

	/* 2. Logo */
	if (_kh_ctx.global.logo)
	{
		_kh_view.logo = lv_img_create(_kh_view.root, NULL);
		if (_kh_view.logo)
		{
			lv_img_set_src(_kh_view.logo, _kh_ctx.global.logo);
			lv_obj_set_pos(_kh_view.logo, KH_LOGO_X, KH_LOGO_Y);
		}
	}

	/* 3. Focus indicator frame (created BEFORE buttons so it's behind them) */
	_kh_view.focus_frame = lv_obj_create(_kh_view.root, NULL);
	if (_kh_view.focus_frame)
	{
		lv_obj_set_size(_kh_view.focus_frame, KH_START_W + KH_FOCUS_WIDTH * 2,
			KH_START_H + KH_FOCUS_WIDTH * 2);
		lv_obj_set_pos(_kh_view.focus_frame,
			KH_START_X - KH_FOCUS_WIDTH, KH_START_Y - KH_FOCUS_WIDTH);
		lv_obj_set_style(_kh_view.focus_frame, &lv_style_plain);
		/* Set background color to semi-transparent white */
		lv_style_t *style = lv_obj_get_style(_kh_view.focus_frame);
		if (style)
		{
			style->body.main_color = LV_COLOR_WHITE;
			style->body.grad_color = LV_COLOR_WHITE;
			/* Set opacity on the object itself */
			lv_obj_set_opa_scale(_kh_view.focus_frame, LV_OPA_50);
		}

		/* Enable opacity scaling, then set to 50% */
		lv_obj_set_opa_scale_enable(_kh_view.focus_frame, true);
		lv_obj_set_opa_scale(_kh_view.focus_frame, LV_OPA_50);

		/* Start hidden — only shown after Joy-Con D-pad input */
		lv_obj_set_hidden(_kh_view.focus_frame, true);
	}

	/* 4. Start button (SPELEN) — use theme assets via semantic IDs */
	{
		lv_img_dsc_t *start_n = _kh_ctx.theme.items[KH_THEME_ASSET_START_NORMAL];
		lv_img_dsc_t *start_p = _kh_ctx.theme.items[KH_THEME_ASSET_START_PRESSED];
		if (start_n && start_p)
		{
			_kh_view.btn_start = _kh_create_imgbtn(_kh_view.root,
				start_n, start_p,
				KH_START_X, KH_START_Y, KH_UI_ACTION_KIDS);
		}
		else
		{
			_kh_view.btn_start = _kh_create_plain_btn(_kh_view.root,
				"SPELEN", KH_START_X, KH_START_Y,
				KH_START_W, KH_START_H, KH_UI_ACTION_KIDS);
		}
	}

	/* 5. Gear button (tandwiel) — use theme assets via semantic IDs */
	{
		lv_img_dsc_t *gear_n = _kh_ctx.theme.items[KH_THEME_ASSET_GEAR_NORMAL];
		lv_img_dsc_t *gear_p = _kh_ctx.theme.items[KH_THEME_ASSET_GEAR_PRESSED];
		if (gear_n && gear_p)
		{
			_kh_view.btn_gear = _kh_create_imgbtn(_kh_view.root,
				gear_n, gear_p,
				KH_GEAR_X, KH_GEAR_Y, KH_UI_ACTION_NONE);
		}
		else
		{
			_kh_view.btn_gear = _kh_create_plain_btn(_kh_view.root,
				"\u2699", KH_GEAR_X, KH_GEAR_Y,
				KH_GEAR_W, KH_GEAR_H, KH_UI_ACTION_NONE);
		}
	}

	/* Reset both buttons to REL state for a fresh session */
	if (_kh_view.btn_start)
		lv_imgbtn_set_state(_kh_view.btn_start, LV_BTN_STATE_REL);
	if (_kh_view.btn_gear)
		lv_imgbtn_set_state(_kh_view.btn_gear, LV_BTN_STATE_REL);

#ifdef KH_DEBUG
	/* 6. Diagnostic corner labels and arrows (only when KH_DEBUG defined) */
	/* Use LV_HOR_RES (1280) and LV_VER_RES (720) from LVGL globals */
	_kh_view.label_tl = _kh_create_diag_label(_kh_view.root,
		"TL", 0, 0, LV_COLOR_RED);
	_kh_view.label_tr = _kh_create_diag_label(_kh_view.root,
		"TR", LV_HOR_RES - KH_DIAG_LABEL_W, 0, LV_COLOR_GREEN);
	_kh_view.label_bl = _kh_create_diag_label(_kh_view.root,
		"BL", 0, LV_VER_RES - KH_DIAG_LABEL_H, LV_COLOR_BLUE);
	_kh_view.label_br = _kh_create_diag_label(_kh_view.root,
		"BR", LV_HOR_RES - KH_DIAG_LABEL_W,
		LV_VER_RES - KH_DIAG_LABEL_H, LV_COLOR_YELLOW);

	/* Top/bottom arrows */
	_kh_view.label_top = _kh_create_diag_label(_kh_view.root,
		"TOP", (LV_HOR_RES - KH_DIAG_ARROW_W) / 2, 10, LV_COLOR_WHITE);
	_kh_view.label_bottom = _kh_create_diag_label(_kh_view.root,
		"BOTTOM", (LV_HOR_RES - KH_DIAG_ARROW_W) / 2,
		LV_VER_RES - KH_DIAG_ARROW_H - 10, LV_COLOR_WHITE);
#endif

	return (_kh_view.btn_start != NULL && _kh_view.btn_gear != NULL);
}

static lv_obj_t *_kh_create_imgbtn(lv_obj_t *parent, lv_img_dsc_t *normal,
	lv_img_dsc_t *pressed, s32 x, s32 y, kh_ui_action_t action)
{
	lv_obj_t *btn = lv_imgbtn_create(parent, NULL);
	if (!btn)
		return NULL;

	/* Set images for REL and PR states */
	lv_imgbtn_set_src(btn, LV_BTN_STATE_REL, normal);
	lv_imgbtn_set_src(btn, LV_BTN_STATE_PR, pressed);

	/* Set action based on which button this is */
	if (action == KH_UI_ACTION_KIDS)
		lv_imgbtn_set_action(btn, LV_BTN_ACTION_CLICK, _kh_on_start_click);
	else
		lv_imgbtn_set_action(btn, LV_BTN_ACTION_CLICK, _kh_on_gear_click);

	lv_obj_set_pos(btn, x, y);

	/*
	 * Controlled Experiment 1 (v0.1.19): Force PR→REL state transition
	 * after final geometry is set. refr_img() (called by lv_imgbtn_set_src)
	 * ran at position (0,0) with BMP dimensions. lv_obj_set_pos() moves
	 * the object but the stale dirty area from refr_img() does not cover
	 * the final bounds. lv_btn_set_state() has a guard:
	 *   if(ext->state != state) — REL→REL is a NO-OP.
	 * PR→REL forces refr_img() twice with actual state changes:
	 *   PR: reads ext->img_src[PR], sets size to BMP dims, invalidates
	 *   REL: reads ext->img_src[REL], sets size to BMP dims, invalidates
	 * The second call (REL) leaves the object in its correct initial state
	 * with the dirty area covering the final position.
	 *
	 * Hardware evidence (v0.1.18): theme lv_imgbtn initial REL rendering
	 * is corrupted; first state transition repairs that individual object.
	 * Global imgbtn assets (PIN digit keys) are NOT affected because they
	 * use lv_btn + lv_img children, not lv_imgbtn.
	 */
	lv_imgbtn_set_state(btn, LV_BTN_STATE_PR);
	lv_imgbtn_set_state(btn, LV_BTN_STATE_REL);
	lv_obj_invalidate(btn);

	return btn;
}

static lv_obj_t *_kh_create_plain_btn(lv_obj_t *parent, const char *text,
	s32 x, s32 y, s32 w, s32 h, kh_ui_action_t action)
{
	lv_obj_t *btn = lv_btn_create(parent, NULL);
	if (!btn)
		return NULL;

	/* Set size and position */
	lv_obj_set_size(btn, w, h);
	lv_obj_set_pos(btn, x, y);

	/* Create label */
	lv_obj_t *label = lv_label_create(btn, NULL);
	if (label)
	{
		lv_label_set_text(label, text);
		lv_obj_align(label, NULL, LV_ALIGN_CENTER, 0, 0);
	}

	/* Set action */
	if (action == KH_UI_ACTION_KIDS)
		lv_btn_set_action(btn, LV_BTN_ACTION_CLICK, _kh_on_start_click);
	else
		lv_btn_set_action(btn, LV_BTN_ACTION_CLICK, _kh_on_gear_click);

	return btn;
}

/* ------------------------------------------------------------------ */
/* Public API                                                         */
/* ------------------------------------------------------------------ */

kh_ui_action_t kh_home_screen(void)
{
	kh_ui_action_t result = KH_UI_ACTION_NONE;
	bool ui_created = false;
	bool jc_initialized = false;
	bool touch_initialized = false;

	/* 1. Mount SD */
	if (sd_mount())
	{
		/* Critical error — cannot proceed without SD */
		return KH_UI_ACTION_NONE;
	}

	/* 2. Load PIN config and store in runtime.
	 * Config ownership is in ui_runtime.c — not in screen_home.c.
	 * The runtime uses this config for PIN verification in the main loop.
	 * If config load fails, PIN verification will return CONFIG_ERROR
	 * and the user will be returned to the home screen (fail closed). */
	{
		kh_pin_config_t cfg;
		kh_config_status_t cfg_status = kh_config_load(&cfg);
		kh_ui_set_pin_config(cfg_status, &cfg);
		/* Clear local copy — runtime now owns the config */
		memset(&cfg, 0, sizeof(cfg));
	}

	/* 3. Load UI config (ui.ini) — parse language setting.
	 *
	 * P5-D: The P5-C gate is removed. The parsed language from ui.ini
	 * is now applied directly to the runtime context. If the parsed
	 * value is invalid (>= KH_LANG_COUNT), the default KH_LANG_NL is
	 * used as a safe fallback. */
	{
		kh_ui_config_t ui_cfg;
		kh_ui_config_set_defaults(&ui_cfg);
		kh_ui_config_load(&ui_cfg);
		memset(&_kh_ctx, 0, sizeof(_kh_ctx));
		_kh_ctx.active_theme    = KH_THEME_ROYAL;
		/* P5-D: Use parsed language from ui.ini (P5-C gate removed) */
		_kh_ctx.active_language = (ui_cfg.language < KH_LANG_COUNT)
			? ui_cfg.language : KH_LANG_NL;
		/* Clear local copy — context now owns the applied language */
		memset(&ui_cfg, 0, sizeof(ui_cfg));
	}

	/* 4. Transactional asset loading — all-or-nothing.
	 * Load 32 global assets first, then 13 theme assets.
	 * If either fails, free everything and return NONE. */

	if (!kh_ui_global_assets_load(&_kh_ctx.global))
	{
		/* Global assets failed — cannot proceed without them */
		display_color_screen(KH_DBG_RED);
		sd_end();
		return KH_UI_ACTION_NONE;
	}

	if (!kh_ui_theme_assets_load(&_kh_ctx.theme,
	                             _kh_ctx.active_theme,
	                             _kh_ctx.active_language))
	{
		/* Theme assets failed — free global assets and return */
		display_color_screen(KH_DBG_ORANGE);
		kh_ui_global_assets_free(&_kh_ctx.global);
		sd_end();
		return KH_UI_ACTION_NONE;
	}


	_kh_ctx.loaded = true;


	/* 4. Initialize LVGL */
	lv_init();

	/* 5. Initialize display driver */
	kh_ui_disp_init();

	/* 6. Initialize touch */
	touch_initialized = (kh_ui_touch_init() == 0);

	/* 7. Initialize Joy-Con (direct call, not via lv_task) */
	jc_initialized = true;
	kh_ui_jc_init();

	/* 8. Create Home UI */
	ui_created = _kh_create_ui();

	if (!ui_created)
	{
		/* Path A: UI creation failed — cleanup and return */
		display_color_screen(KH_DBG_PURPLE);
		if (jc_initialized)
			kh_ui_jc_deinit();

		if (touch_initialized)
			touch_power_off();
		kh_ui_global_assets_free(&_kh_ctx.global);
		kh_ui_theme_assets_free(&_kh_ctx.theme);
		memset(&_kh_ctx, 0, sizeof(_kh_ctx));
		sd_end();
		return KH_UI_ACTION_NONE;
	}

	/* 9. Create PIN screen container (starts hidden) — pass context */
	{
		bool pin_created = kh_pin_screen_create(&_kh_ctx);
		if (!pin_created)
		{
			/* PIN creation failed — rollback: destroy Home, free assets */
			kh_home_destroy();
			if (jc_initialized)
				kh_ui_jc_deinit();
			if (touch_initialized)
				touch_power_off();
			kh_ui_global_assets_free(&_kh_ctx.global);
			kh_ui_theme_assets_free(&_kh_ctx.theme);
			memset(&_kh_ctx, 0, sizeof(_kh_ctx));
			sd_end();
			return KH_UI_ACTION_NONE;
		}
	}

	/* 10. Create Settings screen container (starts hidden) — pass context */
	{
		bool settings_created = kh_settings_screen_create(&_kh_ctx);
		if (!settings_created)
		{
			/* Settings creation failed — rollback: destroy Home + PIN, free assets */
			kh_home_destroy();
			kh_pin_destroy();
			if (jc_initialized)
				kh_ui_jc_deinit();
			if (touch_initialized)
				touch_power_off();
			kh_ui_global_assets_free(&_kh_ctx.global);
			kh_ui_theme_assets_free(&_kh_ctx.theme);
			memset(&_kh_ctx, 0, sizeof(_kh_ctx));
			sd_end();
			return KH_UI_ACTION_NONE;
		}
		kh_ui_set_settings_ready(true);
	}

	/* 11. P5-D: Store asset context pointer in runtime for LANG_CHANGED reload.
	 * This must be set before the Language screen is created, so the runtime
	 * has access to the context when the user changes language. */
	kh_ui_set_asset_context(&_kh_ctx);

	/* 12. P5-D: Create Language selection screen container (starts hidden) */
	{
		bool lang_created = kh_lang_screen_create(&_kh_ctx);
		if (!lang_created)
		{
			/* Lang creation failed — rollback: destroy all screens, free assets */
			kh_home_destroy();
			kh_pin_destroy();
			kh_settings_destroy();
			if (jc_initialized)
				kh_ui_jc_deinit();
			if (touch_initialized)
				touch_power_off();
			kh_ui_global_assets_free(&_kh_ctx.global);
			kh_ui_theme_assets_free(&_kh_ctx.theme);
			memset(&_kh_ctx, 0, sizeof(_kh_ctx));
			sd_end();
			return KH_UI_ACTION_NONE;
		}
		kh_ui_set_lang_ready(true);
	}

	/* 13. Force a synchronous LVGL render pass before entering the main loop.
	 * This ensures all dirty areas from screen creation (lv_obj_set_pos,
	 * lv_obj_set_size, lv_imgbtn_set_src) are flushed to the framebuffer
	 * before the first lv_task_handler() call in the main loop.
	 *
	 * Without this, the initial frame may show scrambled/corrupted content
	 * because the dirty areas collected during creation don't cover the
	 * final object bounds after set_pos + set_size. A click repairs the
	 * display because it triggers LV_SIGNAL_STYLE_CHG → refr_img() →
	 * lv_obj_invalidate() with correct bounds. */
	lv_refr_now();

	/* 14. Run event loop — blocks until user selects an action */
	result = kh_ui_main_loop();

	/* Path B: No teardown — chainload will overwrite memory.
	 * Nyx does no LVGL teardown before chainload (nyx.c:112-163).
	 * LVGL v5.3 has no lv_deinit(). The chainload copies hekatos.bin
	 * to RCM_PAYLOAD_ADDR, calls hw_deinit(false), and jumps.
	 * All memory (heap, VDB, framebuffer) is reclaimed by the reboot. */

	return result;
}

/* ------------------------------------------------------------------ */
/* Destroy the home screen container                                  */
/* ------------------------------------------------------------------ */

void kh_home_destroy(void)
{
	if (_kh_view.root)
	{
		lv_obj_del(_kh_view.root);
		_kh_view.root = NULL;
	}
	memset(&_kh_view, 0, sizeof(kh_home_view_t));
}

/* ------------------------------------------------------------------ */
/* Recreate the home screen UI after a language change                */
/* ------------------------------------------------------------------ */

bool kh_home_recreate(void)
{
	/* Destroy existing view if any */
	kh_home_destroy();

	/* Create new UI with current asset context */
	return _kh_create_ui();
}

/* ------------------------------------------------------------------ */
/* Show/hide the home screen container                                */
/* ------------------------------------------------------------------ */

void kh_home_show(void)
{
	if (_kh_view.root)
		lv_obj_set_hidden(_kh_view.root, false);
}

void kh_home_hide(void)
{
	if (_kh_view.root)
		lv_obj_set_hidden(_kh_view.root, true);
}
