file(GLOB_RECURSE SRC_FILES
    "${CMAKE_CURRENT_SOURCE_DIR}/*.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/*.cc"
    "${CMAKE_CURRENT_SOURCE_DIR}/*.S"
)

idf_component_register(
    SRCS ${SRC_FILES}
    INCLUDE_DIRS "."
    REQUIRES
        driver
        freertos
        esp_system
        esp_timer
        esp_adc
        esp_coex
        nvs_flash
        esp_wifi
        esp_event
        esp_http_client
        esp_https_ota
        app_update
        mbedtls
        bt
        esp_hid
    PRIV_REQUIRES spi_flash
)
