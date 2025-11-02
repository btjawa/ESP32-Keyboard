#include "ble_hid.hpp"

extern "C" {
    #include <nvs_flash.h>
    #include <esp_bt.h>
    #include <esp_hidd.h>
    #include <esp_log.h>

    #include "host/ble_hs.h"
    #include "host/ble_store.h"
    #include "nimble/nimble_port.h"
    #include "nimble/nimble_port_freertos.h"
    void ble_store_config_init(void);
}

namespace ble_hid {

static const char *TAG = "BLE_HID";

static bool active = false;
static bool mounted = false;

static uint8_t modifiers = 0;
static uint8_t keycodes[5] = {0};
static uint8_t keycodes_len = sizeof(keycodes) / sizeof(keycodes[0]);
static esp_hidd_dev_t *hid_dev;

#define GATT_SVR_SVC_HID_UUID 0x1812
static struct ble_hs_adv_fields fields;

const unsigned char keyboardReportMap[] = { //7 bytes input (modifiers, resrvd, keys*5), 1 byte output
    0x05, 0x01,        // Usage Page (Generic Desktop Ctrls)
    0x09, 0x06,        // Usage (Keyboard)
    0xA1, 0x01,        // Collection (Application)
    0x85, 0x01,        //   Report ID (1)
    0x05, 0x07,        //   Usage Page (Kbrd/Keypad)
    0x19, 0xE0,        //   Usage Minimum (0xE0)
    0x29, 0xE7,        //   Usage Maximum (0xE7)
    0x15, 0x00,        //   Logical Minimum (0)
    0x25, 0x01,        //   Logical Maximum (1)
    0x75, 0x01,        //   Report Size (1)
    0x95, 0x08,        //   Report Count (8)
    0x81, 0x02,        //   Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
    0x95, 0x01,        //   Report Count (1)
    0x75, 0x08,        //   Report Size (8)
    0x81, 0x03,        //   Input (Const,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
    0x95, 0x05,        //   Report Count (5)
    0x75, 0x01,        //   Report Size (1)
    0x05, 0x08,        //   Usage Page (LEDs)
    0x19, 0x01,        //   Usage Minimum (Num Lock)
    0x29, 0x05,        //   Usage Maximum (Kana)
    0x91, 0x02,        //   Output (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position,Non-volatile)
    0x95, 0x01,        //   Report Count (1)
    0x75, 0x03,        //   Report Size (3)
    0x91, 0x03,        //   Output (Const,Var,Abs,No Wrap,Linear,Preferred State,No Null Position,Non-volatile)
    0x95, 0x05,        //   Report Count (5)
    0x75, 0x08,        //   Report Size (8)
    0x15, 0x00,        //   Logical Minimum (0)
    0x25, 0x65,        //   Logical Maximum (101)
    0x05, 0x07,        //   Usage Page (Kbrd/Keypad)
    0x19, 0x00,        //   Usage Minimum (0x00)
    0x29, 0x65,        //   Usage Maximum (0x65)
    0x81, 0x00,        //   Input (Data,Array,Abs,No Wrap,Linear,Preferred State,No Null Position)
    0xC0,              // End Collection

    // 65 bytes
};

static esp_hid_raw_report_map_t ble_report_maps[] = {
    {
        .data = keyboardReportMap,
        .len = sizeof(keyboardReportMap)
    }
};

static esp_hid_device_config_t ble_hid_config = {
    .vendor_id          = 0x16C0,
    .product_id         = 0x05DF,
    .version            = 0x0100,
    .device_name        = "ESP32 Keyboard",
    .manufacturer_name  = "btjawa",
    .serial_number      = nullptr,
    .report_maps        = ble_report_maps,
    .report_maps_len    = 1
};

static int nimble_hid_gap_event(struct ble_gap_event *event, void *arg) {
    struct ble_gap_conn_desc desc;
    struct ble_sm_io pkey;
    int rc;
    int key;

    switch (event->type) {
        case BLE_GAP_EVENT_CONNECT:
            /* A new connection was established or a connection attempt failed. */
            ESP_LOGI(
                TAG, "connection %s; status=%d",
                event->connect.status == 0 ? "established" : "failed",
                event->connect.status
            );
            break;
        
        case BLE_GAP_EVENT_DISCONNECT:
            ESP_LOGI(
                TAG,
                "disconnect; reason=%d", event->disconnect.reason
            );
            break;
        
        case BLE_GAP_EVENT_CONN_UPDATE:
            /* The central has updated the connection parameters. */
            ESP_LOGI(
                TAG,
                "connection updated; status=%d",
                event->conn_update.status
            );
            break;
        
        case BLE_GAP_EVENT_ADV_COMPLETE:
            ESP_LOGI(
                TAG,
                "advertise complete; reason=%d",
                event->adv_complete.reason
            );
            break;
        
        case BLE_GAP_EVENT_SUBSCRIBE:
            ESP_LOGI(
                TAG,
                "subscribe event; conn_handle=%d attr_handle=%d "
                "reason=%d prevn=%d curn=%d previ=%d curi=%d\n",
                event->subscribe.conn_handle,
                event->subscribe.attr_handle,
                event->subscribe.reason,
                event->subscribe.prev_notify,
                event->subscribe.cur_notify,
                event->subscribe.prev_indicate,
                event->subscribe.cur_indicate
            );
            break;
        
        case BLE_GAP_EVENT_MTU:
            ESP_LOGI(
                TAG,
                "mtu update event; conn_handle=%d cid=%d mtu=%d",
                event->mtu.conn_handle,
                event->mtu.channel_id,
                event->mtu.value
            );
            break;

        case BLE_GAP_EVENT_ENC_CHANGE:
            /* Encryption has been enabled or disabled for this connection. */
            ESP_LOGI(
                TAG,
                "encryption change event; status=%d ",
                event->enc_change.status
            );
            ble_gap_conn_find(event->enc_change.conn_handle, &desc);
            mounted = true;
            break;
        
        case BLE_GAP_EVENT_NOTIFY_TX:
            ESP_LOGI(
                TAG,
                "notify_tx event; conn_handle=%d attr_handle=%d "
                "status=%d is_indication=%d",
                event->notify_tx.conn_handle,
                event->notify_tx.attr_handle,
                event->notify_tx.status,
                event->notify_tx.indication
            );
            break;
        
        case BLE_GAP_EVENT_REPEAT_PAIRING:
            /* We already have a bond with the peer, but it is attempting to
            * establish a new secure link.  This app sacrifices security for
            * convenience: just throw away the old bond and accept the new link.
            */

            /* Delete the old bond. */
            ble_gap_conn_find(event->repeat_pairing.conn_handle, &desc);
            ble_store_util_delete_peer(&desc.peer_id_addr);

            /* Return BLE_GAP_REPEAT_PAIRING_RETRY to indicate that the host should
            * continue with the pairing operation.
            */
            return BLE_GAP_REPEAT_PAIRING_RETRY;

        case BLE_GAP_EVENT_PASSKEY_ACTION:
            ESP_LOGI(TAG, "PASSKEY_ACTION_EVENT started");
            memset(&pkey, 0, sizeof(pkey));
            memset(&key, 0, sizeof(key));

            if (event->passkey.params.action == BLE_SM_IOACT_DISP) {
                pkey.action = event->passkey.params.action;
                pkey.passkey = 123456; // This is the passkey to be entered on peer
                ESP_LOGI(TAG, "Enter passkey %" PRIu32 " on the peer side", pkey.passkey);
                rc = ble_sm_inject_io(event->passkey.conn_handle, &pkey);
                ESP_LOGI(TAG, "ble_sm_inject_io result: %d", rc);
            } else if (event->passkey.params.action == BLE_SM_IOACT_NUMCMP) {
                ESP_LOGI(TAG, "Accepting passkey..");
                pkey.action = event->passkey.params.action;
                pkey.numcmp_accept = key;
                rc = ble_sm_inject_io(event->passkey.conn_handle, &pkey);
                ESP_LOGI(TAG, "ble_sm_inject_io result: %d", rc);
            } else if (event->passkey.params.action == BLE_SM_IOACT_OOB) {
                static uint8_t tem_oob[16] = {0};
                pkey.action = event->passkey.params.action;
                for (int i = 0; i < 16; i++) {
                    pkey.oob[i] = tem_oob[i];
                }
                rc = ble_sm_inject_io(event->passkey.conn_handle, &pkey);
                ESP_LOGI(TAG, "ble_sm_inject_io result: %d", rc);
            } else if (event->passkey.params.action == BLE_SM_IOACT_INPUT) {
                ESP_LOGI(TAG, "Input not supported passing -> 123456");
                pkey.action = event->passkey.params.action;
                pkey.passkey = 123456;
                rc = ble_sm_inject_io(event->passkey.conn_handle, &pkey);
                ESP_LOGI(TAG, "ble_sm_inject_io result: %d", rc);
            }

            break;

        default:
            break;
    }
    return 0;
}

static void esp_hid_ble_gap_adv_start() {
    int rc;
    struct ble_gap_adv_params adv_params;

    rc = ble_gap_adv_set_fields(&fields);
    if (rc != 0) {
        ESP_LOGE(TAG, "error setting advertisement data; rc=%d\n", rc);
        return;
    }
    /* Begin advertising. */
    memset(&adv_params, 0, sizeof(adv_params));
    adv_params.conn_mode = BLE_GAP_CONN_MODE_UND;
    adv_params.disc_mode = BLE_GAP_DISC_MODE_GEN;
    adv_params.itvl_min = BLE_GAP_ADV_ITVL_MS(30); /* Recommended interval 30ms to 50ms */
    adv_params.itvl_max = BLE_GAP_ADV_ITVL_MS(50);
    
    rc = ble_gap_adv_start(
        BLE_OWN_ADDR_PUBLIC,
        nullptr,
        BLE_HS_FOREVER,
        &adv_params,
        nimble_hid_gap_event,
        nullptr
    );

    if (rc != 0) {
        ESP_LOGE(TAG, "error enabling advertisement; rc=%d\n", rc);
        return;
    }
}

static void ble_hidd_event_callback(
    void *handler_args,
    esp_event_base_t base,
    int32_t id,
    void *event_data
) {
    esp_hidd_event_t event = (esp_hidd_event_t)id;
    esp_hidd_event_data_t *param = (esp_hidd_event_data_t *)event_data;
    switch (event) {
        case ESP_HIDD_START_EVENT:
            ESP_LOGI(TAG, "START");
            esp_hid_ble_gap_adv_start();
            break;
        case ESP_HIDD_CONNECT_EVENT:
            ESP_LOGI(TAG, "CONNECT");
            break;
        case ESP_HIDD_PROTOCOL_MODE_EVENT:
            ESP_LOGI(TAG, "PROTOCOL MODE[%u]: %s", param->protocol_mode.map_index, param->protocol_mode.protocol_mode ? "REPORT" : "BOOT");
            break;
        case ESP_HIDD_CONTROL_EVENT:
            ESP_LOGI(TAG, "CONTROL[%u]: %sSUSPEND", param->control.map_index, param->control.control ? "EXIT_" : "");
            if (param->control.control) {
                // exit suspend
                mounted = true;
            } else {
                // suspend
                mounted = false;
            }
            break;
        case ESP_HIDD_OUTPUT_EVENT:
            ESP_LOGI(TAG, "OUTPUT[%u]: %8s ID: %2u, Len: %d, Data:", param->output.map_index, esp_hid_usage_str(param->output.usage), param->output.report_id, param->output.length);
            ESP_LOG_BUFFER_HEX(TAG, param->output.data, param->output.length);
            break;
        case ESP_HIDD_FEATURE_EVENT:
            ESP_LOGI(TAG, "FEATURE[%u]: %8s ID: %2u, Len: %d, Data:", param->feature.map_index, esp_hid_usage_str(param->feature.usage), param->feature.report_id, param->feature.length);
            ESP_LOG_BUFFER_HEX(TAG, param->feature.data, param->feature.length);
            break;
        case ESP_HIDD_DISCONNECT_EVENT:
            ESP_LOGI(TAG, "DISCONNECT: %s", esp_hid_disconnect_reason_str(esp_hidd_dev_transport_get(param->disconnect.dev), param->disconnect.reason));
            mounted = false;
            esp_hid_ble_gap_adv_start();
            break;
        case ESP_HIDD_STOP_EVENT:
            ESP_LOGI(TAG, "STOP");
            break;
        default:
            break;
    }
}

void ble_hid_device_host_task(void *param) {
    ESP_LOGI(TAG, "BLE Host Task Started");
    nimble_port_run();
    nimble_port_freertos_deinit();
}

void setup(char serial_str[17]) {
    if (active) return;

    ESP_LOGI(TAG, "Setting up BLE HID");

    ble_hid_config.serial_number = serial_str;

    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ESP_ERROR_CHECK(nvs_flash_init());
    }

