#ifndef ADC_HANDLER_H
#define ADC_HANDLER_H

#include "esp_err.h"

esp_err_t adc_handler_init(void);
int adc_handler_read_single(void);
int adc_handler_read_average(int samples);
uint32_t adc_handler_read_voltage(void);

#endif