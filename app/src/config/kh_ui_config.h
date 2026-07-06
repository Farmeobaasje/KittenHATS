/*
 * Copyright (c) 2025 KittenHATS Contributors
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * KittenHATS — P5-C/P5-E: UI configuration loader and writer.
 *
 * Loads, validates, and persists UI language setting from/to ui.ini on SD.
 * Uses Hekate's ini_parse() which internally allocates heap via zalloc.
 * All parser allocations are freed via ini_free() before return.
 *
 * Config file paths (fixed, no s_printf):
 *   bootloader/kittenhats/ui.ini       — canonical config
 *   bootloader/kittenhats/ui.ini.bak   — backup (previous valid config)
 *   bootloader/kittenhats/ui.ini.tmp   — temp (in-progress write)
 *
 * Supported [ui] section schema (1 key):
 *   language = nl | en | de | es | fr
 *
 * P5-C scope: config loading and validation only.
 * P5-E scope: config writing (persist language to ui.ini).
 *
 * Case-insensitive section name, key name, and language value.
 * Whitespace around key and value is trimmed.
 * Last textual occurrence wins for duplicate sections/keys.
 * Hard file-size limit: 4096 bytes before ini_parse.
 */

#ifndef _KH_UI_CONFIG_H_
#define _KH_UI_CONFIG_H_

#include <utils/types.h>
#include "../ui/kh_ui_theme_assets.h"  /* for kh_language_t */

/* ------------------------------------------------------------------ */
/* Constants                                                          */
/* ------------------------------------------------------------------ */

/* SD path for UI config files (fixed — no s_printf needed) */
#define KH_UI_CONFIG_PATH "bootloader/kittenhats/ui.ini"
#define KH_UI_CONFIG_TMP  "bootloader/kittenhats/ui.ini.tmp"
#define KH_UI_CONFIG_BAK  "bootloader/kittenhats/ui.ini.bak"

/* Hard file-size limit — do not parse files larger than this */
#define KH_UI_CONFIG_MAX_SIZE 4096

/* ------------------------------------------------------------------ */
/* Types                                                              */
/* ------------------------------------------------------------------ */

/*
 * UI configuration — loaded from ui.ini [ui] section.
 *
 * Fields are intentionally separate:
 *   - language: the PARSED language value (defaults to KH_LANG_NL)
 *   - loaded:   true if ui.ini was found and parsed successfully
 *   - valid:    true if the language value was one of the 5 supported
 *
 * The runtime owner (screen_home.c) decides which language to APPLY.
 */
typedef struct {
	kh_language_t language;  /* Parsed language (defaults to KH_LANG_NL) */
	bool          loaded;    /* true if ui.ini was successfully parsed */
	bool          valid;     /* true if language value was valid */
} kh_ui_config_t;

/*
 * P5-E: Save status — returned by kh_ui_config_save_language().
 * Save failures are NEVER fatal. The runtime language remains active.
 * Only UI rebuild failure + old-language rebuild failure is fatal.
 */
typedef enum {
	/* Success codes */
	KH_UI_CONFIG_SAVE_OK               = 0,  /* Saved and verified successfully */
	KH_UI_CONFIG_SAVE_OK_BACKUP_REMAINS = 1, /* Saved OK, stale .bak cleanup failed (diagnostic) */

	/* Failure codes — all non-fatal, runtime language stays active */
	KH_UI_CONFIG_SAVE_ERR_NULL         = 2,  /* NULL parameter */
	KH_UI_CONFIG_SAVE_ERR_BOUNDS       = 3,  /* language enum out of bounds */
	KH_UI_CONFIG_SAVE_ERR_CLEANUP      = 4,  /* Stale .tmp cleanup failed (I/O error) */
	KH_UI_CONFIG_SAVE_ERR_OPEN         = 5,  /* f_open(.tmp) failed */
	KH_UI_CONFIG_SAVE_ERR_WRITE        = 6,  /* f_write short or failed */
	KH_UI_CONFIG_SAVE_ERR_SYNC         = 7,  /* f_sync failed */
	KH_UI_CONFIG_SAVE_ERR_RENAME       = 8,  /* f_rename failed (promotion or backup) */
	KH_UI_CONFIG_SAVE_ERR_VALIDATE     = 9,  /* Re-parse of promoted canonical failed */
	KH_UI_CONFIG_SAVE_ERR_RECOVERY     = 10, /* Backup restore also failed */
} kh_ui_config_save_status_t;

