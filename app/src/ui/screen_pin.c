/*
 * Copyright (c) 2025 KittenHATS Contributors
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * KittenHATS — PIN screen with touch keypad.
 *
 * Layout (1280x720 logical LVGL space):
 *
 *   Container: 700x600 at (290, 60)
 *   ┌──────────────────────────────────────────────────────────────┐
 *   │                    ┌─────────────────┐                       │
 *   │                    │   pin_title.bmp  │  (400x60, y=20)      │
 *   │                    └─────────────────┘                       │
 *   │                                                              │
 *   │                    ┌─────────────────┐                       │
 *   │                    │ pin_display_N   │  (400x60, y=90)       │
 *   │                    └─────────────────┘                       │
 *   │                                                              │
 *   │          ┌──────┐ ┌──────┐ ┌──────┐                          │
 *   │          │  1   │ │  2   │ │  3   │  Row 0 (y=160)          │
 *   │          └──────┘ └──────┘ └──────┘                          │
 *   │          ┌──────┐ ┌──────┐ ┌──────┐                          │
 *   │          │  4   │ │  5   │ │  6   │  Row 1 (y=244)          │
 *   │          └──────┘ └──────┘ └──────┘                          │
 *   │          ┌──────┐ ┌──────┐ ┌──────┐                          │
 *   │          │  7   │ │  8   │ │  9   │  Row 2 (y=328)          │
 *   │          └──────┘ └──────┘ └──────┘                          │
 *   │          ┌──────┐ ┌──────┐ ┌──────┐                          │
 *   │          │ CLEAR│ │  0   │ │  OK  │  Row 3 (y=412)          │
 *   │          └──────┘ └──────┘ └──────┘                          │
 *   │                                                              │
 *   │              ┌─────────────────────┐                         │
 *   │              │  Back (160x60)       │  (x=270, y=510)        │
 *   │              └─────────────────────┘                         │
 *   └──────────────────────────────────────────────────────────────┘
 *
 * Keypad layout (72x72 buttons, 3 columns, 4 rows):
 *   Row 0: 1, 2, 3       — y=160
 *   Row 1: 4, 5, 6       — y=244
 *   Row 2: 7, 8, 9       — y=328
 *   Row 3: CLEAR, 0, OK  — y=412
 *
 *   Column x positions: x=226, x=314, x=402
 *   (gap_h = 16px between columns, gap_v = 12px between rows)
 *   Centered in 700px container: (700 - (3*72 + 2*16)) / 2 = 226
 *
 * Back button: 160x60 at (x=270, y=510) inside container
 *
 * Ownership model:
 *   pin_container — root container for all PIN screen objects.
 *   Created once in kh_pin_screen_create(), hidden by default.
 *   Shown/hidden via kh_pin_show()/kh_pin_hide().
 *   No teardown — chainload reclaims all memory.
 *
 * PIN buffer:
 *   char pin[5] — max 4 digits + NUL terminator
 *   uint8_t pin_len — current number of digits entered (0-4)
 *   Always NUL-terminated at pin[pin_len]
 *
 * Display:
 *   5 lv_img objects (pin_display_0..4) stacked at same position.
 *   Only one is visible at a time, based on pin_len.
 *   Swapped by hiding all and showing the one matching pin_len.
 *
 * P4-F: Verify request mechanism:
 *   OK callback (with 4 digits) sets _kh_pin_verify_requested = true.
 *   Central runtime calls kh_pin_take_verify_request() to consume it.
 *   The callback never hashes, compares, or chainloads directly.
 *   No diagnostic state — the old pin_diag_ok is no longer used.
 *
 * Cleanup points (3 total):
 *   1. kh_pin_hide() — clear PIN buffer, reset display to 0
 *   2. kh_pin_show() — clear PIN buffer, reset display to 0
 *   3. Back button — clear PIN buffer, request TO_HOME transition
 *
 * Text rendering approach (v0.1.7 fix v4):
 *   Font-based labels (lv_label) produce missing-glyph blocks in this
 *   Hekate LVGL build — even with correct lv_style_copy() font inheritance.
 *   Root cause: the Hekate LVGL font configuration does not provide usable
 *   glyphs for lv_label text rendering in a standalone payload context.
 *
 *   Solution: Replace ALL lv_label text with BMP image assets, reusing the
 *   proven image rendering path (lv_img / lv_imgbtn) already used for
 *   Start and Gear buttons on the home screen.
 *
 *   PIN title: lv_img with pin_title.bmp (400x60, centered in container)
 *   Cancel:    lv_imgbtn with back_button_normal/pressed.bmp (160x96)
 *   Digits:    lv_imgbtn with pin_N_normal/pressed.bmp (80x80)
 *   Clear:     lv_imgbtn with pin_clear_normal/pressed.bmp (80x80)
 *   OK:        lv_imgbtn with pin_ok_normal/pressed.bmp (80x80)
 *   Display:   lv_img with pin_display_N.bmp (400x60)
 *
 *   No lv_label, no lv_style_t for text, no font pointers anywhere.
 *   Container and button styles remain for background/border only.
 *
 * P4-F: OK button only marks a verify request — never chainloads.
 * Central runtime consumes the request, calls kh_pin_verify(),
 * and only on success requests KH_UI_ACTION_CLEAN.
 */

