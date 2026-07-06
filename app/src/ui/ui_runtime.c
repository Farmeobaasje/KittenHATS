/*
 * Copyright (c) 2025 KittenHATS Contributors
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * KittenHATS — UI runtime: display driver, touch input, event loop.
 *
 * Display flush (F4):
 *   LVGL renders to VDB at NYX_LV_VDB_ADR (1280x720 logical landscape).
 *   The VDB is a compact, packed buffer containing ONLY the dirty area
 *   (x1,y1)-(x2,y2). The flush callback uses area-relative indexing:
 *
 *     dirty_width  = x2 - x1 + 1
 *     src_x        = lx - x1
 *     src_y        = ly - y1
 *     src_idx      = src_y * dirty_width + src_x
 *
 *   Physical mapping (90° CW rotation with Y-flip):
 *     phys_x = ly
 *     phys_y = 1279 - lx
 *     dst_idx = phys_y * 720 + phys_x
 *
 *   F2 hardware test (v0.1.4) PROVED: full-screen invalidation renders
 *   correctly, confirming the rotation matrix, pixel format, and LVGL
 *   object creation are all correct. The root cause was definitively
 *   isolated to the absolute stride (1280) in the source index formula.
 *
 *   Fallback: #define K_DIAG_USE_OLD_FLUSH to revert to the buggy
 *   absolute-stride formula (full-screen only, for diagnostic comparison).
 *
 * Touch input:
 *   The touch driver (_touch_compensate_limits in touch.c) already outputs
 *   LVGL-logical coordinates: touch_event.x = 0..1279, touch_event.y = 0..719.
 *   Direct passthrough to LVGL — no axis swap needed.
 *   Verified against Hekatos v6.5.3: touch.c lines 93-113.
 *
 * Joy-Con:
 *   Polled directly in the main loop (not via lv_indev).
 *   Edge detection on button state to prevent repeated activation.
 *   Focus switching with D-pad, activation with A/ZL/ZR.
 *
 * Confirmation phase:
 *   When an action is requested (via touch callback or Joy-Con), the loop
 *   enters a confirmation phase: input is blocked, one synchronous LVGL
 *   render pass is performed (lv_refr_now), then a brief dwell delay,
 *   then the loop breaks to return the action for chainload.
 *   _kh_pending_action remains set throughout.
 *   All of this happens in the main loop — never in callbacks.
 *
 * Fase 4 P4-A: State machine (HOME ↔ PIN) with transition consumption.
 *   kh_ui_home_loop() replaced by kh_ui_main_loop().
 *   Gear callback requests TO_PIN transition (not CLEAN action).
 *   Cancel callback requests TO_HOME transition.
 *   Joy-Con behavior is state-dependent.
 *
 * No lv_deinit() — this LVGL v5.3 does not provide it (verified by search).
 * Cleanup: stop event loop, power off touch, deinit Joy-Con, delete LVGL
 * objects, free assets, then chainload.
 */


#include <string.h>
#include <stdlib.h>

#include <bdk.h>
#include <libs/lvgl/lvgl.h>
#include <input/touch.h>
#include <input/joycon.h>

#include "ui_runtime.h"
#include "kh_ui_asset_context.h"
#include "screen_home.h"
#include "screen_pin.h"
#include "screen_settings.h"
#include "screen_lang.h"
#include "config/kh_config.h"
#include "config/kh_pin_verify.h"


/* ------------------------------------------------------------------ */
/* Constants                                                          */
/* ------------------------------------------------------------------ */

#define KH_LVGL_HOR_RES 1280
#define KH_LVGL_VER_RES 720
#define KH_FB_WIDTH     720   /* Physical framebuffer width */
#define KH_FB_HEIGHT    1280  /* Physical framebuffer height */

/* Confirmation phase: brief dwell delay after action is reserved */
#define KH_CONFIRM_DELAY_US 80000  /* 80ms */

/* ------------------------------------------------------------------ */
/* Forward declarations                                               */
/* ------------------------------------------------------------------ */

static void _kh_disp_flush(int32_t x1, int32_t y1, int32_t x2, int32_t y2, const lv_color_t *color_p);
static bool _kh_touch_read(lv_indev_data_t *data);

/* ------------------------------------------------------------------ */
/* Global state                                                       */
/* ------------------------------------------------------------------ */

