#include "audio_recorder.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/adc.h"
#include <string.h>
#include <math.h>

static const char *TAG = "AUDIO_RECORDER";

// Configuration
#define SAMPLE_RATE 16000
#define RECORD_BUFFER_SIZE 32000 // 2 seconds
#define ADC_SAMPLING_DELAY_US 62

static bool is_recording = false;
static int16_t *record_buffer = NULL;
static size_t record_index = 0;
static size_t max_samples = 0;

// Precise microsecond delay function - MORE OPTIMIZED
// Actual: ~8.5kHz achieved due to ADC conversion overhead
static void IRAM_ATTR precise_delay_us(uint32_t us)
{
    uint64_t cycles = us * (240); // ESP32-S3 @ 240MHz
    uint64_t start = esp_cpu_get_cycle_count();

    while (esp_cpu_get_cycle_count() - start < cycles)
    {
        // Busy wait
    }
}

// Adaptive gain to prevent clipping
static int16_t adc_to_pcm(int raw_adc)
{
    static const int32_t ADC_CENTER = 2048;
    static const int32_t GAIN = 6; // Reduced from 12 to 6

    int32_t pcm_sample = (int32_t)((raw_adc - ADC_CENTER) * GAIN);

    // Soft clipping at 25000 (76% of full scale)
    if (pcm_sample > 25000)
        pcm_sample = 25000;
    if (pcm_sample < -25000)
        pcm_sample = -25000;

    return (int16_t)pcm_sample;
}

// Improved filter - Simple DC removal and pre-emphasis
void audio_recorder_bandpass_filter(int16_t *buffer, size_t sample_count)
{
    if (!buffer || sample_count < 3)
        return;

    // Simple noise gate: ignore samples that are too small
    int16_t noise_gate = 500;
    // 1. DC removal with exponential moving average
    static int32_t dc_accumulator = 0;
    const float alpha_dc = 0.99f;

    for (size_t i = 0; i < sample_count; i++)
    {
        dc_accumulator = (int32_t)(alpha_dc * dc_accumulator + (1 - alpha_dc) * buffer[i]);
        buffer[i] -= (int16_t)(dc_accumulator >> 8); // Scale down
    }

    // 2. Pre-emphasis filter (boost high frequencies)
    // y[n] = x[n] - 0.95 * x[n-1]
    static int16_t prev_sample = 0;
    const float pre_emphasis = 0.95f;

    for (size_t i = 0; i < sample_count; i++)
    {
        int16_t current = buffer[i];
        // buffer[i] = current - (int16_t)(prev_sample * pre_emphasis);
        int32_t filtered = current - (prev_sample * 97 / 100);
        prev_sample = current;
        // Apply noise gate
        if (abs(filtered) < noise_gate)
        {
            buffer[i] = 0;
        }
        else
        {
            // Slight gain
            filtered = filtered * 3 / 2;
            if (filtered > 25000)
                filtered = 25000;
            if (filtered < -25000)
                filtered = -25000;
            buffer[i] = (int16_t)filtered;
        }
    }

    // 3. Apply moderate gain
    for (size_t i = 0; i < sample_count; i++)
    {
        int32_t sample = buffer[i] * 2; // Gain 2x
        if (sample > 25000)
            sample = 25000;
        if (sample < -25000)
            sample = -25000;
        buffer[i] = (int16_t)sample;
    }
}

