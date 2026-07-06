/*
 * Copyright (c) 2025 KittenHATS Contributors
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * KittenHATS — Fase 4 P4-A/B/F: PIN screen with touch keypad.
 *
 * Public API for the PIN screen. Created once during UI init, starts hidden.
 * Uses BMP image assets for all text rendering (font labels don't work
 * in this Hekate LVGL build — glyphs produce missing-glyph blocks).
 *
 * P4-A: Static PIN screen with title + back button only.
 * P4-B: Full touch keypad with 12 buttons, PIN buffer, masked display.
 * P4-F: Verify request mechanism — OK callback marks a request,
 *       central runtime consumes it via kh_pin_take_verify_request().
 *
 * PIN title: lv_img with pin_title.bmp (400x60)
 * Cancel button: lv_imgbtn with back_button_normal/pressed.bmp (160x96)
 * Keypad: 12 lv_btn buttons (1-9, WIS, 0, OK) with lv_img for digit BMPs
 * Display: 5 lv_img variants (pin_display_0..4) swapped based on pin_len (4-digit PIN)
 */

#ifndef _KH_SCREEN_PIN_H_
#define _KH_SCREEN_PIN_H_

#include <libs/lvgl/lvgl.h>
#include "kh_ui_asset_context.h"

/*
 * Create the PIN screen UI objects.
 * Uses BMP image assets for all text (no lv_label — font rendering broken).
 * @param ctx  Pointer to loaded asset context (must contain all PIN assets)
 * Returns true on success, false on critical allocation failure.
 */
bool kh_pin_screen_create(const kh_ui_asset_context_t *ctx);

/*
 * Show the PIN screen container.
 * Makes the PIN container visible on screen.
 */
void kh_pin_show(void);

/*
 * Hide the PIN screen container.
 * Makes the PIN container invisible.
 * Cleanup point 1: Clears PIN buffer and resets display to pin_display_0.
 */
void kh_pin_hide(void);

/*
 * Periodic update for PIN screen state machine.
 * Must be called from the main loop when _kh_ui_state == KH_UI_STATE_PIN.
 * Handles non-blocking diagnostic timeout and display updates.
 *
 * @param now_ms  Current monotonic time in milliseconds (from get_tmr_ms())
 */
void kh_pin_update(uint32_t now_ms);

/*
 * Take a pending verify request, if one exists.
 *
 * Called by the central runtime (ui_runtime.c) after the OK callback
 * returns. Copies the current 4-digit PIN into out_pin and clears
 * the internal PIN buffer. The request is consumed exactly once.
 *
 * @param out_pin  Output buffer (must be at least 5 bytes).
 * @param out_len  Output: number of digits copied (0 if no request).
 * @return         true if a verify request was consumed, false if none.
 */
bool kh_pin_take_verify_request(char out_pin[5], uint8_t *out_len);

/*
 * Clear the PIN input buffer and reset display.
 * Called by central runtime after verification completes (any result).
 */
void kh_pin_clear_input(void);

/*
 * Destroy the PIN screen container and all its LVGL children.
 * NULL-safe: if the PIN container was never created, does nothing.
 * Sets the internal container pointer to NULL after deletion.
 *
 * Called during rollback cleanup if a subsequent screen creation fails.
 * LVGL v5.3: deleting the root container auto-deletes all children.
 */
void kh_pin_destroy(void);

#endif /* _KH_SCREEN_PIN_H_ */
