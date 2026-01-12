#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "oled_display.h"
#include "i2s_audio.h"
#include "adc_handler.h"
#include "audio_recorder.h"
#include "wifi_manager.h"
#include "my_mqtt.h"
#include "base64_decoder.h"

static const char *TAG = "MAIN";
#define DEBUG_AUDIO 1          // 1 to enable debugging, 0 to disable
#define MAX_DEBUG_SAMPLES 1000 // Maximum samples to debug
// Global variables
static bool is_recording = false;
static int activation_count = 0;
// static uint32_t last_playback_time = 0;
static uint32_t last_playback_time __attribute__((unused)) = 0;
static bool wifi_connected = false;

// Function declarations
void voice_ai_task(void *pvParameters);
void send_audio_to_cloud(int16_t *audio_buffer, size_t sample_count, int adc_value);
void ai_response_handler(const char *response);
void tts_audio_handler(const char *audio_base64, uint32_t sample_rate,
                       uint8_t bits_per_sample, uint8_t channels);

// Audio debug function - logs detailed information about audio
static void debug_audio_data(int16_t *buffer, size_t sample_count, const char *tag)
{
#if DEBUG_AUDIO
    if (!buffer || sample_count == 0)
        return;

    ESP_LOGI(TAG, "🔍 DEBUG AUDIO [%s]:", tag);

    // Basic information
    ESP_LOGI(TAG, "  Samples: %d, Duration: %.2f sec",
             sample_count, (float)sample_count / 16000.0f);

    // Calculate statistical values
    int64_t sum = 0, sum_sq = 0;
    int16_t min_val = 32767, max_val = -32768;
    int clip_pos = 0, clip_neg = 0;
    int silent_samples = 0;
    int16_t silent_threshold = 100;

    for (size_t i = 0; i < sample_count; i++)
    {
        int16_t val = buffer[i];
        int16_t abs_val = abs(val);

        sum += val;
        sum_sq += (int64_t)val * val;

        if (val < min_val)
            min_val = val;
        if (val > max_val)
            max_val = val;

        if (val >= 32000)
            clip_pos++; // Near clipping surface
        if (val <= -32000)
            clip_neg++;

        if (abs_val < silent_threshold)
            silent_samples++;
    }

    float mean = (float)sum / sample_count;
    float rms = sqrt((float)sum_sq / sample_count);
    float silence_percent = (float)silent_samples / sample_count * 100.0f;
    float clip_pos_percent = (float)clip_pos / sample_count * 100.0f;
    float clip_neg_percent = (float)clip_neg / sample_count * 100.0f;

    // Statistical logs
    ESP_LOGI(TAG, "  Min: %d, Max: %d, Peak-to-Peak: %d",
             min_val, max_val, max_val - min_val);
    ESP_LOGI(TAG, "  Mean: %.1f, RMS: %.1f", mean, rms);
    ESP_LOGI(TAG, "  Silence (<%d): %.1f%%", silent_threshold, silence_percent);
    ESP_LOGI(TAG, "  Clipping (+): %.1f%%, (-): %.1f%%",
             clip_pos_percent, clip_neg_percent);

    // Quality checks
    if (rms < 500)
    {
        ESP_LOGW(TAG, "  ⚠️ Audio too quiet (RMS=%.1f)", rms);
    }
    else if (rms > 10000)
    {
        ESP_LOGI(TAG, "  ✅ Good volume (RMS=%.1f)", rms);
    }

    if (clip_pos_percent > 5.0 || clip_neg_percent > 5.0)
    {
        ESP_LOGW(TAG, "  ⚠️ Heavy clipping detected!");
    }

    // Print first 20 samples for waveform check
    ESP_LOGI(TAG, "  First 20 samples:");
    char sample_str[256] = {0};
    int offset = 0;

    for (int i = 0; i < (sample_count < 20 ? sample_count : 20); i++)
    {
        offset += snprintf(sample_str + offset, sizeof(sample_str) - offset,
                           "%6d ", buffer[i]);
        if ((i + 1) % 10 == 0)
        {
            ESP_LOGI(TAG, "    %s", sample_str);
            offset = 0;
            sample_str[0] = '\0';
        }
    }
    if (offset > 0)
    {
        ESP_LOGI(TAG, "    %s", sample_str);
    }
#endif
}

