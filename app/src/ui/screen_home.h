/*
 * Copyright (c) 2025 KittenHATS Contributors
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * KittenHATS — Fase 3+4: Home screen UI.
 *
 * Public API for the home screen. kh_home_screen() is the main entry point.
 * Additional functions allow the runtime loop to control focus visibility,
 * confirmation state, and screen show/hide without accessing private LVGL
 * objects directly.
 *
 * Fase 4 P4-A: Added kh_home_show()/kh_home_hide() for state machine.
 */

#ifndef _KH_SCREEN_HOME_H_
#define _KH_SCREEN_HOME_H_

#include "ui_runtime.h"

/*
 * Show the home screen — blocks until the user selects an action.
 * Returns KH_UI_ACTION_KIDS or KH_UI_ACTION_CLEAN.
 * On critical error (SD mount failure), returns KH_UI_ACTION_NONE.
 */
kh_ui_action_t kh_home_screen(void);

/*
 * Show the home screen container.
 * Makes the home container visible on screen.
 */
void kh_home_show(void);

/*
 * Hide the home screen container.
 * Makes the home container invisible.
 */
void kh_home_hide(void);

/*
 * Set focus indicator visibility.
 * Called from ui_runtime.c when Joy-Con or touch input is detected.
 * When hidden, the focus frame is not rendered.
 */
void kh_home_set_focus_visible(bool visible);

/*
 * Set which button has focus (START or GEAR).
 * Called from ui_runtime.c on Joy-Con D-pad input.
 * Only effective when focus is visible.
 */
void kh_home_set_focus(kh_jc_focus_t focus);

/*
 * Destroy the home screen container and all its LVGL children.
 * NULL-safe: if the home view was never created, does nothing.
 * Sets the internal root pointer to NULL after deletion.
 *
 * Called during rollback cleanup if a subsequent screen creation fails.
 * LVGL v5.3: deleting the root container auto-deletes all children.
 */
void kh_home_destroy(void);

/*
 * Recreate the home screen UI after a language change.
 * Destroys the existing home view and creates a new one with
 * the current asset context (_kh_ctx).
 *
 * Called from ui_runtime.c during LANG_CHANGED transition.
 * Returns true on success, false on critical allocation failure.
 */
bool kh_home_recreate(void);

#endif /* _KH_SCREEN_HOME_H_ */
