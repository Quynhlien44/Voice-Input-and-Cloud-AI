#include "real_mqtt_client.h"
#include "esp_log.h"
#include "mqtt_client.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>

static const char *TAG = "REAL_MQTT";

static esp_mqtt_client_handle_t client = NULL;
static ai_response_cb_t ai_callback = NULL;

static void mqtt_event_handler(void *handler_args, esp_event_base_t base,
                               int32_t event_id, void *event_data)
{
    esp_mqtt_event_handle_t event = event_data;

    switch (event_id)
    {
    case MQTT_EVENT_CONNECTED:
        ESP_LOGI(TAG, "✅ REAL MQTT Connected to broker");
        esp_mqtt_client_subscribe(client, "voice/response", 1);
        break;

    case MQTT_EVENT_DISCONNECTED:
        ESP_LOGI(TAG, "❌ REAL MQTT Disconnected");
        break;

    case MQTT_EVENT_DATA:
        if (strncmp(event->topic, "voice/response", event->topic_len) == 0)
        {
            ESP_LOGI(TAG, "📥 Received AI response: %.*s", event->data_len, event->data);
            if (ai_callback)
            {
                ai_callback(event->data);
            }
        }
        break;

    default:
        break;
    }
}

void real_mqtt_init(void)
{
    ESP_LOGI(TAG, "🚀 Initializing REAL MQTT Client");

    esp_mqtt_client_config_t mqtt_cfg = {
        .broker.address.uri = "mqtt://broker.emqx.io:1883",
    };

    client = esp_mqtt_client_init(&mqtt_cfg);
    esp_mqtt_client_register_event(client, ESP_EVENT_ANY_ID, mqtt_event_handler, NULL);
}

void real_mqtt_start(void)
{
    if (client)
    {
        esp_mqtt_client_start(client);
    }
}

void real_mqtt_publish_audio(const char *topic, const uint8_t *data, size_t len)
{
    if (client)
    {
        esp_mqtt_client_publish(client, topic, (const char *)data, len, 1, 0);
    }
}

void real_mqtt_set_callback(ai_response_cb_t callback)
{
    ai_callback = callback;
}