// Function to save audio to file (if SPIFFS or SD card available)
static void save_audio_to_file(int16_t *buffer, size_t sample_count, const char *filename_prefix)
{
#if DEBUG_AUDIO
    // Check if SPIFFS is available (needs prior configuration)
    // If not, just log information
    ESP_LOGI(TAG, "💾 Would save %d samples to %s_%d.raw",
             sample_count, filename_prefix, activation_count);

    // Create simple ASCII waveform
    if (sample_count > 100)
    {
        ESP_LOGI(TAG, "📈 Waveform preview (100 samples):");

        // Find max value for scaling
        int16_t max_abs = 0;
        for (int i = 0; i < 100; i++)
        {
            int16_t abs_val = abs(buffer[i]);
            if (abs_val > max_abs)
                max_abs = abs_val;
        }

        if (max_abs == 0)
            max_abs = 1;

        // Draw waveform
        for (int row = 10; row >= -10; row--)
        {
            char line[101] = {0};
            int threshold = row * max_abs / 10;

            for (int i = 0; i < 100; i++)
            {
                if (buffer[i] >= threshold - (max_abs / 20) &&
                    buffer[i] <= threshold + (max_abs / 20))
                {
                    line[i] = '*';
                }
                else
                {
                    line[i] = ' ';
                }
            }
            ESP_LOGI(TAG, "  %+5d |%s|", threshold, line);
        }
    }
#endif
}

// Simple test tone constructor for testing
static void generate_and_send_test_tone(int frequency_hz, int duration_ms)
{
    ESP_LOGI(TAG, "🎵 Generating test tone: %dHz, %dms", frequency_hz, duration_ms);

    int sample_rate = 16000;
    int num_samples = sample_rate * duration_ms / 1000;

    // LIMIT: maximum 0.25 seconds (4000 samples)
    if (num_samples > 4000)
    {
        num_samples = 4000;
        duration_ms = 250;
        ESP_LOGI(TAG, "⚠️ Test tone limited to %d samples (0.25s)", num_samples);
    }
    int16_t *test_audio = malloc(num_samples * sizeof(int16_t));
    if (!test_audio)
    {
        ESP_LOGE(TAG, "❌ Failed to allocate test audio");
        return;
    }

    // Create sine wave
    for (int i = 0; i < num_samples; i++)
    {
        float t = (float)i / sample_rate;
        float sample = sin(2 * M_PI * frequency_hz * t) * 0.7f; // 70% volume
        test_audio[i] = (int16_t)(sample * 32767.0f);
    }

    // Debug audio
    debug_audio_data(test_audio, num_samples, "TEST_TONE");

    // Send to cloud
    if (mqtt_client_is_connected())
    {
        ESP_LOGI(TAG, "📤 Sending test tone to cloud...");
        send_audio_to_cloud(test_audio, num_samples, 2500);
    }
    else
    {
        ESP_LOGW(TAG, "⚠️ MQTT not connected for test tone");
    }

    free(test_audio);
}

// Function to test microphone quality by recording a short sample and analyzing it
static void test_microphone_quality(void)
{
    ESP_LOGI(TAG, "🎤 Testing microphone quality...");
    oled_display_status("Mic Test...");

    // Record 0.5 seconds
    int16_t *test_buffer = malloc(8000 * sizeof(int16_t));
    if (!test_buffer)
        return;

    // Start recording
    audio_recorder_start();
    size_t samples = audio_recorder_read(test_buffer, 8000, pdMS_TO_TICKS(600));
    audio_recorder_stop();

    if (samples > 0)
    {
        ESP_LOGI(TAG, "✅ Recorded %d samples for microphone test", samples);

        // Detailed analysis
        debug_audio_data(test_buffer, samples, "MIC_TEST");

        // Display results on OLED
        int64_t sum_sq = 0;
        for (int i = 0; i < samples; i++)
        {
            sum_sq += (int64_t)test_buffer[i] * test_buffer[i];
        }
        float rms = sqrt((float)sum_sq / samples);

        char result[32];
        if (rms < 300)
        {
            snprintf(result, sizeof(result), "Mic: TOO QUIET");
            ESP_LOGW(TAG, "⚠️ Microphone may be disconnected or broken");
        }
        else if (rms > 15000)
        {
            snprintf(result, sizeof(result), "Mic: TOO LOUD");
            ESP_LOGW(TAG, "⚠️ Microphone may be clipping");
        }
        else if (rms > 3000)
        {
            snprintf(result, sizeof(result), "Mic: GOOD");
            ESP_LOGI(TAG, "✅ Microphone quality is good");
        }
        else
        {
            snprintf(result, sizeof(result), "Mic: WEAK");
            ESP_LOGW(TAG, "⚠️ Microphone signal is weak");
        }

        oled_show_message(result);
        vTaskDelay(pdMS_TO_TICKS(2000));
    }
    else
    {
        ESP_LOGE(TAG, "❌ Failed to record microphone test");
        oled_show_message("Mic Test FAILED");
        vTaskDelay(pdMS_TO_TICKS(1000));
    }

    free(test_buffer);
    oled_show_message("");
    oled_display_status("Ready");
}

