/*
 * Copyright (c) 2025 KittenHATS Contributors
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * KittenHATS — PIN configuration loader.
 *
 * Loads and validates PIN configuration from kittenhats.ini on SD.
 * Uses Hekate's ini_parse() which internally allocates heap via zalloc.
 * All parser allocations are freed via ini_free() before return.
 *
 * Validation pipeline:
 *   1. f_stat() to check file existence (distinguishes FILE_NOT_FOUND)
 *   2. ini_parse() to parse the INI file
 *   3. Find exactly one [security] section (type == INI_CHOICE)
 *   4. Validate keys: all 5 required, no duplicates, no unknown keys
 *   5. Validate values: enabled="1", scheme="sha256-salt-v1", length="4"
 *   6. Hex-decode salt (32 hex chars -> 16 bytes) and hash (64 hex chars -> 32 bytes)
 *   7. On any failure: clear out_config, return specific error code
 *
 * Config loading and validation only. No SHA-256 hashing, no PIN comparison.
 */

#include <string.h>
#include <stdlib.h>

#include <bdk.h>
#include <libs/fatfs/ff.h>
#include <utils/ini.h>
#include <utils/list.h>

#include "kh_config.h"

/* ------------------------------------------------------------------ */
/* Hex decoder — validates fully before committing                    */
/* ------------------------------------------------------------------ */

/*
 * Convert a single hex character to its nibble value.
 * Returns -1 on invalid character.
 */
static int _kh_hex_nibble(char c)
{
	if (c >= '0' && c <= '9')
		return c - '0';
	if (c >= 'a' && c <= 'f')
		return c - 'a' + 10;
	if (c >= 'A' && c <= 'F')
		return c - 'A' + 10;
	return -1;
}

/*
 * Decode a hex string to a byte array.
 * Validates ALL characters first, then writes output.
 *
 * @param hex     Input hex string (NUL-terminated).
 * @param out     Output byte array.
 * @param out_len Expected number of output bytes.
 * @return        true on success, false on invalid input.
 */
static bool _kh_hex_decode(const char *hex, uint8_t *out, size_t out_len)
{
	if (!hex || !out || out_len == 0)
		return false;

	/* Expected hex string length = 2 * out_len */
	size_t hex_len = strlen(hex);
	if (hex_len != out_len * 2)
		return false;

	/* Phase 1: validate all characters */
	for (size_t i = 0; i < hex_len; i++)
	{
		if (_kh_hex_nibble(hex[i]) < 0)
			return false;
	}

	/* Phase 2: convert (all chars are valid at this point) */
	for (size_t i = 0; i < out_len; i++)
	{
		int hi = _kh_hex_nibble(hex[i * 2]);
		int lo = _kh_hex_nibble(hex[i * 2 + 1]);
		out[i] = (uint8_t)((hi << 4) | lo);
	}

	return true;
}

/* ------------------------------------------------------------------ */
/* Key validation helpers                                             */
/* ------------------------------------------------------------------ */

/* Known key names for validation */
static const char *_kh_known_keys[KH_CONFIG_REQUIRED_KEYS] = {
	KH_CONFIG_KEY_ENABLED,
	KH_CONFIG_KEY_SCHEME,
	KH_CONFIG_KEY_LENGTH,
	KH_CONFIG_KEY_SALT,
	KH_CONFIG_KEY_HASH,
};

/* Find index of a key in the known keys list, or -1 if unknown */
static int _kh_key_index(const char *key)
{
	for (int i = 0; i < KH_CONFIG_REQUIRED_KEYS; i++)
	{
		if (strcmp(key, _kh_known_keys[i]) == 0)
			return i;
	}
	return -1;
}

/* ------------------------------------------------------------------ */
/* Public API                                                         */
/* ------------------------------------------------------------------ */

