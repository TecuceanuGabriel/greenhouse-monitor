#pragma once

#include "esp_adc/adc_oneshot.h"

#define V_IN 5.0
#define V_REF 3.3
#define ADC_RESOLUTION 4095.0

#define MQ4_RL 20000.0
#define MQ4_RO 61000.0

#define MQ135_RL 20000.0
#define MQ135_RO 80000.0

/**
 * helper function to approximate ro, it returns the medium rs over 50 samples.
 */
float med_rs(const adc_oneshot_unit_handle_t adc_handle,
			 const int adc_channel,
			 const float rl);

float mq4_get_ppm(const adc_oneshot_unit_handle_t adc_handle,
				  const int adc_channel);

float mq135_get_ppm(const adc_oneshot_unit_handle_t adc_handle,
					const int adc_channel);