#include <string.h>
#include <stdlib.h>

#include <bdk.h>
#include <soc/timer.h>
#include <libs/lvgl/lvgl.h>

#include "screen_pin.h"
#include "ui_runtime.h"
#include "kh_ui_asset_context.h"

/* ------------------------------------------------------------------ */
/* Constants                                                          */
/* ------------------------------------------------------------------ */

/* Container geometry (700x600, centered-ish) */
#define KH_PIN_CONTAINER_W   700
#define KH_PIN_CONTAINER_H   600
#define KH_PIN_CONTAINER_X   290
#define KH_PIN_CONTAINER_Y   60

/* PIN title: 400x60 */
#define KH_PIN_TITLE_X       (KH_PIN_CONTAINER_W - 400) / 2  /* 150 */
#define KH_PIN_TITLE_Y       20

/* PIN display: 400x60 */
#define KH_PIN_DISPLAY_X     (KH_PIN_CONTAINER_W - 400) / 2  /* 150 */
#define KH_PIN_DISPLAY_Y     90

/* Keypad button size */
#define KH_PIN_KEY_W         72
#define KH_PIN_KEY_H         72

/* Keypad grid: 3 columns, 4 rows */
#define KH_PIN_KEY_COLS      3
#define KH_PIN_KEY_ROWS      4
#define KH_PIN_KEY_GAP_H     16  /* Horizontal gap between columns */
#define KH_PIN_KEY_GAP_V     12  /* Vertical gap between rows */

/*
 * Column start positions (x offsets within container).
 * Centered in 700px container:
 *   total_width = 3 * 72 + 2 * 16 = 248
 *   start_x = (700 - 248) / 2 = 226
 */
#define KH_PIN_KEY_COL0      226
#define KH_PIN_KEY_COL1      (KH_PIN_KEY_COL0 + KH_PIN_KEY_W + KH_PIN_KEY_GAP_H)  /* 314 */
#define KH_PIN_KEY_COL2      (KH_PIN_KEY_COL1 + KH_PIN_KEY_W + KH_PIN_KEY_GAP_H)  /* 402 */

/* Row start positions (y offsets within container) */
#define KH_PIN_KEY_ROW0      160
#define KH_PIN_KEY_ROW1      (KH_PIN_KEY_ROW0 + KH_PIN_KEY_H + KH_PIN_KEY_GAP_V)  /* 244 */
#define KH_PIN_KEY_ROW2      (KH_PIN_KEY_ROW1 + KH_PIN_KEY_H + KH_PIN_KEY_GAP_V)  /* 328 */
#define KH_PIN_KEY_ROW3      (KH_PIN_KEY_ROW2 + KH_PIN_KEY_H + KH_PIN_KEY_GAP_V)  /* 412 */

/* Back button: 160x60 inside container */
#define KH_PIN_BACK_X        270
#define KH_PIN_BACK_Y        510
#define KH_PIN_BACK_W        160
#define KH_PIN_BACK_H        60

/* PIN buffer limits — exactly 4 digits */
#define KH_PIN_MAX_DIGITS    4
#define KH_PIN_BUFFER_SIZE   (KH_PIN_MAX_DIGITS + 1)  /* +1 for NUL */

/* ------------------------------------------------------------------ */
/* Explicit 12-entry button mapping table                             */
/*                                                                     */
/* Each entry defines one button in the 3x4 keypad grid.              */
/* Index maps to position:                                             */
/*   0-2:   Row 0 (1, 2, 3)                                           */
/*   3-5:   Row 1 (4, 5, 6)                                           */
/*   6-8:   Row 2 (7, 8, 9)                                           */
/*   9-11:  Row 3 (CLEAR, 0, OK)                                      */
/* ------------------------------------------------------------------ */