static void play_fallback_beep(void)
{
    ESP_LOGI(TAG, "🔊 Playing fallback beep");

    // Create simple 0.25 second beep
    int sample_rate = 16000;
    int duration_ms = 250;
    int num_samples = sample_rate * duration_ms / 1000;

    int16_t *beep_data = malloc(num_samples * sizeof(int16_t));
    if (!beep_data)
        return;

    // Create 2 short beeps
    for (int i = 0; i < num_samples; i++)
    {
        float t = (float)i / sample_rate;

        if (t < 0.1 || (t >= 0.15 && t < 0.25))
        {
            // Beep at 660Hz
            beep_data[i] = (int16_t)(sin(2 * M_PI * 660 * t) * 8000);
        }
        else
        {
            beep_data[i] = 0;
        }
    }

    // Play beep
    i2s_audio_play_pcm((uint8_t *)beep_data, num_samples * 2, sample_rate);
    free(beep_data);
}

void tts_audio_handler(const char *audio_base64, uint32_t sample_rate,
                       uint8_t bits_per_sample, uint8_t channels)
{
    ESP_LOGI(TAG, "🎵 TTS Audio Handler: %d chars", strlen(audio_base64));

    // Check if audio is valid
    if (audio_base64 == NULL || strlen(audio_base64) < 100)
    {
        ESP_LOGW(TAG, "⚠️ TTS audio too short or NULL");
        oled_display_status("No Audio");
        play_fallback_beep();
        return;
    }

    // Check if all zeros or all 'A's
    int zero_count = 0;
    int a_count = 0;
    for (int i = 0; i < 100 && i < strlen(audio_base64); i++)
    {
        if (audio_base64[i] == 'A')
            a_count++;
        if (audio_base64[i] == '0')
            zero_count++;
    }

    if (a_count > 90 || zero_count > 90)
    {
        ESP_LOGW(TAG, "⚠️ TTS audio all zeros or As (%d A, %d 0)", a_count, zero_count);
        oled_display_status("Audio Error");
        play_fallback_beep();
        return;
    }

    oled_display_status("Playing AI...");

    // Decode base64
    size_t audio_len;
    uint8_t *audio_data = base64_decode_alloc(audio_base64, &audio_len);

    if (audio_data == NULL || audio_len == 0)
    {
        ESP_LOGE(TAG, "❌ Base64 decode failed");
        oled_display_status("Decode Failed");
        play_fallback_beep();
        return;
    }

    ESP_LOGI(TAG, "✅ Decoded: %d bytes, %dHz", audio_len, sample_rate);

    ESP_LOGI(TAG, "🎵 Base64 length: %d chars", strlen(audio_base64));
    ESP_LOGI(TAG, "🎵 Base64 preview (first 100): %.100s", audio_base64);
    ESP_LOGI(TAG, "🎵 Base64 preview (last 100): %.100s",
             audio_base64 + (strlen(audio_base64) - 100));
    // Check if audio data is all zeros
    zero_count = 0;
    for (int i = 0; i < 100 && i < audio_len; i++)
    {
        if (audio_data[i] == 0)
            zero_count++;
    }

    if (zero_count > 95)
    {
        ESP_LOGW(TAG, "⚠️ Audio data all zeros (%d/100)", zero_count);
        free(audio_data);
        play_fallback_beep();
        return;
    }

    // Play audio
    esp_err_t ret = i2s_audio_play_pcm(audio_data, audio_len, sample_rate);

    if (ret == ESP_OK)
    {
        ESP_LOGI(TAG, "✅ Audio playback completed");
        oled_display_status("Playback Done");
    }
    else
    {
        ESP_LOGE(TAG, "❌ Playback failed: %d", ret);
        oled_display_status("Playback Failed");
        play_fallback_beep();
    }

    free(audio_data);

    // Return to ready state
    vTaskDelay(pdMS_TO_TICKS(500));
    oled_display_status("Ready");
}

void ai_response_handler(const char *response)
{
    if (!response || strlen(response) == 0)
    {
        ESP_LOGE(TAG, "❌ Empty AI response");
        return;
    }

    ESP_LOGI(TAG, "🤖 AI Response: %s", response);
    oled_display_status("AI Processing...");

    // Display on OLED
    if (strlen(response) > 20)
    {
        // Split into 2 lines
        char line1[32], line2[32];
        strncpy(line1, response, 20);
        line1[20] = '\0';
        strncpy(line2, response + 20, 20);
        line2[20] = '\0';

        oled_show_message(line1);
        vTaskDelay(pdMS_TO_TICKS(1500));
        oled_show_message(line2);
    }
    else
    {
        oled_show_message(response);
    }

    vTaskDelay(pdMS_TO_TICKS(2000));

    // Clear display
    oled_show_message("");
    oled_display_status("Ready");
}