static touch_event_t _kh_touch_event;
static bool _kh_touch_enabled = false;

/* Touch diagnostics — driver-compensated and LVGL coordinates */
#ifdef KH_DEBUG_TOUCH
typedef struct _kh_touch_diag_t {
	u16 driver_x;
	u16 driver_y;
	u16 lvgl_x;
	u16 lvgl_y;
	bool touching;
} kh_touch_diag_t;
static kh_touch_diag_t _kh_touch_diag = { 0 };
#endif

/* Joy-Con edge detection: previous button state (u32 bitfield) */
static u32 _kh_jc_prev_buttons = 0;

/* Focus state for Joy-Con navigation (type defined in ui_runtime.h) */
static kh_jc_focus_t _kh_jc_focus = KH_JC_FOCUS_START;

/* Whether Joy-Con has been used since last touch — controls focus visibility */
static bool _kh_jc_active = false;

/* Pending action — first-action-wins request mechanism */
static kh_ui_action_t _kh_pending_action = KH_UI_ACTION_NONE;

/* Explicit UI state — tracked independently of object visibility */
static kh_ui_state_t _kh_ui_state = KH_UI_STATE_HOME;

/* P4-F: PIN config storage — loaded once, owned by runtime */
static kh_pin_config_t _kh_pin_config;
static kh_config_status_t _kh_config_status = KH_CONFIG_SD_NOT_MOUNTED;

/* P5-B: Settings screen readiness — set once after creation */
static bool _kh_settings_ready = false;

/* P5-D: Language selection screen readiness — set once after creation */
static bool _kh_lang_ready = false;

/* P5-D: Active language — stored in runtime for LANG_CHANGED transition */
static kh_language_t _kh_active_language = KH_LANG_NL;

/* P5-D: Asset context pointer — stored for LANG_CHANGED reload */
static kh_ui_asset_context_t *_kh_asset_ctx = NULL;

/* P5-E: Requested language — set by kh_ui_request_language_change() */
static kh_language_t _kh_requested_language = KH_LANG_NL;

/* P5-E: Old language — captured before LANG_CHANGED rebuild */
static kh_language_t _kh_old_language = KH_LANG_NL;

/* P5-E: Last config save status — for diagnostic inspection */
static kh_ui_config_save_status_t _kh_last_save_status = KH_UI_CONFIG_SAVE_OK;

/* ------------------------------------------------------------------ */

/* Display flush — F4: area-relative source indexing                  */
/*                                                                     */
/* F2 hardware test (v0.1.4) PROVED:                                  */
/*   - LVGL object creation, styles, pixel format all correct          */
/*   - 90° CW rotation matrix (phys_x=ly, phys_y=1279-lx) correct     */
/*   - Root cause: absolute stride 1280 in source index formula        */
/*                                                                     */
/* F4 fix: area-relative source indexing:                              */
/*   dirty_width = x2 - x1 + 1                                         */
/*   src_x = lx - x1, src_y = ly - y1                                  */
/*   src_idx = src_y * dirty_width + src_x                             */
/*                                                                     */
/* Fallback: #define K_DIAG_USE_OLD_FLUSH to revert to absolute stride */
/* ------------------------------------------------------------------ */

