// mqtt_client.c
#include "my_mqtt.h"
#include "esp_system.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

// MQTT includes
#include "esp_tls.h"
#include "esp_event.h"
#include "mqtt_client.h"

static const char *TAG = "MQTT_CLIENT";

static esp_mqtt_client_handle_t mqtt_client = NULL;
static bool mqtt_connected = false;
static ai_response_cb_t ai_callback = NULL;
static tts_audio_cb_t tts_callback = NULL;

// Buffer for receiving MQTT data
#define MQTT_MAX_MESSAGE_SIZE 131072 // 128KB max
static char *mqtt_rx_buffer = NULL;
static int mqtt_rx_total_len = 0;
static int mqtt_rx_current_len = 0;
static char mqtt_rx_topic[64] = {0};

// ========== FUNCTION DECLARATIONS ==========
static void mqtt_event_handler(void *handler_args, esp_event_base_t base,
                               int32_t event_id, void *event_data);
static char *base64_encode_simple(const uint8_t *data, size_t len, size_t *out_len);
static void handle_complete_message(void);
// ===========================================

// Simple base64 encoding
static char *base64_encode_simple(const uint8_t *data, size_t len, size_t *out_len)
{
    const char base64_chars[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

    *out_len = 4 * ((len + 2) / 3);

    // DEBUG: Log memory info
    ESP_LOGI(TAG, "📊 Base64 encoding: %d bytes -> %d chars, free heap: %d",
             len, *out_len, esp_get_free_heap_size());

    // If too large, only encode 1 second (32000 bytes PCM)
    if (*out_len > 80000)
    {
        ESP_LOGW(TAG, "⚠️ Base64 output too large, truncating to 1 second");
        len = 32000; // 1 second @ 16kHz 16-bit
        *out_len = 4 * ((len + 2) / 3);
    }

    char *encoded = malloc(*out_len + 1);
    if (!encoded)
    {
        ESP_LOGE(TAG, "❌ Failed to allocate %d bytes for base64", *out_len + 1);
        ESP_LOGE(TAG, "   Free heap: %d", esp_get_free_heap_size());
        return NULL;
    }

    // encode
    for (size_t i = 0, j = 0; i < len;)
    {
        uint32_t octet_a = i < len ? data[i++] : 0;
        uint32_t octet_b = i < len ? data[i++] : 0;
        uint32_t octet_c = i < len ? data[i++] : 0;

        uint32_t triple = (octet_a << 16) | (octet_b << 8) | octet_c;

        encoded[j++] = base64_chars[(triple >> 18) & 0x3F];
        encoded[j++] = base64_chars[(triple >> 12) & 0x3F];
        encoded[j++] = base64_chars[(triple >> 6) & 0x3F];
        encoded[j++] = base64_chars[triple & 0x3F];
    }

    // Padding
    for (size_t pad = 0; pad < (3 - len % 3) % 3; pad++)
    {
        encoded[*out_len - 1 - pad] = '=';
    }

    encoded[*out_len] = '\0';
    return encoded;
}

static void handle_complete_message(void)
{
    if (mqtt_rx_current_len == 0 || strlen(mqtt_rx_topic) == 0)
        return;

    ESP_LOGI(TAG, "✅ Complete message received: %d bytes", mqtt_rx_current_len);

    // Null terminate
    mqtt_rx_buffer[mqtt_rx_current_len] = '\0';

    // DEBUG: Print first 200 characters for debugging
    ESP_LOGI(TAG, "📄 Message preview (200 chars): %.200s", mqtt_rx_buffer);

    // Find tts_audio in response - FIXED: find correct field name
    char *tts_start = strstr(mqtt_rx_buffer, "\"tts_audio\":\"");
    if (tts_start)
    {
        // Find " after :
        tts_start = strchr(tts_start, ':');
        if (tts_start)
        {
            tts_start++; // Skip :
            // Skip whitespace if any
            while (*tts_start == ' ' || *tts_start == '\t')
            {
                tts_start++;
            }
            // Check if there is a "
            if (*tts_start != '\"')
            {
                tts_start = NULL;
            }
            else
            {
                tts_start++; // Skip "
            }
        }
    }
    if (!tts_start)
    {
        // Try another format
        tts_start = strstr(mqtt_rx_buffer, "\"audio_data\":\"");
    }

    if (!tts_start)
    {
        ESP_LOGW(TAG, "⚠️ No TTS audio found in response");
        ESP_LOGI(TAG, "📋 Full response keys (first 300 chars): %.300s", mqtt_rx_buffer);

        // Call AI callback if there is an AI response
        char *ai_start = strstr(mqtt_rx_buffer, "\"ai_response\":\"");
        if (ai_start && ai_callback)
        {
            ai_start += strlen("\"ai_response\":\"");
            char *ai_end = strchr(ai_start, '\"');
            if (ai_end)
            {
                size_t ai_len = ai_end - ai_start;
                if (ai_len > 0 && ai_len < 256)
                {
                    char *ai_text = malloc(ai_len + 1);
                    if (ai_text)
                    {
                        strncpy(ai_text, ai_start, ai_len);
                        ai_text[ai_len] = '\0';
                        ai_callback(ai_text);
                        free(ai_text);
                    }
                }
            }
        }
        return;
    }

    tts_start += strlen("\"tts_audio\":\"");

    // Find closing "
    char *tts_end = strchr(tts_start, '\"');
    if (!tts_end)
    {
        ESP_LOGE(TAG, "❌ No closing quote for tts_audio");
        return;
    }

    size_t tts_len = tts_end - tts_start;
    ESP_LOGI(TAG, "🎵 TTS Audio length: %d chars", tts_len);

    if (tts_len < 100)
    {
        ESP_LOGW(TAG, "⚠️ TTS audio too short: %d chars", tts_len);
        return;
    }

    // Extract sample rate
    uint32_t sample_rate = 16000;
    char *sr_ptr = strstr(mqtt_rx_buffer, "\"audio_sample_rate\":");
    if (sr_ptr)
    {
        sr_ptr += strlen("\"audio_sample_rate\":");
        sample_rate = atoi(sr_ptr);
    }

    // Copy tts audio
    char *audio_data = malloc(tts_len + 1);
    if (!audio_data)
    {
        ESP_LOGE(TAG, "❌ Failed to allocate %d bytes for audio", tts_len + 1);
        ESP_LOGE(TAG, "   Free heap: %d", esp_get_free_heap_size());
        return;
    }

    memcpy(audio_data, tts_start, tts_len);
    audio_data[tts_len] = '\0';

    ESP_LOGI(TAG, "🎵 TTS audio preview (first 100 chars): %.100s", audio_data);
    ESP_LOGI(TAG, "🎵 TTS audio preview (last 100 chars): %.100s",
             audio_data + (tts_len > 100 ? tts_len - 100 : 0));

    // Call callback
    if (tts_callback)
    {
        ESP_LOGI(TAG, "🔊 Calling TTS callback: %dHz, 16-bit, 1 channel", sample_rate);
        tts_callback(audio_data, sample_rate, 16, 1);
    }
    else
    {
        ESP_LOGW(TAG, "⚠️ No TTS callback registered");
    }

    free(audio_data);

    // Call AI callback if there is an AI response
    if (ai_callback)
    {
        char *ai_start = strstr(mqtt_rx_buffer, "\"ai_response\":\"");
        if (ai_start)
        {
            ai_start += strlen("\"ai_response\":\"");
            char *ai_end = strchr(ai_start, '\"');
            if (ai_end)
            {
                size_t ai_len = ai_end - ai_start;
                if (ai_len > 0 && ai_len < 256)
                {
                    char *ai_text = malloc(ai_len + 1);
                    if (ai_text)
                    {
                        strncpy(ai_text, ai_start, ai_len);
                        ai_text[ai_len] = '\0';
                        ai_callback(ai_text);
                        free(ai_text);
                    }
                }
            }
        }
    }
}

// MQTT event handler - UPDATED
static void mqtt_event_handler(void *handler_args, esp_event_base_t base,
                               int32_t event_id, void *event_data)
{
    esp_mqtt_event_handle_t event = event_data;

    switch (event->event_id)
    {
    case MQTT_EVENT_BEFORE_CONNECT:
        ESP_LOGI(TAG, "🔄 MQTT connecting...");
        break;

    case MQTT_EVENT_CONNECTED:
        ESP_LOGI(TAG, "✅ MQTT CONNECTED to Cloud Broker!");
        mqtt_connected = true;

        // Subscribe with QoS 0
        esp_mqtt_client_subscribe(mqtt_client, "voice/response", 0);
        ESP_LOGI(TAG, "📡 Subscribed to: voice/response (QoS 0)");
        break;

    case MQTT_EVENT_DISCONNECTED:
        ESP_LOGI(TAG, "❌ MQTT disconnected");
        mqtt_connected = false;
        // Free buffer when disconnect
        if (mqtt_rx_buffer)
        {
            free(mqtt_rx_buffer);
            mqtt_rx_buffer = NULL;
        }
        break;

    case MQTT_EVENT_DATA:
        // INCREASE limit to 40KB
        if (event->total_data_len > 40960)
        {
            ESP_LOGW(TAG, "⚠️ Skipping large message: %d bytes", event->total_data_len);
            return;
        }

        // Start of new message
        if (event->current_data_offset == 0)
        {
            // Free old buffer if any
            if (mqtt_rx_buffer)
            {
                free(mqtt_rx_buffer);
                mqtt_rx_buffer = NULL;
            }

            // Allocate just enough for message
            mqtt_rx_buffer = malloc(event->total_data_len + 1);
            if (!mqtt_rx_buffer)
            {
                ESP_LOGE(TAG, "❌ Failed to allocate %d bytes", event->total_data_len + 1);
                ESP_LOGE(TAG, "   Free heap: %d", esp_get_free_heap_size());
                mqtt_rx_current_len = 0;
                mqtt_rx_total_len = 0;
                return;
            }

            mqtt_rx_current_len = 0;
            mqtt_rx_total_len = event->total_data_len;

            // Save topic
            if (event->topic_len > 0 && event->topic_len < sizeof(mqtt_rx_topic))
            {
                strncpy(mqtt_rx_topic, event->topic, event->topic_len);
                mqtt_rx_topic[event->topic_len] = '\0';
                ESP_LOGI(TAG, "📥 Receiving message on topic: %s", mqtt_rx_topic);
            }
        }

        // Copy data if buffer is valid
        if (mqtt_rx_buffer &&
            mqtt_rx_current_len + event->data_len <= mqtt_rx_total_len)
        {
            memcpy(mqtt_rx_buffer + mqtt_rx_current_len,
                   event->data, event->data_len);
            mqtt_rx_current_len += event->data_len;

            // Log progress
            ESP_LOGI(TAG, "📥 Received chunk: %d/%d bytes",
                     mqtt_rx_current_len, mqtt_rx_total_len);
        }

        // Message complete
        if (event->current_data_offset + event->data_len >= event->total_data_len)
        {
            if (mqtt_rx_buffer && mqtt_rx_current_len > 0)
            {
                mqtt_rx_buffer[mqtt_rx_current_len] = '\0';
                ESP_LOGI(TAG, "✅ Complete message: %d bytes", mqtt_rx_current_len);

                // ⚠️ HANDLE IMMEDIATELY then free memory
                handle_complete_message();

                free(mqtt_rx_buffer);
                mqtt_rx_buffer = NULL;
                mqtt_rx_current_len = 0;
                mqtt_rx_total_len = 0;
            }
        }
        break;

    case MQTT_EVENT_ERROR:
        ESP_LOGE(TAG, "❌ MQTT ERROR");
        break;

    case MQTT_EVENT_SUBSCRIBED:
        ESP_LOGI(TAG, "✅ Subscribed to topic");
        break;

    case MQTT_EVENT_PUBLISHED:
        ESP_LOGI(TAG, "✅ Message published");
        break;

    default:
        break;
    }
}

// ========== PUBLIC FUNCTIONS ==========

void mqtt_client_init(void)
{
    ESP_LOGI(TAG, "🚀 Initializing MQTT Client");

    // Memory-optimized configuration
    esp_mqtt_client_config_t mqtt_cfg = {
        .broker = {
            .address.uri = "mqtt://broker.emqx.io:1883",
        },
        .credentials = {
            .client_id = "xiao_esp32s3",
        },
        .session = {.keepalive = 60, .last_will = {.topic = "voice/status",
                                                   .msg = "{\"status\":\"offline\"}", // Shorter
                                                   .qos = 0,                          // QoS 0 to save memory
                                                   .retain = 0}},
        .buffer = {
            .size = 32768,    // ⚠️ Reduced: 32KB input buffer (from 128KB)
            .out_size = 8192, // ⚠️ Reduced: 8KB output buffer (from 32KB)
        },
        .task = {
            .stack_size = 6144, // ⚠️ Reduced: 6KB stack (from 12KB)
        },
        .network = {
            .reconnect_timeout_ms = 5000,
        }};

    // Initialize MQTT client
    mqtt_client = esp_mqtt_client_init(&mqtt_cfg);
    if (mqtt_client == NULL)
    {
        ESP_LOGE(TAG, "❌ Failed to initialize MQTT client");
        return;
    }

    // Register event handler
    esp_mqtt_client_register_event(mqtt_client, ESP_EVENT_ANY_ID,
                                   mqtt_event_handler, NULL);

    ESP_LOGI(TAG, "✅ MQTT Client initialized (optimized for memory)");
}

void mqtt_client_start(void)
{
    ESP_LOGI(TAG, "🎯 Starting MQTT Client");
    if (mqtt_client)
    {
        esp_mqtt_client_start(mqtt_client);
    }
    else
    {
        ESP_LOGE(TAG, "❌ MQTT client not initialized");
    }
}

void mqtt_client_stop(void)
{
    ESP_LOGI(TAG, "🛑 Stopping MQTT Client");
    if (mqtt_client)
    {
        esp_mqtt_client_stop(mqtt_client);
    }
    mqtt_connected = false;
}

bool mqtt_client_is_connected(void)
{
    bool connected = mqtt_connected && (mqtt_client != NULL);
    ESP_LOGI(TAG, "🔗 MQTT connected: %s", connected ? "YES" : "NO");
    return connected;
}

// In the function mqtt_client_publish_audio_data, edit:
esp_err_t mqtt_client_publish_audio_data(const uint8_t *data, int len, int count, int value)
{
    if (!mqtt_client || !mqtt_connected)
    {
        ESP_LOGW(TAG, "⚠️ MQTT not ready, skipping");
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "🎵 Audio to send: %d bytes (%.2f sec)",
             len, (float)len / (16000 * 2));

    // Increase to 3200 bytes (0.5 seconds) to be sufficient for Whisper (minimum 0.1s)
    int send_len = len;
    if (send_len > 8000)
    {
        send_len = 8000; // 0.5 seconds - SUFFICIENT FOR WHISPER
        ESP_LOGI(TAG, "⚠️ Truncating audio to %d bytes (0.5 second)", send_len);
    }

    // GUARANTEED MINIMUM 1600 bytes (0.1 seconds)
    if (send_len < 1600)
    {
        ESP_LOGW(TAG, "⚠️ Audio too short, padding to 1600 bytes");
        send_len = 1600; // Minimum 0.1 seconds
    }

    size_t free_heap = esp_get_free_heap_size();
    ESP_LOGI(TAG, "📊 Before encode: free heap = %d", free_heap);

    // Encode audio to base64
    size_t b64_len;
    char *b64_audio = base64_encode_simple(data, send_len, &b64_len);
    if (!b64_audio)
    {
        ESP_LOGE(TAG, "❌ Base64 encode failed");
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "✅ Base64 encoded: %d bytes -> %d chars", send_len, b64_len);

    // ⚠️ Use FULL JSON format for correct server parsing
    int json_size = b64_len + 256; // Enough for full JSON
    char *json_buffer = malloc(json_size);
    if (!json_buffer)
    {
        ESP_LOGE(TAG, "❌ Failed to allocate %d bytes for JSON", json_size);
        free(b64_audio);
        return ESP_FAIL;
    }

    // ⚠️ USE FULL JSON FIELD NAMES for correct server parsing
    int json_len = snprintf(json_buffer, json_size,
                            "{\"device\":\"xiao\",\"count\":%d,\"adc_value\":%d,\"audio_data\":\"%s\"}",
                            count, value, b64_audio);

    free(b64_audio);

    if (json_len <= 0 || json_len >= json_size)
    {
        ESP_LOGE(TAG, "❌ JSON creation failed");
        free(json_buffer);
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "📤 Publishing #%d: %d bytes JSON", count, json_len);
    ESP_LOGI(TAG, "   JSON preview: %.100s...", json_buffer);

    // Publish with QoS 0
    int msg_id = esp_mqtt_client_publish(mqtt_client, "voice/audio",
                                         json_buffer, json_len, 0, 0);

    free(json_buffer);

    if (msg_id < 0)
    {
        ESP_LOGE(TAG, "❌ MQTT publish failed");
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "✅ MQTT publish queued (msg_id=%d)", msg_id);
    return ESP_OK;
}

void mqtt_client_set_ai_callback(ai_response_cb_t callback)
{
    ai_callback = callback;
    ESP_LOGI(TAG, "🎯 AI Callback registered");
}

void mqtt_client_set_tts_callback(tts_audio_cb_t callback)
{
    tts_callback = callback;
    ESP_LOGI(TAG, "🎵 TTS Audio Callback registered");
}

esp_err_t mqtt_client_publish_voice_data(const char *data, int len)
{
    return ESP_OK; // Not used anymore
}
