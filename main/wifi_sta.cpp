#include "wifi_sta.hpp"
#include "wifi_secrets.hpp"

extern "C" {
    #include <nvs_flash.h>
    #include <esp_wifi.h>
    #include <esp_event.h>
    #include <esp_netif.h>
    #include <freertos/event_groups.h>
    #include <esp_log.h>
}

namespace wifi {

#define WIFI_GOT_IP_BIT BIT0

EventGroupHandle_t s_evt = nullptr;
bool s_inited = false;
bool s_started = false;
const char* TAG = "wifi_sta";
const TickType_t TIMEOUT = pdMS_TO_TICKS(5000);

static void on_event(
    void* event_handler_arg,
    esp_event_base_t event_base,
    int32_t event_id,
    void* event_data
) {
    if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        xEventGroupSetBits(s_evt, WIFI_GOT_IP_BIT);
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        xEventGroupClearBits(s_evt, WIFI_GOT_IP_BIT);
    }
}

esp_err_t init() {
    if (s_inited) return ESP_OK;

    esp_err_t nvs_ret = nvs_flash_init();
    if (nvs_ret == ESP_ERR_NVS_NO_FREE_PAGES || nvs_ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ESP_ERROR_CHECK(nvs_flash_init());
    }

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));

    s_evt = xEventGroupCreate();
    ESP_ERROR_CHECK(esp_event_handler_instance_register(
        WIFI_EVENT,
        WIFI_EVENT_STA_DISCONNECTED,
        &on_event,
        nullptr,
        nullptr
    ));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(
        IP_EVENT,
        IP_EVENT_STA_GOT_IP,
        &on_event,
        nullptr,
        nullptr
    ));

    s_inited = true;
    return ESP_OK;
}

esp_err_t connect() {
    if (!s_inited) init();
    wifi_config_t wc{};
    snprintf(
        reinterpret_cast<char*>(wc.sta.ssid),
        sizeof(wc.sta.ssid),
        "%s",
        SSID
    );
    snprintf(
        reinterpret_cast<char*>(wc.sta.password),
        sizeof(wc.sta.password),
        "%s",
        PASS
    );
    wc.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;
    wc.sta.pmf_cfg = {
        .capable = true,
        .required = false
    };

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wc));
    ESP_ERROR_CHECK(esp_wifi_set_ps(WIFI_PS_MIN_MODEM));

    if (!s_started) {
        ESP_ERROR_CHECK(esp_wifi_start());
        s_started = true;
    }

    xEventGroupClearBits(s_evt, WIFI_GOT_IP_BIT);

    ESP_ERROR_CHECK(esp_wifi_connect());
    EventBits_t bits = xEventGroupWaitBits(
        s_evt, WIFI_GOT_IP_BIT, pdFALSE, pdFALSE, TIMEOUT
    );
    if (!(bits & WIFI_GOT_IP_BIT)) {
        ESP_LOGW(TAG, "Wait IP timeout");
        return ESP_ERR_TIMEOUT;
    }
    return ESP_OK;
}

void stop() {
    if (!s_inited) return;
    esp_wifi_disconnect();
    if (s_started) {
        esp_wifi_stop();
        s_started = false;
    }
}

}
