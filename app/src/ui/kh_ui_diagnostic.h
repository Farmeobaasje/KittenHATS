/*
 * Copyright (c) 2025 KittenHATS Contributors
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * KittenHATS — v0.1.17: Asset-level diagnostic instrumentation.
 *
 * DEBUG ONLY — provides typed failure halt with distinct colors for each
 * failure stage and asset index. Colors are DISTINCT from main.c's error
 * palette to avoid ambiguity.
 *
 * main.c error colors (A8B8G8R8):
 *   Purple  = 0xFFFF00FF  (SD mount failed)
 *   Yellow  = 0xFF00FFFF  (splash/hekatos not found)
 *   Cyan    = 0xFFFFFF00  (invalid header/size)
 *   Blue    = 0xFFFF0000  (header read failed)
 *   White   = 0xFFFFFFFF  (scanline read failed)
 *   Magenta = 0xFFFF00FF  (malloc failed)
 *   Orange  = 0xFF2288FF  (bootcfg error)
 *   Red     = 0xFF0000FF  (unknown error / invalid mode)
 *
 * Our diagnostic colors use DIFFERENT values:
 *   Dark green  = GLOBAL stage (0xFF00AA00)
 *   Teal        = THEME stage  (0xFF00AAAA)
 *   Brown       = SCREEN stage (0xFF4444AA)
 *
 * The asset index is encoded in the color's blue channel:
 *   color = stage_base | (index & 0xFF)
 *   Example: GLOBAL asset 7 = 0xFF00AA00 | 0x07 = 0xFF00AA07
 *
 * The failure status is encoded in the color's green channel:
 *   status_code << 8
 *   Example: FILE_READ (status=2) = 0x0200
 *
 * Combined: GLOBAL asset 7, FILE_READ failure:
 *   0xFF00AA00 | 0x0200 | 0x07 = 0xFF02AA07
 *
 * This allows the user to report the exact hex value shown on screen.
 */

#ifndef _KH_UI_DIAGNOSTIC_H_
#define _KH_UI_DIAGNOSTIC_H_

#include <display/di.h>
#include "kh_ui_asset_loader.h"

/* ------------------------------------------------------------------ */
/* Stage base colors — distinct from main.c's error palette           */
/* ------------------------------------------------------------------ */

#define KH_DBG_STAGE_GLOBAL  0xFF00AA00  /* Dark green */
#define KH_DBG_STAGE_THEME   0xFF00AAAA  /* Teal */
#define KH_DBG_STAGE_SCREEN  0xFF4444AA  /* Brown */

/* ------------------------------------------------------------------ */
/* Diagnostic halt — shows encoded failure info, never returns        */
/* ------------------------------------------------------------------ */

__attribute__((noreturn))
static inline void kh_diag_halt(u32 stage_color, int index,
                                const char *name, const char *path,
                                kh_asset_load_status_t status)
{
	/*
	 * Encode failure info into the display color:
	 *   Bits 31-24: alpha (0xFF)
	 *   Bits 23-16: status code
	 *   Bits 15-8:  stage base (upper bits)
	 *   Bits 7-0:   asset index
	 *
	 * The stage base provides the dominant hue.
	 * The status shifts the green channel.
	 * The index shifts the blue channel.
	 */
	u32 color = stage_color
	          | ((u32)status << 16)
	          | ((u32)(index & 0xFF));

	/* Show the encoded failure color permanently */
	while (1)
	{
		display_color_screen(color);
	}

	/* Never reached */
}

#endif /* _KH_UI_DIAGNOSTIC_H_ */
