/*
 * Copyright (c) 2025 KittenHATS Contributors
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * KittenHATS — UI configuration loader and writer.
 *
 * Loads, validates, and persists UI language setting from/to ui.ini on SD.
 * Uses Hekate's ini_parse() which internally allocates heap via zalloc.
 * All parser allocations are freed via ini_free() before return.
 *
 * Loading: multi-candidate startup recovery (canonical → backup → temp).
 * Writing: atomic backup-swap strategy with post-write verification.
 *
 * Save failures are NEVER fatal. The runtime language remains active.
 * Only UI rebuild failure + old-language rebuild failure is fatal.
 */

#include <string.h>
#include <stdlib.h>

#include <bdk.h>
#include <libs/fatfs/ff.h>
#include <utils/ini.h>
#include <utils/list.h>

#include "kh_ui_config.h"

/* ------------------------------------------------------------------ */
/* Language name table — canonical lowercase values                   */
/* ------------------------------------------------------------------ */

static const char *_kh_lang_strings[KH_LANG_COUNT] = {
	[KH_LANG_NL] = "nl",
	[KH_LANG_EN] = "en",
	[KH_LANG_DE] = "de",
	[KH_LANG_ES] = "es",
	[KH_LANG_FR] = "fr",
};

/*
 * P5-E: Fixed canonical INI payload strings — one per language.
 * No s_printf needed. Length is compile-time known.
 */
static const char *_kh_language_ini_payload[KH_LANG_COUNT] = {
	[KH_LANG_NL] = "[ui]\nlanguage=nl\n",
	[KH_LANG_EN] = "[ui]\nlanguage=en\n",
	[KH_LANG_DE] = "[ui]\nlanguage=de\n",
	[KH_LANG_ES] = "[ui]\nlanguage=es\n",
	[KH_LANG_FR] = "[ui]\nlanguage=fr\n",
};

/* ------------------------------------------------------------------ */
/* P5-E: Fault injection hooks — no-op in production                  */
/* ------------------------------------------------------------------ */

/*
 * Each hook wraps a specific FatFs operation. Define the corresponding
 * KH_TEST_CONFIG_FAIL_* macro to inject a failure at that exact point.
 * The hook returns the injected FRESULT or passes through the real result.
 */

static FRESULT _kh_config_io_open(FIL *fp, const char *path, BYTE mode)
{
#ifdef KH_TEST_CONFIG_FAIL_OPEN_CANONICAL
	if (strcmp(path, KH_UI_CONFIG_PATH) == 0)
		return FR_DISK_ERR;
#endif
#ifdef KH_TEST_CONFIG_FAIL_OPEN_BACKUP
	if (strcmp(path, KH_UI_CONFIG_BAK) == 0)
		return FR_DISK_ERR;
#endif
#ifdef KH_TEST_CONFIG_FAIL_OPEN_TEMP
	if (strcmp(path, KH_UI_CONFIG_TMP) == 0)
		return FR_DISK_ERR;
#endif
	return f_open(fp, path, mode);
}

static FRESULT _kh_config_io_write(FIL *fp, const void *buff, UINT btw, UINT *bw)
{
#ifdef KH_TEST_CONFIG_FAIL_WRITE_TEMP
	return FR_DISK_ERR;
#else
	return f_write(fp, buff, btw, bw);
#endif
}

static FRESULT _kh_config_io_sync(FIL *fp)
{
#ifdef KH_TEST_CONFIG_FAIL_SYNC_TEMP
	return FR_DISK_ERR;
#else
	return f_sync(fp);
#endif
}

static FRESULT _kh_config_io_rename(const char *old, const char *newpath)
{
#ifdef KH_TEST_CONFIG_FAIL_RENAME_CANONICAL_TO_BACKUP
	if (strcmp(old, KH_UI_CONFIG_PATH) == 0 && strcmp(newpath, KH_UI_CONFIG_BAK) == 0)
		return FR_DISK_ERR;
#endif
#ifdef KH_TEST_CONFIG_FAIL_RENAME_TEMP_TO_CANONICAL
	if (strcmp(old, KH_UI_CONFIG_TMP) == 0 && strcmp(newpath, KH_UI_CONFIG_PATH) == 0)
		return FR_DISK_ERR;
#endif
#ifdef KH_TEST_CONFIG_FAIL_RESTORE_BACKUP
	if (strcmp(old, KH_UI_CONFIG_BAK) == 0 && strcmp(newpath, KH_UI_CONFIG_PATH) == 0)
		return FR_DISK_ERR;
#endif
	return f_rename(old, newpath);
}

