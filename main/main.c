#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/adc.h"
#include "driver/gpio.h"
#include "driver/i2s.h"
#include "esp_log.h"
#include "string.h"
#include <math.h>
#include "ssd1306.h"

#define TAG "MAIN"
#define ADC_CHANNEL ADC1_CHANNEL_4
#define OLED_SDA_GPIO GPIO_NUM_8
#define OLED_SCL_GPIO GPIO_NUM_10
#define OLED_RESET_GPIO -1
#define I2S_BCK_GPIO GPIO_NUM_12
#define I2S_WS_GPIO GPIO_NUM_11
#define I2S_DO_GPIO GPIO_NUM_14
#define LED_GPIO GPIO_NUM_4

SSD1306_t dev;

void mic_adc_init()
{
    adc1_config_width(ADC_WIDTH_BIT_12);
    adc1_config_channel_atten(ADC_CHANNEL, ADC_ATTEN_DB_11);
}

int mic_adc_read()
{
    return adc1_get_raw(ADC_CHANNEL);
}

void oled_setup()
{
    i2c_master_init(&dev, OLED_SDA_GPIO, OLED_SCL_GPIO, OLED_RESET_GPIO);
    dev._address = 0x3C;
    ssd1306_init(&dev, 128, 64);
    ssd1306_clear_screen(&dev, false);
    ssd1306_display_text(&dev, 0, "Mic ADC:", strlen("Mic ADC:"), false);
}

void oled_print_adc(int val)
{
    char buf[16];
    snprintf(buf, sizeof(buf), "%4d", val);
    ssd1306_clear_line(&dev, 1, false);
    ssd1306_display_text(&dev, 1, buf, strlen(buf), false);
}

void i2s_init()
{
    i2s_config_t i2s_config = {
        .mode = I2S_MODE_MASTER | I2S_MODE_TX,
        .sample_rate = 16000,
        .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
        .channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT,
        .communication_format = I2S_COMM_FORMAT_STAND_I2S,
        .dma_buf_count = 6,
        .dma_buf_len = 60,
        .use_apll = false,
        .intr_alloc_flags = 0,
    };
    i2s_pin_config_t pin_config = {
        .bck_io_num = I2S_BCK_GPIO,
        .ws_io_num = I2S_WS_GPIO,
        .data_out_num = I2S_DO_GPIO,
        .data_in_num = I2S_PIN_NO_CHANGE};
    i2s_driver_install(I2S_NUM_0, &i2s_config, 0, NULL);
    i2s_set_pin(I2S_NUM_0, &pin_config);
}

void i2s_write_sample(int16_t *buffer, size_t size)
{
    size_t bytes_written;
    i2s_write(I2S_NUM_0, (const char *)buffer, size * sizeof(int16_t), &bytes_written, portMAX_DELAY);
}

void play_sine_tone()
{
    const int sample_rate = 16000;
    const float freq = 1000.0f;
    int16_t sample_buffer[256];
    for (int i = 0; i < 256; i++)
    {
        sample_buffer[i] = (int16_t)(10000 * sinf(2 * M_PI * freq * i / sample_rate));
    }
    for (int j = 0; j < 100; j++)
    {
        i2s_write_sample(sample_buffer, 256);
    }
}

void gpio_init()
{
    gpio_reset_pin(LED_GPIO);
    gpio_set_direction(LED_GPIO, GPIO_MODE_OUTPUT);
    gpio_set_level(LED_GPIO, 0);
}

void adc_task(void *pv)
{
    while (1)
    {
        int val = mic_adc_read();
        oled_print_adc(val);
        vTaskDelay(pdMS_TO_TICKS(200));
    }
}
void i2s_task(void *pv)
{
    while (1)
    {
        play_sine_tone();
        vTaskDelay(pdMS_TO_TICKS(2000));
    }
}
void led_task(void *pv)
{
    while (1)
    {
        gpio_set_level(LED_GPIO, 1);
        vTaskDelay(pdMS_TO_TICKS(500));
        gpio_set_level(LED_GPIO, 0);
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}

void app_main()
{
    ESP_LOGI(TAG, "Starting app");
    mic_adc_init();
    oled_setup();
    i2s_init();
    gpio_init();
    ESP_LOGI(TAG, "Hardware initialized");

    xTaskCreate(adc_task, "adc_task", 2048, NULL, 5, NULL);
    xTaskCreate(i2s_task, "i2s_task", 2048, NULL, 6, NULL);
    xTaskCreate(led_task, "led_task", 1024, NULL, 4, NULL);
}
