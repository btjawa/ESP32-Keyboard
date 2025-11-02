#include "ota.hpp"
#include "led.hpp"
#include "wifi_sta.hpp"

extern "C" {
    #include <esp_crt_bundle.h>
    #include <esp_coexist.h>
    #include <esp_ota_ops.h>
    #include <esp_partition.h>
    #include <esp_http_client.h>
    #include <esp_https_ota.h>
    #include <freertos/task.h>
    #include <driver/gpio.h>
    #include <esp_log.h>
}

#include <string>

static std::string fwHash;
static bool isChecking = false;

#define OTA_PIN GPIO_NUM_39

void checkOTA() {
    ESP_LOGI("OTA", "Checking for OTA update...");

    wifi::init();
    if (wifi::connect() != ESP_OK) {
        ESP_LOGE("OTA", "Failed to connect to WiFi");
        wifi::stop();
        return;
    }

    ESP_LOGI("OTA", "Checking firmware version...");

    esp_http_client_config_t http_cfg = {};
    http_cfg.url = "http://192.168.3.213:8080/firmware.bin";
    http_cfg.timeout_ms = 2000;
    http_cfg.event_handler = [](esp_http_client_event_t* evt) {
        switch (evt->event_id) {
            case HTTP_EVENT_ON_CONNECTED:
                esp_http_client_set_header(evt->client, "If-None-Match", fwHash.c_str());
                break;
            case HTTP_EVENT_ON_DATA:
                fillDark();
                break;
            default: break;
        }
        return ESP_OK;
    };
    http_cfg.crt_bundle_attach = esp_crt_bundle_attach;

    esp_https_ota_config_t ota_config = {};
    ota_config.http_config = &http_cfg;

    esp_err_t ret = esp_https_ota(&ota_config);

    wifi::stop();

    if (ret == ESP_ERR_OTA_VALIDATE_FAILED) {
        ESP_LOGW("OTA", "No new firmware available");
    } else if (ret != ESP_OK) {
        ESP_LOGE("OTA", "OTA update failed: %s", esp_err_to_name(ret));
    } else

    if (ret == ESP_OK) {
        esp_restart();
    }
}

void OTATask(void*) {
    for (;;) {
        bool pressed = (bool)(gpio_get_level(OTA_PIN));
        if (pressed && !isChecking) {
            isChecking = true;
            checkOTA();
            isChecking = false;
        }
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

void setupOTA() {
    gpio_reset_pin(OTA_PIN);
    gpio_config_t io = {
        .pin_bit_mask = 1ULL << OTA_PIN,
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_ENABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&io);

    const esp_partition_t* p = esp_ota_get_running_partition();
    uint8_t sha[32];
    if (p && esp_partition_get_sha256(p, sha) == ESP_OK) {
        char hex[65];
        for (int i = 0; i < 32; ++i) sprintf(&hex[i*2], "%02x", sha[i]);
        hex[64] = '\0';
        fwHash = std::string(hex);
    } else {
        fwHash.clear();
    }

    xTaskCreatePinnedToCore(
        OTATask,
        "OTATask",
        8192,
        nullptr,
        1,
        nullptr,
        APP_CPU_NUM
    );
}
