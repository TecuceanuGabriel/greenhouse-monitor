#include "sensors.h"

#include "driver/gpio.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include <math.h>

// generic function to read voltage from sensor
static float sensor_read_voltage(const adc_oneshot_unit_handle_t adc_handle,
								 const int adc_channel,
								 const float vref)
{
	int raw = 0;
	adc_oneshot_read(adc_handle, adc_channel, &raw);
	return ((float)raw / ADC_RESOLUTION) * vref;
}

// generic function to calculate rs base on voltage divider formula
static float
sensor_calculate_rs(const float v_adc, const float vc, const float rl_value)
{
	float v_out = 2 * v_adc; // we read the output from the voltage divider
	return rl_value * ((vc - v_out) / v_out);
}

// generic function to calculate ppm using logaritmic interpolation
static float sensor_get_ppm(const float rs,
							const float ro_clean_air,
							const float log_slope,
							const float log_intercept)
{
	float ratio = rs / ro_clean_air;
	float log_ppm = log_slope * log10(ratio) + log_intercept;
	return powf(10, log_ppm);
}

float med_rs(const adc_oneshot_unit_handle_t adc_handle,
			 const int adc_channel,
			 const float rl)
{
	float sum_rs = 0;
	const int samples = 50;
	for (int i = 0; i < samples; i++) {
		float v_adc = sensor_read_voltage(adc_handle, adc_channel, V_REF);
		float rs = sensor_calculate_rs(v_adc, V_IN, rl);
		sum_rs += rs;
		vTaskDelay(pdMS_TO_TICKS(200));
	}
	return sum_rs / samples;
}

float mq4_get_ppm(const adc_oneshot_unit_handle_t adc_handle,
				  const int adc_channel)
{
	float v_adc = sensor_read_voltage(adc_handle, adc_channel, V_REF);
	float rs = sensor_calculate_rs(v_adc, V_IN, MQ4_RL);
	return sensor_get_ppm(rs, MQ4_RO, -4.51, 3.0);
}

float mq135_get_ppm(const adc_oneshot_unit_handle_t adc_handle,
					const int adc_channel)
{
	float v_adc = sensor_read_voltage(adc_handle, adc_channel, V_REF);
	float rs = sensor_calculate_rs(v_adc, V_IN, MQ135_RL);
	return sensor_get_ppm(rs, MQ135_RO, -2.1, 2.0);
}

static void delay_us(const uint32_t us)
{
	int64_t start = esp_timer_get_time();
	while ((esp_timer_get_time() - start) < us)
		;
}

static void dht11_start_signal(const int pin)
{
	gpio_set_direction(pin, GPIO_MODE_OUTPUT);
	gpio_set_level(pin, 0);
	vTaskDelay(pdMS_TO_TICKS(20)); // > 18 ms
	gpio_set_level(pin, 1);
	delay_us(30); // 20–40 us
}

// wait for a specific level (0/1) until the timer expires
static int
dht11_wait_for_level(const int pin, const int level, const uint32_t timeout_us)
{
	int64_t start = esp_timer_get_time();
	while (gpio_get_level(pin) != level) {
		if ((esp_timer_get_time() - start) > timeout_us)
			return -1;
	}
	return 0;
}

uint8_t dht11_read_byte(const int pin)
{
	uint8_t result = 0;
	for (int i = 0; i < 8; i++) {
		// bit transmission is signeled by a 50us low pulse
		if (dht11_wait_for_level(pin, 0, 100) != 0)
			break;

		// measure high pulse duration
		if (dht11_wait_for_level(pin, 1, 100) != 0)
			break;

		int64_t start = esp_timer_get_time();
		if (dht11_wait_for_level(pin, 0, 100) != 0)
			break;

		// long pulse high pulse -> 1, short high pulse -> 0
		int64_t duration = esp_timer_get_time() - start;
		result <<= 1;
		if (duration > 50)
			result |= 1;
	}
	return result;
}

esp_err_t dht11_get_temp_and_humidity(const int pin, int *temp, int *humidity)
{
	uint8_t data[5] = {0};

	dht11_start_signal(pin);
	gpio_set_direction(pin, GPIO_MODE_INPUT);

	// wait for dht response
	if (dht11_wait_for_level(pin, 0, 100) != 0)
		return ESP_FAIL;
	if (dht11_wait_for_level(pin, 1, 100) != 0)
		return ESP_FAIL;
	if (dht11_wait_for_level(pin, 0, 100) != 0)
		return ESP_FAIL;

	for (int i = 0; i < 5; i++) {
		data[i] = dht11_read_byte(pin);
	}

	uint8_t checksum = 0;
	for (int i = 0; i < 4; ++i)
		checksum += data[i];
	if (data[4] != checksum)
		return ESP_ERR_INVALID_CRC;

	// since we use dht11 the fractionary part is 0
	// data[1] for humidity and data[3] for temp
	*humidity = data[0];
	*temp = data[2];
	return ESP_OK;
}
