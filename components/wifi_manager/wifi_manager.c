#include "wifi_manager.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "nvs_flash.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "string.h"
#include "sdkconfig.h"

static const char *TAG = "WIFI_MANAGER";

static EventGroupHandle_t s_wifi_event_group;
static const int WIFI_CONNECTED_BIT = BIT0;
static char s_ip_addr[16] = {0};
static int s_connection_attempts = 0;

void wifi_hardware_diagnostic(void)
{
    ESP_LOGI(TAG, "🔧 === HARDWARE DIAGNOSTIC START ===");

    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ESP_LOGI(TAG, "NVS needs erase, erasing...");
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "❌ NVS INIT FAILED: %s", esp_err_to_name(ret));
        return;
    }
    ESP_LOGI(TAG, "✅ NVS initialized");

    // Test 1: Check if WiFi driver can be initialized
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ret = esp_wifi_init(&cfg);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "❌ WIFI INIT FAILED: %s", esp_err_to_name(ret));
        return;
    }
    ESP_LOGI(TAG, "✅ WiFi driver init: OK");

    // Test 2: Get MAC address
    uint8_t mac[6];
    esp_wifi_get_mac(WIFI_IF_STA, mac);
    ESP_LOGI(TAG, "📡 MAC Address: %02X:%02X:%02X:%02X:%02X:%02X",
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

    // Test 3: Set mode and start
    ret = esp_wifi_set_mode(WIFI_MODE_STA);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "❌ SET MODE FAILED: %s", esp_err_to_name(ret));
        esp_wifi_deinit();
        return;
    }

    ret = esp_wifi_start();
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "❌ WIFI START FAILED: %s", esp_err_to_name(ret));
        esp_wifi_deinit();
        return;
    }
    ESP_LOGI(TAG, "✅ WiFi start: OK");

    // Test 4: Perform scan
    ESP_LOGI(TAG, "🔍 Testing network scan capability...");
    wifi_scan_config_t scan_config = {
        .ssid = NULL,
        .bssid = NULL,
        .channel = 0,
        .show_hidden = true};

    vTaskDelay(pdMS_TO_TICKS(3000));
    ret = esp_wifi_scan_start(&scan_config, true);

    if (ret == ESP_OK)
    {
        uint16_t ap_count = 0;
        esp_wifi_scan_get_ap_num(&ap_count);
        ESP_LOGI(TAG, "✅ SCAN SUCCESS: Found %d networks", ap_count);

        if (ap_count > 0)
        {
            wifi_ap_record_t *ap_list = malloc(sizeof(wifi_ap_record_t) * ap_count);
            esp_wifi_scan_get_ap_records(&ap_count, ap_list);

            ESP_LOGI(TAG, "📶 Top 5 networks:");
            for (int i = 0; i < (ap_count < 5 ? ap_count : 5); i++)
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
                    auth_str = "WPA_PSK";
                    break;
                case WIFI_AUTH_WPA2_PSK:
                    auth_str = "WPA2_PSK";
                    break;
                case WIFI_AUTH_WPA_WPA2_PSK:
                    auth_str = "WPA_WPA2_PSK";
                    break;
                default:
                    auth_str = "UNKNOWN";
                    break;
                }
                ESP_LOGI(TAG, "   %d. %s (RSSI: %d, Auth: %s)",
                         i + 1, ap_list[i].ssid, ap_list[i].rssi, auth_str);
            }
            free(ap_list);
        }
    }
    else
    {
        ESP_LOGE(TAG, "❌ SCAN FAILED: %s", esp_err_to_name(ret));
    }

    // Cleanup
    esp_wifi_stop();
    esp_wifi_deinit();

    ESP_LOGI(TAG, "🔧 === HARDWARE DIAGNOSTIC COMPLETE ===");
}

