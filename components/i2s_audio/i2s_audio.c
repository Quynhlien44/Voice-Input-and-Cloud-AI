#include "i2s_audio.h"
#include "driver/i2s.h"
#include "driver/gpio.h"
#include <math.h>

#define I2S_BCK_GPIO GPIO_NUM_12
#define I2S_WS_GPIO GPIO_NUM_11
#define I2S_DO_GPIO GPIO_NUM_14

static const int i2s_num = I2S_NUM_0;

void i2s_init(void)
{
    i2s_config_t i2s_config = {
        .mode = I2S_MODE_MASTER | I2S_MODE_TX,
        .sample_rate = 16000,
        .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
        .channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT,
        .communication_format = I2S_COMM_FORMAT_I2S_MSB,
        .dma_buf_count = 6,
        .dma_buf_len = 60,
        .use_apll = false,
        .intr_alloc_flags = 0,
    };
    i2s_pin_config_t pin_config = {
        .bck_io_num = I2S_BCK_GPIO,
        .ws_io_num = I2S_WS_GPIO,
        .data_out_num = I2S_DO_GPIO,
        .data_in_num = I2S_PIN_NO_CHANGE,
    };

    i2s_driver_install(i2s_num, &i2s_config, 0, NULL);
    i2s_set_pin(i2s_num, &pin_config);
}

void play_sine_tone(void)
{
    int16_t sample_buffer[256];
    for (int i = 0; i < 256; i++)
    {
        sample_buffer[i] = (int16_t)(10000 * sinf(2 * M_PI * 1000 * i / 16000));
    }
    for (int j = 0; j < 100; j++)
    {
        size_t bytes_written = 0;
        i2s_write(i2s_num, sample_buffer, 256 * sizeof(int16_t), &bytes_written, portMAX_DELAY);
    }
}

void i2s_task(void *arg)
{
    while (1)
    {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
