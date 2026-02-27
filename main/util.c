#include "util.h"

#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_sntp.h"
#include "esp_wifi.h"
#include "freertos/event_groups.h"
#include "freertos/task.h"

#include <string.h>
#include <time.h>

static const char *WIFI_TAG = "WIFI";

static EventGroupHandle_t wifi_event_group;

adc_oneshot_unit_handle_t adc_handle;

void init_adc()
{
	// setup unit
	adc_oneshot_unit_init_cfg_t unit_cfg = {
		.unit_id = ADC_UNIT,
	};
	adc_oneshot_new_unit(&unit_cfg, &adc_handle);

	// channel config
	adc_oneshot_chan_cfg_t chan_cfg = {
		.bitwidth = ADC_BITWIDTH_DEFAULT,
		.atten = ADC_ATTEN_DB_12,
	};

	// setup pins
	adc_oneshot_config_channel(adc_handle, MQ4_PIN, &chan_cfg);
	adc_oneshot_config_channel(adc_handle, MQ135_PIN, &chan_cfg);
}

static void wifi_event_handler(void *arg,
							   esp_event_base_t event_base,
							   int32_t event_id,
							   void *event_data)
{
	if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
		esp_wifi_connect();
	} else if (event_base == WIFI_EVENT &&
			   event_id == WIFI_EVENT_STA_DISCONNECTED) {
		ESP_LOGW(WIFI_TAG, "Disconnected, retrying...");
		esp_wifi_connect();
	} else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
		ESP_LOGD(WIFI_TAG, "Got IP address.");
		xEventGroupSetBits(wifi_event_group, WIFI_CONNECTED_BIT);
	}
}

void init_wifi()
{
	wifi_event_group = xEventGroupCreate();

	ESP_ERROR_CHECK(esp_netif_init());
	ESP_ERROR_CHECK(esp_event_loop_create_default());
	esp_netif_create_default_wifi_sta();

	wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
	ESP_ERROR_CHECK(esp_wifi_init(&cfg));

	esp_event_handler_instance_t instance_any_id;
	esp_event_handler_instance_t instance_got_ip;
	ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
														ESP_EVENT_ANY_ID,
														&wifi_event_handler,
														NULL,
														&instance_any_id));
	ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
														IP_EVENT_STA_GOT_IP,
														&wifi_event_handler,
														NULL,
														&instance_got_ip));

	wifi_config_t wifi_config = {
		.sta =
			{
				.threshold.authmode = WIFI_AUTH_WPA2_PSK,
			},
	};
	strncpy((char *)wifi_config.sta.ssid, CONFIG_WIFI_SSID,
			sizeof(wifi_config.sta.ssid));
	strncpy((char *)wifi_config.sta.password, CONFIG_WIFI_PASSWORD,
			sizeof(wifi_config.sta.password));
	ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
	ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
	ESP_ERROR_CHECK(esp_wifi_start());

	ESP_LOGD(WIFI_TAG, "Wi-Fi initialization finished.");

	// wait for connection
	EventBits_t bits = xEventGroupWaitBits(wifi_event_group,
										   WIFI_CONNECTED_BIT,
										   pdFALSE,
										   pdFALSE,
										   portMAX_DELAY);
	if (bits & WIFI_CONNECTED_BIT) {
		ESP_LOGD(WIFI_TAG, "Connected to Wi-Fi network");
	} else {
		ESP_LOGE(WIFI_TAG, "Failed to connect");
	}
}

void init_sntp()
{
	esp_sntp_setoperatingmode(SNTP_OPMODE_POLL);
	esp_sntp_setservername(0, "pool.ntp.org");

	// add backup NTP servers
	esp_sntp_setservername(1, "time.nist.gov");
	esp_sntp_setservername(2, "time.google.com");
	esp_sntp_init();

	setenv("TZ", "UTC-3", 1);
	tzset();

	// wait to sync time
	vTaskDelay(pdMS_TO_TICKS(3000));
}