static FRESULT _kh_config_io_unlink(const char *path)
{
#ifdef KH_TEST_CONFIG_FAIL_UNLINK_BACKUP
	if (strcmp(path, KH_UI_CONFIG_BAK) == 0)
		return FR_DISK_ERR;
#endif
	return f_unlink(path);
}

/* ------------------------------------------------------------------ */
/* Case-insensitive comparison helpers                                */
/* ------------------------------------------------------------------ */

/*
 * Convert a single ASCII character to lowercase.
 * Non-ASCII characters are returned unchanged.
 */
static char _kh_tolower(char c)
{
	if (c >= 'A' && c <= 'Z')
		return c - 'A' + 'a';
	return c;
}

/*
 * Case-insensitive string comparison.
 * Returns 0 if strings match (ignoring case), non-zero otherwise.
 * Only handles ASCII characters — sufficient for INI section/key names
 * and language values which are all ASCII.
 */
static int _kh_stricmp(const char *a, const char *b)
{
	if (!a || !b)
		return -1;

	while (*a && *b)
	{
		char ca = _kh_tolower(*a);
		char cb = _kh_tolower(*b);
		if (ca != cb)
			return (int)(unsigned char)ca - (int)(unsigned char)cb;
		a++;
		b++;
	}

	return (int)(unsigned char)*a - (int)(unsigned char)*b;
}

/* ------------------------------------------------------------------ */
/* Whitespace trimming helper                                         */
/* ------------------------------------------------------------------ */

/*
 * Trim leading and trailing whitespace from a string in-place.
 * Modifies the input string by moving the start pointer and
 * inserting NUL at the new end position.
 *
 * Whitespace characters: space (0x20), tab (0x09).
 *
 * @param str  Pointer to NUL-terminated string. Modified in-place.
 *             If NULL or empty, returns without modification.
 */
static void _kh_trim(char *str)
{
	if (!str || !*str)
		return;

	char *start = str;
	char *end;

	/* Trim leading whitespace */
	while (*start == ' ' || *start == '\t')
		start++;

	/* If all whitespace, return empty string */
	if (*start == '\0')
	{
		str[0] = '\0';
		return;
	}

	/* Trim trailing whitespace */
	end = start + strlen(start) - 1;
	while (end > start && (*end == ' ' || *end == '\t'))
		end--;

	/* Move trimmed string to beginning of buffer if needed */
	if (start != str)
		memmove(str, start, (size_t)(end - start + 1));

	str[end - start + 1] = '\0';
}

/* ------------------------------------------------------------------ */
/* Language value parser — case-insensitive                           */
/* ------------------------------------------------------------------ */

/*
 * Parse a language value string to kh_language_t enum.
 * Case-insensitive — accepts "nl", "NL", "Nl", "nL", etc.
 *
 * @param value  NUL-terminated string (already trimmed).
 * @param out    Output: parsed language enum value.
 * @return       true if value matched one of the 5 supported languages.
 */
static bool _kh_parse_language(const char *value, kh_language_t *out)
{
	if (!value || !out)
		return false;

	for (int i = 0; i < KH_LANG_COUNT; i++)
	{
		if (_kh_stricmp(value, _kh_lang_strings[i]) == 0)
		{
			*out = (kh_language_t)i;
			return true;
		}
	}

	return false;
}

/* ------------------------------------------------------------------ */
/* P5-E: Parse a single config file — internal helper                 */
/* ------------------------------------------------------------------ */

/*
 * Internal: parse a single config file and return candidate status.
 * Does NOT modify *config on failure — caller must reset before each call.
 *
 * @param path    SD path to config file to parse.
 * @param config  Output: filled with parsed values on VALID.
 * @return        Candidate status.
 */
