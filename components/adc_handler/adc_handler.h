#ifndef ADC_HANDLER_H
#define ADC_HANDLER_H

void adc_init(void);
int adc_read(void);
void adc_task(void *arg);

#endif
