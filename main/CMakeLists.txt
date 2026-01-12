# main/CMakeLists.txt
idf_component_register(
    SRCS "main.c"
    INCLUDE_DIRS 
        "."
        "../components/mqtt_client"
    REQUIRES 
        mqtt
        adc_handler
        audio_recorder
        i2s_audio
        oled_display
        wifi_manager
        base64_decoder
)

idf_build_set_property(COMPILE_OPTIONS "-DmainTASK_STACK_SIZE=8192" APPEND)