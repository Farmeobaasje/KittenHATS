/*
 * Copyright (c) 2025 KittenHATS Contributors
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * KittenHATS — P5-D: Language selection screen.
 *
 * Public API for the language selection screen.
 * Created once during UI init, starts hidden.
 * Shows 5 language buttons (NL, EN, DE, ES, FR).
 * Current language is highlighted.
 * On selection: triggers language reload via runtime.
 *
 * All text via BMP images (lv_label font rendering broken).
 */

#ifndef _KH_SCREEN_LANG_H_
#define _KH_SCREEN_LANG_H_

#include <libs/lvgl/lvgl.h>
#include "kh_ui_asset_context.h"

/*
 * Create the Language selection screen UI objects.
 * Uses global language button assets (language-independent).
 *
 * @param ctx  Pointer to loaded asset context.
 * @return     true on success, false on critical allocation failure.
 */
bool kh_lang_screen_create(const kh_ui_asset_context_t *ctx);

/*
 * Show the Language selection screen container.
 */
void kh_lang_show(void);

/*
 * Hide the Language selection screen container.
 */
void kh_lang_hide(void);

/*
 * Destroy the Language selection screen container and all LVGL children.
 * NULL-safe: if container was never created, does nothing.
 * Sets internal container pointer to NULL after deletion.
 */
void kh_lang_destroy(void);

#endif /* _KH_SCREEN_LANG_H_ */
