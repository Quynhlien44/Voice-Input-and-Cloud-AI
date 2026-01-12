#ifndef AUDIO_RECORDER_H
#define AUDIO_RECORDER_H

#include <stdint.h>
#include <stdbool.h>
#include "freertos/portmacro.h"
#include "esp_err.h"

// Initialize/Release
esp_err_t audio_recorder_init(void);
void audio_recorder_deinit(void);

// Control recording
esp_err_t audio_recorder_start(void);
esp_err_t audio_recorder_stop(void);
size_t audio_recorder_read(int16_t *buffer, size_t max_samples, TickType_t timeout);

// Audio processing
void audio_recorder_amplify(int16_t *buffer, size_t sample_count, float gain);
void audio_recorder_normalize(int16_t *buffer, size_t sample_count);
void audio_recorder_bandpass_filter(int16_t *buffer, size_t sample_count);

// Analysis
void audio_recorder_analyze_data(int16_t *buffer, size_t sample_count);

// Helper functions
bool audio_recorder_is_recording(void);
size_t audio_recorder_get_recorded_samples(void);

#endif