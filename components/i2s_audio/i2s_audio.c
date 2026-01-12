#include "i2s_audio.h"
#include "driver/i2s_std.h"
#include "esp_log.h"
#include <math.h>
#include "freertos/semphr.h"

static const char *TAG = "I2S_AUDIO";
static i2s_chan_handle_t tx_chan;
static SemaphoreHandle_t audio_mutex = NULL;
static bool is_playing = false;

// Helper function to check if it's WAV format
static bool is_wav_format(const uint8_t *data, size_t len)
{
    if (len < 12)
        return false;
    return (data[0] == 'R' && data[1] == 'I' && data[2] == 'F' && data[3] == 'F') ||
           (data[0] == 'S' && data[1] == 'U' && data[2] == 'Q' && data[3] == 'z');
}

// Extract PCM data from WAV (skip header)
static size_t extract_pcm_from_wav(const uint8_t *wav_data, size_t wav_len, uint8_t **pcm_data)
{
    // Simple WAV header check
    if (wav_len < 44)
    {
        *pcm_data = NULL;
        return 0;
    }

    // Skip WAV header (44 bytes)
    size_t pcm_len = wav_len - 44;
    *pcm_data = malloc(pcm_len);
    if (*pcm_data == NULL)
    {
        return 0;
    }

    memcpy(*pcm_data, wav_data + 44, pcm_len);
    return pcm_len;
}

esp_err_t i2s_audio_init(void)
{
    esp_err_t ret = ESP_OK;

    // Create mutex for audio playback
    audio_mutex = xSemaphoreCreateMutex();
    if (audio_mutex == NULL)
    {
        ESP_LOGE(TAG, "Failed to create audio mutex");
        return ESP_FAIL;
    }

    i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0, I2S_ROLE_MASTER);
    ret = i2s_new_channel(&chan_cfg, &tx_chan, NULL);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to create I2S channel");
        return ret;
    }

    i2s_std_config_t std_cfg = {
        .clk_cfg = {
            .sample_rate_hz = 16000, // Fixed 16kHz for TTS
            .clk_src = I2S_CLK_SRC_DEFAULT,
            .mclk_multiple = I2S_MCLK_MULTIPLE_256,
        },
        .slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_MONO),
        .gpio_cfg = {
            .mclk = I2S_GPIO_UNUSED,
            .bclk = GPIO_NUM_6,
            .ws = GPIO_NUM_7,
            .dout = GPIO_NUM_8,
            .din = I2S_GPIO_UNUSED,
            .invert_flags = {
                .mclk_inv = false,
                .bclk_inv = false,
                .ws_inv = false,
            },
        },
    };

    ret = i2s_channel_init_std_mode(tx_chan, &std_cfg);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to init I2S in standard mode");
        return ret;
    }

    ret = i2s_channel_enable(tx_chan);
    ESP_LOGI(TAG, "✅ I2S audio initialized successfully - 16kHz, 16-bit, mono");
    ESP_LOGI(TAG, "   Pins: BCLK=GPIO6, LRC=GPIO7, DOUT=GPIO8");
    return ret;
}

esp_err_t i2s_audio_play(const uint8_t *data, size_t size, TickType_t timeout)
{
    if (xSemaphoreTake(audio_mutex, pdMS_TO_TICKS(100)) != pdTRUE)
    {
        ESP_LOGW(TAG, "Audio busy, skipping playback");
        return ESP_ERR_TIMEOUT;
    }

    is_playing = true;
    size_t bytes_written = 0;
    esp_err_t ret = i2s_channel_write(tx_chan, data, size, &bytes_written, timeout);

    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to write I2S data: %d", ret);
    }
    else
    {
        ESP_LOGD(TAG, "I2S write: %d/%d bytes", bytes_written, size);
    }

    is_playing = false;
    xSemaphoreGive(audio_mutex);
    return ret;
}

// SIMPLIFIED: Play PCM audio - NO RECONFIGURATION
esp_err_t i2s_audio_play_pcm(const uint8_t *pcm_data, size_t data_len, uint32_t sample_rate)
{
    if (pcm_data == NULL || data_len == 0)
    {
        ESP_LOGE(TAG, "Invalid PCM data");
        return ESP_ERR_INVALID_ARG;
    }

    if (xSemaphoreTake(audio_mutex, pdMS_TO_TICKS(100)) != pdTRUE)
    {
        ESP_LOGW(TAG, "Audio busy, cannot play PCM");
        return ESP_ERR_TIMEOUT;
    }

    ESP_LOGI(TAG, "🔊 Playing PCM audio: %d bytes, %dHz", data_len, sample_rate);

    // IMPORTANT: Check if we need to convert sample rate
    // MAX98357A is fixed at 16kHz in our config
    if (sample_rate != 16000)
    {
        ESP_LOGW(TAG, "⚠️ Sample rate mismatch: got %dHz, expected 16000Hz", sample_rate);
        ESP_LOGW(TAG, "   The amplifier is configured for 16kHz only");
    }

    // Play the audio
    size_t bytes_written = 0;
    esp_err_t ret = i2s_channel_write(tx_chan, pcm_data, data_len, &bytes_written, portMAX_DELAY);

    if (ret == ESP_OK)
    {
        ESP_LOGI(TAG, "✅ PCM audio playback completed: %d bytes written", bytes_written);
    }
    else
    {
        ESP_LOGE(TAG, "❌ PCM playback failed: %d", ret);
    }

    xSemaphoreGive(audio_mutex);
    return ret;
}

void i2s_audio_stop(void)
{
    if (xSemaphoreTake(audio_mutex, pdMS_TO_TICKS(50)) == pdTRUE)
    {
        i2s_channel_disable(tx_chan);
        xSemaphoreGive(audio_mutex);
    }
}

bool i2s_audio_is_playing(void)
{
    return is_playing;
}

void generate_sine_wave(int16_t *buffer, int num_samples, int frequency, int sample_rate, float volume)
{
    for (int i = 0; i < num_samples; i++)
    {
        float sample = sin(2 * M_PI * frequency * i / sample_rate);
        buffer[i] = (int16_t)(sample * volume * 32767.0f);
    }
}

void test_audio_playback(void)
{
    if (xSemaphoreTake(audio_mutex, pdMS_TO_TICKS(100)) != pdTRUE)
    {
        ESP_LOGW(TAG, "Audio busy, skipping test");
        return;
    }

    ESP_LOGI(TAG, "Testing audio playback with XIAO ESP32S3...");

    const int sample_rate = 16000;
    const int duration_ms = 1000;
    const int frequency = 440;
    const int num_samples = (sample_rate * duration_ms) / 1000;
    const float volume = 0.3f;

    int16_t *audio_buffer = malloc(num_samples * sizeof(int16_t));
    if (audio_buffer == NULL)
    {
        ESP_LOGE(TAG, "Failed to allocate audio buffer");
        xSemaphoreGive(audio_mutex);
        return;
    }

    generate_sine_wave(audio_buffer, num_samples, frequency, sample_rate, volume);

    ESP_LOGI(TAG, "Playing sine wave %dHz for %dms", frequency, duration_ms);

    size_t bytes_written;
    i2s_channel_write(tx_chan, (uint8_t *)audio_buffer,
                      num_samples * sizeof(int16_t), &bytes_written, portMAX_DELAY);

    free(audio_buffer);
    ESP_LOGI(TAG, "Audio test completed: %d bytes written", bytes_written);

    xSemaphoreGive(audio_mutex);
}
