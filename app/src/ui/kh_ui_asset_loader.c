/*
 * Copyright (c) 2025 KittenHATS Contributors
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * KittenHATS — Low-level BMP asset loader implementation.
 *
 * Single BMP decoder. This is the ONLY compiled BMP decoder.
 *
 * Algorithm matches Nyx's bmp_to_lvimg_obj() (gui.c lines 655-738).
 * Each asset is a single allocation: sd_file_read() returns a buffer
 * containing [lv_img_dsc_t | padding(0x10) | pixels]. Freeing the
 * returned pointer frees everything.
 *
 * v0.1.17: Added kh_asset_load_bmp_diagnostic() with typed status codes
 * for per-asset failure identification.
 */

#include <string.h>
#include <stdlib.h>

#include <bdk.h>
#include <libs/fatfs/ff.h>
#include <storage/sd.h>

#include "kh_ui_asset_loader.h"

/* ------------------------------------------------------------------ */
/* BMP-to-LVGL-image converter — diagnostic version                   */
/* ------------------------------------------------------------------ */

kh_asset_load_status_t kh_asset_load_bmp_diagnostic(
	const char *path, lv_img_dsc_t **out_asset)
{
	/* Validate arguments */
	if (!path || !out_asset)
		return KH_ASSET_LOAD_ERR_ARGUMENT;

	*out_asset = NULL;

	u32 fsize;
	u8 *bitmap = sd_file_read(path, &fsize);
	if (!bitmap)
		return KH_ASSET_LOAD_ERR_FILE_READ;

	/* Minimum size: BMP header (14) + DIB header (at least 40) = 54 bytes */
	if (fsize < 54)
	{
		free(bitmap);
		return KH_ASSET_LOAD_ERR_FILE_TOO_SMALL;
	}

	struct _bmp_data
	{
		u32 size;
		u32 size_x;
		u32 size_y;
		u32 offset;
	};

	struct _bmp_data bmpData;

	/* Get values manually to avoid unaligned access. */
	bmpData.size   = bitmap[2]  | bitmap[3]  << 8 |
	                 bitmap[4]  << 16 | bitmap[5]  << 24;
	bmpData.offset = bitmap[10] | bitmap[11] << 8 |
	                 bitmap[12] << 16 | bitmap[13] << 24;
	bmpData.size_x = bitmap[18] | bitmap[19] << 8 |
	                 bitmap[20] << 16 | bitmap[21] << 24;
	bmpData.size_y = bitmap[22] | bitmap[23] << 8 |
	                 bitmap[24] << 16 | bitmap[25] << 24;

	/* Sanity check. */
	if (bitmap[0] != 'B' || bitmap[1] != 'M')
	{
		free(bitmap);
		return KH_ASSET_LOAD_ERR_BMP_SIGNATURE;
	}

	if (bitmap[28] != 32)
	{
		free(bitmap);
		return KH_ASSET_LOAD_ERR_BPP;
	}

	if (bmpData.size > fsize)
	{
		free(bitmap);
		return KH_ASSET_LOAD_ERR_SIZE_OVERFLOW;
	}

	/* Check if non-default Bottom-Top. */
	bool flipped = false;
	if (bmpData.size_y & 0x80000000)
	{
		bmpData.size_y = ~(bmpData.size_y) + 1;
		flipped = true;
	}

	lv_img_dsc_t *img_desc = (lv_img_dsc_t *)bitmap;
	uptr offset_copy = ALIGN((uptr)bitmap + sizeof(lv_img_dsc_t), 0x10);

	img_desc->header.always_zero = 0;
	img_desc->header.w = bmpData.size_x;
	img_desc->header.h = bmpData.size_y;
	img_desc->header.cf = LV_IMG_CF_TRUE_COLOR_ALPHA;
	img_desc->data_size = bmpData.size - bmpData.offset;
	img_desc->data = (u8 *)offset_copy;

	u32 *tmp = (u32 *)malloc(bmpData.size);
	if (!tmp)
	{
		free(bitmap);
		return KH_ASSET_LOAD_ERR_ALLOC;
	}

	u32 *tmp2 = (u32 *)offset_copy;

	/* Copy the unaligned data to an aligned buffer. */
	memcpy((u8 *)tmp, bitmap + bmpData.offset, img_desc->data_size);
	u32 j = 0;

	if (!flipped)
	{
		/* Standard bottom-up BMP: flip rows to top-down for LVGL. */
		for (u32 y = 0; y < bmpData.size_y; y++)
		{
			for (u32 x = 0; x < bmpData.size_x; x++)
				tmp2[j++] = tmp[(bmpData.size_y - 1 - y) * bmpData.size_x + x];
		}
	}
	else
	{
		/* Already top-down: copy as-is. */
		for (u32 y = 0; y < bmpData.size_y; y++)
		{
			for (u32 x = 0; x < bmpData.size_x; x++)
				tmp2[j++] = tmp[y * bmpData.size_x + x];
		}
	}

	free(tmp);

	*out_asset = (lv_img_dsc_t *)bitmap;
	return KH_ASSET_LOAD_OK;
}

/* ------------------------------------------------------------------ */
/* Legacy wrapper — returns NULL on failure (no status)               */
/* ------------------------------------------------------------------ */

lv_img_dsc_t *kh_asset_load_bmp(const char *path)
{
	lv_img_dsc_t *asset = NULL;
	kh_asset_load_status_t status = kh_asset_load_bmp_diagnostic(path, &asset);
	(void)status;
	return asset;
}

/* ------------------------------------------------------------------ */
/* Asset free helper — NULL-safe                                      */
/* ------------------------------------------------------------------ */

void kh_asset_free(lv_img_dsc_t **ptr)
{
	if (!ptr || !(*ptr))
		return;

	free(*ptr);
	*ptr = NULL;
}
