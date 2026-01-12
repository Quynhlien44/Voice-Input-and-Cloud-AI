#ifndef MY_MQTT_H
#define MY_MQTT_H

#include <stdbool.h>
#include "esp_err.h"

// Callback types
typedef void (*ai_response_cb_t)(const char *response);
typedef void (*tts_audio_cb_t)(const char *audio_base64, uint32_t sample_rate,
                               uint8_t bits_per_sample, uint8_t channels);

// Initialization
void mqtt_client_init(void);
void mqtt_client_start(void);
void mqtt_client_stop(void);
bool mqtt_client_is_connected(void);

// Publishing
esp_err_t mqtt_client_publish_audio_data(const uint8_t *data, int len,
                                         int count, int value);
esp_err_t mqtt_client_publish_voice_data(const char *data, int len);

// Callbacks
void mqtt_client_set_ai_callback(ai_response_cb_t callback);
void mqtt_client_set_tts_callback(tts_audio_cb_t callback);

#endif