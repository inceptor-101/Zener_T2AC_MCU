#include "wifi_task.h"
// Custom Data Structures 
WIFI_STATES curr_wifi_state = BOOT_UP;

//Main Task
void wifi_task_core0(void* pvParameters) {
    initialise_wifi();
    int scan_retry = 0;
    int connect_retry = 0;
    const int MAX_SCAN_RETRY = 3;
    const int MAX_CONNECT_RETRY = 3;

    while (1) {
        // ---------- Check BLE Commands ----------
        if (WifiService.LAP == ENABLE) {
            curr_wifi_state = WIFI_SCANNING;
            WifiService.LAP = DISABLE;
        } 
        else if (WifiService.CAP == ENABLE) {
            curr_wifi_state = CHANGING_CONNECTION;
            WifiService.CAP = DISABLE;
        } 
        else if (WifiService.GAP == ENABLE) {
            wifi_get_ap();
            WifiService.GAP = DISABLE;
        }

        // ---------- State Machine ----------
        switch (curr_wifi_state) {
            case BOOT_UP: {
                ESP_LOGI("WiFi", "BOOT_UP_MODE");
                wifi_connect(connected_ssid, connected_password);
             
                if (wifiConnected && internet_connected) {
                    curr_wifi_state = CONNECTION_EST;
                    connect_retry = 0;
                }else {
                    connect_retry++;
                    if (connect_retry >= MAX_CONNECT_RETRY) {
                        curr_wifi_state = WIFI_SCANNING;
                        connect_retry = 0;
                    }
                }
                break;
            }

            case CONNECTION_EST: {
                ESP_LOGI("WiFi", "CONNECTION ESTABLISHED");
                if (!wifiConnected || !internet_connected) {
                    curr_wifi_state = NO_INTERNET;
                }
                break;
            }

            case NO_INTERNET: {
                ESP_LOGW("WiFi", "NO INTERNET.");
                
                if (wifiConnected && internet_connected) {
                    curr_wifi_state = CONNECTION_EST;
                    connect_retry = 0;
                }else {
                connect_retry++;
                if (connect_retry >= MAX_CONNECT_RETRY) {
                    curr_wifi_state = WIFI_SCANNING;
                    connect_retry = 0;
                }
            }
            break;
            }

            case CHANGING_CONNECTION: {
                wifi_connect(ap_info[WifiService.SSID_No - 1].ssid, WifiService.password_received);
                if (internet_connected && wifiConnected){
                    curr_wifi_state = CONNECTION_EST;
                }
                break;
            }

            case WIFI_SCANNING: {
                ESP_LOGI("WiFi", "SCANNING");
                wifi_scan();
                if (WiFiServiceLocal.NAP_count > 0) {
                    curr_wifi_state = BOOT_UP; // Now try connecting
                    scan_retry = 0;
                } else {
                    scan_retry++;
                    if (scan_retry >= MAX_SCAN_RETRY) {
                        ESP_LOGE("WiFi", "NO APs FOUND");
                        scan_retry = 0;
                        curr_wifi_state = NO_INTERNET;
                    }
                }
                break;
            }
        }

        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
