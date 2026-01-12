#include "oled_display.h"
#include "esp_log.h"
#include "driver/i2c.h"
#include "string.h"
#include "freertos/task.h"

static const char *TAG = "OLED_DISPLAY";
static bool oled_initialized = false;

#define I2C_MASTER_SCL_IO 5 // GPIO5 - D5
#define I2C_MASTER_SDA_IO 4 // GPIO4 - D4
#define I2C_MASTER_NUM I2C_NUM_0
#define I2C_MASTER_FREQ_HZ 400000

#define OLED_ADDR 0x3C
#define OLED_CMD_MODE 0x00
#define OLED_DATA_MODE 0x40

static const uint8_t font_5x8[95][5] = {
    {0x00, 0x00, 0x00, 0x00, 0x00}, // space
    {0x00, 0x00, 0x5F, 0x00, 0x00}, // !
    {0x00, 0x07, 0x00, 0x07, 0x00}, // "
    {0x14, 0x7F, 0x14, 0x7F, 0x14}, // #
    {0x24, 0x2A, 0x7F, 0x2A, 0x12}, // $
    {0x23, 0x13, 0x08, 0x64, 0x62}, // %
    {0x36, 0x49, 0x55, 0x22, 0x50}, // &
    {0x00, 0x05, 0x03, 0x00, 0x00}, // '
    {0x00, 0x1C, 0x22, 0x41, 0x00}, // (
    {0x00, 0x41, 0x22, 0x1C, 0x00}, // )
    {0x14, 0x08, 0x3E, 0x08, 0x14}, // *
    {0x08, 0x08, 0x3E, 0x08, 0x08}, // +
    {0x00, 0x50, 0x30, 0x00, 0x00}, // ,
    {0x08, 0x08, 0x08, 0x08, 0x08}, // -
    {0x00, 0x60, 0x60, 0x00, 0x00}, // .
    {0x20, 0x10, 0x08, 0x04, 0x02}, // /
    {0x3E, 0x51, 0x49, 0x45, 0x3E}, // 0
    {0x00, 0x42, 0x7F, 0x40, 0x00}, // 1
    {0x42, 0x61, 0x51, 0x49, 0x46}, // 2
    {0x21, 0x41, 0x45, 0x4B, 0x31}, // 3
    {0x18, 0x14, 0x12, 0x7F, 0x10}, // 4
    {0x27, 0x45, 0x45, 0x45, 0x39}, // 5
    {0x3C, 0x4A, 0x49, 0x49, 0x30}, // 6
    {0x01, 0x71, 0x09, 0x05, 0x03}, // 7
    {0x36, 0x49, 0x49, 0x49, 0x36}, // 8
    {0x06, 0x49, 0x49, 0x29, 0x1E}, // 9
    {0x00, 0x36, 0x36, 0x00, 0x00}, // :
    {0x00, 0x56, 0x36, 0x00, 0x00}, // ;
    {0x08, 0x14, 0x22, 0x41, 0x00}, // <
    {0x14, 0x14, 0x14, 0x14, 0x14}, // =
    {0x00, 0x41, 0x22, 0x14, 0x08}, // >
    {0x02, 0x01, 0x51, 0x09, 0x06}, // ?
    {0x32, 0x49, 0x79, 0x41, 0x3E}, // @
    {0x7E, 0x11, 0x11, 0x11, 0x7E}, // A
    {0x7F, 0x49, 0x49, 0x49, 0x36}, // B
    {0x3E, 0x41, 0x41, 0x41, 0x22}, // C
    {0x7F, 0x41, 0x41, 0x22, 0x1C}, // D
    {0x7F, 0x49, 0x49, 0x49, 0x41}, // E
    {0x7F, 0x09, 0x09, 0x09, 0x01}, // F
    {0x3E, 0x41, 0x49, 0x49, 0x7A}, // G
    {0x7F, 0x08, 0x08, 0x08, 0x7F}, // H
    {0x00, 0x41, 0x7F, 0x41, 0x00}, // I
    {0x20, 0x40, 0x41, 0x3F, 0x01}, // J
    {0x7F, 0x08, 0x14, 0x22, 0x41}, // K
    {0x7F, 0x40, 0x40, 0x40, 0x40}, // L
    {0x7F, 0x02, 0x0C, 0x02, 0x7F}, // M
    {0x7F, 0x04, 0x08, 0x10, 0x7F}, // N
    {0x3E, 0x41, 0x41, 0x41, 0x3E}, // O
    {0x7F, 0x09, 0x09, 0x09, 0x06}, // P
    {0x3E, 0x41, 0x51, 0x21, 0x5E}, // Q
    {0x7F, 0x09, 0x19, 0x29, 0x46}, // R
    {0x46, 0x49, 0x49, 0x49, 0x31}, // S
    {0x01, 0x01, 0x7F, 0x01, 0x01}, // T
    {0x3F, 0x40, 0x40, 0x40, 0x3F}, // U
    {0x1F, 0x20, 0x40, 0x20, 0x1F}, // V
    {0x3F, 0x40, 0x38, 0x40, 0x3F}, // W
    {0x63, 0x14, 0x08, 0x14, 0x63}, // X
    {0x07, 0x08, 0x70, 0x08, 0x07}, // Y
    {0x61, 0x51, 0x49, 0x45, 0x43}, // Z
    {0x00, 0x7F, 0x41, 0x41, 0x00}, // [
    {0x55, 0x2A, 0x55, 0x2A, 0x55}, // backslash
    {0x00, 0x41, 0x41, 0x7F, 0x00}, // ]
    {0x04, 0x02, 0x01, 0x02, 0x04}, // ^
    {0x40, 0x40, 0x40, 0x40, 0x40}, // _
    {0x00, 0x01, 0x02, 0x04, 0x00}, // `
    {0x20, 0x54, 0x54, 0x54, 0x78}, // a
    {0x7F, 0x48, 0x44, 0x44, 0x38}, // b
    {0x38, 0x44, 0x44, 0x44, 0x20}, // c
    {0x38, 0x44, 0x44, 0x48, 0x7F}, // d
    {0x38, 0x54, 0x54, 0x54, 0x18}, // e
    {0x08, 0x7E, 0x09, 0x01, 0x02}, // f
    {0x0C, 0x52, 0x52, 0x52, 0x3E}, // g
    {0x7F, 0x08, 0x04, 0x04, 0x78}, // h
    {0x00, 0x44, 0x7D, 0x40, 0x00}, // i
    {0x20, 0x40, 0x44, 0x3D, 0x00}, // j
    {0x7F, 0x10, 0x28, 0x44, 0x00}, // k
    {0x00, 0x41, 0x7F, 0x40, 0x00}, // l
    {0x7C, 0x04, 0x18, 0x04, 0x78}, // m
    {0x7C, 0x08, 0x04, 0x04, 0x78}, // n
    {0x38, 0x44, 0x44, 0x44, 0x38}, // o
    {0x7C, 0x14, 0x14, 0x14, 0x08}, // p
    {0x08, 0x14, 0x14, 0x18, 0x7C}, // q
    {0x7C, 0x08, 0x04, 0x04, 0x08}, // r
    {0x48, 0x54, 0x54, 0x54, 0x20}, // s
    {0x04, 0x3F, 0x44, 0x40, 0x20}, // t
    {0x3C, 0x40, 0x40, 0x20, 0x7C}, // u
    {0x1C, 0x20, 0x40, 0x20, 0x1C}, // v
    {0x3C, 0x40, 0x30, 0x40, 0x3C}, // w
    {0x44, 0x28, 0x10, 0x28, 0x44}, // x
    {0x0C, 0x50, 0x50, 0x50, 0x3C}, // y
    {0x44, 0x64, 0x54, 0x4C, 0x44}, // z
    {0x00, 0x08, 0x36, 0x41, 0x00}, // {
    {0x00, 0x00, 0x7F, 0x00, 0x00}, // |
    {0x00, 0x41, 0x36, 0x08, 0x00}, // }
    {0x08, 0x04, 0x08, 0x10, 0x08}, // ~
};

