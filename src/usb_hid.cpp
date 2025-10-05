#include "usb_hid.h"
#include <USB.h>
#include <USBHIDKeyboard.h>
#include <USBHIDConsumerControl.h>

USBHID HID;
USBHIDKeyboard hidKeyboard;
USBHIDConsumerControl hidConsumer;

namespace usb_hid {
    void setup() {
        HID.begin();
        USB.begin();
        hidKeyboard.begin();
        hidConsumer.begin();
    };
    void press(const uint8_t key) {
        hidKeyboard.press(key);
    }
    void write(uint16_t usage) {
        hidConsumer.press(usage);
        hidConsumer.release();
    }
    void release(const uint8_t key) {
        hidKeyboard.release(key);
    }
}
