#ifndef I2S_AUDIO_H
#define I2S_AUDIO_H

#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

esp_err_t i2s_audio_init(void);
esp_err_t i2s_audio_play(const uint8_t *data, size_t size, TickType_t timeout);
void test_audio_playback(void);

esp_err_t i2s_audio_play_pcm(const uint8_t *pcm_data, size_t data_len, uint32_t sample_rate);

#endif