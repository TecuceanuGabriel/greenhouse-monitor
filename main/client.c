#include "client.h"

#include <arpa/inet.h>
#include <netdb.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#include "esp_log.h"
#include "freertos/task.h"

static const char *CLIENT_TAG = "TCP_CLIENT";

esp_err_t my_connect(int *sock)
{
	struct sockaddr_in dest_addr;
	dest_addr.sin_addr.s_addr = inet_addr(SERVER_IP);
	dest_addr.sin_family = AF_INET;
	dest_addr.sin_port = htons(SERVER_PORT);

	*sock = socket(AF_INET, SOCK_STREAM, IPPROTO_IP);
	if (*sock < 0) {
		ESP_LOGE(CLIENT_TAG, "Unable to create socket: errno %d.", errno);
		return ESP_FAIL;
	}

	int err = connect(*sock, (struct sockaddr *)&dest_addr, sizeof(dest_addr));
	if (err != 0) {
		ESP_LOGE(CLIENT_TAG, "Socket unable to connect: errno %d.", errno);
		close(*sock);
		return ESP_FAIL;
	}

	ESP_LOGI(CLIENT_TAG, "Successfully connected to server.");
	return ESP_OK;
}

void disconnect(const int sock)
{
	ESP_LOGI(CLIENT_TAG, "Closing socket.");
	close(sock);
}

esp_err_t send_all(const int sock, const void *data, size_t len)
{
	const uint8_t *ptr = (const uint8_t *)data;
	while (len > 0) {
		int sent = send(sock, ptr, len, 0);
		if (sent < 0) {
			ESP_LOGE(CLIENT_TAG, "Error during sending: errno %d.", errno);
			return ESP_FAIL;
		}
		ptr += sent;
		len -= sent;
	}
	return ESP_OK;
}
