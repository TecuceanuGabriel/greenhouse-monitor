#pragma once

#include "freertos/FreeRTOS.h"

#include <stdint.h>
#include <time.h>

#define PACKET_MAGIC 0x4748 // "GH"
#define PACKET_VERSION 1

typedef struct __attribute__((packed)) {
	uint16_t magic;
	uint8_t version;
	uint8_t reserved;
	uint32_t seq;
	int32_t temp;
	int32_t humidity;
	float ch4;
	float co2;
	int64_t timestamp;
} sensor_data_t; // 32 bytes

#define HMAC_SHA256_LEN 32

typedef struct __attribute__((packed)) {
	sensor_data_t data;
	uint8_t hmac[HMAC_SHA256_LEN];
} authenticated_packet_t; // 64 bytes

/*
 * connect to server
 */
esp_err_t my_connect(int *sock);

/*
 * disconnect from server
 */
void disconnect(const int sock);

/*
 * send exactly len bytes to server
 */
esp_err_t send_all(const int sock, const void *data, size_t len);
