#include "driver/gpio.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs_flash.h"

#include "sensors.h"
#include "util.h"

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

	int temp, humidity;
	while (true) {
		esp_err_t status = dht11_get_temp_and_humidity(DHT_PIN, &temp, &humidity);
		if (status == ESP_OK) {
			float ch4 = mq4_get_ppm(adc_handle, MQ4_PIN);
			float co2 = mq135_get_ppm(adc_handle, MQ135_PIN);
			ESP_LOGI(TAG, "Temp=%d, Humidity=%d, CH4=%.2f, CO2=%.2f",
					 temp, humidity, ch4, co2);
		} else {
			ESP_LOGW(TAG, "DHT11 read failed.");
		}
		vTaskDelay(pdMS_TO_TICKS(5000));
	}
}
