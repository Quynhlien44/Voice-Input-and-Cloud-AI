#ifndef WIFI_MANAGER_H
#define WIFI_MANAGER_H

#include "esp_err.h"
#include <stdbool.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"

void wifi_manager_init(void);
bool wifi_manager_is_connected(void);
char *wifi_manager_get_ip(void);
esp_err_t wifi_manager_wait_connected(TickType_t timeout);
esp_err_t wifi_manager_reconnect(void);
void wifi_manager_stop(void);
void wifi_hardware_diagnostic(void);
void wifi_test_all_networks(void);

#endif