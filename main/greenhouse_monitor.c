#include "driver/gpio.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs_flash.h"

#include <time.h>

#include "client.h"
#include "sensors.h"
#include "util.h"

#define NR_RETIRES 5

static const char *TAG = "MAIN";

void app_main()
{
	// power on sensors
	gpio_reset_pin(NPN_PIN);
	gpio_set_direction(NPN_PIN, GPIO_MODE_OUTPUT);
	gpio_set_level(NPN_PIN, 1);

	ESP_ERROR_CHECK(nvs_flash_init());
	init_wifi();
	init_sntp();
	init_adc();

	sensor_data_t data;
	int sock;

	float mq135_ro_estimate = med_rs(adc_handle, MQ135_PIN, MQ135_RL);
	float mq4_ro_estimate = med_rs(adc_handle, MQ4_PIN, MQ4_RL);

	ESP_LOGI(TAG, "ch4=%.2f co2=%.2f", mq4_ro_estimate, mq135_ro_estimate);

	int connected = 0;
	for (int attempt = 0; attempt < NR_RETIRES; ++attempt) {
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