static esp_err_t init_i2c(void)
{
    i2c_config_t conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = I2C_MASTER_SDA_IO,
        .scl_io_num = I2C_MASTER_SCL_IO,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = I2C_MASTER_FREQ_HZ,
    };

    esp_err_t ret = i2c_param_config(I2C_MASTER_NUM, &conf);
    if (ret != ESP_OK)
        return ret;

    return i2c_driver_install(I2C_MASTER_NUM, conf.mode, 0, 0, 0);
}

static void oled_cmd(uint8_t cmd)
{
    i2c_cmd_handle_t handle = i2c_cmd_link_create();
    i2c_master_start(handle);
    i2c_master_write_byte(handle, (OLED_ADDR << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write_byte(handle, OLED_CMD_MODE, true);
    i2c_master_write_byte(handle, cmd, true);
    i2c_master_stop(handle);
    esp_err_t ret = i2c_master_cmd_begin(I2C_MASTER_NUM, handle, pdMS_TO_TICKS(1000));
    i2c_cmd_link_delete(handle);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "OLED command failed: 0x%02X", cmd);
    }
}

static void oled_data(const uint8_t *data, size_t len)
{
    i2c_cmd_handle_t handle = i2c_cmd_link_create();
    i2c_master_start(handle);
    i2c_master_write_byte(handle, (OLED_ADDR << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write_byte(handle, OLED_DATA_MODE, true);
    i2c_master_write(handle, data, len, true);
    i2c_master_stop(handle);
    esp_err_t ret = i2c_master_cmd_begin(I2C_MASTER_NUM, handle, pdMS_TO_TICKS(1000));
    i2c_cmd_link_delete(handle);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "OLED data failed, len: %d", len);
    }
}

