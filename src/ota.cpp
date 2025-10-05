#include "ota.h"
#include "secrets.h"

#include <WiFi.h>
#include <esp_ota_ops.h>
#include <esp_partition.h>
#include <esp_http_client.h>
#include <esp_https_ota.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

constexpr TickType_t PERIOD = pdMS_TO_TICKS(1000);

String fwHash;
static WiFiClient wifiClient;

static esp_err_t http_evt(esp_http_client_event_t* evt) {
    switch (evt->event_id) {
        case HTTP_EVENT_ON_CONNECTED:
            esp_http_client_set_header(evt->client, "If-None-Match", fwHash.c_str());
            break;
        case HTTP_EVENT_ON_DATA:
            // Turn off LEDs
            digitalWrite(45, LOW);
            break;
        default: break;
    }
    return ESP_OK;
}

void checkOTA() {
    if (WiFi.status() != WL_CONNECTED) {
        WiFi.mode(WIFI_STA);
        WiFi.persistent(false);
        WiFi.setSleep(true);
        WiFi.begin(SSID, PASS);
        uint32_t start = millis();
        while (WiFi.status() != WL_CONNECTED && millis() - start < 8000) {
            vTaskDelay(pdMS_TO_TICKS(200));
        }
    } else {
        esp_http_client_config_t http_cfg = {};
        http_cfg.url               = OTA_URL;
        http_cfg.timeout_ms        = 2500;
        http_cfg.event_handler     = http_evt;
        http_cfg.cert_pem          = OTA_CERT_PEM;
    
        esp_err_t ret = esp_https_ota(&http_cfg);
        if (ret == ESP_OK) {
            esp_restart();
        }
    }
}

static void OTATask(void*) {
    for (;;) {
        checkOTA();
        vTaskDelay(PERIOD);
    }
}

void setupOTA() {
    const esp_partition_t* p = esp_ota_get_running_partition();
    uint8_t sha[32];
    if (p && esp_partition_get_sha256(p, sha) == ESP_OK) {
        char hex[65];
        for (int i = 0; i < 32; ++i) sprintf(&hex[i*2], "%02x", sha[i]);
        hex[64] = '\0';
        fwHash = String(hex);
    } else {
        fwHash.clear();
    }
    /*
    xTaskCreatePinnedToCore(
        OTATask,
        "OTATask",
        6144,
        nullptr,
        1,
        nullptr,
        APP_CPU_NUM
    );
    */
}
