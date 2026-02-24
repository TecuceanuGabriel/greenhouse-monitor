#pragma once

#include "freertos/FreeRTOS.h"

#include <time.h>

#define SERVER_IP "127.0.0.1"
#define SERVER_PORT 1234

typedef struct {
	int temp;
	int humidity;
	float ch4;
	float co2;
	time_t timestamp;
} sensor_data_t;

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
