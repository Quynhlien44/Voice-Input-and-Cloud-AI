#include "oled_display.h"
#include "ssd1306.h"
#include "string.h"

#define OLED_SDA_GPIO GPIO_NUM_8
#define OLED_SCL_GPIO GPIO_NUM_10
#define OLED_RESET_GPIO -1

static SSD1306_t dev;

void oled_init(void)
{
    i2c_master_init(&dev, OLED_SDA_GPIO, OLED_SCL_GPIO, OLED_RESET_GPIO);
    dev._address = 0x3C;
    ssd1306_init(&dev, 128, 64);
    ssd1306_clear_screen(&dev, false);
    ssd1306_display_text(&dev, 0, "Mic ADC:", strlen("Mic ADC:"), false);
}

void oled_display_adc(int val)
{
    char buf[16];
    snprintf(buf, sizeof(buf), "%4d", val);
    ssd1306_clear_line(&dev, 1, false);
    ssd1306_display_text(&dev, 1, buf, strlen(buf), false);
}
