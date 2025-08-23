#include <stdio.h>
#include "bleFunction.h"

const char *BLE_TAG = "BLE_SERVICE";

static uint8_t g_own_addr_type;
bool ble_is_connected = false;
bool chargingStatusNotifyEnabled = false;

//Variables for WiFi Service 
uint8_t password_index = 0;
uint8_t ssid_index = 0;
uint8_t notification_state = NOT_SET;
bool notify_wifi_state = false;
bool notify_indicate_enabled = false;
wifi_ap_record_t ap_info_display[1];
uint16_t conn_handle;
uint16_t notify_wifi_handle;
bool notify_ready = true;
float power = 0.0f;

//Variables for Restricted Access Service 
uint16_t notify_connection_Config_handle;
uint16_t notify_first_time_handle;
uint16_t notify_security_setting_handle;

bool notify_connection_Config_signal = false;
bool notify_first_time_signal = false;
bool notify_security_setting_signal = false;

WiFiServiceLocalStruct WiFiServiceLocal ={
    .NAP_count = 0,
    .retry_num = 0,
    .connection_result = 0,
    .connection_status = 0,
};

static void ble_app_on_sync(void);

/**
 * @brief BLE GATT service and characteristic table used for NimBLE registration.
 *
 * Defines four primary services: Wi-Fi Service, Restricted-Access Service,
 * Boot-Up Service, and Charging-Session Service. Each service exposes multiple
 * characteristics with associated UUIDs, access callbacks, notification handles,
 * and characteristic-level permissions such as READ/WRITE/NOTIFY/INDICATE.
 * The `{0}` entries at the end of each characteristic array and at the end of
 * the service array act as list terminators as required by NimBLE.
 */
static struct ble_gatt_svc_def ble_service[] = {
    {
        /* Service 1: Wi-Fi Service */
        .type = BLE_GATT_SVC_TYPE_PRIMARY,
        .uuid = UUID_DECLARE(WIFI_SERVICE),
        .characteristics = (struct ble_gatt_chr_def[]) {
            {
                .uuid = UUID_DECLARE(DATA_EXCHANGE_CHR),
                .access_cb = wifi_svr,
                .val_handle = &notify_wifi_handle,
                .flags = BLE_GATT_CHR_F_READ |
                         BLE_GATT_CHR_F_WRITE |
                         BLE_GATT_CHR_F_INDICATE,
            },
            {0} // ✅ Add this line to terminate the characteristics list
        },
    },
    {
        /* Service 2: Restricted Access */
        .type = BLE_GATT_SVC_TYPE_PRIMARY,
        .uuid = UUID_DECLARE(RESTRICTED_ACCESS_SERVICE),
        .characteristics = (struct ble_gatt_chr_def[]){
            {
                .uuid = UUID_DECLARE(SERVER_SETTINGS_CHR),
                .access_cb = restricted_access_svr,
                .val_handle = &notify_connection_Config_handle,
                .flags = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_WRITE | BLE_GATT_CHR_F_NOTIFY,
            },
            {
                .uuid = UUID_DECLARE(FIRST_TIME_CHR),
                .access_cb = restricted_access_svr,
                .val_handle = &notify_first_time_handle,
                .flags = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_WRITE | BLE_GATT_CHR_F_NOTIFY,
            },
            {
                .uuid = UUID_DECLARE(SECURITY_SETTING_CHR),
                .access_cb = restricted_access_svr,
                .val_handle = &notify_security_setting_handle,
                .flags = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_WRITE | BLE_GATT_CHR_F_NOTIFY,
            },
            {0} // ✅ Add this line to terminate the characteristics list
        }
    },
    {0} // <-- Also terminate the service list!
};

/**
 * @brief FreeRTOS task entry function for running the NimBLE host stack.
 *
 * Continuously executes the BLE host scheduler loop via `nimble_port_run()`,
 * which handles GAP/GATT processing and connection management. This call
 * normally never returns while BLE is active. If it does return (e.g. during
 * a stack shutdown or reset), the task performs cleanup by calling
 * `nimble_port_freertos_deinit()` to free associated NimBLE resources.
 */
void ble_host_task(void *param) {
    nimble_port_run(); // This function will not return - this task keeps running as long as BLE is active
    nimble_port_freertos_deinit(); // Called only if nimble_port_run() returns, performs clean-up during shutdown/reset
}

/**
 * @brief NimBLE GAP event callback to handle all BLE connection-level events.
 *
 * Responds to GAP events such as connection establishment, disconnection,
 * client subscriptions on GATT characteristics, MTU negotiation, and notification
 * transmission results.  Maintains connection state, tracks which characteristics
 * have notifications/indications enabled, and provides basic flow-control for GATT
 * notifications using the `notify_ready` flag.
 *
 * @param event Pointer to GAP event structure containing event-specific information
 * @param arg   Optional argument passed from the GAP start function (unused here)
 * @return Always returns 0 to indicate event was handled
 */
