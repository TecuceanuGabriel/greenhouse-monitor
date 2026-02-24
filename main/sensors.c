#include "sensors.h"

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
