#include "keyboard.hpp"
#include "led.hpp"
#include "usb_hid.hpp"
#include "ble_hid.hpp"

extern "C" {
    #include <keyboard_button.h>
    #include <driver/usb_serial_jtag.h>
    #include <esp_mac.h>
    #include <esp_log.h>
}

constexpr TickType_t SLEEP_TICKS = pdMS_TO_TICKS(5 * 60 * 1000);
TickType_t gLastTick = 0;

static keyboard_btn_handle_t s_kbd = nullptr;

constexpr int ROWS[] = {9, 10, 12, 13};
constexpr int ROWS_LEN = sizeof(ROWS) / sizeof(ROWS[0]);
constexpr int COLS[] = {3, 11, 14, 21};
constexpr int COLS_LEN = sizeof(COLS) / sizeof(COLS[0]);

constexpr uint8_t KeyMap[ROWS_LEN][COLS_LEN] = {
    { HID_KEY_KEYPAD_7, HID_KEY_KEYPAD_8,       HID_KEY_KEYPAD_9, HID_KEY_MUTE, },
    { HID_KEY_KEYPAD_4, HID_KEY_KEYPAD_5,       HID_KEY_KEYPAD_6, HID_KEY_KEYPAD_ADD, },
    { HID_KEY_KEYPAD_1, HID_KEY_KEYPAD_2,       HID_KEY_KEYPAD_3, HID_KEY_KEYPAD_SUBTRACT, },
    { HID_KEY_KEYPAD_0, HID_KEY_KEYPAD_DECIMAL, HID_KEY_TAB,      HID_KEY_ENTER, },
};

#define VBUS_MONITOR_IO GPIO_NUM_1

static char serial_str[17];

void inline tick() {
    gLastTick = xTaskGetTickCount();
}

bool inline isUsb() {
    return gpio_get_level(VBUS_MONITOR_IO);
}

void press(const uint8_t key) {
    isUsb() ?
        usb_hid::press(key) :
        ble_hid::press(key);
}

void release(const uint8_t key) {
    isUsb() ?
        usb_hid::release(key) :
        ble_hid::release(key);
}

static void keyboard_cb(keyboard_btn_handle_t kbd_handle, keyboard_btn_report_t kbd_report, void *user_data) {
    for (auto i = 0; i < kbd_report.key_pressed_num; i++) {
        auto d = kbd_report.key_data[i];
        uint8_t key = KeyMap[d.output_index][d.input_index];
        press(key);
        tick();
    }
    for (auto i = 0; i < kbd_report.key_release_num; i++) {
        auto d = kbd_report.key_release_data[i];
        uint8_t key = KeyMap[d.output_index][d.input_index];
        release(key);
        tick();
    }
}

void LinkTask(void*) {
    const TickType_t interval = pdMS_TO_TICKS(500);
    TickType_t last = xTaskGetTickCount();
    for (;;) {
        if (isUsb()) {
            ble_hid::end();
            usb_hid::setup(serial_str);
        } else {
            ble_hid::setup(serial_str);
            usb_hid::end();
        }
        vTaskDelayUntil(&last, interval);
    }
}

void setupKeyboard() {
    gpio_reset_pin(VBUS_MONITOR_IO);
    gpio_config_t io = {
        .pin_bit_mask = 1ULL << VBUS_MONITOR_IO,
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&io);

    uint64_t mac = 0;
    esp_efuse_mac_get_default((uint8_t*)&mac);
    snprintf(
        serial_str,
        sizeof(serial_str),
        "%04lX%08lX",
        (uint32_t)(mac >> 32),
        (uint32_t)mac
    );

    keyboard_btn_config_t cfg = {
        .output_gpios = ROWS,
        .input_gpios = COLS,
        .output_gpio_num = ROWS_LEN,
        .input_gpio_num = COLS_LEN,
        .active_level = 1,
        .debounce_ticks = 2,
        .ticks_interval = 1000, // us
        .enable_power_save = false,
        .priority = 5,
        .core_id = APP_CPU_NUM,
    };
    ESP_ERROR_CHECK(keyboard_button_create(&cfg, &s_kbd));
    keyboard_btn_cb_config_t cb_cfg = {};
    cb_cfg.event = KBD_EVENT_PRESSED;
    cb_cfg.callback = keyboard_cb;
    ESP_ERROR_CHECK(keyboard_button_register_cb(s_kbd, cb_cfg, NULL));
    tick();
    xTaskCreatePinnedToCore(
        LinkTask,
        "LinkTask",
        4096,
        nullptr,
        1,
        nullptr,
        APP_CPU_NUM
    );
}