static void _kh_disp_flush(int32_t x1, int32_t y1, int32_t x2, int32_t y2, const lv_color_t *color_p)
{
	/* ---- Bounds checks ---- */
	/* Clamp to valid display area — prevent underflow on corrupt coords */
	if (x1 < 0) x1 = 0;
	if (y1 < 0) y1 = 0;
	if (x2 >= KH_LVGL_HOR_RES) x2 = KH_LVGL_HOR_RES - 1;
	if (y2 >= KH_LVGL_VER_RES) y2 = KH_LVGL_VER_RES - 1;

	/* Validate bounding box consistency — reject inverted areas */
	if (x1 > x2 || y1 > y2)
	{
		lv_flush_ready();
		return;
	}

	u32 *fb = (u32 *)IPL_FB_ADDRESS;  /* 720x1280 physical framebuffer */

	/* Compute dynamic dirty area dimensions */
	const s32 dirty_width  = x2 - x1 + 1;
	const s32 dirty_height = y2 - y1 + 1;

	(void)dirty_height;  /* Used implicitly by loop bounds */

#ifdef K_DIAG_USE_OLD_FLUSH
	/* ---- OLD (buggy) path: absolute stride 1280 — for diagnostic comparison ---- */
	for (s32 ly = y1; ly <= y2; ly++)
	{
		for (s32 lx = x1; lx <= x2; lx++)
		{
			u32 phys_x  = (u32)ly;
			u32 phys_y  = 1279u - (u32)lx;
			u32 src_idx = (u32)ly * KH_LVGL_HOR_RES + (u32)lx;
			u32 dst_idx = phys_y * KH_FB_WIDTH + phys_x;
			fb[dst_idx] = ((u32 *)color_p)[src_idx];
		}
	}
#else
	/* ---- F4 (correct) path: area-relative packed VDB indexing ---- */
	/*
	 * LVGL v5.3 VDB contract (proven by tracing lv_draw_vbasic.c):
	 *   color_p is a packed buffer containing ONLY the dirty area.
	 *   color_p[0]                    = pixel at logical (x1, y1)
	 *   color_p[1]                    = pixel at logical (x1+1, y1)
	 *   color_p[dirty_width]          = pixel at logical (x1, y1+1)
	 *   color_p[src_y*dirty_width+src_x] = pixel at logical (lx, ly)
	 *
	 * Source index is RELATIVE to (x1, y1):
	 *   src_x = lx - x1
	 *   src_y = ly - y1
	 *   src_idx = src_y * dirty_width + src_x
	 *
	 * Destination uses proven 90° CW rotation with Y-flip:
	 *   phys_x = ly
	 *   phys_y = 1279 - lx
	 *   dst_idx = phys_y * 720 + phys_x
	 */
	for (s32 ly = y1; ly <= y2; ly++)
	{
		/* Precompute src_y * dirty_width for this row */
		const s32 src_y_offset = (ly - y1) * dirty_width;

		for (s32 lx = x1; lx <= x2; lx++)
		{
			u32 phys_x  = (u32)ly;
			u32 phys_y  = 1279u - (u32)lx;
			u32 src_idx = (u32)(src_y_offset + (lx - x1));
			u32 dst_idx = phys_y * KH_FB_WIDTH + phys_x;

			fb[dst_idx] = ((u32 *)color_p)[src_idx];
		}
	}
#endif

	lv_flush_ready();
}


/* ------------------------------------------------------------------ */
/* Touch input — 1:1 passthrough (same as Nyx gui.c lines 372-373)    */
/* ------------------------------------------------------------------ */

static bool _kh_touch_read(lv_indev_data_t *data)
{
	if (!_kh_touch_enabled)
	{
		data->state = LV_INDEV_STATE_REL;
		return false;
	}

	int res = touch_poll(&_kh_touch_event);

	/*
	 * Touch driver already outputs LVGL-logical coordinates (1280x720).
	 * _touch_compensate_limits() in touch.c maps:
	 *   raw FTS4 → edge compensation → scale to 1280x720
	 *   touch_event.x = 0..1279, touch_event.y = 0..719
	 * Direct passthrough — no axis swap needed.
	 * Verified against Hekatos v6.5.3: touch.c lines 93-113.
	 */
	data->point.x = MIN(_kh_touch_event.x, KH_LVGL_HOR_RES - 1);
	data->point.y = MIN(_kh_touch_event.y, KH_LVGL_VER_RES - 1);

	/* Set state based on touch detection */
	if (_kh_touch_event.touch)
		data->state = LV_INDEV_STATE_PR;
	else
		data->state = LV_INDEV_STATE_REL;

	(void)res;  /* Unused — Nyx ignores poll result for coordinates */

#ifdef KH_DEBUG_TOUCH
	/* Capture diagnostic values */
	_kh_touch_diag.driver_x  = _kh_touch_event.x;
	_kh_touch_diag.driver_y  = _kh_touch_event.y;
	_kh_touch_diag.lvgl_x    = data->point.x;
	_kh_touch_diag.lvgl_y    = data->point.y;
	_kh_touch_diag.touching  = _kh_touch_event.touch;
#endif

	return false;  /* No buffering */
}

/* ------------------------------------------------------------------ */
/* Public API                                                         */
/* ------------------------------------------------------------------ */

#ifdef KH_DEBUG_TOUCH
kh_touch_diag_t *kh_ui_touch_get_diag(void)
{
	return &_kh_touch_diag;
}
#endif
/* ------------------------------------------------------------------ */