typedef enum {
	KH_BTN_DIGIT,   /* Digit button (0-9) */
	KH_BTN_CLEAR,   /* Clear/backspace */
	KH_BTN_OK       /* OK/confirm */
} kh_btn_type_t;

typedef struct {
	kh_btn_type_t type;   /* Button type */
	int digit;            /* Digit value (0-9, only for KH_BTN_DIGIT) */
	int col;              /* Column index (0-2) */
	int row;              /* Row index (0-3) */
} kh_keypad_entry_t;

static const kh_keypad_entry_t _kh_keypad_map[12] = {
	/* Row 0: digits 1, 2, 3 */
	{KH_BTN_DIGIT, 1, 0, 0},
	{KH_BTN_DIGIT, 2, 1, 0},
	{KH_BTN_DIGIT, 3, 2, 0},

	/* Row 1: digits 4, 5, 6 */
	{KH_BTN_DIGIT, 4, 0, 1},
	{KH_BTN_DIGIT, 5, 1, 1},
	{KH_BTN_DIGIT, 6, 2, 1},

	/* Row 2: digits 7, 8, 9 */
	{KH_BTN_DIGIT, 7, 0, 2},
	{KH_BTN_DIGIT, 8, 1, 2},
	{KH_BTN_DIGIT, 9, 2, 2},

	/* Row 3: CLEAR, 0, OK */
	{KH_BTN_CLEAR, 0, 0, 3},
	{KH_BTN_DIGIT, 0, 1, 3},
	{KH_BTN_OK,    0, 2, 3},
};

/* ------------------------------------------------------------------ */
/* Static styles — persistent copies, never stack-allocated           */
/* Initialised once on first kh_pin_screen_create() call.             */
/* ------------------------------------------------------------------ */

static lv_style_t kh_pin_bg_style;
static bool _kh_pin_styles_init = false;

/* ------------------------------------------------------------------ */
/* Static state                                                       */
/* ------------------------------------------------------------------ */

static lv_obj_t *_kh_pin_container = NULL;

/* PIN buffer — exactly 4 digits + NUL */
static char _kh_pin[KH_PIN_BUFFER_SIZE];
static uint8_t _kh_pin_len = 0;

/* Display image objects (5 stacked variants, one visible at a time) */
static lv_obj_t *_kh_pin_display[5];

/* P4-F: Verify request flag — set by OK callback, consumed by runtime */
static bool _kh_pin_verify_requested = false;

/* ------------------------------------------------------------------ */
/* Forward declarations                                               */
/* ------------------------------------------------------------------ */

static void _kh_pin_update_display(void);
static void _kh_pin_clear_buffer(void);

/* ------------------------------------------------------------------ */
/* Helper: secure zero of PIN buffer (volatile to prevent optimization)*/
/* ------------------------------------------------------------------ */

static void _kh_secure_zero(volatile char *buf, size_t len)
{
	volatile char *p = buf;
	while (len--)
		*p++ = 0;
}

/* ------------------------------------------------------------------ */
/* Button callbacks                                                    */
/* ------------------------------------------------------------------ */

/* Digit button callback (0-9) */
static lv_res_t _kh_on_digit_click(lv_obj_t *btn)
{
	if (!btn)
		return LV_RES_OK;

	/* Extract digit from user data */
	int digit = (int)(intptr_t)lv_obj_get_free_ptr(btn);

	/* Ignore if buffer is full */
	if (_kh_pin_len >= KH_PIN_MAX_DIGITS)
		return LV_RES_OK;

	/* Append digit to buffer */
	_kh_pin[_kh_pin_len] = '0' + (char)digit;
	_kh_pin[_kh_pin_len + 1] = '\0';
	_kh_pin_len++;

	/* Update display */
	_kh_pin_update_display();

	return LV_RES_OK;
}

/* Clear button callback */
static lv_res_t _kh_on_clear_click(lv_obj_t *btn)
{
	(void)btn;

	/* Clear the last digit (backspace) */
	if (_kh_pin_len > 0)
	{
		_kh_pin_len--;
		_kh_pin[_kh_pin_len] = '\0';
		_kh_pin_update_display();
	}

	return LV_RES_OK;
}

