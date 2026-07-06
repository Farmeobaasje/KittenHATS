/*
 * Copyright (c) 2025 KittenHATS Contributors
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * KittenHATS — P4-D: PIN configuration loader.
 *
 * Loads and validates PIN configuration from kittenhats.ini on SD.
 * Uses Hekate's ini_parse() which internally allocates heap via zalloc.
 * All parser allocations are freed via ini_free() before return.
 *
 * Config file path: bootloader/kittenhats/kittenhats.ini
 *
 * Required [security] section schema (5 keys):
 *   pin_gate_enabled  = 1              (must be exactly "1")
 *   pin_scheme        = sha256-salt-v1 (must be exactly "sha256-salt-v1")
 *   pin_length        = 4              (must be exactly "4")
 *   pin_salt_hex      = <32 hex chars> (16 bytes)
 *   pin_hash_hex      = <64 hex chars> (32 bytes)
 *
 * P4-D scope: config loading and validation only.
 * No SHA-256 hashing, no PIN comparison, no CLEAN action.
 */

#ifndef _KH_CONFIG_H_
#define _KH_CONFIG_H_

#include <utils/types.h>

/* ------------------------------------------------------------------ */
/* Constants                                                          */
/* ------------------------------------------------------------------ */

#define KH_CONFIG_DIGEST_SIZE  32  /* SHA-256 output: 32 bytes */
#define KH_CONFIG_SALT_SIZE    16  /* Salt: 16 bytes */
#define KH_CONFIG_PIN_LENGTH   4   /* Fixed PIN length */

/* SD path for config file */
#define KH_CONFIG_PATH "bootloader/kittenhats/kittenhats.ini"

/* Expected key names within [security] section */
#define KH_CONFIG_KEY_ENABLED  "pin_gate_enabled"
#define KH_CONFIG_KEY_SCHEME   "pin_scheme"
#define KH_CONFIG_KEY_LENGTH   "pin_length"
#define KH_CONFIG_KEY_SALT     "pin_salt_hex"
#define KH_CONFIG_KEY_HASH     "pin_hash_hex"

/* Expected values */
#define KH_CONFIG_EXPECTED_ENABLED "1"
#define KH_CONFIG_EXPECTED_SCHEME  "sha256-salt-v1"
#define KH_CONFIG_EXPECTED_LENGTH  "4"

/* Number of required keys */
#define KH_CONFIG_REQUIRED_KEYS 5

/* ------------------------------------------------------------------ */
/* Types                                                              */
/* ------------------------------------------------------------------ */

/*
 * PIN configuration — loaded from kittenhats.ini [security] section.
 * Only filled on successful validation; zeroed on any failure.
 */
typedef struct {
	uint8_t salt[KH_CONFIG_SALT_SIZE];             /* 16-byte salt (hex decoded) */
	uint8_t expected_digest[KH_CONFIG_DIGEST_SIZE]; /* 32-byte SHA-256 digest */
	uint8_t pin_length;                            /* Expected PIN length (4) */
	bool    enabled;                               /* PIN gate enabled */
} kh_pin_config_t;

/*
 * Status codes for kh_config_load().
 * Each distinct failure mode has its own code for diagnostic clarity.
 * No error distinction is invented beyond what the local APIs support.
 */
typedef enum {
	KH_CONFIG_OK                  = 0,  /* Success */
	KH_CONFIG_SD_NOT_MOUNTED      = 1,  /* SD not mounted (f_stat fails with FR_NOT_READY etc.) */
	KH_CONFIG_FILE_NOT_FOUND      = 2,  /* File does not exist (f_stat returns FR_NO_FILE) */
	KH_CONFIG_FILE_OPEN_FAILED    = 3,  /* File exists but cannot be opened */
	KH_CONFIG_PARSE_ERROR         = 4,  /* ini_parse() returned failure */
	KH_CONFIG_NO_SECURITY_SECTION = 5,  /* No [security] section found */
	KH_CONFIG_MULTIPLE_SECTIONS   = 6,  /* More than one [security] section */
	KH_CONFIG_MISSING_KEY         = 7,  /* One or more required keys missing */
	KH_CONFIG_DUPLICATE_KEY       = 8,  /* A required key appears more than once */
	KH_CONFIG_UNKNOWN_KEY         = 9,  /* Unknown key within [security] section */
	KH_CONFIG_INVALID_ENABLED     = 10, /* pin_gate_enabled != "1" */
	KH_CONFIG_INVALID_SCHEME      = 11, /* pin_scheme != "sha256-salt-v1" */
	KH_CONFIG_INVALID_LENGTH      = 12, /* pin_length != "4" */
	KH_CONFIG_INVALID_SALT_HEX    = 13, /* pin_salt_hex not valid hex or wrong length */
	KH_CONFIG_INVALID_HASH_HEX    = 14, /* pin_hash_hex not valid hex or wrong length */
} kh_config_status_t;

/* ------------------------------------------------------------------ */
/* Public API                                                         */
/* ------------------------------------------------------------------ */

/*
 * Load and validate PIN configuration from SD.
 *
 * SD must already be mounted (mounted in kh_home_screen() before LVGL init).
 * Uses ini_parse() which internally allocates heap via zalloc.
 * All parser allocations are freed via ini_free() before return.
 *
 * Validation order:
 *   1. Check file existence via f_stat (distinguishes FILE_NOT_FOUND)
 *   2. Parse with ini_parse()
 *   3. Find exactly one [security] section (type == INI_CHOICE)
 *   4. Validate all 5 required keys present, no duplicates, no unknown keys
 *   5. Validate each key value against expected schema
 *   6. Hex-decode salt (32 hex chars -> 16 bytes) and hash (64 hex chars -> 32 bytes)
 *   7. On any failure: clear out_config, return specific error code
 *
 * @param config  Output: filled on success, zeroed on failure.
 * @return        Status code (KH_CONFIG_OK on success).
 */
kh_config_status_t kh_config_load(kh_pin_config_t *config);

/*
 * Clear PIN config (zero all fields).
 * @param config  Pointer to config to clear.
 */
void kh_config_clear(kh_pin_config_t *config);

/*
 * Return human-readable name for a status code.
 * @param status  Status code.
 * @return        Pointer to static string (never NULL).
 */
const char *kh_config_status_name(kh_config_status_t status);

#endif /* _KH_CONFIG_H_ */