kh_config_status_t kh_config_load(kh_pin_config_t *config)
{
	kh_config_status_t status = KH_CONFIG_OK;
	link_t sections;
	ini_sec_t *sec = NULL;
	FILINFO fno;
	FRESULT fr;
	int key_count[KH_CONFIG_REQUIRED_KEYS];
	int sec_count = 0;

	/* Clear output config first — on any failure, it stays zeroed */
	if (config)
		memset(config, 0, sizeof(*config));

	if (!config)
		return KH_CONFIG_PARSE_ERROR;  /* Null pointer — treat as internal error */

	/* ---- Step 1: Check file existence ---- */
	fr = f_stat(KH_CONFIG_PATH, &fno);
	if (fr != FR_OK)
	{
		if (fr == FR_NO_FILE)
			return KH_CONFIG_FILE_NOT_FOUND;
		else
			return KH_CONFIG_SD_NOT_MOUNTED;
	}

	/* ---- Step 2: Parse INI file ---- */
	list_init(&sections);

	if (ini_parse(&sections, KH_CONFIG_PATH, false) != 0)
	{
		/* ini_parse returns 1 on failure (f_open fail, malloc fail, etc.)
		 * It frees internal allocations on failure internally.
		 * The sections list may be partially populated — free it to be safe. */
		ini_free(&sections);
		return KH_CONFIG_PARSE_ERROR;
	}

	/* ---- Step 3: Find exactly one [security] section ---- */
	LIST_FOREACH_ENTRY(ini_sec_t, iter, &sections, link)
	{
		/* Only count real sections (type == INI_CHOICE for [section] syntax) */
		if (iter->type != INI_CHOICE)
			continue;

		if (strcmp(iter->name, "security") == 0)
		{
			sec = iter;
			sec_count++;
		}
	}

	if (sec_count == 0)
	{
		status = KH_CONFIG_NO_SECURITY_SECTION;
		goto cleanup;
	}

	if (sec_count > 1)
	{
		status = KH_CONFIG_MULTIPLE_SECTIONS;
		goto cleanup;
	}

	/* ---- Step 4: Validate keys within [security] ---- */
	/* Initialize key count array */
	for (int i = 0; i < KH_CONFIG_REQUIRED_KEYS; i++)
		key_count[i] = 0;

	LIST_FOREACH_ENTRY(ini_kv_t, kv, &sec->kvs, link)
	{
		int idx = _kh_key_index(kv->key);

		if (idx < 0)
		{
			/* Unknown key in [security] section */
			status = KH_CONFIG_UNKNOWN_KEY;
			goto cleanup;
		}

		key_count[idx]++;

		if (key_count[idx] > 1)
		{
			/* Duplicate key */
			status = KH_CONFIG_DUPLICATE_KEY;
			goto cleanup;
		}
	}

	/* Check all required keys are present */
	for (int i = 0; i < KH_CONFIG_REQUIRED_KEYS; i++)
	{
		if (key_count[i] == 0)
		{
			status = KH_CONFIG_MISSING_KEY;
			goto cleanup;
		}
	}

	/* ---- Step 5: Validate values ---- */
	/* We need to iterate again to get the values */
	LIST_FOREACH_ENTRY(ini_kv_t, kv, &sec->kvs, link)
	{
		if (strcmp(kv->key, KH_CONFIG_KEY_ENABLED) == 0)
		{
			if (strcmp(kv->val, KH_CONFIG_EXPECTED_ENABLED) != 0)
			{
				status = KH_CONFIG_INVALID_ENABLED;
				goto cleanup;
			}
		}
		else if (strcmp(kv->key, KH_CONFIG_KEY_SCHEME) == 0)
		{
			if (strcmp(kv->val, KH_CONFIG_EXPECTED_SCHEME) != 0)
			{
				status = KH_CONFIG_INVALID_SCHEME;
				goto cleanup;
			}
		}
		else if (strcmp(kv->key, KH_CONFIG_KEY_LENGTH) == 0)
		{
			if (strcmp(kv->val, KH_CONFIG_EXPECTED_LENGTH) != 0)
			{
				status = KH_CONFIG_INVALID_LENGTH;
				goto cleanup;
			}
		}
	}

	/* ---- Step 6: Hex-decode salt and hash ---- */
	/* Parse into temporary local buffers first */
	uint8_t tmp_salt[KH_CONFIG_SALT_SIZE];
	uint8_t tmp_digest[KH_CONFIG_DIGEST_SIZE];
	bool salt_ok = false;
	bool hash_ok = false;

	LIST_FOREACH_ENTRY(ini_kv_t, kv, &sec->kvs, link)
	{
		if (strcmp(kv->key, KH_CONFIG_KEY_SALT) == 0)
		{
			salt_ok = _kh_hex_decode(kv->val, tmp_salt, KH_CONFIG_SALT_SIZE);
			if (!salt_ok)
			{
				status = KH_CONFIG_INVALID_SALT_HEX;
				goto cleanup;
			}
		}
		else if (strcmp(kv->key, KH_CONFIG_KEY_HASH) == 0)
		{
			hash_ok = _kh_hex_decode(kv->val, tmp_digest, KH_CONFIG_DIGEST_SIZE);
			if (!hash_ok)
			{
				status = KH_CONFIG_INVALID_HASH_HEX;
				goto cleanup;
			}
		}
	}

	/* ---- Step 7: Commit to output config ---- */
	/* All validation passed — copy to output */
	memcpy(config->salt, tmp_salt, KH_CONFIG_SALT_SIZE);
	memcpy(config->expected_digest, tmp_digest, KH_CONFIG_DIGEST_SIZE);
	config->pin_length = KH_CONFIG_PIN_LENGTH;
	config->enabled = true;

	status = KH_CONFIG_OK;

cleanup:
	ini_free(&sections);

	/* On failure, ensure output config is zeroed */
	if (status != KH_CONFIG_OK && config)
		memset(config, 0, sizeof(*config));

	return status;
}

void kh_config_clear(kh_pin_config_t *config)
{
	if (config)
		memset(config, 0, sizeof(*config));
}

const char *kh_config_status_name(kh_config_status_t status)
{
	switch (status)
	{
	case KH_CONFIG_OK:                  return "OK";
	case KH_CONFIG_SD_NOT_MOUNTED:      return "SD_NOT_MOUNTED";
	case KH_CONFIG_FILE_NOT_FOUND:      return "FILE_NOT_FOUND";
	case KH_CONFIG_FILE_OPEN_FAILED:    return "FILE_OPEN_FAILED";
	case KH_CONFIG_PARSE_ERROR:         return "PARSE_ERROR";
	case KH_CONFIG_NO_SECURITY_SECTION: return "NO_SECURITY_SECTION";
	case KH_CONFIG_MULTIPLE_SECTIONS:   return "MULTIPLE_SECTIONS";
	case KH_CONFIG_MISSING_KEY:         return "MISSING_KEY";
	case KH_CONFIG_DUPLICATE_KEY:       return "DUPLICATE_KEY";
	case KH_CONFIG_UNKNOWN_KEY:         return "UNKNOWN_KEY";
	case KH_CONFIG_INVALID_ENABLED:     return "INVALID_ENABLED";
	case KH_CONFIG_INVALID_SCHEME:      return "INVALID_SCHEME";
	case KH_CONFIG_INVALID_LENGTH:      return "INVALID_LENGTH";
	case KH_CONFIG_INVALID_SALT_HEX:    return "INVALID_SALT_HEX";
	case KH_CONFIG_INVALID_HASH_HEX:    return "INVALID_HASH_HEX";
	default:                            return "UNKNOWN";
	}
}
