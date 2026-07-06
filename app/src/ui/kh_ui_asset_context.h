/*
 * Copyright (c) 2025 KittenHATS Contributors
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * KittenHATS — Refactor D Gate 1: Combined asset context with theme/lang.
 *
 * Wraps global assets + theme assets into a single context struct.
 * This is the primary interface passed to all screen creation functions.
 *
 * The context is loaded transactionally: all 32 global + all 13 theme
 * assets must load successfully before any screen is created.
 * On failure, the context is freed and no UI is started.
 *
 * Refactor D Gate 1: Added active_theme and active_language fields so
 * screens can query which theme/language is active without needing
 * separate runtime state. These are set once at boot and never change
 * (runtime theme/language switching is P5-D/E).
 */

#ifndef _KH_UI_ASSET_CONTEXT_H_
#define _KH_UI_ASSET_CONTEXT_H_

#include <utils/types.h>
#include <libs/lvgl/lvgl.h>

#include "kh_ui_global_assets.h"
#include "kh_ui_theme_assets.h"

/* ------------------------------------------------------------------ */
/* Combined asset context                                             */
/* ------------------------------------------------------------------ */

typedef struct _kh_ui_asset_context_t
{
	kh_ui_global_assets_t    global;          /* 32 global assets (common/) */
	kh_ui_theme_asset_set_t  theme;           /* 13 theme/language assets */
	kh_theme_t               active_theme;    /* Active theme profile */
	kh_language_t            active_language; /* Active language profile */
	bool                     loaded;          /* true if all assets loaded successfully */
} kh_ui_asset_context_t;

#endif /* _KH_UI_ASSET_CONTEXT_H_ */
