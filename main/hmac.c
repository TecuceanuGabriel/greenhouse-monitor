#include "hmac.h"

#include "esp_log.h"
#include "mbedtls/md.h"

#include <string.h>

static const char *TAG = "HMAC";

esp_err_t compute_hmac_sha256(const uint8_t *data, size_t data_len,
							  uint8_t *out_hmac)
{
	const char *key = CONFIG_HMAC_KEY;
	const mbedtls_md_info_t *md_info =
		mbedtls_md_info_from_type(MBEDTLS_MD_SHA256);

	int ret = mbedtls_md_hmac(md_info, (const unsigned char *)key, strlen(key),
							  data, data_len, out_hmac);
	if (ret != 0) {
		ESP_LOGE(TAG, "mbedtls_md_hmac failed: -0x%04x", -ret);
		return ESP_FAIL;
	}

	return ESP_OK;
}