esp_err_t audio_recorder_init(void)
{
    ESP_LOGI(TAG, "🎤 Initializing Audio Recorder (Optimized Gain)");

    // Configure ADC
    adc1_config_width(ADC_WIDTH_BIT_12);
    adc1_config_channel_atten(ADC1_CHANNEL_0, ADC_ATTEN_DB_11);

    // Test ADC - read multiple times for stabilization
    int adc_sum = 0;
    for (int i = 0; i < 10; i++)
    {
        adc_sum += adc1_get_raw(ADC1_CHANNEL_0);
        vTaskDelay(1);
    }
    ESP_LOGI(TAG, "📊 Average ADC reading: %d", adc_sum / 10);

    // Allocate buffer
    record_buffer = malloc(RECORD_BUFFER_SIZE * sizeof(int16_t));
    if (!record_buffer)
    {
        ESP_LOGE(TAG, "Failed to allocate record buffer");
        return ESP_ERR_NO_MEM;
    }

    ESP_LOGI(TAG, "✅ Audio Recorder initialized @ 16kHz");
    ESP_LOGI(TAG, "   Buffer: %d samples (1.0 sec)", RECORD_BUFFER_SIZE);
    ESP_LOGI(TAG, "   ADC Gain: 2x (reduced from 16x)");

    return ESP_OK;
}

esp_err_t audio_recorder_start(void)
{
    if (!record_buffer)
        return ESP_FAIL;

    if (is_recording)
        return ESP_OK;

    memset(record_buffer, 0, RECORD_BUFFER_SIZE * sizeof(int16_t));
    record_index = 0;
    max_samples = RECORD_BUFFER_SIZE;
    is_recording = true;

    ESP_LOGI(TAG, "🎙️ Recording STARTED");
    return ESP_OK;
}

esp_err_t audio_recorder_stop(void)
{
    is_recording = false;
    ESP_LOGI(TAG, "🛑 Recording STOPPED (%d samples)", record_index);
    return ESP_OK;
}

// In the audio_recorder_read function:
size_t audio_recorder_read(int16_t *buffer, size_t max_samples_to_read, TickType_t timeout)
{
    if (!buffer || max_samples_to_read == 0 || !record_buffer)
        return 0;

    uint32_t start_time = xTaskGetTickCount();
    size_t samples_to_record = (max_samples_to_read > RECORD_BUFFER_SIZE) ? RECORD_BUFFER_SIZE : max_samples_to_read;

    audio_recorder_start();

    ESP_LOGI(TAG, "⏺️ Recording %d samples (%.2f sec)",
             samples_to_record, (float)samples_to_record / SAMPLE_RATE);

    // ⚠️ INCREASE TIMEOUT TO ENSURE COMPLETE RECORDING
    is_recording = true;
    record_index = 0;

    // Calculate time per sample
    uint32_t time_per_sample = timeout * portTICK_PERIOD_MS * 1000 / samples_to_record; // microseconds

    for (record_index = 0; record_index < samples_to_record && is_recording; record_index++)
    {
        // Read ADC
        int raw_adc = adc1_get_raw(ADC1_CHANNEL_0);

        // Convert with gain
        record_buffer[record_index] = adc_to_pcm(raw_adc);

        // Precise delay - 62.5 microseconds for 16kHz
        precise_delay_us(ADC_SAMPLING_DELAY_US);

        // Check timeout (20% extra time)
        if ((xTaskGetTickCount() - start_time) > (timeout * 12 / 10))
        {
            ESP_LOGW(TAG, "Recording timeout at sample %d", record_index);
            break;
        }
    }

    audio_recorder_stop();

    uint32_t duration_ms = (xTaskGetTickCount() - start_time) * portTICK_PERIOD_MS;
    ESP_LOGI(TAG, "✅ Recorded %d samples in %d ms", record_index, duration_ms);

    // Copy and process
    size_t samples_copied = record_index;
    if (samples_copied > 0)
    {
        memcpy(buffer, record_buffer, samples_copied * sizeof(int16_t));
        ESP_LOGI(TAG, "📥 Copied %d samples", samples_copied);
    }

    return samples_copied;
}

void audio_recorder_amplify(int16_t *buffer, size_t sample_count, float gain)
{
    if (!buffer || sample_count == 0 || gain <= 1.0f)
        return;

    for (size_t i = 0; i < sample_count; i++)
    {
        int32_t amplified = (int32_t)(buffer[i] * gain);

        // Soft clipping
        if (amplified > 28000)
            amplified = 28000;
        if (amplified < -28000)
            amplified = -28000;

        buffer[i] = (int16_t)amplified;
    }
}

