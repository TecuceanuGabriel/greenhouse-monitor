#pragma once

#include "esp_adc/adc_oneshot.h"

#define NPN_PIN GPIO_NUM_27
#define DHT_PIN GPIO_NUM_25
#define MQ4_PIN ADC_CHANNEL_6
#define MQ135_PIN ADC_CHANNEL_7
#define ADC_UNIT ADC_UNIT_1

#define WIFI_SSID "ssid"
#define WIFI_PASS "pass"

#define WIFI_CONNECTED_BIT BIT0

extern adc_oneshot_unit_handle_t adc_handle;

void init_wifi();
void init_adc();
void init_sntp();
