#pragma once
extern "C" {
    #include <esp_err.h>
}

namespace wifi {
    esp_err_t init();
    esp_err_t connect();
    void stop();
}