/*
 * P5-E: Per-candidate candidate status for startup recovery.
 * Used to distinguish "file absent" from "invalid content" from "I/O error".
 */
typedef enum {
	KH_UI_CANDIDATE_MISSING   = 0,  /* FR_NO_FILE — file does not exist */
	KH_UI_CANDIDATE_VALID     = 1,  /* Parsed and validated successfully */
	KH_UI_CANDIDATE_INVALID   = 2,  /* Parsed but content invalid */
	KH_UI_CANDIDATE_IO_ERROR  = 3,  /* Filesystem error, content unknown */
} kh_ui_config_candidate_status_t;

/* ------------------------------------------------------------------ */
/* Public API                                                         */
/* ------------------------------------------------------------------ */

/*
 * Set config to safe defaults.
 * language = KH_LANG_NL, loaded = false, valid = false.
 *
 * @param config  Pointer to config struct to initialize.
 */
void kh_ui_config_set_defaults(kh_ui_config_t *config);

/*
 * Load and parse ui.ini from SD with multi-candidate recovery.
 *
 * SD must already be mounted (mounted in kh_home_screen() before LVGL init).
 * Uses ini_parse() which internally allocates heap via zalloc.
 * All parser allocations are freed via ini_free() before return.
 *
 * Recovery order:
 *   1. Try canonical (ui.ini) — if VALID, use it
 *   2. Try backup (ui.ini.bak) — if VALID, promote to canonical, re-parse, use
 *   3. Try temp (ui.ini.tmp) — if VALID, promote to canonical, re-parse, use
 *   4. All failed: use NL defaults
 *      - INVALID candidates: deleted (proven corrupt)
 *      - IO_ERROR candidates: preserved (content unknown)
 *      - MISSING candidates: ignored
 *
 * @param config  Output: filled with parsed values or defaults.
 * @return        true if a valid config was found (from any candidate).
 *                false if all candidates failed (defaults used).
 */
bool kh_ui_config_load(kh_ui_config_t *config);

/*
 * P5-E: Parse a single config file and return candidate status.
 * Does NOT modify *config on failure — caller must reset before each call.
 *
 * @param path    SD path to config file to parse.
 * @param config  Output: filled with parsed values on VALID.
 * @return        Candidate status (MISSING/VALID/INVALID/IO_ERROR).
 */
kh_ui_config_candidate_status_t
kh_ui_config_try_parse(const char *path, kh_ui_config_t *config);

/*
 * P5-E: Persist language to ui.ini with atomic backup-swap strategy.
 *
 * Write flow:
 *   1. Bounds-check language enum
 *   2. Remove stale .tmp (FR_NO_FILE is acceptable)
 *   3. f_open(.tmp, CREATE_ALWAYS | WRITE)
 *   4. f_write canonical payload (fixed string from lookup)
 *   5. f_sync to flush
 *   6. f_close
 *   7. f_rename(canonical → .bak) — existing .bak is overwritten
 *   8. f_rename(.tmp → canonical)
 *   9. Re-parse canonical to verify integrity
 *   10. If verify OK: f_unlink(.bak) — FR_NO_FILE acceptable
 *   11. If verify fails: restore .bak → canonical
 *   12. If restore also fails: return ERR_RECOVERY
 *
 * Save failures are NEVER fatal. The runtime language remains active.
 *
 * @param language  Language to persist (KH_LANG_NL through KH_LANG_FR).
 * @return          Save status code.
 */
kh_ui_config_save_status_t
kh_ui_config_save_language(kh_language_t language);

#endif /* _KH_UI_CONFIG_H_ */