void send_audio_to_cloud(int16_t *audio_buffer, size_t sample_count, int adc_value)
{
    if (!audio_buffer || sample_count == 0)
    {
        ESP_LOGE(TAG, "❌ No audio to send");
        return;
    }

    ESP_LOGI(TAG, "☁️ Sending %d samples (%d bytes) to cloud",
             sample_count, sample_count * sizeof(int16_t));
    // GUARANTEED MINIMUM 0.2 SECONDS (3200 bytes)
    if (sample_count * 2 < 3200)
    {
        ESP_LOGW(TAG, "⚠️ Audio too short (%.2f sec), but sending anyway",
                 (float)sample_count / 16000.0f);
    }
    // DEBUG: Analyze audio details before sending
    debug_audio_data(audio_buffer, sample_count, "BEFORE_SEND");

    // DEBUG: Save waveform preview
    save_audio_to_file(audio_buffer, sample_count, "audio");

    // Log audio statistics - FIXED calculation
    int16_t min = 32767, max = -32768;
    int64_t sum = 0; // Use int64_t to avoid overflow

    for (int i = 0; i < sample_count; i++)
    {
        int16_t val = audio_buffer[i];
        if (val < min)
            min = val;
        if (val > max)
            max = val;
        sum += val;
    }

    // Calculate average correctly
    int32_t avg = (int32_t)(sum / sample_count);

    // Check if audio is silent
    int silent_samples = 0;
    for (int i = 0; i < sample_count; i++)
    {
        if (abs(audio_buffer[i]) < 100) // Almost silent
            silent_samples++;
    }

    float silent_percent = (silent_samples * 100.0f) / sample_count;

    ESP_LOGI(TAG, "📊 Audio stats: min=%d, max=%d, avg=%d, silent=%.1f%%",
             min, max, avg, silent_percent);

    // Check quality issues
    if (min <= -32000 && max >= 32000)
    {
        ESP_LOGW(TAG, "⚠️ SEVERE CLIPPING DETECTED!");
        ESP_LOGW(TAG, "   Consider reducing gain in audio_recorder.c");
    }

    if (silent_percent > 80.0f)
    {
        ESP_LOGW(TAG, "⚠️ Mostly silent audio - check microphone!");
    }

    oled_display_status("Sending...");

    // Send ALL audio data
    esp_err_t result = mqtt_client_publish_audio_data(
        (uint8_t *)audio_buffer,
        sample_count * sizeof(int16_t),
        activation_count,
        adc_value);

    if (result == ESP_OK)
    {
        ESP_LOGI(TAG, "✅ Audio sent successfully");
        oled_display_status("Sent!");
    }
    else
    {
        ESP_LOGE(TAG, "❌ Send failed");
        oled_display_status("Send Failed");
    }

    vTaskDelay(pdMS_TO_TICKS(100));
}

void check_mqtt_connection(void)
{
    static int last_connection_check = 0;
    static bool was_connected = false;

    int current_time = pdTICKS_TO_MS(xTaskGetTickCount());

    // Check every 2 seconds
    if (current_time - last_connection_check > 2000)
    {
        bool is_connected = mqtt_client_is_connected();

        if (is_connected && !was_connected)
        {
            ESP_LOGI(TAG, "🎉 MQTT CONNECTED!");
            oled_display_status("Cloud Ready");
            was_connected = true;
        }
        else if (!is_connected && was_connected)
        {
            ESP_LOGW(TAG, "⚠️ MQTT DISCONNECTED");
            oled_display_status("Cloud Offline");
            was_connected = false;
        }

        last_connection_check = current_time;
    }
}

void test_mqtt_publish_manual(void)
{
    if (!mqtt_client_is_connected())
    {
        ESP_LOGI(TAG, "⚠️ MQTT not connected, skipping test");
        return;
    }

    ESP_LOGI(TAG, "🔧 MANUAL TEST: Publishing test audio");

    // Create test audio data
    int16_t test_audio[400]; // 400 samples = 800 bytes
    for (int i = 0; i < 400; i++)
    {
        test_audio[i] = (int16_t)(sin(2 * M_PI * 440 * i / 16000) * 32767 * 0.3);
    }

    // Send via MQTT
    esp_err_t result = mqtt_client_publish_audio_data(
        (uint8_t *)test_audio,
        400 * sizeof(int16_t),
        999, // Test count
        1234 // Test ADC
    );

    if (result == ESP_OK)
    {
        ESP_LOGI(TAG, "✅ Manual test published");
        oled_show_message("Test Sent");
    }
    else
    {
        ESP_LOGE(TAG, "❌ Manual test failed");
        oled_show_message("Test Failed");
    }

    vTaskDelay(pdMS_TO_TICKS(2000));
    oled_show_message("");
}

