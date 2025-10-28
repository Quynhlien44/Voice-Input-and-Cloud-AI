#include "adc_handler.h"
#include "oled_display.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "driver/adc.h"
#include "esp_log.h"

#define TAG "ADC"
#define ADC_CHANNEL ADC1_CHANNEL_4

void adc_init(void)
{
    adc1_config_width(ADC_WIDTH_BIT_12);
    adc1_config_channel_atten(ADC_CHANNEL, ADC_ATTEN_DB_11);
}

int adc_read(void)
{
    return adc1_get_raw(ADC_CHANNEL);
}

void adc_task(void *arg)
{
    (void)arg;
    while (1)
    {
        int val = adc_read();
        oled_display_adc(val);
        vTaskDelay(pdMS_TO_TICKS(200));
    }
}