static kh_ui_config_candidate_status_t
_kh_try_parse_internal(const char *path, kh_ui_config_t *config)
{
	FILINFO fno;
	FRESULT fr;
	link_t sections;
	ini_sec_t *ui_sec = NULL;
	bool found_language = false;
	kh_language_t parsed_lang = KH_LANG_NL;

	if (!path || !config)
		return KH_UI_CANDIDATE_IO_ERROR;

	/* ---- Step 1: Check file existence and size ---- */
	fr = f_stat(path, &fno);
	if (fr == FR_NO_FILE)
	{
		/* File absent — not an error */
		return KH_UI_CANDIDATE_MISSING;
	}
	if (fr != FR_OK)
	{
		/* Real filesystem error — content unknown */
		return KH_UI_CANDIDATE_IO_ERROR;
	}

	/* Reject oversized files */
	if (fno.fsize > KH_UI_CONFIG_MAX_SIZE)
	{
		/* File too large — treat as invalid content */
		return KH_UI_CANDIDATE_INVALID;
	}

	/* ---- Step 2: Parse INI file ---- */
	list_init(&sections);

	if (ini_parse(&sections, path, false) != 0)
	{
		/* Parse error — free any partial allocations */
		ini_free(&sections);
		return KH_UI_CANDIDATE_INVALID;
	}

	/* ---- Step 3: Find the LAST [ui] section ---- */
	LIST_FOREACH_ENTRY(ini_sec_t, iter, &sections, link)
	{
		if (iter->type != INI_CHOICE)
			continue;

		if (_kh_stricmp(iter->name, "ui") == 0)
			ui_sec = iter;
	}

	/* No [ui] section found — mark as loaded but invalid */
	if (!ui_sec)
	{
		config->loaded = true;
		config->valid  = false;
		config->language = KH_LANG_NL;
		ini_free(&sections);
		return KH_UI_CANDIDATE_INVALID;
	}

	/* ---- Step 4: Find the LAST "language" key within [ui] ---- */
	LIST_FOREACH_ENTRY(ini_kv_t, kv, &ui_sec->kvs, link)
	{
		if (_kh_stricmp(kv->key, "language") == 0)
		{
			size_t vlen = kv->val ? strlen(kv->val) : 0;
			if (vlen > 0)
			{
				char val_buf[64];
				size_t copy_len = vlen < sizeof(val_buf) - 1 ? vlen : sizeof(val_buf) - 1;
				memcpy(val_buf, kv->val, copy_len);
				val_buf[copy_len] = '\0';

				_kh_trim(val_buf);

				if (val_buf[0] != '\0' && _kh_parse_language(val_buf, &parsed_lang))
				{
					found_language = true;
				}
			}
		}
	}

	/* ---- Step 5: Commit results ---- */
	config->language = parsed_lang;
	config->loaded   = true;
	config->valid    = found_language;

	ini_free(&sections);

	return found_language ? KH_UI_CANDIDATE_VALID : KH_UI_CANDIDATE_INVALID;
}

/* ------------------------------------------------------------------ */
/* Public API                                                         */
/* ------------------------------------------------------------------ */

void kh_ui_config_set_defaults(kh_ui_config_t *config)
{
	if (!config)
		return;

	config->language = KH_LANG_NL;
	config->loaded   = false;
	config->valid    = false;
}

/* ------------------------------------------------------------------ */
/* P5-E: Public try_parse — wraps internal helper                     */
/* ------------------------------------------------------------------ */

kh_ui_config_candidate_status_t
kh_ui_config_try_parse(const char *path, kh_ui_config_t *config)
{
	return _kh_try_parse_internal(path, config);
}

/* ------------------------------------------------------------------ */
/* P5-E: Multi-candidate startup recovery                             */
/* ------------------------------------------------------------------ */