void kh_ui_disp_init(void)
{
	lv_disp_drv_t disp_drv;
	lv_disp_drv_init(&disp_drv);
	disp_drv.disp_flush = _kh_disp_flush;
	lv_disp_drv_register(&disp_drv);
}

int kh_ui_touch_init(void)
{
	int res = touch_power_on();
	_kh_touch_enabled = (res == 0);
	_kh_touch_event.touch = false;

	if (_kh_touch_enabled)
	{
		lv_indev_drv_t indev_drv;
		lv_indev_drv_init(&indev_drv);
		indev_drv.type = LV_INDEV_TYPE_POINTER;
		indev_drv.read = _kh_touch_read;
		lv_indev_drv_register(&indev_drv);
	}

	return res;
}

void kh_ui_jc_init(void)
{
	jc_init_hw();
	_kh_jc_prev_buttons = 0;
	_kh_jc_focus = KH_JC_FOCUS_START;
	_kh_jc_active = false;
}

void kh_ui_jc_deinit(void)
{
	jc_deinit();
}

/*
 * Request an action (first-action-wins).
 * Only the first call sets _kh_pending_action — subsequent calls are ignored.
 * Returns true if this call was the first to set the action.
 * Called by LVGL button callbacks and Joy-Con handlers.
 */
bool kh_ui_request_action(kh_ui_action_t action)
{
	if (_kh_pending_action == KH_UI_ACTION_NONE)
	{
		_kh_pending_action = action;
		return true;
	}
	return false;
}

/* ------------------------------------------------------------------ */
/* Transition request (first-request-wins)                            */
/* ------------------------------------------------------------------ */

static kh_ui_transition_t _kh_pending_transition = KH_UI_TRANSITION_NONE;

void kh_ui_request_transition(kh_ui_transition_t transition)
{
	if (_kh_pending_transition == KH_UI_TRANSITION_NONE)
		_kh_pending_transition = transition;
}

/* ------------------------------------------------------------------ */
/* P4-F: Store loaded PIN config in runtime                           */
/* ------------------------------------------------------------------ */

void kh_ui_set_pin_config(kh_config_status_t status, const kh_pin_config_t *config)
{
	_kh_config_status = status;

	if (config && status == KH_CONFIG_OK)
	{
		/* Copy config into runtime-owned storage */
		memcpy(&_kh_pin_config, config, sizeof(kh_pin_config_t));
	}
	else
	{
		/* Clear config on failure */
		memset(&_kh_pin_config, 0, sizeof(kh_pin_config_t));
	}
}

/* ------------------------------------------------------------------ */
/* P5-B: Store Settings screen readiness in runtime                   */
/* ------------------------------------------------------------------ */

void kh_ui_set_settings_ready(bool ready)
{
	_kh_settings_ready = ready;
}

/* ------------------------------------------------------------------ */
/* P5-D: Language API — get/set active language                       */
/* ------------------------------------------------------------------ */

void kh_ui_set_active_language(kh_language_t lang)
{
	if (lang < KH_LANG_COUNT)
		_kh_active_language = lang;
}

kh_language_t kh_ui_get_active_language(void)
{
	return _kh_active_language;
}

/* ------------------------------------------------------------------ */
/* P5-D: Store asset context pointer in runtime                       */
/* ------------------------------------------------------------------ */

void kh_ui_set_asset_context(kh_ui_asset_context_t *ctx)
{
	_kh_asset_ctx = ctx;
}

/* ------------------------------------------------------------------ */
/* P5-D: Store Language screen readiness in runtime                   */
/* ------------------------------------------------------------------ */

void kh_ui_set_lang_ready(bool ready)
{
	_kh_lang_ready = ready;
}

/* ------------------------------------------------------------------ */
/* P5-E: Single-purpose rebuild helper — no commit, no restore        */
/* ------------------------------------------------------------------ */

/*
 * Build all screens with the given language.
 * Single-purpose: does NOT commit the language change, does NOT save.
 * Returns true if all screens were created successfully.
 *
 * @param lang  Language to build screens with.
 * @return      true if all screens created, false on any failure.
 */