    ESP_LOGI(TAG, "setting hid gap");

    /* START esp_hid_gap_init */
    /* START init_low_level */
    esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_bt_controller_mem_release(ESP_BT_MODE_CLASSIC_BT));
    ESP_ERROR_CHECK(esp_bt_controller_init(&bt_cfg));
    ESP_ERROR_CHECK(esp_bt_controller_enable(ESP_BT_MODE_BLE));
    ESP_ERROR_CHECK(esp_nimble_init());
    /* END init_low_level */
    /* END esp_hid_gap_init */

    /* START esp_hid_ble_gap_adv_init */
    memset(&fields, 0, sizeof fields);

    fields.flags =
        BLE_HS_ADV_F_DISC_GEN |
        BLE_HS_ADV_F_BREDR_UNSUP;

    ble_uuid16_t *uuid16 = (ble_uuid16_t *)malloc(sizeof(ble_uuid16_t));
    *uuid16 = BLE_UUID16_INIT(GATT_SVR_SVC_HID_UUID);
    fields.uuids16 = uuid16;
    fields.num_uuids16 = 1;
    fields.uuids16_is_complete = 1;

    fields.name = (uint8_t *)ble_hid_config.device_name;
    fields.name_len = strlen(ble_hid_config.device_name);
    fields.name_is_complete = 1;

    fields.tx_pwr_lvl = BLE_HS_ADV_TX_PWR_LVL_AUTO;
    fields.tx_pwr_lvl_is_present = 1;
    
    fields.appearance = ESP_HID_APPEARANCE_KEYBOARD;
    fields.appearance_is_present = 1;

    ble_hs_cfg.sm_io_cap = BLE_SM_IO_CAP_DISP_ONLY;
    ble_hs_cfg.sm_bonding = 1;
    ble_hs_cfg.sm_mitm = 1;
    ble_hs_cfg.sm_sc = 1;
    ble_hs_cfg.sm_our_key_dist = BLE_SM_PAIR_KEY_DIST_ID | BLE_SM_PAIR_KEY_DIST_ENC;
    ble_hs_cfg.sm_their_key_dist |= BLE_SM_PAIR_KEY_DIST_ID | BLE_SM_PAIR_KEY_DIST_ENC;
    /* END esp_hid_ble_gap_adv_init */

    ESP_LOGI(TAG, "setting ble device");
    ESP_ERROR_CHECK(
        esp_hidd_dev_init(&ble_hid_config, ESP_HID_TRANSPORT_BLE, ble_hidd_event_callback, &hid_dev)
    );

    ble_store_config_init();

    ble_hs_cfg.store_status_cb = ble_store_util_status_rr;
    ESP_ERROR_CHECK(esp_nimble_enable((void *)ble_hid_device_host_task));
    
    active = true;
    ESP_LOGI(TAG, "BLE HID setup complete");
}

void end() {
    if (!active) return;
    active = false;
    esp_hidd_dev_deinit(hid_dev);
    esp_nimble_disable();
    esp_bt_controller_disable();
    esp_bt_controller_deinit();
    ESP_LOGI(TAG, "BLE HID ended");
}

void press(const uint8_t key) {
    if (!mounted) return;
    for (auto i = 0; i < keycodes_len; i++) {
        if (keycodes[i] == key) {
            return;
        }
        if (keycodes[i] == 0) {
            keycodes[i] = key;
            break;
        }
    }
    uint8_t report[7] = {modifiers, 0};
    memcpy(&report[2], keycodes, 5);
    ESP_ERROR_CHECK(esp_hidd_dev_input_set(hid_dev, 0, 1, report, sizeof(report)));
}

void release(const uint8_t key) {
    if (!mounted) return;
    for (auto i = 0; i < keycodes_len; i++) {
        if (keycodes[i] == key) {
            keycodes[i] = 0;
        }
    }
    uint8_t report[7] = {modifiers, 0};
    memcpy(&report[2], keycodes, 5);
    ESP_ERROR_CHECK(esp_hidd_dev_input_set(hid_dev, 0, 1, report, sizeof(report)));
}

}
