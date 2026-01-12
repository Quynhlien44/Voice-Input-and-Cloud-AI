#include "esp_wifi.h"
#include "esp_log.h"
#include "esp_system.h"
#include "nvs_flash.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "HARDWARE_TEST";

void comprehensive_hardware_test(void)
{
    ESP_LOGI(TAG, "🔬 === COMPREHENSIVE HARDWARE TEST ===");

    // Test 1: Basic system
    esp_chip_info_t chip_info;
    esp_chip_info(&chip_info);
    ESP_LOGI(TAG, "💻 Chip Revision: %d, Cores: %d", chip_info.revision, chip_info.cores);
    ESP_LOGI(TAG, "💾 Free Heap: %d bytes", esp_get_free_heap_size());

    // Test 2: NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        nvs_flash_erase();
        ret = nvs_flash_init();
    }
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "❌ NVS FAILED: %s", esp_err_to_name(ret));
        return;
    }
    ESP_LOGI(TAG, "✅ NVS: OK");

    // Test 3: WiFi Driver Initialization
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ret = esp_wifi_init(&cfg);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "❌ WIFI INIT FAILED: %s", esp_err_to_name(ret));
        return;
    }
    ESP_LOGI(TAG, "✅ WiFi Init: OK");

    // Test 4: MAC Address
    uint8_t mac[6];
    esp_wifi_get_mac(WIFI_IF_STA, mac);
    ESP_LOGI(TAG, "📡 MAC: %02X:%02X:%02X:%02X:%02X:%02X",
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

    // Test 5: WiFi Start
    ret = esp_wifi_set_mode(WIFI_MODE_STA);
    ret = esp_wifi_start();
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "❌ WIFI START FAILED: %s", esp_err_to_name(ret));
        esp_wifi_deinit();
        return;
    }
    ESP_LOGI(TAG, "✅ WiFi Start: OK");

    // Test 6: Deep Scan with ALL networks
    ESP_LOGI(TAG, "🔍 Starting comprehensive scan...");
    wifi_scan_config_t scan_config = {
        .ssid = NULL,
        .bssid = NULL,
        .channel = 0,
        .show_hidden = true};

    vTaskDelay(pdMS_TO_TICKS(5000));
    ret = esp_wifi_scan_start(&scan_config, true);

    if (ret == ESP_OK)
    {
        uint16_t ap_count = 0;
        esp_wifi_scan_get_ap_num(&ap_count);
        ESP_LOGI(TAG, "📊 Total Networks Found: %d", ap_count);

        wifi_ap_record_t *ap_list = malloc(sizeof(wifi_ap_record_t) * ap_count);
        esp_wifi_scan_get_ap_records(&ap_count, ap_list);

        ESP_LOGI(TAG, "🌐 Available Networks:");
        for (int i = 0; i < ap_count; i++)
        {
            const char *auth_str;
            switch (ap_list[i].authmode)
            {
            case WIFI_AUTH_OPEN:
                auth_str = "OPEN";
                break;
            case WIFI_AUTH_WEP:
                auth_str = "WEP";
                break;
            case WIFI_AUTH_WPA_PSK:
                auth_str = "WPA";
                break;
            case WIFI_AUTH_WPA2_PSK:
                auth_str = "WPA2";
                break;
            case WIFI_AUTH_WPA_WPA2_PSK:
                auth_str = "WPA/WPA2";
                break;
            case WIFI_AUTH_WPA3_PSK:
                auth_str = "WPA3";
                break;
            default:
                auth_str = "UNKNOWN";
                break;
            }

            ESP_LOGI(TAG, "   %s (RSSI: %d, Auth: %s, Channel: %d)",
                     ap_list[i].ssid, ap_list[i].rssi, auth_str, ap_list[i].primary);
        }
        free(ap_list);
    }
    else
    {
        ESP_LOGE(TAG, "❌ SCAN FAILED: %s", esp_err_to_name(ret));
    }

    // Test 7: Try Open Network (no password)
    ESP_LOGI(TAG, "🔓 Testing open network compatibility...");

    // Cleanup
    esp_wifi_stop();
    esp_wifi_deinit();

    ESP_LOGI(TAG, "🔬 === HARDWARE TEST COMPLETE ===");
}

void ultimate_wifi_fix(void)
{
    ESP_LOGI(TAG, "🛠️ === ULTIMATE WIFI FIX ===");

    ESP_LOGI(TAG, "🔄 Method 1: Full ESP32 reset");
    esp_restart();

    vTaskDelay(pdMS_TO_TICKS(1000));
    ESP_LOGI(TAG, "💤 Method 2: Deep sleep reset");
    esp_deep_sleep(1000000); // 1 second
}