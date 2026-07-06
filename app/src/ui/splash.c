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
 * KittenHATS — Splash screen display.
 *
 * Streams splash.bin from SD scanline-by-scanline to the framebuffer.
 * No large heap allocation — uses a fixed 2880-byte stack buffer.
 *
 * NOTE: This function does NOT call display_color_screen() or any other
 * display mode-changing functions. Window A pitch-mode must be configured
 * by the caller (main.c) before calling kh_splash_show().
 */

#include <string.h>
#include <stdlib.h>

#include <bdk.h>
#include <libs/fatfs/ff.h>
#include <storage/sd.h>

#include "splash.h"

/* Framebuffer: 720x1280, 32bpp = 3,686,400 bytes */
#define FB_SIZE (720 * 1280 * 4)

/* Splash file path on SD */
#define SPLASH_PATH "bootloader/kittenhats/splash.bin"

/* Max supported scanline stride (720px * 4 bytes) */
#define KH_SPLASH_MAX_STRIDE (720u * 4u)

/* Expected splash dimensions */
#define KH_SPLASH_EXPECTED_WIDTH  720
#define KH_SPLASH_EXPECTED_HEIGHT 1280
#define KH_SPLASH_EXPECTED_STRIDE (KH_SPLASH_EXPECTED_WIDTH * 4)
#define KH_SPLASH_EXPECTED_DATA_SIZE (KH_SPLASH_EXPECTED_WIDTH * KH_SPLASH_EXPECTED_HEIGHT * 4)

/* Forward declarations */
static kh_splash_result_t _kh_splash_validate_header(const kh_splash_header_t *hdr);

kh_splash_result_t kh_splash_show(void)
{
	FIL file;
	UINT bytes_read;
	kh_splash_header_t header;
	kh_splash_result_t result = KH_SPLASH_OK;

	/* 1. Mount SD card */
	if (sd_mount())
		return KH_SPLASH_SD_MOUNT_FAILED;

	/* 2. Open splash.bin */
	if (f_open(&file, SPLASH_PATH, FA_READ) != FR_OK)
	{
		sd_end();
		return KH_SPLASH_OPEN_FAILED;
	}

	/* 3. Read header (20 bytes) */
	if (f_read(&file, &header, KH_SPLASH_HEADER_SIZE, &bytes_read) != FR_OK ||
		bytes_read != KH_SPLASH_HEADER_SIZE)
	{
		f_close(&file);
		sd_end();
		return KH_SPLASH_HEADER_READ_FAILED;
	}

	/* 4. Validate header */
	result = _kh_splash_validate_header(&header);
	if (result != KH_SPLASH_OK)
	{
		f_close(&file);
		sd_end();
		return result;
	}

	/* 5. Verify file size matches expected */
	u32 file_size = f_size(&file);
	u32 expected_total = KH_SPLASH_HEADER_SIZE + header.data_size;
	if (file_size < expected_total)
	{
		f_close(&file);
		sd_end();
		return KH_SPLASH_INVALID_SIZE;
	}

	/* 6. Verify dimensions don't exceed framebuffer */
	if (header.width > KH_SPLASH_EXPECTED_WIDTH || header.height > KH_SPLASH_EXPECTED_HEIGHT)
	{
		f_close(&file);
		sd_end();
		return KH_SPLASH_INVALID_DIMENSIONS;
	}

	/* 7. Verify stride matches expected */
	u32 fb_stride = KH_SPLASH_EXPECTED_WIDTH * 4;
	if (header.stride != fb_stride)
	{
		f_close(&file);
		sd_end();
		return KH_SPLASH_INVALID_STRIDE;
	}

	/* 8. Stream scanline-by-scanline to framebuffer */
	u8 *fb = (u8 *)IPL_FB_ADDRESS;
	u8 line_buffer[KH_SPLASH_MAX_STRIDE];

	/* Center the image if smaller than framebuffer */
	u32 dst_x = (KH_SPLASH_EXPECTED_WIDTH - header.width) / 2;
	u32 dst_y = (KH_SPLASH_EXPECTED_HEIGHT - header.height) / 2;

	/* Clear framebuffer first (black background) */
	memset(fb, 0, FB_SIZE);

	for (u32 y = 0; y < header.height; y++)
	{
		/* Read one scanline (normal order from file) */
		if (f_read(&file, line_buffer, header.stride, &bytes_read) != FR_OK ||
			bytes_read != header.stride)
		{
			f_close(&file);
			sd_end();
			return KH_SPLASH_READ_FAILED;
		}

		/* Copy to framebuffer with both X-flip and Y-flip (180° rotation).
		 * The Tegra X1 display controller on this panel scans Window A
		 * from bottom to top AND right to left in pitch mode, so we need
		 * to reverse both axes. */
		u32 fb_y = header.height - 1u - y;
		for (u32 x = 0; x < header.width; x++)
		{
			u32 fb_x = header.width - 1u - x;
			memcpy(
				fb + ((dst_y + fb_y) * fb_stride) + ((dst_x + fb_x) * 4u),
				line_buffer + (x * 4u),
				4
			);
		}
	}

	/* 9. Close file and unmount SD */
	f_close(&file);
	sd_end();

	/* 10. Wait ~500ms so the splash is visible */
	msleep(500);

	return KH_SPLASH_OK;
}

static kh_splash_result_t _kh_splash_validate_header(const kh_splash_header_t *hdr)
{
	/* Check magic */
	if (hdr->magic != KH_SPLASH_MAGIC)
		return KH_SPLASH_INVALID_HEADER;

	/* Check version */
	if (hdr->version != 1)
		return KH_SPLASH_INVALID_HEADER;

	/* Check format */
	if (hdr->format != KH_SPLASH_FORMAT_B8G8R8A8)
		return KH_SPLASH_INVALID_HEADER;

	/* Check dimensions */
	if (hdr->width == 0 || hdr->height == 0)
		return KH_SPLASH_INVALID_HEADER;

	if (hdr->width > KH_SPLASH_EXPECTED_WIDTH || hdr->height > KH_SPLASH_EXPECTED_HEIGHT)
		return KH_SPLASH_INVALID_DIMENSIONS;

	/* Check stride */
	if (hdr->stride < hdr->width * 4)
		return KH_SPLASH_INVALID_STRIDE;

	/* Check data_size */
	if (hdr->data_size == 0)
		return KH_SPLASH_INVALID_HEADER;

	/* Check for integer overflow */
	u32 expected_data = (u32)hdr->width * (u32)hdr->height * 4;
	if (hdr->data_size != expected_data)
		return KH_SPLASH_INVALID_SIZE;

	return KH_SPLASH_OK;
}
