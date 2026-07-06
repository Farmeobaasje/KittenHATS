/*
 * Copyright (c) 2025 KittenHATS Contributors
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * KittenHATS — P4-F: PIN verification via SHA-256.
 *
 * Verifies a 4-digit PIN against the loaded config by computing:
 *   sha256(salt[16] || pin_ascii[4])
 * and comparing the digest with expected_digest from config.
 *
 * Uses the Tegra X1 Security Engine (se_sha_hash_256_oneshot).
 * Hash output is 32 bytes, byte_swap_32'd by the SE driver to standard
 * big-endian SHA-256 (verified in vendor/hekate/bdk/sec/se.c line 486).
 */

#ifndef _KH_PIN_VERIFY_H_
#define _KH_PIN_VERIFY_H_

#include <utils/types.h>
#include "kh_config.h"

/* ------------------------------------------------------------------ */
/* Result codes                                                        */
/* ------------------------------------------------------------------ */

typedef enum {
	KH_PIN_VERIFY_OK           = 0,  /* PIN matches config */
	KH_PIN_VERIFY_WRONG        = 1,  /* PIN does not match */
	KH_PIN_VERIFY_CONFIG_ERROR = 2,  /* Config not loaded or invalid */
	KH_PIN_VERIFY_HASH_ERROR   = 3,  /* SHA-256 hardware returned error */
	KH_PIN_VERIFY_INVALID_INPUT = 4, /* NULL pointers or wrong pin_len */
} kh_pin_verify_result_t;

/* ------------------------------------------------------------------ */
/* Public API                                                          */
/* ------------------------------------------------------------------ */

/*
 * Verify a 4-digit PIN against the loaded config.
 *
 * Computes sha256(salt || pin_ascii) and compares with expected_digest
 * in constant time (no early-exit on mismatch).
 *
 * @param config   Loaded PIN config (must be KH_CONFIG_OK).
 * @param pin      ASCII PIN string (exactly 4 digits, NUL-terminated).
 * @param pin_len  Length of PIN (must be exactly 4).
 * @return         KH_PIN_VERIFY_OK on match, or error code.
 */
kh_pin_verify_result_t kh_pin_verify(
	const kh_pin_config_t *config,
	const char *pin,
	uint8_t pin_len);

#endif /* _KH_PIN_VERIFY_H_ */
