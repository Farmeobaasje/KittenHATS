/*
 * Copyright (c) 2025 KittenHATS Contributors
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * KittenHATS — Fase 5 P5-B: Settings screen.
 *
 * Public API for the Settings screen container.
 * Created once, shown/hidden via kh_settings_show()/kh_settings_hide().
 * Contains: TAAL (disabled), PIN, TERUG buttons.
 */

#ifndef _KH_SCREEN_SETTINGS_H_
#define _KH_SCREEN_SETTINGS_H_

#include <utils/types.h>
#include <libs/lvgl/lvgl.h>
#include "kh_ui_asset_context.h"

/*
 * Create the Settings screen container.
 * All LVGL objects are children of the container.
 * Container starts hidden — shown only on TO_SETTINGS transition.
 *
 * @param ctx  Pointer to loaded asset context (may be NULL).
 * @return     true on success, false on failure.
 */
bool kh_settings_screen_create(const kh_ui_asset_context_t *ctx);

/*
 * Show the Settings screen container.
 * Clears any pending state before showing.
 */
void kh_settings_show(void);

/*
 * Hide the Settings screen container.
 */
void kh_settings_hide(void);

/*
 * Destroy the Settings screen container and all its LVGL children.
 * NULL-safe: if the Settings container was never created, does nothing.
 * Sets the internal container pointer to NULL after deletion.
 *
 * Called during rollback cleanup if a subsequent screen creation fails.
 * LVGL v5.3: deleting the root container auto-deletes all children.
 */
void kh_settings_destroy(void);

#endif /* _KH_SCREEN_SETTINGS_H_ */
