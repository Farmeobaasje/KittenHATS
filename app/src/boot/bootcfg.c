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
 * KittenHATS — Boot storage patching for Hekatos chainload.
 *
 * Patches the boot_cfg_t structure inside a loaded Hekatos payload buffer
 * at file offset 0x94. Only modifies the first 12 bytes (boot_cfg flags,
 * autoboot, autoboot_list, extra_cfg, and id[8]).
 *
 * Offset 0x94 verified against Hekatos v6.5.3:
 *   - bootloader/main.c: PATCHED_RELOC_SZ = 0x94 (line 94)
 *   - bootloader/main.c: is_ipl_updated() uses buf + 0x94 + sizeof(boot_cfg_t) (line 115)
 *   - bootloader/main.c: memcpy to RCM_PAYLOAD_ADDR + 0x94 (line 198)
 *   - bdk/utils/types.h: boot_cfg_t = 0x84 bytes packed (line 150)
 *   - bootloader/link.ld: ._boot_cfg after .text._start (line 8)
 *   - Hexdump: _start code ends ~0x64, literal pool to 0x93,
 *     then 0x84 bytes of zeros at 0x94-0x117
 */

#include <string.h>

#include "bootcfg.h"

/* Compile-time checks for critical constants.
 * These verify our local definitions match the vendor's boot_cfg_t layout.
 * boot_cfg_t is 0x84 bytes packed (verified by Hekatos static_assert). */
#if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L
_Static_assert(KH_HEKATOS_BOOTCFG_SIZE == 0x84,
	"KH_HEKATOS_BOOTCFG_SIZE must match sizeof(boot_cfg_t) = 0x84");
_Static_assert(KH_HEKATOS_BOOTCFG_FILE_OFFSET == 0x94,
	"KH_HEKATOS_BOOTCFG_FILE_OFFSET must match PATCHED_RELOC_SZ = 0x94");
_Static_assert(KH_BOOT_CFG_AUTOBOOT_EN == 0x01,
	"KH_BOOT_CFG_AUTOBOOT_EN must be BIT(0) = 0x01");
_Static_assert(KH_BOOT_CFG_FROM_ID == 0x04,
	"KH_BOOT_CFG_FROM_ID must be BIT(2) = 0x04");
#endif

kh_bootcfg_result_t bootcfg_clear(void *payload, u32 payload_size)
{
	u8 *cfg;

	/* 1. Null pointer check */
	if (!payload)
		return KH_BOOTCFG_NULL_PAYLOAD;

	/* 2. Size check: must contain full boot_cfg_t at offset 0x94 */
	if (payload_size < KH_HEKATOS_BOOTCFG_MIN_SIZE)
		return KH_BOOTCFG_PAYLOAD_TOO_SMALL;

	/* 3. Point to boot_cfg_t inside the loaded binary */
	cfg = (u8 *)payload + KH_HEKATOS_BOOTCFG_FILE_OFFSET;

	/*
	 * 4. Clear first 12 bytes of boot_cfg_t:
	 *    boot_cfg[0], autoboot[1], autoboot_list[2], extra_cfg[3],
	 *    id[8] at offset 4-11.
	 *    This disables autoboot, so Hekatos shows the full Nyx menu.
	 *    emummc_path (offset 0x0C) and xt_str remain untouched.
	 */
	memset(cfg, 0, 12);

	return KH_BOOTCFG_OK;
}

kh_bootcfg_result_t bootcfg_patch_kids(void *payload, u32 payload_size)
{
	u8 *cfg;

	/* 1. Null pointer check */
	if (!payload)
		return KH_BOOTCFG_NULL_PAYLOAD;

	/* 2. Size check: must contain full boot_cfg_t at offset 0x94 */
	if (payload_size < KH_HEKATOS_BOOTCFG_MIN_SIZE)
		return KH_BOOTCFG_PAYLOAD_TOO_SMALL;

	/* 3. Point to boot_cfg_t inside the loaded binary */
	cfg = (u8 *)payload + KH_HEKATOS_BOOTCFG_FILE_OFFSET;

	/*
	 * 4. Set boot config flags:
	 *    boot_cfg[0] = BOOT_CFG_AUTOBOOT_EN | BOOT_CFG_FROM_ID = 0x05
	 *    autoboot[1] = 0
	 *    autoboot_list[2] = 0
	 *    extra_cfg[3] = 0
	 */
	cfg[0] = KH_BOOT_CFG_AUTOBOOT_EN | KH_BOOT_CFG_FROM_ID;
	cfg[1] = 0;
	cfg[2] = 0;
	cfg[3] = 0;

	/*
	 * 5. Set ID buffer: clear all 8 bytes, then copy "KIDS".
	 *    id[8] is at offset 0x04 within boot_cfg_t.
	 *    Must be null-terminated (7 chars + NUL max).
	 */
	memset(cfg + 4, 0, 8);
	memcpy(cfg + 4, "KIDS", 4);

	/*
	 * 6. emummc_path (offset 0x0C within boot_cfg_t) and all remaining
	 *    bytes (0x0C through 0x83) are intentionally left untouched.
	 *    Only the first 12 bytes of boot_cfg_t were modified.
	 */

	return KH_BOOTCFG_OK;
}
