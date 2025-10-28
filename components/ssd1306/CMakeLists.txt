set(component_srcs "ssd1306.c" "ssd1306_spi.c")

if (IDF_VERSION_MAJOR GREATER_EQUAL 5)
    if (CONFIG_LEGACY_DRIVER)
        list(APPEND component_srcs "ssd1306_i2c_legacy.c")
    else()
        list(APPEND component_srcs "ssd1306_i2c_new.c")
    endif()
else()
    list(APPEND component_srcs "ssd1306_i2c_legacy.c")
endif()

idf_component_register(
    SRCS "${component_srcs}"
    PRIV_REQUIRES driver
    INCLUDE_DIRS "."
)