static int ble_gap_event(struct ble_gap_event *event, void *arg) {
    switch (event->type) {
        case BLE_GAP_EVENT_CONNECT:
            ESP_LOGI(BLE_TAG, "BLE connected.");
            notify_ready = true;
            ble_is_connected = true;
            conn_handle = event->connect.conn_handle;
            global_conn_handle = event->connect.conn_handle;
            ESP_LOGI("BLE", "Connected, handle: %d", conn_handle);
            break;

        case BLE_GAP_EVENT_DISCONNECT:
            ble_is_connected = false;
            conn_handle = 0xFFFF;
            global_conn_handle = BLE_HS_CONN_HANDLE_NONE;
            ESP_LOGI(BLE_TAG, "BLE disconnected and handle: %d", conn_handle);
            ble_app_on_sync();
            break;

        case BLE_GAP_EVENT_SUBSCRIBE:
            ESP_LOGI(BLE_TAG, "Subscribe event: notify=%d, indicate=%d, handle=%d",
            event->subscribe.cur_notify, event->subscribe.cur_indicate,
            event->subscribe.attr_handle);

                if (event->subscribe.attr_handle == notify_wifi_handle) {
                    notify_wifi_state = event->subscribe.cur_notify || event->subscribe.cur_indicate;
                    notify_indicate_enabled = event->subscribe.cur_indicate;
                    if (arg != NULL) {
                        ESP_LOGI(BLE_TAG, "notify test time = %d", *(int *)arg);
                    }
                }
                else if (event->subscribe.attr_handle == notify_connection_Config_handle) {
                    notify_connection_Config_signal = event->subscribe.cur_notify;
                    if (arg != NULL) {
                        ESP_LOGI(BLE_TAG, "notify test time = %d", *(int *)arg);
                    }
                }
            break;
        
        case BLE_GAP_EVENT_NOTIFY_TX:
            // //ESP_LOGI(BLE_TAG, "Notify TX for handle=%d, status=%d", 
            // event->notify_tx.attr_handle, event->notify_tx.status);
            if (event->notify_tx.status == 0 || event->notify_tx.status == 14) 
            {
                notify_ready = true;
                if(event->notify_tx.status == 14)
                {
                    ESP_LOGI(BLE_TAG,"ACK Received");
                }
                else
                    ESP_LOGI(BLE_TAG," ");
            } else {
                ESP_LOGE(BLE_TAG, "Notify TX failed: status = %d", event->notify_tx.status);
                notify_ready = true;
            }
            break;
          
        case BLE_GAP_EVENT_MTU:
            ESP_LOGI(BLE_TAG, "Negotiated MTU: %d", event->mtu.value);
            break;

        default:
            break;
    }
    return 0;
}

/**
 * @brief Formats a shorter scan response name from a full serial number.
 *
 * Takes the first three characters and the last eight characters of the provided
 * serial number to create an 11-character truncated identifier for use in the
 * BLE Scan Response packet. Returns NULL if the serial number is too short (<5).
 *
 * @param serialNumber Full serial number string.
 * @return Pointer to a static formatted scan response name string, or NULL on error.
 */
char* format_scan_rsp_name(const char* serialNumber) {
    static char scan_name[15];  // Max 14 characters + null terminator
    size_t serial_len = strlen(serialNumber);

    if (serial_len < 5) {
        // Needs at least 5 characters
        return NULL;
    }

    // Copy first 3 characters
    memcpy(scan_name, serialNumber, 3);

    // Copy last 8 characters
    memcpy(&scan_name[3], &serialNumber[serial_len - 8], 8);

    // Null-terminate
    scan_name[11] = '\0';

    return scan_name;
}

/**
 * @brief Configure and start BLE advertising with custom manufacturer data and scan response.
 *
 * Populates the advertisement packet with manufacturer-specific data
 * containing the device’s serial number, and sets BLE advertising flags.
 * The scan response is configured with a formatted, truncated device name 
 * (based on serial number) to be visible to BLE scanners. 
 * Starts undirected connectable advertising with the specified
 * local address type and registers the GAP event handler for runtime callbacks.
 * @param own_addr_type  BLE address type to use for advertising (public/random).
 */
