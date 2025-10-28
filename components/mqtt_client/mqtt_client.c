#include "mqtt_client.h"
#include "my_mqtt_client.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_event.h"
#include <stdio.h>

static const char *TAG = "MQTT";
static esp_mqtt_client_handle_t client = NULL;

static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data)
{
    esp_mqtt_event_handle_t event = event_data;
    switch (event->event_id)
    {
    case MQTT_EVENT_CONNECTED:
        ESP_LOGI(TAG, "MQTT connected");
        break;
    case MQTT_EVENT_DISCONNECTED:
        ESP_LOGI(TAG, "MQTT disconnected");
        break;
    case MQTT_EVENT_DATA:
        ESP_LOGI(TAG, "MQTT topic: %.*s", event->topic_len, event->topic);
        ESP_LOGI(TAG, "MQTT data: %.*s", event->data_len, event->data);
        break;
    default:
        break;
    }
}

void mqtt_app_start(void)
{
    esp_mqtt_client_config_t mqtt_cfg = {
        .broker = {
            .address = {
                .uri = "mqtt://broker.hivemq.com",
            }}};

    client = esp_mqtt_client_init(&mqtt_cfg);
    esp_mqtt_client_register_event(client, ESP_EVENT_ANY_ID, mqtt_event_handler, NULL);
    esp_mqtt_client_start(client);
}

void mqtt_publish_adc(int val)
{
    if (client)
    {
        char msg[16];
        snprintf(msg, sizeof(msg), "%d", val);
        esp_mqtt_client_publish(client, "esp32/adc", msg, 0, 1, 0);
    }
}

void mqtt_task(void *arg)
{
    while (1)
    {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