/* OK button callback — P4-F: request deferred verification */
static lv_res_t _kh_on_ok_click(lv_obj_t *btn)
{
	(void)btn;

	/* Ignore if fewer than 4 digits */
	if (_kh_pin_len < KH_PIN_MAX_DIGITS)
		return LV_RES_OK;

	/*
	 * P4-F: Mark a verify request for the central runtime.
	 * The callback must NOT hash, compare, chainload, or sleep.
	 * Only sets a flag — consumption happens in kh_ui_main_loop().
	 * First-request-wins: if already set, subsequent OK presses
	 * are silently ignored.
	 */
	if (!_kh_pin_verify_requested)
		_kh_pin_verify_requested = true;

	return LV_RES_OK;
}

/* Back button callback — cleanup point 3 */
static lv_res_t _kh_on_back_click(lv_obj_t *btn)
{
	(void)btn;

	/* Clear PIN buffer (cleanup point 3) */
	_kh_pin_clear_buffer();

	/* Request transition back to settings screen */
	kh_ui_request_transition(KH_UI_TRANSITION_TO_SETTINGS);

	return LV_RES_OK;
}

/* ------------------------------------------------------------------ */
/* Display update — show correct pin_display_N based on pin_len       */
/* ------------------------------------------------------------------ */

static void _kh_pin_update_display(void)
{
	/* Hide all display variants */
	for (int i = 0; i < 5; i++)
	{
		if (_kh_pin_display[i])
			lv_obj_set_hidden(_kh_pin_display[i], true);
	}

	/* Show the one matching pin_len (clamped to 0-4) */
	uint8_t idx = (_kh_pin_len <= 4) ? _kh_pin_len : 4;
	if (_kh_pin_display[idx])
		lv_obj_set_hidden(_kh_pin_display[idx], false);
}

/* ------------------------------------------------------------------ */
/* Clear PIN buffer — cleanup point helper                            */
/* ------------------------------------------------------------------ */

static void _kh_pin_clear_buffer(void)
{
	/* Secure zero the PIN buffer */
	_kh_secure_zero(_kh_pin, sizeof(_kh_pin));
	_kh_pin_len = 0;

	/* Reset display to pin_display_0 */
	_kh_pin_update_display();

	/* Clear verify request flag */
	_kh_pin_verify_requested = false;
}

/* ------------------------------------------------------------------ */
/* Style initialisation — called once from kh_pin_screen_create()     */
/* ------------------------------------------------------------------ */

static void _kh_pin_init_styles(void)
{
	if (_kh_pin_styles_init)
		return;

	/* Background container style (dark charcoal, opaque, rounded) */
	lv_style_copy(&kh_pin_bg_style, &lv_style_plain);
	kh_pin_bg_style.body.main_color = LV_COLOR_HEX(0x111111);
	kh_pin_bg_style.body.grad_color = LV_COLOR_HEX(0x111111);
	kh_pin_bg_style.body.radius = 8;
	kh_pin_bg_style.body.border.color = LV_COLOR_HEX(0x333333);
	kh_pin_bg_style.body.border.width = 2;
	kh_pin_bg_style.body.border.part = LV_BORDER_FULL;
	kh_pin_bg_style.body.empty = 0;  /* Not transparent */

	/*
	 * NOTE: No text styles needed — all text is rendered via BMP images.
	 * Font labels (lv_label) are not used in this file.
	 * See header comment for explanation of the font rendering issue.
	 */

	_kh_pin_styles_init = true;
}

/* ------------------------------------------------------------------ */
/* Helper: create a button at (col, row) with given type               */
/* ------------------------------------------------------------------ */

static lv_obj_t *_kh_create_keypad_button(int col, int row,
										   lv_img_dsc_t *normal, lv_img_dsc_t *pressed,
										   lv_action_t action, void *free_ptr)

{
	/* Calculate position from column/row indices */
	int x = KH_PIN_KEY_COL0 + col * (KH_PIN_KEY_W + KH_PIN_KEY_GAP_H);
	int y = KH_PIN_KEY_ROW0 + row * (KH_PIN_KEY_H + KH_PIN_KEY_GAP_V);

	lv_obj_t *btn = lv_btn_create(_kh_pin_container, NULL);
	if (!btn)
		return NULL;

	lv_obj_set_size(btn, KH_PIN_KEY_W, KH_PIN_KEY_H);
	lv_obj_set_pos(btn, x, y);

	/* Store free_ptr for callback (digit value or NULL) */
	lv_obj_set_free_ptr(btn, free_ptr);

	/* Set action */
	lv_btn_set_action(btn, LV_BTN_ACTION_CLICK, action);

	/* Create image inside button */
	if (normal)
	{
		lv_obj_t *img = lv_img_create(btn, NULL);
		if (img)
		{
			lv_img_set_src(img, normal);
			/* Center the image in the button */
			lv_obj_align(img, NULL, LV_ALIGN_CENTER, 0, 0);
		}
	}

	return btn;
}