void voice_ai_task(void *pvParameters)
{
    ESP_LOGI(TAG, "🎤 Voice AI Task Started");

    // Calibration
    oled_display_status("Calibrating...");

    // Read 50 samples for calibration
    int readings[50];
    int baseline = 0;
    // Calibration: 50 samples over 0.5 seconds
    for (int i = 0; i < 50; i++)
    {
        readings[i] = adc_handler_read_single();
        baseline += readings[i];
        vTaskDelay(pdMS_TO_TICKS(10));
    }
    baseline /= 50;

    // Calculate noise level (average deviation)
    int noise = 0;
    for (int i = 0; i < 50; i++)
    {
        noise += abs(readings[i] - baseline);
    }
    noise /= 50;

    // Threshold: baseline + 1.5 * noise
    int threshold = baseline + noise * 3 / 2; // 1.5x noise

    // Ensure reasonable threshold
    if (threshold < baseline + 150)
        threshold = baseline + 150;
    if (threshold > 2500)
        threshold = 2500;

    ESP_LOGI(TAG, "🎯 Calibration: Baseline=%d, Noise=%d, Threshold=%d",
             baseline, noise, threshold);

    oled_show_message("Ready");
    oled_display_status("Speak Now");

    uint32_t last_voice_time = 0;
    const uint32_t DEBOUNCE_MS = 3000; // 3 seconds debounce

    while (1)
    {
        int mic_value = adc_handler_read_single();
        uint32_t now = pdTICKS_TO_MS(xTaskGetTickCount());

        // Update OLED every 500ms
        static uint32_t last_oled_update = 0;
        if (now - last_oled_update > 500)
        {
            oled_display_adc(mic_value);
            last_oled_update = now;
        }

        // Voice detection (not recording and debounce passed)
        if (mic_value > threshold && !is_recording &&
            (now - last_voice_time) > DEBOUNCE_MS)
        {
            activation_count++;
            ESP_LOGI(TAG, "🎤 Voice #%d detected: %d > %d",
                     activation_count, mic_value, threshold);

            oled_display_status("Recording...");
            is_recording = true;

            // ⚠️ REDUCE buffer to only 8000 samples (0.5 seconds)
            int16_t *audio_buffer = malloc(8000 * sizeof(int16_t));
            if (audio_buffer)
            {
                // Record 0.5 seconds (8000 samples)
                audio_recorder_start();
                size_t samples = audio_recorder_read(audio_buffer, 8000,
                                                     pdMS_TO_TICKS(600)); // 600ms timeout

                audio_recorder_stop();
                if (samples >= 4000) // At least 0.25 seconds
                {
                    ESP_LOGI(TAG, "🎙️ Recorded %d samples (%.3f sec)",
                             samples, (float)samples / 16000.0f);

                    // Calculate simple RMS
                    int64_t sum_sq = 0;
                    for (int i = 0; i < samples; i++)
                    {
                        sum_sq += (int64_t)audio_buffer[i] * audio_buffer[i];
                    }
                    float rms = sqrt((float)sum_sq / samples);

                    ESP_LOGI(TAG, "📊 Audio RMS: %.1f", rms);

                    if (rms > 1000.0f) // Has real sound
                    {
                        if (mqtt_client_is_connected())
                        {
                            send_audio_to_cloud(audio_buffer, samples, mic_value);
                        }
                        else
                        {
                            ESP_LOGW(TAG, "⚠️ MQTT offline");
                            oled_display_status("Cloud Offline");
                            vTaskDelay(pdMS_TO_TICKS(1000));
                        }
                    }
                    else
                    {
                        ESP_LOGW(TAG, "⚠️ Audio too quiet (RMS=%.1f)", rms);
                        oled_display_status("Too Quiet");
                        vTaskDelay(pdMS_TO_TICKS(500));
                    }
                }
                else
                {
                    ESP_LOGW(TAG, "⚠️ Not enough samples: %d", samples);
                    oled_display_status("No Audio");
                }

                free(audio_buffer);
            }
            else
            {
                ESP_LOGE(TAG, "❌ Failed to allocate audio buffer");
            }

            is_recording = false;
            last_voice_time = now;
            oled_display_status("Ready");
        }

        vTaskDelay(pdMS_TO_TICKS(20)); // 50Hz sampling
    }
}

void test_json_format(void)
{
    if (!mqtt_client_is_connected())
    {
        ESP_LOGI(TAG, "⚠️ MQTT not connected");
        return;
    }

    ESP_LOGI(TAG, "🔧 Testing JSON format...");

    // Create test audio data
    int sample_count = 1000; // 0.0625 s
    int16_t *test_audio = malloc(sample_count * sizeof(int16_t));
    if (!test_audio)
        return;

    // Create 440Hz sine wave
    for (int i = 0; i < sample_count; i++)
    {
        float t = (float)i / 16000.0f;
        float sample = sin(2 * M_PI * 440 * t) * 0.5f;
        test_audio[i] = (int16_t)(sample * 32767.0f);
    }

    // Send test
    esp_err_t result = mqtt_client_publish_audio_data(
        (uint8_t *)test_audio,
        sample_count * sizeof(int16_t),
        999, // Test count
        1234 // Test ADC
    );

    if (result == ESP_OK)
    {
        ESP_LOGI(TAG, "✅ Test JSON sent successfully");
        oled_show_message("JSON Test OK");
    }
    else
    {
        ESP_LOGE(TAG, "❌ Test JSON failed");
        oled_show_message("JSON Test FAIL");
    }

    free(test_audio);
    vTaskDelay(pdMS_TO_TICKS(2000));
    oled_show_message("");
}

