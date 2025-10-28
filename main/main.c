#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

#include "adc_handler.h"
#include "oled_display.h"
#include "wifi_manager.h"
#include "my_mqtt_client.h"
#include "i2s_audio.h"

#define TAG "MAIN"

void app_main()
{
    ESP_LOGI(TAG, "Starting app");

    adc_init();
    oled_init();
    wifi_init();
    ESP_LOGI(TAG, "Waiting for WiFi connection...");
    vTaskDelay(pdMS_TO_TICKS(5000));
    mqtt_app_start();
    i2s_init();

    ESP_LOGI(TAG, "Hardware initialized");

    xTaskCreate(adc_task, "adc_task", 2048, NULL, 5, NULL);
    xTaskCreate(mqtt_task, "mqtt_task", 4096, NULL, 6, NULL);
    xTaskCreate(i2s_task, "i2s_task", 2048, NULL, 5, NULL);
}
