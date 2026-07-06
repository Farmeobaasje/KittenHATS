/*
 * Copyright (c) 2025 KittenHATS Contributors
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * KittenHATS — Fase 1+3: chainload Hekatos from SD.
 *
 * Fase 3: Added kh_boot_mode_t to support KIDS and CLEAN modes.
 * chainload_hekatos() now accepts a boot mode parameter.
 */

#ifndef _KH_CHAINLOAD_H_
#define _KH_CHAINLOAD_H_

#include <utils/types.h>

/*
 * Boot modes for chainload.
 * KIDS:  autoboot to emuMMC via FROM_ID="KIDS"
 * CLEAN: clear boot config → full Nyx menu
 */
typedef enum
{
	KH_BOOT_MODE_KIDS  = 0,  /* Autoboot KIDS (emuMMC) */
	KH_BOOT_MODE_CLEAN = 1,  /* Clear boot config → Nyx */
} kh_boot_mode_t;

typedef enum {
	KH_CHAINLOAD_OK               = 0,  /* Success — never actually returned */
	KH_CHAINLOAD_SD_ERROR         = 1,  /* SD mount failed */
	KH_CHAINLOAD_FILE_NOT_FOUND   = 2,  /* hekatos.bin not found on SD */
	KH_CHAINLOAD_INVALID_SIZE     = 3,  /* File too large (>0x30000) or too small */
	KH_CHAINLOAD_OUT_OF_MEMORY    = 4,  /* malloc failed */
	KH_CHAINLOAD_BOOTCFG_ERROR    = 5,  /* Boot config patch failed */
	KH_CHAINLOAD_INVALID_MODE     = 6,  /* Invalid boot mode */
} kh_chainload_result_t;

/*
 * Chainload Hekatos from SD with the specified boot mode.
 *
 * @param mode  KH_BOOT_MODE_KIDS or KH_BOOT_MODE_CLEAN.
 * @return      Error code on failure. On success, this function never returns.
 */
kh_chainload_result_t chainload_hekatos(kh_boot_mode_t mode);

#endif /* _KH_CHAINLOAD_H_ */