void audio_recorder_normalize(int16_t *buffer, size_t sample_count)
{
    if (!buffer || sample_count == 0)
        return;

    // Find peak value
    int16_t max_val = 0;
    for (size_t i = 0; i < sample_count; i++)
    {
        int16_t abs_val = abs(buffer[i]);
        if (abs_val > max_val)
            max_val = abs_val;
    }

    // Only normalize if peak > 5000
    if (max_val < 5000)
        return;

    // Calculate gain to bring peak to 70% full scale
    float gain = 22936.0f / max_val; // 70% of 32767

    // Apply gain
    for (size_t i = 0; i < sample_count; i++)
    {
        int32_t sample = (int32_t)(buffer[i] * gain);

        // Soft clipping
        if (sample > 28000)
            sample = 28000;
        if (sample < -28000)
            sample = -28000;

        buffer[i] = (int16_t)sample;
    }
}

void audio_recorder_analyze_data(int16_t *buffer, size_t sample_count)
{
    if (!buffer || sample_count == 0)
        return;

    int64_t sum = 0, sum_sq = 0;
    int16_t min_val = 32767, max_val = -32768;
    int silent_samples = 0;
    int clip_pos = 0, clip_neg = 0;
    int16_t silent_threshold = 200;

    for (size_t i = 0; i < sample_count; i++)
    {
        int16_t val = buffer[i];
        int16_t abs_val = abs(val);

        sum += abs_val;
        sum_sq += (int64_t)val * val;

        if (val < min_val)
            min_val = val;
        if (val > max_val)
            max_val = val;

        if (abs_val < silent_threshold)
            silent_samples++;
        if (val > 30000)
            clip_pos++;
        if (val < -30000)
            clip_neg++;
    }

    float avg = (float)sum / (float)sample_count;
    float rms = sqrt((float)sum_sq / sample_count);
    float silence_percent = (float)silent_samples / sample_count * 100.0f;
    float clip_percent = (float)(clip_pos + clip_neg) / sample_count * 100.0f;

    ESP_LOGI(TAG, "📊 Audio Analysis:");
    ESP_LOGI(TAG, "  Samples: %d (%.2f sec)", sample_count, (float)sample_count / SAMPLE_RATE);
    ESP_LOGI(TAG, "  Min: %d, Max: %d, Range: %d", min_val, max_val, max_val - min_val);
    ESP_LOGI(TAG, "  Avg: %.1f, RMS: %.1f", avg, rms);
    ESP_LOGI(TAG, "  Silence (<%d): %.1f%%", silent_threshold, silence_percent);
    ESP_LOGI(TAG, "  Clipping (>30000): %.1f%%", clip_percent);

    // Evaluate quality
    if (clip_percent > 2.0)
    {
        ESP_LOGW(TAG, "  ⚠️ Clipping detected - reduce gain!");
    }

    if (rms < 500)
    {
        ESP_LOGW(TAG, "  ⚠️ Audio too quiet - increase gain or speak louder");
    }
    else if (rms > 15000)
    {
        ESP_LOGW(TAG, "  ⚠️ Audio too loud - reduce gain");
    }
    else if (rms > 3000)
    {
        ESP_LOGI(TAG, "  ✅ Good audio level");
    }

    // Check dynamic range
    if ((max_val - min_val) < 10000)
    {
        ESP_LOGW(TAG, "  ⚠️ Low dynamic range - check mic placement");
    }
}

// HHelper function to check recording status
bool audio_recorder_is_recording(void)
{
    return is_recording;
}

// HHelper function to get the number of recorded samples
size_t audio_recorder_get_recorded_samples(void)
{
    return record_index;
}