/* ------------------------------------------------------------------ */
/* Public API                                                         */
/* ------------------------------------------------------------------ */

bool kh_pin_screen_create(const kh_ui_asset_context_t *ctx)
{
	/* Initialise styles once */
	_kh_pin_init_styles();

	/* Create root container (fixed position, fixed size) */
	_kh_pin_container = lv_cont_create(lv_scr_act(), NULL);
	if (!_kh_pin_container)
		return false;

	lv_obj_set_size(_kh_pin_container, KH_PIN_CONTAINER_W, KH_PIN_CONTAINER_H);
	lv_obj_set_pos(_kh_pin_container, KH_PIN_CONTAINER_X, KH_PIN_CONTAINER_Y);
	lv_cont_set_layout(_kh_pin_container, LV_LAYOUT_OFF);

	/* Set background style (persistent static copy, not theme) */
	lv_obj_set_style(_kh_pin_container, &kh_pin_bg_style);

	/*
	 * PIN title — lv_img with pin_title.bmp (400x60).
	 * No lv_label — font rendering is broken in this Hekate LVGL build.
	 */
	if (ctx && ctx->theme.items[KH_THEME_ASSET_PIN_TITLE])
	{
		lv_obj_t *title = lv_img_create(_kh_pin_container, NULL);
		if (title)
		{
			lv_img_set_src(title, ctx->theme.items[KH_THEME_ASSET_PIN_TITLE]);
			lv_obj_set_pos(title, KH_PIN_TITLE_X, KH_PIN_TITLE_Y);
		}
	}

	/*
	 * PIN display variants 0-4 — 5 lv_img objects stacked at same position.
	 * Only one is visible at a time, controlled by _kh_pin_update_display().
	 */
	for (int i = 0; i < 5; i++)
	{
		_kh_pin_display[i] = NULL;

		lv_img_dsc_t *disp_src = NULL;
		if (ctx)
		{
			disp_src = ctx->global.pin_display[i];
		}

		if (disp_src)
		{
			_kh_pin_display[i] = lv_img_create(_kh_pin_container, NULL);
			if (_kh_pin_display[i])
			{
				lv_img_set_src(_kh_pin_display[i], disp_src);
				lv_obj_set_pos(_kh_pin_display[i], KH_PIN_DISPLAY_X, KH_PIN_DISPLAY_Y);
			}
		}
	}

	/*
	 * Keypad: 12 buttons arranged in 4 rows × 3 columns.
	 * Built from the explicit 12-entry mapping table _kh_keypad_map.
	 *
	 * Row 0 (y=160): 1, 2, 3
	 * Row 1 (y=244): 4, 5, 6
	 * Row 2 (y=328): 7, 8, 9
	 * Row 3 (y=412): CLEAR, 0, OK
	 *
	 * Column x positions: 226, 314, 402
	 */
	for (int i = 0; i < 12; i++)
	{
		const kh_keypad_entry_t *entry = &_kh_keypad_map[i];

		switch (entry->type)
		{
		case KH_BTN_DIGIT:
		{
			/* Digit button: use global pin_digit_n/p assets */
			lv_img_dsc_t *normal = NULL;
			lv_img_dsc_t *pressed = NULL;

			if (ctx)
			{
				int d = entry->digit;
				if (d >= 0 && d < 10)
				{
					normal = ctx->global.pin_digit_n[d];
					pressed = ctx->global.pin_digit_p[d];
				}
			}

			_kh_create_keypad_button(entry->col, entry->row,
									 normal, pressed,
									 _kh_on_digit_click,
									 (void *)(intptr_t)entry->digit);
			break;
		}

		case KH_BTN_CLEAR:
		{
			/* Clear button: use global pin_clear_n/p assets */
			lv_img_dsc_t *normal = ctx ? ctx->global.pin_clear_n : NULL;
			lv_img_dsc_t *pressed = ctx ? ctx->global.pin_clear_p : NULL;

			_kh_create_keypad_button(entry->col, entry->row,
									 normal, pressed,
									 _kh_on_clear_click, NULL);
			break;
		}

		case KH_BTN_OK:
		{
			/* OK button: use global pin_ok_n/p assets */
			lv_img_dsc_t *normal = ctx ? ctx->global.pin_ok_n : NULL;
			lv_img_dsc_t *pressed = ctx ? ctx->global.pin_ok_p : NULL;

			_kh_create_keypad_button(entry->col, entry->row,
									 normal, pressed,
									 _kh_on_ok_click, NULL);
			break;
		}
		}
	}

	/*
	 * Back button — lv_imgbtn with back_button_normal/pressed.bmp (160x60).
	 * Uses the same lv_imgbtn pattern as Start/Gear buttons on home screen.
	 * The back_button assets say "TERUG" (Dutch for "BACK").
	 */
	if (ctx && ctx->theme.items[KH_THEME_ASSET_PIN_BACK_NORMAL]
		&& ctx->theme.items[KH_THEME_ASSET_PIN_BACK_PRESSED])
	{
		lv_obj_t *cancel = lv_imgbtn_create(_kh_pin_container, NULL);
		if (cancel)
		{
			lv_imgbtn_set_src(cancel, LV_BTN_STATE_REL, ctx->theme.items[KH_THEME_ASSET_PIN_BACK_NORMAL]);
			lv_imgbtn_set_src(cancel, LV_BTN_STATE_PR, ctx->theme.items[KH_THEME_ASSET_PIN_BACK_PRESSED]);
			lv_imgbtn_set_action(cancel, LV_BTN_ACTION_CLICK, _kh_on_back_click);
			lv_obj_set_pos(cancel, KH_PIN_BACK_X, KH_PIN_BACK_Y);
			lv_obj_set_size(cancel, KH_PIN_BACK_W, KH_PIN_BACK_H);
			/*
			 * Controlled Experiment 1 (v0.1.19): Force PR→REL state transition
			 * after final geometry is set. Same rationale as home screen imgbtns.
			 */
			lv_imgbtn_set_state(cancel, LV_BTN_STATE_PR);
			lv_imgbtn_set_state(cancel, LV_BTN_STATE_REL);
			lv_obj_invalidate(cancel);
		}
	}

	/* Start hidden — shown only on TO_PIN transition */
	lv_obj_set_hidden(_kh_pin_container, true);

	/* Initialise PIN buffer */
	_kh_pin_clear_buffer();

	return true;
}

