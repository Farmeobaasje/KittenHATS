/*
 * Copyright (c) 2025 KittenHATS Contributors
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * KittenHATS — PIN verification via SHA-256.
 *
 * Computes sha256(salt[16] || pin_ascii[4]) and compares with
 * expected_digest from config in constant time.
 *
 * Hash input buffer layout:
 *   bytes 0-15:  salt (16 bytes from config)
 *   bytes 16-19: PIN ASCII digits (4 bytes)
 *   Total: 20 bytes
 *
 * Uses se_sha_hash_256_oneshot() from the Tegra X1 Security Engine.
 * The SE driver outputs standard big-endian SHA-256 (byte_swap_32
 * applied in _se_sha_hash_256_get_hash, se.c line 486).
 *
 * Constant-time comparison: XOR all 32 digest bytes together.
 * If the result is 0, the digests match. No early-exit on mismatch.
 */

#include <string.h>

#include <bdk.h>
#include <sec/se.h>

#include "kh_pin_verify.h"

/* ------------------------------------------------------------------ */
/* Constants                                                          */
/* ------------------------------------------------------------------ */

/* Hash input: 16 bytes salt + 4 bytes PIN = 20 bytes */
#define KH_VERIFY_HASH_INPUT_SIZE  (KH_CONFIG_SALT_SIZE + KH_CONFIG_PIN_LENGTH)

/* ------------------------------------------------------------------ */
/* Public API                                                         */
/* ------------------------------------------------------------------ */

kh_pin_verify_result_t kh_pin_verify(
	const kh_pin_config_t *config,
	const char *pin,
	uint8_t pin_len)
{
	/* ---- Input validation ---- */
	if (!config || !pin)
		return KH_PIN_VERIFY_INVALID_INPUT;

	if (pin_len != KH_CONFIG_PIN_LENGTH)
		return KH_PIN_VERIFY_INVALID_INPUT;

	if (!config->enabled)
		return KH_PIN_VERIFY_CONFIG_ERROR;

	/* ---- Build hash input: salt || pin ---- */
	/*
	 * Stack-allocated buffer: 20 bytes.
	 * No NUL terminator needed — SHA-256 hashes raw bytes.
	 * Cleared on every exit path.
	 */
	uint8_t hash_input[KH_VERIFY_HASH_INPUT_SIZE];
	uint8_t calculated_digest[KH_CONFIG_DIGEST_SIZE];
	int se_result;
	kh_pin_verify_result_t result;
	uint8_t xor_acc;
	int i;

	/* Copy salt (16 bytes) */
	memcpy(hash_input, config->salt, KH_CONFIG_SALT_SIZE);

	/* Copy PIN ASCII bytes (4 bytes) — no NUL included in hash */
	memcpy(hash_input + KH_CONFIG_SALT_SIZE, pin, KH_CONFIG_PIN_LENGTH);

	/* ---- Compute SHA-256 ---- */
	se_result = se_sha_hash_256_oneshot(
		calculated_digest,
		hash_input,
		KH_VERIFY_HASH_INPUT_SIZE);

	/* Clear hash input immediately — no longer needed */
	memset(hash_input, 0, sizeof(hash_input));

	if (se_result != 0)
	{
		/* SHA-256 hardware returned error — clear digest, return error */
		memset(calculated_digest, 0, sizeof(calculated_digest));
		return KH_PIN_VERIFY_HASH_ERROR;
	}

	/* ---- Constant-time digest comparison ---- */
	/*
	 * XOR all 32 bytes together. If result is 0, digests match.
	 * No early-exit — always processes all 32 bytes.
	 */
	xor_acc = 0;
	for (i = 0; i < (int)KH_CONFIG_DIGEST_SIZE; i++)
		xor_acc |= calculated_digest[i] ^ config->expected_digest[i];

	result = (xor_acc == 0) ? KH_PIN_VERIFY_OK : KH_PIN_VERIFY_WRONG;

	/* ---- Clear calculated digest ---- */
	memset(calculated_digest, 0, sizeof(calculated_digest));

	return result;
}
