idf_component_register(
    SRCS "main.c"
    INCLUDE_DIRS "."
    REQUIRES adc_handler oled_display wifi_manager mqtt_client i2s_audio nvs_flash esp_wifi mqtt esp_event ssd1306 driver
)