void kh_pin_show(void)
{
	if (!_kh_pin_container)
		return;

	/* Cleanup point 2: Clear PIN buffer when showing */
	_kh_pin_clear_buffer();

	lv_obj_set_hidden(_kh_pin_container, false);
}

void kh_pin_hide(void)
{
	if (!_kh_pin_container)
		return;

	/* Cleanup point 1: Clear PIN buffer when hiding */
	_kh_pin_clear_buffer();

	lv_obj_set_hidden(_kh_pin_container, true);
}

void kh_pin_update(uint32_t now_ms)
{
	/*
	 * P4-F: No diagnostic timeout needed.
	 * The verify request is consumed synchronously in the main loop.
	 * This function is kept as a no-op stub for API compatibility.
	 */
	(void)now_ms;
}

/* ------------------------------------------------------------------ */
/* P4-F: Verify request consumption                                   */
/* ------------------------------------------------------------------ */

bool kh_pin_take_verify_request(char out_pin[5], uint8_t *out_len)
{
	if (!out_pin || !out_len)
		return false;

	*out_len = 0;

	/* Check if a verify request is pending */
	if (!_kh_pin_verify_requested)
		return false;

	/* Verify that exactly 4 digits are entered */
	if (_kh_pin_len != KH_PIN_MAX_DIGITS)
	{
		/* Invalid state — clear request and return false */
		_kh_pin_verify_requested = false;
		return false;
	}

	/* Copy PIN to output buffer */
	memcpy(out_pin, _kh_pin, KH_PIN_BUFFER_SIZE);
	*out_len = _kh_pin_len;

	/* Clear internal PIN buffer and request flag */
	_kh_pin_clear_buffer();

	return true;
}

void kh_pin_clear_input(void)
{
	_kh_pin_clear_buffer();
}

/* ------------------------------------------------------------------ */
/* Destroy the PIN screen container                                    */
/* ------------------------------------------------------------------ */

void kh_pin_destroy(void)
{
	if (_kh_pin_container)
	{
		lv_obj_del(_kh_pin_container);
		_kh_pin_container = NULL;
	}
	/* Clear display pointers — they were children of the container */
	for (int i = 0; i < 5; i++)
		_kh_pin_display[i] = NULL;
	_kh_pin_clear_buffer();
}
