#include "adc_handler.h"
#include "esp_log.h"
#include "driver/adc.h"

static const char *TAG = "ADC_HANDLER";

esp_err_t adc_handler_init(void)
{
    ESP_LOGI(TAG, "Initializing ADC for XIAO ESP32S3 - A0/D1 (GPIO1)");

    // Use DRIVER LEGACY - SYNCHRONIZATION and AUDIO_RECORDER
    adc1_config_width(ADC_WIDTH_BIT_12);
    adc1_config_channel_atten(ADC1_CHANNEL_0, ADC_ATTEN_DB_11);

    ESP_LOGI(TAG, "✅ ADC Handler initialized successfully on A0/D1 (GPIO1)");
    return ESP_OK;
}

int adc_handler_read_single(void)
{
    return adc1_get_raw(ADC1_CHANNEL_0);
}

int adc_handler_read_average(int samples)
{
    if (samples <= 0)
        return 0;

    int sum = 0;
    for (int i = 0; i < samples; i++)
    {
        sum += adc_handler_read_single();
    }
    return sum / samples;
}

uint32_t adc_handler_read_voltage(void)
{
    int raw_value = adc_handler_read_single();
    if (raw_value < 0)
        return 0;

    return (uint32_t)(raw_value * 3300 / 4095);
}