void ble_app_advertise(uint8_t own_addr_type) {
    struct ble_hs_adv_fields adv_fields;
    struct ble_hs_adv_fields scan_rsp_fields;
    memset(&adv_fields, 0, sizeof(adv_fields));
    memset(&scan_rsp_fields, 0, sizeof(scan_rsp_fields));

    // ✅ Manufacturer Data (Full Serial)
    static uint8_t mfg_data[2 + 32];  // 2 for ID + serial
    size_t serial_len = strlen(firstTimeConfigData.serialNumber);
    mfg_data[0] = 0xFF;  // Dummy manufacturer ID LSB
    mfg_data[1] = 0xFF;  // MSB
    memcpy(&mfg_data[2], firstTimeConfigData.serialNumber, serial_len);
    adv_fields.mfg_data = mfg_data;
    adv_fields.mfg_data_len = 2 + serial_len;

    // ✅ Flags
    adv_fields.flags = BLE_HS_ADV_F_DISC_GEN | BLE_HS_ADV_F_BREDR_UNSUP;

    char *formatted_name = format_scan_rsp_name(firstTimeConfigData.serialNumber);
    if (formatted_name == NULL) {    
        ESP_LOGE("BLE", "Serial number too short for scan response name");
        return;
    }
    // ✅ Device Name in Scan Response (truncated)
    scan_rsp_fields.name = (uint8_t *)formatted_name;
    scan_rsp_fields.name_len = strlen(formatted_name);
    scan_rsp_fields.name_is_complete = 1;

    // Set advertisement and scan response
    int rc = ble_gap_adv_set_fields(&adv_fields);
    if (rc != 0) {
        ESP_LOGE("BLE", "Failed to set advertisement data: %d", rc);
        return;
    }

    rc = ble_gap_adv_rsp_set_fields(&scan_rsp_fields);
    if (rc != 0) {
        ESP_LOGE("BLE", "Failed to set scan response data: %d", rc);
        return;
    }

    // Start advertising
    struct ble_gap_adv_params adv_params = {
        .conn_mode = BLE_GAP_CONN_MODE_UND,
        .disc_mode = BLE_GAP_DISC_MODE_GEN,
    };

    rc = ble_gap_adv_start(own_addr_type, NULL, BLE_HS_FOREVER, &adv_params, ble_gap_event, NULL);
    if (rc != 0) {
        ESP_LOGE("BLE", "Advertising start failed: %d", rc);
    } else {
        ESP_LOGI("BLE", "Advertising started");
        // ESP_LOGI("BLE", "Adv MfgData: %s", firstTimeConfigData.serialNumber);
        // ESP_LOGI("BLE", "ScanResp Name: %.14s", firstTimeConfigData.serialNumber);
    }
}

/**
 * @brief Callback invoked once the NimBLE host and controller are synchronized.
 *
 * Sets the preferred GATT ATT MTU size, determines the proper public/random
 * address to use, prints the device’s BLE MAC address, and then starts
 * advertising using the resolved local address type.
 * This function is typically registered as `ble_hs_cfg.sync_cb`.
 */
static void ble_app_on_sync(void) {
    
    ESP_LOGI("BLE", "BLE synced. Setting preferred MTU...");
    ble_att_set_preferred_mtu(185); // Safe to call here

    int rc = ble_hs_id_infer_auto(0, &g_own_addr_type);
    if (rc != 0) {
        ESP_LOGE("BLE", "ble_hs_id_infer_auto failed: %d", rc);
        return;
    }

    uint8_t addr_val[6];
    ble_hs_id_copy_addr(g_own_addr_type, addr_val, NULL);  // ✅ This fills addr_val with correct MAC

    ESP_LOGI("BLE", "BLE address: %02X:%02X:%02X:%02X:%02X:%02X",
             addr_val[5], addr_val[4], addr_val[3],
             addr_val[2], addr_val[1], addr_val[0]);

    ble_app_advertise(g_own_addr_type);  // ✅ pass correct address type
}

/**
 * @brief Initializes the NimBLE stack, GATT services, and launches the BLE host task.
 *
 * Performs NimBLE host initialization, sets the sync callback, initializes the GAP
 * and GATT standard services, assigns the device name, registers the application’s
 * custom GATT services defined in `ble_service[]`, and finally starts the host stack
 * scheduler loop on a dedicated FreeRTOS task via `nimble_port_freertos_init()`.
 */
void ble_init(void)
{
    nimble_port_init();
    ble_hs_cfg.sync_cb = ble_app_on_sync;

    ble_svc_gap_init();
    ble_svc_gatt_init();
    ble_svc_gap_device_name_set(firstTimeConfigData.serialNumber);

    ble_gatts_count_cfg(ble_service);
    ble_gatts_add_svcs(ble_service);

    nimble_port_freertos_init(ble_host_task);
}
