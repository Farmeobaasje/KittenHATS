/*
 * Copyright (c) 2025 KittenHATS Contributors
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * KittenHATS — Fase 3+4: UI runtime types and display/input API.
 *
 * Contains the display driver, touch input driver, and LVGL runtime
 * initialization. Does NOT include chainload.h or boot types —
 * the UI layer is independent of the boot module.
 *
 * Fase 4 P4-A: Added transition types and kh_ui_main_loop() state machine.
 * kh_ui_home_loop() is replaced by kh_ui_main_loop().
 *
 * Fase 4 P4-F: Added config ownership and verify request mechanism.
 * Config is loaded once after SD mount, stored in ui_runtime.c.
 * PIN verification is triggered by the central runtime, not callbacks.
 */

#ifndef _KH_UI_RUNTIME_H_
#define _KH_UI_RUNTIME_H_

#include <utils/types.h>
#include <libs/lvgl/lvgl.h>
#include "config/kh_config.h"
#include "config/kh_ui_config.h"
#include "kh_ui_theme_assets.h"

/* Forward declaration — full definition in kh_ui_asset_context.h */
typedef struct _kh_ui_asset_context_t kh_ui_asset_context_t;

/*
 * Joy-Con focus state — which button is currently highlighted.
 * Used by kh_home_update_focus() for the focus indicator.
 *
 * P5-B: Added KH_JC_FOCUS_SETTINGS_PIN and KH_JC_FOCUS_SETTINGS_BACK
 * for Settings screen Joy-Con navigation.
 *
 * P5-D: Added KH_JC_FOCUS_LANG_* for language selection screen.
 */
typedef enum
{
	KH_JC_FOCUS_START = 0,
	KH_JC_FOCUS_GEAR  = 1,
	KH_JC_FOCUS_SETTINGS_PIN  = 2,
	KH_JC_FOCUS_SETTINGS_BACK = 3,
	KH_JC_FOCUS_LANG_NL  = 4,
	KH_JC_FOCUS_LANG_EN  = 5,
	KH_JC_FOCUS_LANG_DE  = 6,
	KH_JC_FOCUS_LANG_ES  = 7,
	KH_JC_FOCUS_LANG_FR  = 8,
	KH_JC_FOCUS_LANG_BACK = 9,
} kh_jc_focus_t;


/*
 * UI action codes — returned by kh_ui_main_loop() to main.c.
 * main.c translates these to kh_boot_mode_t for chainload.
 *
 * P4-F: No VERIFY action added. Verification uses a separate private
 * request mechanism (kh_pin_take_verify_request). Only KIDS and CLEAN
 * are valid boot actions.
 */
typedef enum
{
	KH_UI_ACTION_NONE  = 0,  /* No action selected (error/fallback) */
	KH_UI_ACTION_KIDS  = 1,  /* Boot into KIDS mode (emuMMC) */
	KH_UI_ACTION_CLEAN = 2,  /* Boot into CLEAN mode (full Nyx) */
} kh_ui_action_t;

/*
 * Screen transition codes — requested by callbacks, consumed by main loop.
 * Callbacks may only request transitions — they never trigger actions directly.
 *
 * P5-B: Added KH_UI_TRANSITION_TO_SETTINGS for Gear→Settings flow.
 * P5-D: Added KH_UI_TRANSITION_TO_LANG_SELECT and KH_UI_TRANSITION_LANG_CHANGED.
 */
typedef enum
{
	KH_UI_TRANSITION_NONE          = 0,  /* No pending transition */
	KH_UI_TRANSITION_TO_PIN        = 1,  /* Show PIN screen, hide current */
	KH_UI_TRANSITION_TO_HOME       = 2,  /* Show Home screen, hide current */
	KH_UI_TRANSITION_TO_SETTINGS   = 3,  /* Show Settings screen, hide current */
	KH_UI_TRANSITION_TO_LANG_SELECT = 4, /* Show Language selection screen */
	KH_UI_TRANSITION_LANG_CHANGED  = 5,  /* Language changed — reload all screens */
} kh_ui_transition_t;

/*
 * Explicit UI state — tracked independently of object visibility.
 * Updated on every transition, checked before Joy-Con activation.
 * Never inferred from container visibility.
 *
 * P5-B: Added KH_UI_STATE_SETTINGS for Settings screen.
 * P5-D: Added KH_UI_STATE_LANG_SELECT for Language selection screen.
 */
typedef enum
{
	KH_UI_STATE_HOME       = 0,  /* Home screen visible */
	KH_UI_STATE_PIN        = 1,  /* PIN screen visible */
	KH_UI_STATE_SETTINGS   = 2,  /* Settings screen visible */
	KH_UI_STATE_LANG_SELECT = 3, /* Language selection screen visible */
} kh_ui_state_t;


/*
 * Initialize LVGL display driver.
 * Registers _kh_disp_flush as the flush callback.
 * Must be called after lv_init() and before any LVGL object creation.
 */
void kh_ui_disp_init(void);

/*
 * Initialize touch input driver.
 * Powers on the touch controller and registers _kh_touch_read.
 * Returns 0 on success, non-zero on failure.
 */
int kh_ui_touch_init(void);