bool kh_ui_config_load(kh_ui_config_t *config)
{
	kh_ui_config_t candidate;
	kh_ui_config_candidate_status_t canonical_status;
	kh_ui_config_candidate_status_t backup_status;
	kh_ui_config_candidate_status_t temp_status;

	/* Set safe defaults first — on any failure, these remain */
	if (config)
		kh_ui_config_set_defaults(config);

	if (!config)
		return false;

	/* ---- Step 1: Try canonical (ui.ini) ---- */
	kh_ui_config_set_defaults(&candidate);
	canonical_status = _kh_try_parse_internal(KH_UI_CONFIG_PATH, &candidate);

	if (canonical_status == KH_UI_CANDIDATE_VALID)
	{
		/* Canonical is valid — use it directly */
		*config = candidate;
		return true;
	}

	/* ---- Step 2: Try backup (ui.ini.bak) ---- */
	kh_ui_config_set_defaults(&candidate);
	backup_status = _kh_try_parse_internal(KH_UI_CONFIG_BAK, &candidate);

	if (backup_status == KH_UI_CANDIDATE_VALID)
	{
		/* Promote backup to canonical */
		FRESULT fr_unlink = f_unlink(KH_UI_CONFIG_PATH);
		if (fr_unlink != FR_OK && fr_unlink != FR_NO_FILE)
		{
			/* I/O error during promotion — preserve backup, use defaults */
			return false;
		}

		FRESULT fr_rename = f_rename(KH_UI_CONFIG_BAK, KH_UI_CONFIG_PATH);
		if (fr_rename != FR_OK)
		{
			/* I/O error during promotion — preserve backup, use defaults */
			return false;
		}

		/* Re-parse promoted canonical to verify */
		kh_ui_config_t verify;
		kh_ui_config_set_defaults(&verify);
		kh_ui_config_candidate_status_t verify_status =
			_kh_try_parse_internal(KH_UI_CONFIG_PATH, &verify);

		if (verify_status == KH_UI_CANDIDATE_VALID)
		{
			*config = verify;
			return true;
		}

		/* Promoted canonical failed verification — use defaults */
		return false;
	}

	/* ---- Step 3: Try temp (ui.ini.tmp) ---- */
	kh_ui_config_set_defaults(&candidate);
	temp_status = _kh_try_parse_internal(KH_UI_CONFIG_TMP, &candidate);

	if (temp_status == KH_UI_CANDIDATE_VALID)
	{
		/* Promote temp to canonical */
		FRESULT fr_unlink = f_unlink(KH_UI_CONFIG_PATH);
		if (fr_unlink != FR_OK && fr_unlink != FR_NO_FILE)
		{
			return false;
		}

		FRESULT fr_rename = f_rename(KH_UI_CONFIG_TMP, KH_UI_CONFIG_PATH);
		if (fr_rename != FR_OK)
		{
			return false;
		}

		/* Re-parse promoted canonical to verify */
		kh_ui_config_t verify;
		kh_ui_config_set_defaults(&verify);
		kh_ui_config_candidate_status_t verify_status =
			_kh_try_parse_internal(KH_UI_CONFIG_PATH, &verify);

		if (verify_status == KH_UI_CANDIDATE_VALID)
		{
			*config = verify;
			return true;
		}

		return false;
	}

	/* ---- Step 4: All candidates failed — cleanup and use defaults ---- */

	/* Only delete proven INVALID candidates */
	if (canonical_status == KH_UI_CANDIDATE_INVALID)
		f_unlink(KH_UI_CONFIG_PATH);  /* ignore result */

	if (backup_status == KH_UI_CANDIDATE_INVALID)
		f_unlink(KH_UI_CONFIG_BAK);   /* ignore result */

	if (temp_status == KH_UI_CANDIDATE_INVALID)
		f_unlink(KH_UI_CONFIG_TMP);   /* ignore result */

	/* IO_ERROR candidates are PRESERVED (content unknown) */
	/* MISSING candidates need no action */

	/* Defaults already set at top of function */
	return false;
}

/* ------------------------------------------------------------------ */
/* P5-E: Persist language to ui.ini with atomic backup-swap           */
/* ------------------------------------------------------------------ */