void i2c_scanner(void)
{
    ESP_LOGI(TAG, "🔍 Scanning I2C bus...");
    int found = 0;

    for (uint8_t addr = 1; addr < 127; addr++)
    {
        i2c_cmd_handle_t cmd = i2c_cmd_link_create();
        i2c_master_start(cmd);
        i2c_master_write_byte(cmd, (addr << 1) | I2C_MASTER_WRITE, true);
        i2c_master_stop(cmd);

        esp_err_t ret = i2c_master_cmd_begin(I2C_MASTER_NUM, cmd, 1000 / portTICK_PERIOD_MS);
        i2c_cmd_link_delete(cmd);

        if (ret == ESP_OK)
        {
            ESP_LOGI(TAG, "✅ Found device at: 0x%02X", addr);
            found++;
        }
    }

    if (found == 0)
    {
        ESP_LOGE(TAG, "❌ No I2C devices found!");
    }
}

static void oled_clear_screen(void)
{
    uint8_t zero_buffer[128] = {0};
    for (uint8_t page = 0; page < 8; page++)
    {
        oled_cmd(0xB0 + page); // Set page
        oled_cmd(0x00);        // Low column
        oled_cmd(0x10);        // High column
        oled_data(zero_buffer, 128);
    }
}

static void oled_show_line(int line, const char *text)
{
    if (line < 0 || line > 7)
        return;

    // Set cursor position
    oled_cmd(0xB0 + line); // Page address
    oled_cmd(0x00);        // Column low
    oled_cmd(0x10);        // Column high

    int len = strlen(text);
    if (len > 21)
        len = 21;
    uint8_t line_buffer[128] = {0};

    for (int i = 0; i < len; i++)
    {
        char c = text[i];
        if (c >= 32 && c <= 126)
        {
            const uint8_t *char_data = font_5x8[c - 32];
            int start_pixel = i * 6;

            for (int col = 0; col < 5; col++)
            {
                if (start_pixel + col < 128)
                {
                    line_buffer[start_pixel + col] = char_data[col];
                }
            }

            if (start_pixel + 5 < 128)
            {
                line_buffer[start_pixel + 5] = 0x00;
            }
        }
    }

    oled_data(line_buffer, 128);
}

void oled_init(void)
{
    ESP_LOGI(TAG, "🔄 Initializing OLED (FINAL FIX WITH FONT)...");

    if (init_i2c() != ESP_OK)
    {
        ESP_LOGE(TAG, "❌ I2C init failed!");
        return;
    }

    i2c_scanner();

    vTaskDelay(pdMS_TO_TICKS(100));

    oled_cmd(0xAE); // Display OFF

    oled_cmd(0x20);
    oled_cmd(0x00); // Horizontal addressing mode
    oled_cmd(0x21);
    oled_cmd(0x00);
    oled_cmd(0x7F); // Column address 0-127
    oled_cmd(0x22);
    oled_cmd(0x00);
    oled_cmd(0x07); // Page address 0-7

    oled_cmd(0x40); // Display start line = 0
    oled_cmd(0x81);
    oled_cmd(0xFF); // Contrast max
    oled_cmd(0xA1); // Segment remap (horizontal flip)
    oled_cmd(0xA6); // Normal display (not inverted)
    oled_cmd(0xA8);
    oled_cmd(0x3F); // Multiplex ratio = 64
    oled_cmd(0xC8); // COM scan direction (vertical flip)
    oled_cmd(0xD3);
    oled_cmd(0x00); // Display offset = 0
    oled_cmd(0xD5);
    oled_cmd(0x80); // Oscillator frequency
    oled_cmd(0xD9);
    oled_cmd(0xF1); // Pre-charge period
    oled_cmd(0xDA);
    oled_cmd(0x12); // COM pins hardware configuration
    oled_cmd(0xDB);
    oled_cmd(0x40); // VCOMH deselect level
    oled_cmd(0x8D);
    oled_cmd(0x14); // Charge pump enable

    oled_clear_screen();
    vTaskDelay(pdMS_TO_TICKS(50));

    oled_cmd(0xAF); // Display ON
    vTaskDelay(pdMS_TO_TICKS(50));

    oled_show_line(0, "VOICE AI SYSTEM");
    oled_show_line(1, "ADC: ---");
    oled_show_line(2, "Status: Ready");
    oled_show_line(3, "XIAO ESP32S3");

    oled_show_line(4, "");
    oled_show_line(5, "");
    oled_show_line(6, "");
    oled_show_line(7, "");

    oled_initialized = true;
    ESP_LOGI(TAG, "✅ OLED initialized with FONT!");
}

void oled_display_adc(int val)
{
    if (!oled_initialized)
        return;

    char buf[32];
    snprintf(buf, sizeof(buf), "ADC: %d", val);
    oled_show_line(1, buf);
}

void oled_display_status(const char *status)
{
    if (!oled_initialized)
    {
        ESP_LOGW(TAG, "OLED not available, status: %s", status);
        return;
    }

    ESP_LOGI(TAG, "OLED Status: %s", status);

    char status_line[32];
    snprintf(status_line, sizeof(status_line), "Status: %s", status);
    oled_show_line(2, status_line);
}

bool oled_is_initialized(void)
{
    return oled_initialized;
}

void oled_clear(void)
{
    oled_clear_screen();
}

void oled_show_message(const char *message)
{
    oled_display_status(message);
}

void oled_show_text(int line, const char *text)
{
    oled_show_line(line, text);
}