/*
 * Initialize Joy-Con.
 * Calls jc_init_hw() directly (not via lv_task — signature mismatch).
 * Must be called before kh_ui_main_loop().
 */
void kh_ui_jc_init(void);

/*
 * Deinitialize Joy-Con.
 * Calls jc_deinit() to power down the controllers.
 */
void kh_ui_jc_deinit(void);

/*
 * Request an action from the UI event loop (first-action-wins).
 * Called by button callbacks and Joy-Con handlers.
 * Only the first call sets the action — subsequent calls are ignored.
 * Returns true if this call was the first to set the action,
 * false if an action was already pending.
 * Thread-safe for single-threaded LVGL event model.
 */
bool kh_ui_request_action(kh_ui_action_t action);

/*
 * Request a screen transition (first-request-wins).
 * Called by button callbacks (Gear→TO_PIN, Cancel→TO_HOME).
 * Consumed by kh_ui_main_loop() after callbacks return.
 * Never triggers actions directly — only changes visible screen.
 */
void kh_ui_request_transition(kh_ui_transition_t transition);

/*
 * Main UI event loop with state machine.
 * Polls LVGL tasks, touch, and Joy-Con in a tight loop.
 * Handles screen transitions (HOME ↔ PIN) and action requests.
 * Returns when the user selects an action (KIDS via Start button,
 * or CLEAN via verified PIN).
 *
 * P4-F: PIN verification is consumed inside this loop.
 * The OK callback only marks a verify request; the loop
 * calls kh_pin_take_verify_request() and kh_pin_verify(),
 * and only on success requests KH_UI_ACTION_CLEAN.
 *
 * @return        KH_UI_ACTION_KIDS, KH_UI_ACTION_CLEAN, or KH_UI_ACTION_NONE.
 */
kh_ui_action_t kh_ui_main_loop(void);

/*
 * Store the loaded PIN config and its status in the runtime.
 * Called once from kh_home_screen() after SD mount and config load.
 * Config ownership is in ui_runtime.c — not in screen_home.c.
 *
 * @param status  Config load status (KH_CONFIG_OK on success).
 * @param config  Pointer to loaded config (copied internally).
 */
void kh_ui_set_pin_config(kh_config_status_t status, const kh_pin_config_t *config);

/*
 * Set whether the Settings screen is ready for transitions.
 * Called once from kh_home_screen() after kh_settings_screen_create().
 * When false, TO_SETTINGS transitions are consumed safely without
 * changing state — Home remains visible.
 *
 * @param ready  true if Settings screen was created successfully.
 */
void kh_ui_set_settings_ready(bool ready);

/*
 * P5-D: Language API — get/set active language in the runtime.
 * The active language is stored in ui_runtime.c and used by the
 * main loop to reload theme assets when the language changes.
 *
 * kh_ui_set_active_language() sets the runtime language.
 * kh_ui_get_active_language() returns the current runtime language.
 *
 * @param lang  Language to set.
 */
void kh_ui_set_active_language(kh_language_t lang);
kh_language_t kh_ui_get_active_language(void);

/*
 * P5-D: Store the asset context pointer in the runtime.
 * Used by the LANG_CHANGED transition to reload theme assets
 * and recreate all screens with the new language.
 *
 * @param ctx  Pointer to the loaded asset context.
 */
void kh_ui_set_asset_context(kh_ui_asset_context_t *ctx);

/*
 * P5-D: Set whether the Language selection screen is ready for transitions.
 * Called once from kh_home_screen() after kh_lang_screen_create().
 * When false, TO_LANG_SELECT transitions are consumed safely.
 *
 * @param ready  true if Language screen was created successfully.
 */
void kh_ui_set_lang_ready(bool ready);

/*
 * P5-E: Request a language change with full transaction.
 * Called by language button callbacks (screen_lang.c) and Joy-Con handlers.
 * Stores the requested language and old language, then requests LANG_CHANGED.
 * The main loop processes the transition: rebuild → commit → save.
 *
 * Save failures are NEVER fatal. The runtime language remains active.
 * Only UI rebuild failure + old-language rebuild failure is fatal.
 *
 * @param lang  Requested language (KH_LANG_NL through KH_LANG_FR).
 */
void kh_ui_request_language_change(kh_language_t lang);

/*
 * P5-E: Get the last config save status for diagnostic purposes.
 * Returns the status from the most recent kh_ui_config_save_language() call.
 * The runtime stores this for inspection but does not act on it
 * (save failures are non-fatal).
 *
 * @return  Last save status code.
 */
kh_ui_config_save_status_t kh_ui_get_last_save_status(void);

/*
 * Touch diagnostics — only available when KH_DEBUG_TOUCH is defined.

 * Returns pointer to static struct with driver and LVGL coordinates.
 */
#ifdef KH_DEBUG_TOUCH
typedef struct _kh_touch_diag_t {
	u16 driver_x;
	u16 driver_y;
	u16 lvgl_x;
	u16 lvgl_y;
	bool touching;
} kh_touch_diag_t;
kh_touch_diag_t *kh_ui_touch_get_diag(void);
#endif

#endif /* _KH_UI_RUNTIME_H_ */