kh_ui_config_save_status_t
kh_ui_config_save_language(kh_language_t language)
{
	FIL file;
	FRESULT fr;
	UINT bytes_written;
	const char *payload;
	UINT payload_len;

	/* ---- Step 1: Bounds-check ---- */
	/* KH_LANG_NL = 0, so language < KH_LANG_NL is always false for unsigned.
	 * Only check >= KH_LANG_COUNT to avoid -Wtype-limits warning. */
	if (language >= KH_LANG_COUNT)
		return KH_UI_CONFIG_SAVE_ERR_BOUNDS;


	payload = _kh_language_ini_payload[language];
	payload_len = (UINT)strlen(payload);

	/* ---- Step 2: Remove stale .tmp ---- */
	fr = _kh_config_io_unlink(KH_UI_CONFIG_TMP);
	if (fr != FR_OK && fr != FR_NO_FILE)
	{
		/* I/O error during cleanup — cannot proceed safely */
		return KH_UI_CONFIG_SAVE_ERR_CLEANUP;
	}

	/* ---- Step 3: f_open(.tmp, CREATE_ALWAYS | WRITE) ---- */
	fr = _kh_config_io_open(&file, KH_UI_CONFIG_TMP, FA_CREATE_ALWAYS | FA_WRITE);
	if (fr != FR_OK)
		return KH_UI_CONFIG_SAVE_ERR_OPEN;

	/* ---- Step 4: f_write canonical payload ---- */
	fr = _kh_config_io_write(&file, payload, payload_len, &bytes_written);
	if (fr != FR_OK || bytes_written != payload_len)
	{
		f_close(&file);
		return KH_UI_CONFIG_SAVE_ERR_WRITE;
	}

	/* ---- Step 5: f_sync to flush ---- */
	fr = _kh_config_io_sync(&file);
	if (fr != FR_OK)
	{
		f_close(&file);
		return KH_UI_CONFIG_SAVE_ERR_SYNC;
	}

	/* ---- Step 6: f_close ---- */
	f_close(&file);

	/* ---- Step 7: f_rename(canonical → .bak) ---- */
	fr = _kh_config_io_rename(KH_UI_CONFIG_PATH, KH_UI_CONFIG_BAK);
	if (fr != FR_OK)
	{
		/* Rename failed — .tmp remains, canonical unchanged */
		return KH_UI_CONFIG_SAVE_ERR_RENAME;
	}

	/* ---- Step 8: f_rename(.tmp → canonical) ---- */
	fr = _kh_config_io_rename(KH_UI_CONFIG_TMP, KH_UI_CONFIG_PATH);
	if (fr != FR_OK)
	{
		/*
		 * Promotion failed — restore .bak → canonical.
		 * .tmp remains on SD for potential recovery.
		 */
		FRESULT fr_restore = _kh_config_io_rename(KH_UI_CONFIG_BAK, KH_UI_CONFIG_PATH);
		if (fr_restore != FR_OK)
		{
			/* Restore also failed — double failure */
			return KH_UI_CONFIG_SAVE_ERR_RECOVERY;
		}
		return KH_UI_CONFIG_SAVE_ERR_RENAME;
	}

	/* ---- Step 9: Re-parse canonical to verify integrity ---- */
	{
		kh_ui_config_t verify;
		kh_ui_config_set_defaults(&verify);
		kh_ui_config_candidate_status_t vstatus =
			_kh_try_parse_internal(KH_UI_CONFIG_PATH, &verify);

		if (vstatus != KH_UI_CANDIDATE_VALID)
		{
			/*
			 * Verification failed — restore .bak → canonical.
			 * .tmp may still exist from failed promotion.
			 */
			FRESULT fr_restore = _kh_config_io_rename(KH_UI_CONFIG_BAK, KH_UI_CONFIG_PATH);
			if (fr_restore != FR_OK)
			{
				return KH_UI_CONFIG_SAVE_ERR_RECOVERY;
			}
			return KH_UI_CONFIG_SAVE_ERR_VALIDATE;
		}
	}

	/* ---- Step 10: Remove stale .bak (FR_NO_FILE acceptable) ---- */
	fr = _kh_config_io_unlink(KH_UI_CONFIG_BAK);
	if (fr != FR_OK && fr != FR_NO_FILE)
	{
		/* Backup cleanup failed — diagnostic only, save succeeded */
		return KH_UI_CONFIG_SAVE_OK_BACKUP_REMAINS;
	}

	return KH_UI_CONFIG_SAVE_OK;
}
