#include "usb_hid.hpp"

extern "C" {
    #include <tinyusb.h>
    #include <tinyusb_default_config.h>
    #include <class/hid/hid_device.h>
    #include <driver/gpio.h>
    #include <esp_log.h>
}

#define VBUS_MONITOR_IO GPIO_NUM_1

namespace usb_hid {

static const char *TAG = "USB_HID";

static bool active = false;

static uint8_t modifiers = 0;
static uint8_t keycodes[6] = {0};
static uint8_t keycodes_len = sizeof(keycodes) / sizeof(keycodes[0]);

#define TUSB_DESC_TOTAL_LEN      (TUD_CONFIG_DESC_LEN + CFG_TUD_HID * TUD_HID_DESC_LEN)

const uint8_t hid_report_descriptor[] = {
    TUD_HID_REPORT_DESC_KEYBOARD(HID_REPORT_ID(HID_ITF_PROTOCOL_KEYBOARD)),
};

const char *hid_string_descriptor[5] = {
    // array of pointer to string descriptors
    (char[]){0x09, 0x04},  // 0: is supported language is English (0x0409)
    "btjawa",              // 1: Manufacturer
    "ESP32 Keyboard",      // 2: Product
    nullptr,               // 3: Serials, should use chip ID
    "Example HID interface",  // 4: HID
};

static const uint8_t hid_configuration_descriptor[] = {
    // Configuration number, interface count, string index, total length, attribute, power in mA
    TUD_CONFIG_DESCRIPTOR(1, 1, 0, TUSB_DESC_TOTAL_LEN, TUSB_DESC_CONFIG_ATT_REMOTE_WAKEUP, 100),

    // Interface number, string index, boot protocol, report descriptor len, EP In address, size & polling interval
    TUD_HID_DESCRIPTOR(0, 4, false, sizeof(hid_report_descriptor), 0x81, 16, 10),
};

void setup(char serial_str[17]) {
    if (active) return;

    ESP_LOGI(TAG, "Setting up USB HID");

    hid_string_descriptor[3] = serial_str;

    gpio_reset_pin(VBUS_MONITOR_IO);
    gpio_config_t io = {
        .pin_bit_mask = 1ULL << VBUS_MONITOR_IO,
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&io);
    gpio_set_drive_capability(VBUS_MONITOR_IO, GPIO_DRIVE_CAP_0);

    tinyusb_config_t tusb_cfg = TINYUSB_DEFAULT_CONFIG();
    tusb_cfg.phy.self_powered = true;
    tusb_cfg.phy.vbus_monitor_io = VBUS_MONITOR_IO;
    tusb_cfg.descriptor.device = nullptr;
    tusb_cfg.descriptor.full_speed_config = hid_configuration_descriptor;
    tusb_cfg.descriptor.string = hid_string_descriptor;
    tusb_cfg.descriptor.string_count = sizeof(hid_string_descriptor) / sizeof(hid_string_descriptor[0]);

    ESP_ERROR_CHECK(tinyusb_driver_install(&tusb_cfg));
    active = true;
    ESP_LOGI(TAG, "USB HID setup complete");
}

void end() {
    if (!active) return;
    active = false;

    memset(keycodes, 0, sizeof(keycodes));
    if (tud_mounted()) {
        tud_hid_keyboard_report(HID_ITF_PROTOCOL_KEYBOARD, modifiers, keycodes);
        tud_disconnect();
    }
    tinyusb_driver_uninstall();
    ESP_LOGI(TAG, "USB HID ended");
}

void press(const uint8_t key) {
    if (!tud_ready()) return;

    for (auto i = 0; i < keycodes_len; i++) {
        if (keycodes[i] == key) {
            return;
        }
        if (keycodes[i] == 0) {
            keycodes[i] = key;
            break;
        }
    }
    tud_hid_keyboard_report(HID_ITF_PROTOCOL_KEYBOARD, modifiers, keycodes);
}

void release(const uint8_t key) {
    if (!tud_ready()) return;

    for (auto i = 0; i < keycodes_len; i++) {
        if (keycodes[i] == key) {
            keycodes[i] = 0;
        }
    }
    tud_hid_keyboard_report(HID_ITF_PROTOCOL_KEYBOARD, modifiers, keycodes);
}

}

extern "C" {

/********* TinyUSB HID callbacks ***************/

// Invoked when received GET HID REPORT DESCRIPTOR request
// Application return pointer to descriptor, whose contents must exist long enough for transfer to complete
uint8_t const *tud_hid_descriptor_report_cb(uint8_t instance)
{
    // We use only one interface and one HID report descriptor, so we can ignore parameter 'instance'
    return usb_hid::hid_report_descriptor;
}

// Invoked when received GET_REPORT control request
// Application must fill buffer report's content and return its length.
// Return zero will cause the stack to STALL request
uint16_t tud_hid_get_report_cb(uint8_t instance, uint8_t report_id, hid_report_type_t report_type, uint8_t *buffer, uint16_t reqlen)
{
    (void) instance;
    (void) report_id;
    (void) report_type;
    (void) buffer;
    (void) reqlen;

    return 0;
}

// Invoked when received SET_REPORT control request or
// received data on OUT endpoint ( Report ID = 0, Type = 0 )
void tud_hid_set_report_cb(uint8_t instance, uint8_t report_id, hid_report_type_t report_type, uint8_t const *buffer, uint16_t bufsize)
{
}

}