static void check_memory_status(void)
{
    ESP_LOGI(TAG, "📊 Memory Status:");
    ESP_LOGI(TAG, "  Free heap: %d bytes", esp_get_free_heap_size());
    ESP_LOGI(TAG, "  Minimum free heap: %d bytes", esp_get_minimum_free_heap_size());

    // Check heap size
    multi_heap_info_t info;
    heap_caps_get_info(&info, MALLOC_CAP_8BIT);
    ESP_LOGI(TAG, "  Total free: %d, Largest free block: %d",
             info.total_free_bytes, info.largest_free_block);
}

void test_direct_tts_playback(void)
{
    ESP_LOGI(TAG, "🔊 TEST: Direct TTS playback");

    // Create test sine wave 0.5 s
    int sample_rate = 16000;
    int duration_ms = 500; // 0.5 s
    int num_samples = sample_rate * duration_ms / 1000;

    int16_t *audio_data = malloc(num_samples * sizeof(int16_t));
    if (!audio_data)
    {
        ESP_LOGE(TAG, "❌ Failed to allocate test audio");
        return;
    }

    // Create 440Hz sine wave
    for (int i = 0; i < num_samples; i++)
    {
        audio_data[i] = (int16_t)(sin(2 * M_PI * 440 * i / sample_rate) * 32767 * 0.3);
    }

    ESP_LOGI(TAG, "🎵 Playing test tone: %d samples, %dHz", num_samples, sample_rate);

    oled_display_status("Test Playback...");

    // Play audio
    esp_err_t ret = i2s_audio_play_pcm((uint8_t *)audio_data,
                                       num_samples * sizeof(int16_t),
                                       sample_rate);

    if (ret == ESP_OK)
    {
        ESP_LOGI(TAG, "✅ Test playback completed");
        oled_display_status("Test OK");
    }
    else
    {
        ESP_LOGE(TAG, "❌ Test playback failed: %d", ret);
        oled_display_status("Test Failed");
    }

    free(audio_data);

    vTaskDelay(pdMS_TO_TICKS(1000));
    oled_display_status("Ready");
}

void test_complete_pipeline(void)
{
    ESP_LOGI(TAG, "🔧 TEST: Complete AI Pipeline");

    // Create longer test audio (2 s)
    int sample_rate = 16000;
    int duration_ms = 2000; // 2 s
    int num_samples = sample_rate * duration_ms / 1000;

    int16_t *test_audio = malloc(num_samples * sizeof(int16_t));
    if (!test_audio)
        return;

    // Create audio with simulated speech
    for (int i = 0; i < num_samples; i++)
    {
        float t = (float)i / sample_rate;

        // Create sound resembling speech with multiple frequencies
        float sample =
            sin(2 * M_PI * 200 * t) * 0.3f + // Base
            sin(2 * M_PI * 400 * t) * 0.2f + // Harmonic 1
            sin(2 * M_PI * 600 * t) * 0.1f;  // Harmonic 2

        // Add modulation to resemble speech
        float mod = sin(2 * M_PI * 5 * t) * 0.1f + 1.0f;
        sample *= mod;

        // Fade in/out
        float envelope = 1.0f;
        if (t < 0.1f)
            envelope = t / 0.1f;
        if (t > 1.9f)
            envelope = (2.0f - t) / 0.1f;

        sample = sample * envelope * 0.5f * 32767.0f;
        test_audio[i] = (int16_t)sample;
    }

    ESP_LOGI(TAG, "🎵 Generated %d samples (%d bytes)",
             num_samples, num_samples * 2);

    oled_display_status("Test Mode...");
    oled_show_message("Sending to AI...");

    // Send to cloud
    if (mqtt_client_is_connected())
    {
        esp_err_t result = mqtt_client_publish_audio_data(
            (uint8_t *)test_audio,
            num_samples * sizeof(int16_t),
            999, // Test count
            2500 // Test ADC
        );

        if (result == ESP_OK)
        {
            ESP_LOGI(TAG, "✅ Test audio sent to AI");
            oled_show_message("Waiting AI...");

            // Wait for response (timeout 10 seconds)
            for (int i = 0; i < 20; i++)
            {
                vTaskDelay(pdMS_TO_TICKS(500));
                ESP_LOGI(TAG, "⏳ Waiting AI response... %d/20", i + 1);
            }
        }
        else
        {
            ESP_LOGE(TAG, "❌ Test send failed");
            oled_show_message("Send Failed");
        }
    }
    else
    {
        ESP_LOGE(TAG, "❌ MQTT not connected");
        oled_show_message("No Connection");
    }

    free(test_audio);

    vTaskDelay(pdMS_TO_TICKS(2000));
    oled_show_message("");
    oled_display_status("Ready");
}