static bool kh_ui_build_language(kh_language_t lang)
{
	if (!_kh_asset_ctx)
		return false;

	/* Free current theme assets */
	kh_ui_theme_assets_free(&_kh_asset_ctx->theme);

	/* Reload theme assets with new language */
	bool reloaded = kh_ui_theme_assets_load(
		&_kh_asset_ctx->theme,
		_kh_asset_ctx->active_theme,
		lang);

	if (!reloaded)
		return false;

	/* Update context language */
	_kh_asset_ctx->active_language = lang;

	/* Recreate all screens */
	bool home_ok = kh_home_recreate();
	bool pin_ok = kh_pin_screen_create(_kh_asset_ctx);
	bool settings_ok = kh_settings_screen_create(_kh_asset_ctx);
	bool lang_ok = kh_lang_screen_create(_kh_asset_ctx);

	/*
	 * Update ready flags to reflect actual container state.
	 * Runtime-owned: flags are reset to false on destroy (in LANG_CHANGED
	 * transition) and set to true here after successful creation.
	 * This ensures TO_SETTINGS and TO_LANG_SELECT transitions only proceed
	 * when the containers actually exist.
	 */
	_kh_settings_ready = settings_ok;
	_kh_lang_ready = lang_ok;

	return home_ok && pin_ok && settings_ok && lang_ok;
}

/* ------------------------------------------------------------------ */
/* P5-E: Request language change with full transaction                */
/* ------------------------------------------------------------------ */

void kh_ui_request_language_change(kh_language_t lang)
{
	if (lang >= KH_LANG_COUNT)
		return;

	/* Store requested language and old language */
	_kh_requested_language = lang;
	_kh_old_language = _kh_active_language;

	/* Request LANG_CHANGED transition — main loop processes it */
	kh_ui_request_transition(KH_UI_TRANSITION_LANG_CHANGED);
}

/* ------------------------------------------------------------------ */
/* P5-E: Get last config save status                                  */
/* ------------------------------------------------------------------ */

kh_ui_config_save_status_t kh_ui_get_last_save_status(void)
{
	return _kh_last_save_status;
}

/* ------------------------------------------------------------------ */
/* Main event loop — state machine (HOME ↔ PIN ↔ SETTINGS ↔ LANG)    */
/* ------------------------------------------------------------------ */


kh_ui_action_t kh_ui_main_loop(void)

