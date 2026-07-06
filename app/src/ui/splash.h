/*
 * Copyright (c) 2025 KittenHATS Contributors
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * KittenHATS — Fase 1: splash screen display.
 */

#ifndef _KH_SPLASH_H_
#define _KH_SPLASH_H_

#include <utils/types.h>

/* KittenHATS splash file magic: "KHSP" */
#define KH_SPLASH_MAGIC  0x5048534Bu

/* Supported splash format identifiers */
#define KH_SPLASH_FORMAT_B8G8R8A8  1  /* Native Tegra X1 display format */

/* Splash file header (on-disk format, little-endian) */
typedef struct __attribute__((packed))
{
	u32 magic;       /* KH_SPLASH_MAGIC */
	u16 version;     /* Header version (currently 1) */
	u16 format;      /* Pixel format identifier */
	u16 width;       /* Image width in pixels */
	u16 height;      /* Image height in pixels */
	u32 stride;      /* Bytes per row (width * bytes_per_pixel) */
	u32 data_size;   /* Size of pixel data in bytes */
} kh_splash_header_t;

#define KH_SPLASH_HEADER_SIZE  sizeof(kh_splash_header_t)

/* Splash load result codes */
typedef enum
{
	KH_SPLASH_OK                  = 0,  /* Splash displayed successfully */
	KH_SPLASH_SD_MOUNT_FAILED     = 1,  /* SD card could not be mounted */
	KH_SPLASH_FILE_NOT_FOUND      = 2,  /* splash.bin not found on SD */
	KH_SPLASH_INVALID_SIZE        = 3,  /* File size doesn't match expected */
	KH_SPLASH_INVALID_HEADER      = 4,  /* Bad magic, version, or format */
	KH_SPLASH_OUT_OF_MEMORY       = 5,  /* malloc failed */
	KH_SPLASH_FRAMEBUFFER_ERROR   = 6,  /* Framebuffer write failed */
	KH_SPLASH_OPEN_FAILED         = 7,  /* f_open failed */
	KH_SPLASH_HEADER_READ_FAILED  = 8,  /* f_read of header failed */
	KH_SPLASH_READ_FAILED         = 9,  /* f_read of scanline failed */
	KH_SPLASH_INVALID_DIMENSIONS  = 10, /* width/height exceed max */
	KH_SPLASH_INVALID_STRIDE      = 11, /* stride mismatch */
} kh_splash_result_t;

/*
 * Show the KittenHATS splash screen.
 * Loads splash.bin from SD (bootloader/kittenhats/splash.bin) and
 * streams it scanline-by-scanline to the framebuffer at IPL_FB_ADDRESS.
 * Uses a fixed small stack buffer — no large heap allocation.
 *
 * Note: This function handles its own SD mount/unmount cycle.
 * Returns a result code indicating success or failure.
 */
kh_splash_result_t kh_splash_show(void);

#endif /* _KH_SPLASH_H_ */