void test_small_audio(void)
{
    ESP_LOGI(TAG, "🔊 TEST: Small audio test (0.25s)");

    // Create test audio 0.25 s
    int sample_rate = 16000;
    int duration_ms = 250; // 0.25 s
    int num_samples = sample_rate * duration_ms / 1000;

    int16_t *test_audio = malloc(num_samples * sizeof(int16_t));
    if (!test_audio)
        return;

    // Create 440Hz sine wave
    for (int i = 0; i < num_samples; i++)
    {
        float t = (float)i / sample_rate;
        float sample = sin(2 * M_PI * 440 * t) * 0.7f;
        test_audio[i] = (int16_t)(sample * 32767.0f);
    }

    // Send test
    if (mqtt_client_is_connected())
    {
        esp_err_t result = mqtt_client_publish_audio_data(
            (uint8_t *)test_audio,
            num_samples * sizeof(int16_t),
            999, // Test count
            2500 // Test ADC
        );

        if (result == ESP_OK)
        {
            ESP_LOGI(TAG, "✅ Test audio sent");
            oled_show_message("Test Sent");
        }
        else
        {
            ESP_LOGE(TAG, "❌ Test send failed");
            oled_show_message("Test Failed");
        }
    }

    free(test_audio);
    vTaskDelay(pdMS_TO_TICKS(2000));
    oled_show_message("");
}

void test_mini_audio(void)
{
    ESP_LOGI(TAG, "🔊 TEST: Mini audio test (0.1s)");

    // Create VERY SMALL test audio 0.1 s (1600 samples)
    int sample_rate = 16000;
    int duration_ms = 100; // 0.1 s
    int num_samples = sample_rate * duration_ms / 1000;

    int16_t *test_audio = malloc(num_samples * sizeof(int16_t));
    if (!test_audio)
    {
        ESP_LOGE(TAG, "❌ Failed to allocate mini audio");
        return;
    }

    // Create 440Hz sine wave
    for (int i = 0; i < num_samples; i++)
    {
        float t = (float)i / sample_rate;
        float sample = sin(2 * M_PI * 440 * t) * 0.7f;
        test_audio[i] = (int16_t)(sample * 32767.0f);
    }

    // Send test
    if (mqtt_client_is_connected())
    {
        ESP_LOGI(TAG, "📤 Sending mini test audio: %d bytes", num_samples * 2);

        esp_err_t result = mqtt_client_publish_audio_data(
            (uint8_t *)test_audio,
            num_samples * sizeof(int16_t),
            999, // Test count
            2500 // Test ADC
        );

        if (result == ESP_OK)
        {
            ESP_LOGI(TAG, "✅ Mini test audio sent");
            oled_show_message("Test Sent");
        }
        else
        {
            ESP_LOGE(TAG, "❌ Mini test send failed");
            oled_show_message("Test Failed");
        }
    }
    else
    {
        ESP_LOGW(TAG, "⚠️ MQTT not connected for mini test");
    }

    free(test_audio);
    vTaskDelay(pdMS_TO_TICKS(2000));
    oled_show_message("");
}

void test_audio_playback_short(void)
{
    // Only play 0.1 s for testing
    ESP_LOGI(TAG, "🔊 Testing short audio playback (0.1s)...");

    int sample_rate = 16000;
    int duration_ms = 100; // 0.1 s
    int num_samples = sample_rate * duration_ms / 1000;

    int16_t *audio_data = malloc(num_samples * sizeof(int16_t));
    if (!audio_data)
        return;

    // Create 440Hz sine wave
    for (int i = 0; i < num_samples; i++)
    {
        audio_data[i] = (int16_t)(sin(2 * M_PI * 440 * i / sample_rate) * 32767 * 0.2);
    }

    i2s_audio_play_pcm((uint8_t *)audio_data, num_samples * 2, sample_rate);
    free(audio_data);
}

