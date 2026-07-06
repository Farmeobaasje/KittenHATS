/*
 * Copyright (c) 2025 KittenHATS Contributors
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * KittenHATS — Chainload Hekatos from SD.
 *
 * Reads hekatos.bin from SD, patches boot_cfg for the requested mode
 * (KIDS: autoboot emuMMC via FROM_ID; CLEAN: clear boot config → full Nyx),
 * then copies the payload to RCM_PAYLOAD_ADDR, appends relocator metadata,
 * deinitializes hardware, and jumps to EXT_PAYLOAD_ADDR.
 *
 * Implements the same chainload mechanism as Hekate's bootloader/main.c
 * and nyx/nyx_gui/nyx.c: read payload, copy to RCM_PAYLOAD_ADDR,
 * append relocator, hw_deinit, jump to EXT_PAYLOAD_ADDR.
 *
 * Relocator constants verified against Hekatos v6.5.3:
 *   bootloader/main.c lines 93-98, 101-111
 *   nyx/nyx_gui/nyx.c lines 93-98, 100-110
 */

#include <string.h>
#include <stdlib.h>

#include <bdk.h>
#include <libs/fatfs/ff.h>
#include <storage/sd.h>

#include "chainload.h"
#include "bootcfg.h"

/*
 * Relocator constants — mirrors Hekatos bootloader/main.c and nyx/nyx_gui/nyx.c.
 * These are used to patch the relocator metadata so the payload
 * copies itself from EXT_PAYLOAD_ADDR to PATCHED_RELOC_ENTRY and starts there.
 */
#define PATCHED_RELOC_SZ    0x94
#define RELOC_META_OFF      0x7C
#define PATCHED_RELOC_STACK 0x40007000
#define PATCHED_RELOC_ENTRY 0x40010000
#define EXT_PAYLOAD_ADDR    0xC0000000
#define RCM_PAYLOAD_ADDR    (EXT_PAYLOAD_ADDR + ALIGN(PATCHED_RELOC_SZ, 0x10))

/* Max payload size: 192KB — same limit as Hekate uses (0x30000) */
#define MAX_PAYLOAD_SIZE    0x30000

/*
 * Append relocator metadata — mirrors Hekatos' _reloc_append().
 *
 * Copies the first PATCHED_RELOC_SZ bytes (boot_cfg + IPL header) from
 * RCM_PAYLOAD_ADDR to payload_src (EXT_PAYLOAD_ADDR), then fills in the
 * reloc_meta_t struct so the relocator knows where to copy and jump.
 *
 * Source: Hekatos bootloader/main.c lines 101-111
 */
static void _reloc_append(u32 payload_dst, u32 payload_src, u32 payload_size)
{
	/* Copy boot_cfg + IPL header from RCM_PAYLOAD_ADDR to EXT_PAYLOAD_ADDR */
	memcpy((u8 *)payload_src, (u8 *)RCM_PAYLOAD_ADDR, PATCHED_RELOC_SZ);

	/* Fill relocator metadata at payload_src + RELOC_META_OFF */
	volatile reloc_meta_t *relocator = (reloc_meta_t *)(payload_src + RELOC_META_OFF);

	relocator->start = payload_dst - ALIGN(PATCHED_RELOC_SZ, 0x10);
	relocator->stack = PATCHED_RELOC_STACK;
	relocator->end   = payload_dst + payload_size;
	relocator->ep    = payload_dst;
}

kh_chainload_result_t chainload_hekatos(kh_boot_mode_t mode)
{
	/* 1. Mount SD card */
	if (sd_mount())
		return KH_CHAINLOAD_SD_ERROR;

	/* 2. Read hekatos.bin from SD */
	u32 size = 0;
	void *buf = sd_file_read("bootloader/kittenhats/hekatos.bin", &size);
	if (!buf)
	{
		sd_end();
		return KH_CHAINLOAD_FILE_NOT_FOUND;
	}

	/* 3. Validate size: must fit in IRAM and have at least relocator header */
	if (size > MAX_PAYLOAD_SIZE || size < PATCHED_RELOC_SZ)
	{
		free(buf);
		sd_end();
		return KH_CHAINLOAD_INVALID_SIZE;
	}

	/* 4. Patch boot storage based on mode */
	kh_bootcfg_result_t bootcfg_result;

	switch (mode)
	{
	case KH_BOOT_MODE_KIDS:
		bootcfg_result = bootcfg_patch_kids(buf, size);
		break;

	case KH_BOOT_MODE_CLEAN:
		bootcfg_result = bootcfg_clear(buf, size);
		break;

	default:
		free(buf);
		sd_end();
		return KH_CHAINLOAD_INVALID_MODE;
	}

	if (bootcfg_result != KH_BOOTCFG_OK)
	{
		free(buf);
		sd_end();
		return KH_CHAINLOAD_BOOTCFG_ERROR;
	}

	/* 5. Copy payload to RCM_PAYLOAD_ADDR */
	memcpy((void *)RCM_PAYLOAD_ADDR, buf, size);

	/* 6. Free the SD buffer — no longer needed */
	free(buf);

	/* 7. Append relocator metadata */
	_reloc_append(PATCHED_RELOC_ENTRY, EXT_PAYLOAD_ADDR, ALIGN(size, 0x10));

	/* 8. Close SD */
	sd_end();

	/* 9. Deinitialize hardware (keep display on for visual continuity) */
	hw_deinit(false);

	/* 10. Jump to EXT_PAYLOAD_ADDR — the relocator will copy the payload
	 *     to PATCHED_RELOC_ENTRY (0x40010000) and start it there.
	 *     This function never returns on success. */
	void (*payload_ptr)() = (void *)EXT_PAYLOAD_ADDR;
	(*payload_ptr)();

	/* Should never reach here */
	return KH_CHAINLOAD_OK;
}
