/*
 * Copyright (c) 2025 KittenHATS Contributors
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * KittenHATS — Refactor C: Low-level BMP asset loader.
 *
 * Single public BMP decoder extracted from ui_assets.c.
 * This is the ONLY compiled BMP decoder in the project.
 * ui_assets.c/.h remain as deprecated reference only.
 *
 * Loads a BMP file from SD and converts to lv_img_dsc_t.
 * BMP format expected: 32-bit BGRA, bottom-up (standard BMP).
 * Returns a single allocation: [lv_img_dsc_t | padding(0x10) | pixels].
 * The caller owns the allocation and must call kh_asset_free().
 *
 * v0.1.17: Added typed diagnostic loader (kh_asset_load_bmp_diagnostic)
 * for per-asset failure identification.
 */

#ifndef _KH_UI_ASSET_LOADER_H_
#define _KH_UI_ASSET_LOADER_H_

#include <utils/types.h>
#include <libs/lvgl/lvgl.h>

/* ------------------------------------------------------------------ */
/* Typed asset load status — distinguishes exact failure mode          */
/* ------------------------------------------------------------------ */

typedef enum _kh_asset_load_status_t
{
	KH_ASSET_LOAD_OK               = 0,
	KH_ASSET_LOAD_ERR_ARGUMENT     = 1,  /* NULL path or out pointer */
	KH_ASSET_LOAD_ERR_FILE_READ    = 2,  /* sd_file_read returned NULL */
	KH_ASSET_LOAD_ERR_FILE_TOO_SMALL = 3, /* File smaller than BMP header */
	KH_ASSET_LOAD_ERR_BMP_SIGNATURE = 4,  /* Missing 'BM' magic */
	KH_ASSET_LOAD_ERR_BPP          = 5,  /* Not 32 bits per pixel */
	KH_ASSET_LOAD_ERR_SIZE_OVERFLOW = 6, /* bmpData.size > fsize */
	KH_ASSET_LOAD_ERR_ALLOC        = 7,  /* malloc failed */
	KH_ASSET_LOAD_ERR_INTERNAL     = 8,  /* Unexpected decoder error */
} kh_asset_load_status_t;

/*
 * Load a BMP file from SD and convert to lv_img_dsc_t.
 * Returns NULL on failure (legacy API, no status).
 *
 * @param path  Full SD path to the BMP file.
 * @return      Pointer to lv_img_dsc_t, or NULL on failure.
 */
lv_img_dsc_t *kh_asset_load_bmp(const char *path);

/*
 * Diagnostic version — returns typed status instead of NULL.
 * Distinguishes file-read failure from BMP validation failure.
 *
 * @param path      Full SD path to the BMP file.
 * @param out_asset Output pointer for loaded descriptor (NULL on failure).
 * @return          kh_asset_load_status_t with exact failure reason.
 */
kh_asset_load_status_t kh_asset_load_bmp_diagnostic(
	const char *path, lv_img_dsc_t **out_asset);

/*
 * Free a BMP asset previously loaded by kh_asset_load_bmp().
 * NULL-safe: if ptr is NULL or *ptr is NULL, does nothing.
 * Sets *ptr to NULL after freeing.
 *
 * @param ptr  Pointer to the lv_img_dsc_t pointer to free.
 */
void kh_asset_free(lv_img_dsc_t **ptr);

#endif /* _KH_UI_ASSET_LOADER_H_ */
