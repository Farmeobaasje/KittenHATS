/*
 * Copyright (c) 2025 KittenHATS Contributors
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * KittenHATS — Fase 2+3: boot storage patching for Hekatos chainload.
 *
 * Patches the boot_cfg_t structure inside a loaded Hekatos payload buffer
 * to force autoboot with FROM_ID matching "KIDS", or clears it for CLEAN mode.
 *
 * Offset 0x94 verified against Hekatos v6.5.3:
 *   app/vendor/hekate/bootloader/main.c lines 93-99, 115, 198
 *   app/vendor/hekate/bdk/utils/types.h lines 132-150
 *   app/vendor/hekate/bootloader/link.ld lines 7-9
 *   Hexdump of hekatos.bin: _start code ends ~0x64, literal pool to 0x93,
 *   then 0x84 bytes of zeros (boot_cfg_t) at 0x94-0x117.
 */

#ifndef _KH_BOOTCFG_H_
#define _KH_BOOTCFG_H_

#include <utils/types.h>

/* File offset of boot_cfg_t within a Hekatos payload binary.
 * Verified: PATCHED_RELOC_SZ = 0x94 is the size of _start code + literal pool.
 * boot_cfg_t sits immediately after, at file offset 0x94. */
#define KH_HEKATOS_BOOTCFG_FILE_OFFSET 0x94u

/* Total size of boot_cfg_t struct (packed, verified by static_assert). */
#define KH_HEKATOS_BOOTCFG_SIZE        0x84u

/* Minimum payload size needed to contain a full boot_cfg_t. */
#define KH_HEKATOS_BOOTCFG_MIN_SIZE \
	(KH_HEKATOS_BOOTCFG_FILE_OFFSET + KH_HEKATOS_BOOTCFG_SIZE)

/* Boot config flags — local copies to avoid vendor header dependency.
 * Values verified against Hekatos v6.5.3:
 *   app/vendor/hekate/bdk/utils/types.h lines 109-112 */
#define KH_BOOT_CFG_AUTOBOOT_EN BIT(0)
#define KH_BOOT_CFG_FROM_ID     BIT(2)

typedef enum {
	KH_BOOTCFG_OK               = 0,  /* Patch applied successfully */
	KH_BOOTCFG_NULL_PAYLOAD     = 1,  /* payload pointer is NULL */
	KH_BOOTCFG_PAYLOAD_TOO_SMALL = 2, /* payload smaller than MIN_SIZE */
} kh_bootcfg_result_t;

/*
 * Patch the boot_cfg_t inside a loaded Hekatos payload buffer for KIDS mode.
 *
 * Sets BOOT_CFG_AUTOBOOT_EN | BOOT_CFG_FROM_ID flags and writes "KIDS"
 * as the boot-from-ID string. Only modifies the first 12 bytes of boot_cfg_t
 * (boot_cfg, autoboot, autoboot_list, extra_cfg, id[8]).
 * The emummc_path and xt_str fields remain untouched.
 *
 * @param payload      Pointer to the loaded Hekatos binary buffer.
 * @param payload_size Size of the loaded binary in bytes.
 * @return KH_BOOTCFG_OK on success, or an error code.
 */
kh_bootcfg_result_t bootcfg_patch_kids(void *payload, u32 payload_size);

/*
 * Clear the boot_cfg_t inside a loaded Hekatos payload buffer for CLEAN mode.
 *
 * Zeroes the first 12 bytes of boot_cfg_t (boot_cfg, autoboot, autoboot_list,
 * extra_cfg, id[8]). This disables autoboot, so Hekatos shows the full Nyx menu.
 * The emummc_path and xt_str fields remain untouched.
 *
 * @param payload      Pointer to the loaded Hekatos binary buffer.
 * @param payload_size Size of the loaded binary in bytes.
 * @return KH_BOOTCFG_OK on success, or an error code.
 */
kh_bootcfg_result_t bootcfg_clear(void *payload, u32 payload_size);

#endif /* _KH_BOOTCFG_H_ */