{
	/* Reset all state for this loop iteration */
	_kh_pending_action = KH_UI_ACTION_NONE;
	_kh_pending_transition = KH_UI_TRANSITION_NONE;
	_kh_jc_focus = KH_JC_FOCUS_START;
	_kh_jc_active = false;

	/*
	 * Synchronize Joy-Con edge detection with current held state.
	 * Do NOT reset to 0 — a button held during splash would appear
	 * as a rising edge on the first poll, triggering a false activation.
	 * Instead, poll once and record the current state as baseline.
	 */
	jc_gamepad_rpt_t *initial_pad = joycon_poll();
	_kh_jc_prev_buttons = initial_pad ? initial_pad->buttons : 0;

	/* Explicit UI state starts at HOME */
	_kh_ui_state = KH_UI_STATE_HOME;

	/* Focus frame starts hidden — only shown after Joy-Con D-pad input */
	kh_home_set_focus_visible(false);

	while (1)
	{
		/* 1. Handle LVGL tasks (rendering, animations, input processing) */
		lv_task_handler();

		/* 2. Consume pending transition (requested by callbacks) */
		if (_kh_pending_transition != KH_UI_TRANSITION_NONE)
		{
			switch (_kh_pending_transition)
			{
			case KH_UI_TRANSITION_TO_PIN:
				kh_home_hide();
				kh_settings_hide();
				kh_pin_show();
				_kh_ui_state = KH_UI_STATE_PIN;
				break;
			case KH_UI_TRANSITION_TO_HOME:
				kh_pin_hide();
				kh_settings_hide();
				kh_home_show();
				_kh_ui_state = KH_UI_STATE_HOME;
				/* Reset Joy-Con focus for home screen */
				_kh_jc_focus = KH_JC_FOCUS_START;
				_kh_jc_active = false;
				kh_home_set_focus_visible(false);
				kh_home_set_focus(KH_JC_FOCUS_START);
				break;
			case KH_UI_TRANSITION_TO_SETTINGS:
				/*
				 * Fail-safe: only transition to Settings if the
				 * container was successfully created. If not ready,
				 * ensure Home is visible and stay in HOME state.
				 * The transition is consumed either way — no retry.
				 */
				if (!_kh_settings_ready)
				{
					kh_home_show();
					kh_pin_hide();
					_kh_ui_state = KH_UI_STATE_HOME;
					break;
				}
				kh_settings_show();
				kh_lang_hide();  /* P5-D: hide Language when returning to Settings */
				kh_home_hide();
				kh_pin_hide();
				_kh_ui_state = KH_UI_STATE_SETTINGS;
				/* Reset Joy-Con focus for settings screen */
				_kh_jc_focus = KH_JC_FOCUS_SETTINGS_PIN;
				_kh_jc_active = false;
				break;
			case KH_UI_TRANSITION_TO_LANG_SELECT:
				/*
				 * Fail-safe: only transition to Language selection if the
				 * container was successfully created. If not ready,
				 * ensure Settings is visible and stay in SETTINGS state.
				 */
				if (!_kh_lang_ready)
				{
					kh_settings_show();
					kh_home_hide();
					kh_pin_hide();
					_kh_ui_state = KH_UI_STATE_SETTINGS;
					break;
				}
				kh_lang_show();
				kh_settings_hide();
				kh_home_hide();
				kh_pin_hide();
				_kh_ui_state = KH_UI_STATE_LANG_SELECT;
				/* Reset Joy-Con focus for language screen */
				_kh_jc_focus = KH_JC_FOCUS_LANG_NL;
				_kh_jc_active = false;
				break;
			case KH_UI_TRANSITION_LANG_CHANGED:
				/*
				 * P5-E: Language change transaction with full rollback.
				 *
				 * Transaction:
				 *   1. Destroy all screens
				 *   2. Build with requested language (via kh_ui_build_language)
				 *   3. If success: commit language, save to SD, show Settings
				 *   4. If build fails: build with old language
				 *      - If old build succeeds: old language stays, show Settings
				 *      - If old build also fails: FATAL — halt
				 *
				 * Save failures are NEVER fatal. The runtime language remains active.
				 * Only UI rebuild failure + old-language rebuild failure is fatal.
				 */
				{
					/* 1. Destroy all screens */
					kh_home_destroy();
					kh_pin_destroy();
					kh_settings_destroy();
					kh_lang_destroy();

					/* 2. Build with requested language */
					bool build_ok = kh_ui_build_language(_kh_requested_language);

					if (build_ok)
					{
						/*
						 * 3. Success — commit language change.
						 *     _kh_active_language is the runtime's active language.
						 *     Save to SD (non-fatal on failure).
						 */
						_kh_active_language = _kh_requested_language;

						/* Persist to SD — save failures are NEVER fatal */
						_kh_last_save_status = kh_ui_config_save_language(_kh_active_language);

						/* Show Settings screen */
						kh_settings_show();
						_kh_ui_state = KH_UI_STATE_SETTINGS;
						_kh_jc_focus = KH_JC_FOCUS_SETTINGS_PIN;
						_kh_jc_active = false;
					}
					else
					{
						/*
						 * 4. Build failed — rollback to old language.
						 *     Try to rebuild with _kh_old_language.
						 */
						bool old_ok = kh_ui_build_language(_kh_old_language);

						if (old_ok)
						{
							/* Old language restored — show Settings */
							kh_settings_show();
							_kh_ui_state = KH_UI_STATE_SETTINGS;
							_kh_jc_focus = KH_JC_FOCUS_SETTINGS_PIN;
							_kh_jc_active = false;
						}
						else
						{
							/*
							 * 5. Both builds failed — FATAL.
							 *     Cannot recover UI. Halt.
							 */
							kh_home_show();
							_kh_ui_state = KH_UI_STATE_HOME;
							_kh_jc_focus = KH_JC_FOCUS_START;
							_kh_jc_active = false;
						}
					}
				}
				break;

			default:
				break;
			}
			_kh_pending_transition = KH_UI_TRANSITION_NONE;

			/*
			 * Conflict check: if a boot action was also requested during
			 * the same callback cycle (e.g. a callback set both transition
			 * and action), treat it as a diagnostic conflict and fail closed.
			 * A transition callback must never request a boot action.
			 */
			if (_kh_pending_action != KH_UI_ACTION_NONE)
			{
				/* Diagnostic conflict — fail closed, return NONE */
				_kh_pending_action = KH_UI_ACTION_NONE;
			}

			/*
			 * Synchronize Joy-Con edge detection after transition.
			 * A button held during the transition must not count as
			 * a new edge in the next iteration.
			 */
			jc_gamepad_rpt_t *sync_pad = joycon_poll();
			_kh_jc_prev_buttons = sync_pad ? sync_pad->buttons : 0;

			/* One synchronous LVGL render pass to show the new screen */
			lv_refr_now();

			/* Skip Joy-Con poll this iteration — transition just happened */
			usleep(400);
			continue;
		}

		/* 3. Check if an action was requested (by touch callback or Joy-Con) */
		if (_kh_pending_action != KH_UI_ACTION_NONE)
		{
			/*
			 * Confirmation phase:
			 *   One synchronous LVGL render pass (lv_refr_now) to flush
			 *   the final frame, then a brief dwell delay for visual
			 *   stabilization, then break to return the action for chainload.
			 *   F4 area-relative source indexing handles partial dirty areas.
			 */
			lv_refr_now();

			/* Dwell: brief delay for visual stabilization */
			usleep(KH_CONFIRM_DELAY_US);

			/* Break loop — return action for chainload */
			break;
		}

		/* 3.5 P4-F: Consume PIN verify request (only when in PIN state) */
		if (_kh_ui_state == KH_UI_STATE_PIN)
		{
			char verify_pin[5];
			uint8_t verify_len = 0;

			if (kh_pin_take_verify_request(verify_pin, &verify_len))
			{
				/*
				 * A verify request was made (OK button pressed with 4 digits).
				 * Call kh_pin_verify() with the runtime-owned config.
				 * Only KH_PIN_VERIFY_OK leads to CLEAN chainload.
				 * All other outcomes: clear PIN, return to home (fail closed).
				 */
				kh_pin_verify_result_t vresult = kh_pin_verify(
					&_kh_pin_config, verify_pin, verify_len);

				/* Clear verify PIN buffer immediately */
				memset(verify_pin, 0, sizeof(verify_pin));

				if (vresult == KH_PIN_VERIFY_OK)
				{
					/*
					 * PIN verified — request CLEAN action.
					 * First-action-wins: this is the first action request,
					 * so it will be set. The loop will enter confirmation
					 * phase on the next iteration.
					 */
					kh_ui_request_action(KH_UI_ACTION_CLEAN);
				}
				else
				{
					/*
					 * PIN verification failed (wrong PIN, config error,
					 * hash error, or invalid input).
					 * Fail closed: clear PIN input, return to home screen.
					 * The user must start over from the gear icon.
					 */
					kh_pin_clear_input();
					kh_ui_request_transition(KH_UI_TRANSITION_TO_HOME);
				}
			}
		}

		/* 4. Poll Joy-Con directly (not via lv_indev) */
		jc_gamepad_rpt_t *pad = joycon_poll();
		if (pad)
		{
			/*
			 * Build a u32 bitmask from the bitfield struct members.
			 * The jc_gamepad_rpt_t uses bitfields (joycon.h lines 40-76):
			 *   y(0), x(1), b(2), a(3), sr_r(4), sl_r(5), r(6), zr(7),
			 *   minus(8), plus(9), r3(10), l3(11), home(12), cap(13),
			 *   pad(14), wired(15), down(16), up(17), right(18), left(19),
			 *   sr_l(20), sl_l(21), l(22), zl(23)
			 */
			u32 btn = pad->buttons;
			u32 edge = btn & ~_kh_jc_prev_buttons;  /* Rising edge only */
			_kh_jc_prev_buttons = btn;

			/* Only process Joy-Con when no transition is pending */
			if (_kh_pending_transition == KH_UI_TRANSITION_NONE)
			{
				/* D-pad: switch focus based on current state */
				if (_kh_ui_state == KH_UI_STATE_HOME)
				{
					if (edge & (BIT(16) | BIT(17) | BIT(18) | BIT(19)))
					{
						/* down(16), up(17), right(18), left(19) */
						_kh_jc_focus = (_kh_jc_focus == KH_JC_FOCUS_START)
							? KH_JC_FOCUS_GEAR : KH_JC_FOCUS_START;

						/* First Joy-Con D-pad input: show focus indicator */
						if (!_kh_jc_active)
						{
							_kh_jc_active = true;
							kh_home_set_focus_visible(true);
						}

						kh_home_set_focus(_kh_jc_focus);
					}

					/* A(3), ZL(23), ZR(7): activate (rising edge) */
					if (edge & (BIT(3) | BIT(23) | BIT(7)))
					{
						/* State-dependent: HOME→KIDS/TO_PIN */
						if (_kh_jc_focus == KH_JC_FOCUS_START)
							kh_ui_request_action(KH_UI_ACTION_KIDS);
						else
							kh_ui_request_transition(KH_UI_TRANSITION_TO_PIN);
					}
				}
				else if (_kh_ui_state == KH_UI_STATE_SETTINGS)
				{
					/* P5-B: Settings screen Joy-Con navigation.
					 * Two focusable items: PIN button and TERUG button.
					 * D-pad toggles between them.
					 */
					if (edge & (BIT(16) | BIT(17) | BIT(18) | BIT(19)))
					{
						/* Toggle between PIN and TERUG */
						_kh_jc_focus = (_kh_jc_focus == KH_JC_FOCUS_SETTINGS_PIN)
							? KH_JC_FOCUS_SETTINGS_BACK : KH_JC_FOCUS_SETTINGS_PIN;
					}

					/* A(3), ZL(23), ZR(7): activate (rising edge) */
					if (edge & (BIT(3) | BIT(23) | BIT(7)))
					{
						if (_kh_jc_focus == KH_JC_FOCUS_SETTINGS_PIN)
							kh_ui_request_transition(KH_UI_TRANSITION_TO_PIN);
						else
							kh_ui_request_transition(KH_UI_TRANSITION_TO_HOME);
					}
				}
				else if (_kh_ui_state == KH_UI_STATE_LANG_SELECT)
				{
					/* P5-D: Language selection screen Joy-Con navigation.
					 * Six focusable items: NL, EN, DE, ES, FR, TERUG.
					 * D-pad cycles through them in order.
					 */
					if (edge & (BIT(16) | BIT(17) | BIT(18) | BIT(19)))
					{
						/* Cycle forward through focus items */
						switch (_kh_jc_focus)
						{
						case KH_JC_FOCUS_LANG_NL:
							_kh_jc_focus = KH_JC_FOCUS_LANG_EN;
							break;
						case KH_JC_FOCUS_LANG_EN:
							_kh_jc_focus = KH_JC_FOCUS_LANG_DE;
							break;
						case KH_JC_FOCUS_LANG_DE:
							_kh_jc_focus = KH_JC_FOCUS_LANG_ES;
							break;
						case KH_JC_FOCUS_LANG_ES:
							_kh_jc_focus = KH_JC_FOCUS_LANG_FR;
							break;
						case KH_JC_FOCUS_LANG_FR:
							_kh_jc_focus = KH_JC_FOCUS_LANG_BACK;
							break;
						case KH_JC_FOCUS_LANG_BACK:
							_kh_jc_focus = KH_JC_FOCUS_LANG_NL;
							break;
						default:
							_kh_jc_focus = KH_JC_FOCUS_LANG_NL;
							break;
						}
					}

					/* A(3), ZL(23), ZR(7): activate (rising edge) */
					if (edge & (BIT(3) | BIT(23) | BIT(7)))
					{
						switch (_kh_jc_focus)
						{
						case KH_JC_FOCUS_LANG_NL:
							kh_ui_request_language_change(KH_LANG_NL);
							break;
						case KH_JC_FOCUS_LANG_EN:
							kh_ui_request_language_change(KH_LANG_EN);
							break;
						case KH_JC_FOCUS_LANG_DE:
							kh_ui_request_language_change(KH_LANG_DE);
							break;
						case KH_JC_FOCUS_LANG_ES:
							kh_ui_request_language_change(KH_LANG_ES);
							break;
						case KH_JC_FOCUS_LANG_FR:
							kh_ui_request_language_change(KH_LANG_FR);
							break;
						case KH_JC_FOCUS_LANG_BACK:
							kh_ui_request_transition(KH_UI_TRANSITION_TO_SETTINGS);
							break;
						default:
							break;
						}
					}

				}
			}

			/* B(2): does nothing in P4-A */
		}

		/* 5. Periodic PIN screen update (non-blocking diagnostic timeout) */
		if (_kh_ui_state == KH_UI_STATE_PIN)
			kh_pin_update(get_tmr_ms());

		/* 6. Brief sleep to prevent busy-wait */
		usleep(400);
	}

	return _kh_pending_action;
}
