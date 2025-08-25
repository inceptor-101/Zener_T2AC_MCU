#include "wifi_task.h"

void communicate_with_BLE(void){
    // BLE Notification logic for Wi-Fi
        if(!NOTIFY && notify_ready){
            notify_ready = false;                   // Prevent overlapping sends
            printf("Entered in nOTIFY \n");
            uint8_t payload[20] = {0};
            struct os_mbuf *om;
            int rc = 0;

            ESP_LOGI("notification_state", "%u", notification_state);

            switch (notification_state) {
                case SEND_NAP:
                    payload[0] = START_SCAN_DATA_FRAME;
                    payload[1] = WiFiServiceLocal.NAP_count;
                    break;

                case SEND_CONNECTION_RESULT:
                    payload[0] = CONNECTION_RESULT_DATA_FRAME;
                    payload[1] = WiFiServiceLocal.connection_result;
                    break;

                case SEND_CONNECTION_STATUS:
                    payload[0] = CONNECTION_STATUS_DATA_FRAME;
                    payload[1] = WiFiServiceLocal.connection_status;
                    if (WiFiServiceLocal.connection_status == WIFI_CONNECTED) {
                        memcpy(&payload[2], ap_info_display[0].ssid, 18);
                    }
                    break;

                default:
                    notify_ready = true;
                    break;
            }

            // Log payload
            for (uint8_t i = 0; i < 20; i++) {
                ESP_LOGI("Payload", "%u", payload[i]);
            }

            // Allocate and send BLE notification
            if (os_msys_num_free() >= MIN_REQUIRED_MBUF) {
                om = ble_hs_mbuf_from_flat(payload, sizeof(payload));
                if (om == NULL) {
                    ESP_LOGE("BLE", "No MBUFs available, retrying...");
                    vTaskDelay(100 / portTICK_PERIOD_MS);
                    om = ble_hs_mbuf_from_flat(payload, sizeof(payload));
                    assert(om != NULL);
                }

                if (notify_wifi_state) {
                    notify_wifi_state = false;
                    int max_retries = 3;
                    int retry_delay_ms = 50;
                    bool sent_successfully = false;

                    for (int i = 0; i < max_retries && !sent_successfully; i++) {
                    if (notification_state != 0) {
        
                        if (notify_indicate_enabled) {
                            notify_indicate_enabled = false;
                            rc = ble_gatts_indicate_custom(conn_handle, notify_wifi_handle, om);
                            ESP_LOGI("BLE", "Sent via INDICATE (attempt %d)", i + 1);
                        } 
                    } 

                    if (rc == 0) {
                        sent_successfully = true;
                    } else if (rc == BLE_HS_EBUSY) {
                        ESP_LOGW("BLE", "Stack busy, retrying in %dms...", retry_delay_ms);
                        vTaskDelay(retry_delay_ms / portTICK_PERIOD_MS);
                        retry_delay_ms *= 2; // Exponential backoff
                    } else {
                        ESP_LOGE("BLE", "Fatal error: rc=%d", rc);
                        break;
                    }
                }
                if (!sent_successfully) {
                    ESP_LOGE("BLE", "Failed after %d retries", max_retries);
                }
            }
            } else {
                ESP_LOGE("BLE", "Insufficient MBUFs!");
                notify_ready = true;
            }
        
            notification_state = NOT_SET;
        }
        else{
            return;
        }
}