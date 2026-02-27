#pragma once

#include "esp_err.h"

#include <stddef.h>
#include <stdint.h>

#define HMAC_SHA256_LEN 32

/*
 * Compute HMAC-SHA256 over data using the pre-shared key from Kconfig.
 * out_hmac must be at least HMAC_SHA256_LEN bytes.
 */
esp_err_t compute_hmac_sha256(const uint8_t *data, size_t data_len,
							  uint8_t *out_hmac);
