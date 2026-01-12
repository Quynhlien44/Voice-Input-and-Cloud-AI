idf_component_register(
    SRCS "mqtt_client.c"
    INCLUDE_DIRS "."
    REQUIRES esp_netif esp_event mqtt
)