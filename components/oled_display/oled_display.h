#ifndef OLED_DISPLAY_H
#define OLED_DISPLAY_H

#include <stdbool.h>

void oled_init(void);
void oled_display_status(const char *status);
void oled_display_adc(int val);
bool oled_is_initialized(void);
void oled_clear(void);
void oled_show_message(const char *message);

#endif