// ==================== WIFI EVENT HANDLER ====================
static void wifi_event_handler(void *arg, esp_event_base_t event_base,
                               int32_t event_id, void *event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START)
    {
        ESP_LOGI(TAG, "📡 WiFi STA started");
        s_connection_attempts = 0;
        vTaskDelay(pdMS_TO_TICKS(2000));
        esp_wifi_connect();
        ESP_LOGI(TAG, "🔄 Attempting connection...");
    }
    else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED)
    {
        wifi_event_sta_disconnected_t *event = (wifi_event_sta_disconnected_t *)event_data;
        s_connection_attempts++;

        const char *reason_str;
        switch (event->reason)
        {
        case 2:
            reason_str = "AUTH_EXPIRE";
            break;
        case 201:
            reason_str = "ASSOC_FAIL";
            break;
        case 205:
            reason_str = "AUTH_FAIL";
            break;
        case 15:
            reason_str = "ASSOC_TOOMANY";
            break;
        default:
            reason_str = "UNKNOWN";
            break;
        }

        ESP_LOGW(TAG, "❌ Disconnected: %s (reason: %d), Attempt: %d",
                 reason_str, event->reason, s_connection_attempts);
        xEventGroupClearBits(s_wifi_event_group, WIFI_CONNECTED_BIT);

        if (s_connection_attempts <= 5)
        {
            ESP_LOGI(TAG, "🔄 Reconnecting in 5 seconds...");
            vTaskDelay(pdMS_TO_TICKS(5000));
            esp_wifi_connect();
        }
        else
        {
            ESP_LOGE(TAG, "🛑 Too many failures, stopping retries");
        }
    }
    else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP)
    {
        ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
        snprintf(s_ip_addr, sizeof(s_ip_addr), IPSTR, IP2STR(&event->ip_info.ip));

        ESP_LOGI(TAG, "🎉 ✅ WiFi CONNECTED SUCCESSFULLY!");
        ESP_LOGI(TAG, "📱 IP: %s, Gateway: " IPSTR, s_ip_addr, IP2STR(&event->ip_info.gw));
        ESP_LOGI(TAG, "🔗 Connection attempts: %d", s_connection_attempts);

        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
        s_connection_attempts = 0;
    }
}

// ==================== WIFI MANAGER INIT ====================
void wifi_manager_init(void)
{
    ESP_LOGI(TAG, "🚀 === WIFI MANAGER INITIALIZATION ===");

    ESP_LOGI(TAG, "🧹 Initializing NVS...");
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ESP_LOGI(TAG, "NVS needs erase, erasing...");
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
    ESP_LOGI(TAG, "✅ NVS initialized");

    s_wifi_event_group = xEventGroupCreate();
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID,
                                                        &wifi_event_handler, NULL, NULL));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP,
                                                        &wifi_event_handler, NULL, NULL));

    wifi_config_t wifi_config = {
        .sta = {
            .ssid = "Keenetic-5122",
            .password = "FnLPXGKj",
            .scan_method = WIFI_FAST_SCAN,
            .sort_method = WIFI_CONNECT_AP_BY_SIGNAL,
            .threshold.rssi = -127,
            .threshold.authmode = WIFI_AUTH_WPA2_PSK,
        },
    };

    ESP_LOGI(TAG, "🎯 === WIFI CONFIGURATION ===");
    ESP_LOGI(TAG, "📶 SSID: %s", wifi_config.sta.ssid);
    ESP_LOGI(TAG, "🔑 Password: %s", wifi_config.sta.password);

    // Start WiFi
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "⏳ WiFi started - waiting for connection...");
    s_connection_attempts = 0;
}

// ==================== UTILITY FUNCTIONS ====================
bool wifi_manager_is_connected(void)
{
    return (xEventGroupGetBits(s_wifi_event_group) & WIFI_CONNECTED_BIT) != 0;
}

char *wifi_manager_get_ip(void)
{
    return wifi_manager_is_connected() ? s_ip_addr : "0.0.0.0";
}

esp_err_t wifi_manager_wait_connected(TickType_t timeout)
{
    EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group,
                                           WIFI_CONNECTED_BIT,
                                           pdFALSE, pdTRUE, timeout);
    return (bits & WIFI_CONNECTED_BIT) ? ESP_OK : ESP_ERR_TIMEOUT;
}

esp_err_t wifi_manager_reconnect(void)
{
    ESP_LOGI(TAG, "🔄 Manual reconnect triggered");
    s_connection_attempts = 0;
    xEventGroupClearBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
    vTaskDelay(pdMS_TO_TICKS(2000));
    return esp_wifi_connect();
}

void wifi_manager_stop(void)
{
    ESP_LOGI(TAG, "🛑 Stopping WiFi...");
    xEventGroupClearBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
    esp_wifi_disconnect();
    esp_wifi_stop();
    s_connection_attempts = 0;
}

void wifi_test_all_networks(void)
{

    ESP_LOGI(TAG, "Network testing disabled");
}