#include "driver/gpio.h"
#include "esp_interface.h"
#include "esp_log.h"
#include "esp_sleep.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/task.h"
#include "nvs_flash.h"

#include <time.h>

#include "client.h"
#include "sensors.h"
#include "util.h"

#define DEEP_SLEEP_DURATION_SEC 1800
#define WARM_UP_DURATION_SEC 180
#define NR_RETRIES 5

static const char *TAG = "MAIN";

// #define TEST_MODE // comment to deactivate test mode

#ifdef TEST_MODE
void test()
{
	sensor_data_t data;
	int sock;

	float mq135_ro_estimate = med_rs(adc_handle, MQ135_PIN, MQ135_RL);
	float mq4_ro_estimate = med_rs(adc_handle, MQ4_PIN, MQ4_RL);

	ESP_LOGI(TAG, "ch4=%.2f co2=%.2f", mq4_ro_estimate, mq135_ro_estimate);

	int connected = 0;
	for (int attempt = 0; attempt < NR_RETRIES; ++attempt) {
		if (my_connect(&sock) == ESP_OK) {
			ESP_LOGI(TAG, "Connected to the server.");
			connected = 1;
			break;
		}
		vTaskDelay(pdMS_TO_TICKS(2000));
	}

	if (!connected) {
		ESP_LOGW(TAG, "Failed to connect to server.");
	}

	while (true) {
		esp_err_t status =
			dht11_get_temp_and_humidity(DHT_PIN, &data.temp, &data.humidity);
		if (status != ESP_OK) {
			ESP_LOGW(TAG, "DHT11 read failed. Skipping this cycle.");
			vTaskDelay(pdMS_TO_TICKS(2000));
			continue;
		}

		data.ch4 = mq4_get_ppm(adc_handle, MQ4_PIN);
		data.co2 = mq135_get_ppm(adc_handle, MQ135_PIN);
		data.timestamp = time(NULL);

		ESP_LOGI(TAG,
				 "Temp=%d, Humidity=%d, CH4=%.2f, CO2=%.2f Timestamp=%lld",
				 data.temp,
				 data.humidity,
				 data.ch4,
				 data.co2,
				 data.timestamp);

		if (connected && send_all(sock, &data, sizeof(data)) == ESP_OK) {
			ESP_LOGI(TAG, "Test data sent.");
		} else {
			ESP_LOGW(TAG, "Failed to send test data.");
		}

		vTaskDelay(pdMS_TO_TICKS(5000));
	}

	ESP_LOGI(TAG, "Disconnecting from server...");
	disconnect(sock);
}
#endif

void app_main()
{
	// power on sensors
	gpio_reset_pin(NPN_PIN);
	gpio_set_direction(NPN_PIN, GPIO_MODE_OUTPUT);
	gpio_set_level(NPN_PIN, 1);

	// wait for warmup
	// vTaskDelay(pdMS_TO_TICKS(1000 * WARM_UP_DURATION_SEC));

	ESP_ERROR_CHECK(nvs_flash_init()); // needed for wifi
	init_wifi();
	init_sntp(); // needed for time sync
	init_adc();

#ifdef TEST_MODE
	test();
#else
	esp_log_level_set("*", ESP_LOG_NONE); // disable all logs

	sensor_data_t data;
	esp_err_t status = ESP_FAIL;

	// try reading from dht
	for (int attempt = 0; attempt < NR_RETRIES; ++attempt) {
		status =
			dht11_get_temp_and_humidity(DHT_PIN, &data.temp, &data.humidity);
		if (status == ESP_OK) {
			break;
		}
		ESP_LOGW(TAG, "Attempt %d failed, retrying...", attempt + 1);
		vTaskDelay(pdMS_TO_TICKS(2000)); // wait before retry
	}

	// if reading fails we don't send the data and go immediately to sleep
	if (status != ESP_OK) {
		ESP_LOGE(TAG,
				 "All attempts to read DHT11 failed. Going to deep sleep.");
		goto sleep;
	}

	// read ch4
	data.ch4 = mq4_get_ppm(adc_handle, MQ4_PIN);
	data.co2 = mq135_get_ppm(adc_handle, MQ135_PIN);
	data.timestamp = time(NULL);

	ESP_LOGI(TAG,
			 "Sensor Read: Temp=%d Humidity=%d CH4=%.2f CO2=%0.2f - %lld",
			 data.temp,
			 data.humidity,
			 data.ch4,
			 data.co2,
			 data.timestamp);

	// try sending the data to the server
	int sock;
	bool sent = false;

	for (int attempt = 0; attempt < NR_RETRIES; ++attempt) {
		if (my_connect(&sock) == ESP_OK) {
			if (send_all(sock, &data, sizeof(data)) == ESP_OK) {
				ESP_LOGI(TAG, "Data sent successfully.");
				sent = true;
			}
			disconnect(sock);
			break;
		}
		ESP_LOGW(TAG, "Attempt %d failed, retrying...", attempt + 1);
		vTaskDelay(pdMS_TO_TICKS(2000));
	}

	if (!sent) {
		ESP_LOGW(TAG, "All attempts to send data failed.");
	}

sleep:
	gpio_set_level(NPN_PIN, 0);
	ESP_LOGI(TAG,
			 "Entering deep sleep for %d seconds.",
			 DEEP_SLEEP_DURATION_SEC);
	esp_deep_sleep(1000000 * DEEP_SLEEP_DURATION_SEC);
#endif
}