void test_json_format_simple(void)
{
    if (!mqtt_client_is_connected())
    {
        ESP_LOGI(TAG, "⚠️ MQTT not connected");
        return;
    }

    ESP_LOGI(TAG, "🔧 Testing simple JSON...");

    // Create very short test audio 0.05 s (800 samples)
    int sample_count = 800;
    int16_t *test_audio = malloc(sample_count * sizeof(int16_t));
    if (!test_audio)
        return;

    // Create simple sine wave
    for (int i = 0; i < sample_count; i++)
    {
        test_audio[i] = (int16_t)(sin(2 * M_PI * 440 * i / 16000.0f) * 32767 * 0.3);
    }

    // Send test
    esp_err_t result = mqtt_client_publish_audio_data(
        (uint8_t *)test_audio,
        sample_count * sizeof(int16_t),
        1,   // Small count
        2000 // ADC
    );

    if (result == ESP_OK)
    {
        ESP_LOGI(TAG, "✅ Test sent");
        oled_show_message("Test OK");
    }
    else
    {
        ESP_LOGE(TAG, "❌ Test failed");
        oled_show_message("Test FAIL");
    }

    free(test_audio);
    vTaskDelay(pdMS_TO_TICKS(1000)); // Reduced from 2s
    oled_show_message("");
}

// App main
void app_main(void)
{
    ESP_LOGI(TAG, "🚀 Voice AI System Booting...");

    // Log memory status first
    check_memory_status();

    // Initialize OLED
    oled_init();
    oled_display_status("Booting...");

    // Initialize I2S Audio Output
    if (i2s_audio_init() != ESP_OK)
    {
        ESP_LOGE(TAG, "❌ Audio init failed");
        oled_display_status("Audio Fail");
        return;
    }

    // ⚠️ REDUCE test audio playback time
    test_audio_playback_short(); // Instead of test_audio_playback()

    // Check memory
    check_memory_status();

    // Initialize Audio Recorder
    if (audio_recorder_init() != ESP_OK)
    {
        ESP_LOGE(TAG, "❌ Recorder init failed");
        oled_display_status("Recorder Fail");
        return;
    }

    // Initialize ADC
    if (adc_handler_init() != ESP_OK)
    {
        ESP_LOGE(TAG, "❌ ADC init failed");
        oled_display_status("ADC Fail");
        return;
    }

    ESP_LOGI(TAG, "✅ Hardware initialized");
    check_memory_status();

    // Initialize WiFi
    wifi_manager_init();
    oled_display_status("WiFi Connecting...");

    // WAIT WiFi CONNECTED
    if (wifi_manager_wait_connected(pdMS_TO_TICKS(15000)) == ESP_OK) // Reduced from 20s
    {
        wifi_connected = true;
        ESP_LOGI(TAG, "✅ WiFi Connected");
        oled_display_status("WiFi OK");

        // INITIALIZE MQTT
        mqtt_client_init();
        mqtt_client_set_ai_callback(ai_response_handler);
        mqtt_client_set_tts_callback(tts_audio_handler);
        mqtt_client_start();

        // Wait for MQTT connection
        ESP_LOGI(TAG, "⏳ Waiting for MQTT connection...");
        oled_display_status("MQTT Connecting...");

        int retry_count = 0;
        while (retry_count < 8) // Reduced from 10 to 8
        {
            if (mqtt_client_is_connected())
            {
                ESP_LOGI(TAG, "🎉 MQTT Connected!");
                oled_display_status("Cloud Ready");
                vTaskDelay(pdMS_TO_TICKS(1000)); // Reduced from 2s

                // ⚠️ SKIP microphone test to save memory
                // test_microphone_quality();
                // vTaskDelay(pdMS_TO_TICKS(500));

                // SIMPLE TEST
                test_json_format_simple(); // Shorter test
                vTaskDelay(pdMS_TO_TICKS(500));

                break;
            }

            ESP_LOGI(TAG, "⏳ MQTT not connected yet, retry %d/8", retry_count + 1);
            vTaskDelay(pdMS_TO_TICKS(1000));
            retry_count++;
        }

        if (!mqtt_client_is_connected())
        {
            ESP_LOGW(TAG, "⚠️ MQTT failed to connect");
            oled_display_status("Cloud Offline");
        }
    }
    else
    {
        ESP_LOGW(TAG, "⚠️ WiFi failed - offline mode");
        wifi_connected = false;
        oled_display_status("Offline Mode");
    }

    // Create voice AI task with smaller stack
    xTaskCreate(voice_ai_task,   // Task function
                "voice_ai_task", // Task name
                6144,            // ⚠️ REDUCED: 6KB stack (from 8KB)
                NULL,            // Parameters
                2,               // Priority
                NULL);           // Task handle

    ESP_LOGI(TAG, "✅ System Ready");

    // Keep main task alive
    while (1)
    {
        vTaskDelay(pdMS_TO_TICKS(5000)); // Reduced from 10s
        // Periodically check connection
        if (wifi_connected && !mqtt_client_is_connected())
        {
            ESP_LOGW(TAG, "🔄 Reconnecting MQTT...");
            mqtt_client_stop();
            vTaskDelay(pdMS_TO_TICKS(2000)); // Reduced from 5s
            mqtt_client_start();
        }
    }
}
