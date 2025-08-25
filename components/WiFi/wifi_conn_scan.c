#include "wifi_task.h"
uint8_t scan_task_started = 0;
uint16_t number = 0;
uint16_t ap_count = 0;

void wifi_scan(void)
{
    //Reinitialising and declaring global variables variables 
    ssid_index = 0;
    number = DEFAULT_SCAN_LIST_SIZE;
    ap_count = 0;
    memset(ap_info, 0, sizeof(ap_info)); // ap_info is an array of wifi_ap_records_t structures

    //Starting the wi-fi scan and the following functions fetches the access points 
    esp_wifi_scan_start(NULL, false); // Non blocking task is created 
}

void wifi_connect(uint8_t* ssid, uint8_t* password) {
    if ((ssid == NULL) || (password == NULL)){
        ESP_LOGE(WIFI_TAG, "Got SSID or PASSWORD value as NULL");
        return;
    }

   //Logging SSID and PASSWORD
    ESP_LOGI(WIFI_TAG, "Detected:\tSSID: %s\tPASSWORD: %s", (char*)ssid, (char*)password);
    ESP_LOG_BUFFER_HEXDUMP("WiFi", password, strlen((char*)password), ESP_LOG_INFO); // Hex dump for non-printable chars
    ESP_LOGI(WIFI_TAG, "Establishing Connection...");

    // This structure is used for loading the new ssid and password
    wifi_config_t wifi_config = {0};

    // Copy SSID (ensure null-termination)
    strncpy((char*)wifi_config.sta.ssid, (char*)ssid, sizeof(wifi_config.sta.ssid) - 1);
    wifi_config.sta.ssid[sizeof(wifi_config.sta.ssid) - 1] = '\0';

    // Copy password (ensure null-termination)
    strncpy((char*)wifi_config.sta.password, (char*)password, sizeof(wifi_config.sta.password) - 1);
    wifi_config.sta.password[sizeof(wifi_config.sta.password) - 1] = '\0';

    // Disconnect from previous AP......if connected
    esp_wifi_disconnect();

    // Set new config
    esp_err_t return_val = esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config);
    if (return_val != ESP_OK){ESP_LOGE(WIFI_TAG, "Could not set configuration");};

    // Starting the connection attempt 
    return_val = esp_wifi_connect(); // This function is used to establish the wifi connection
    if (return_val != ESP_OK){ESP_LOGE(WIFI_TAG, "Could not connect but config was right");};

    ESP_LOGI(WIFI_TAG, "Connection attempt started successful. Waiting for connecting event...");

    //Copy to global variables
    strncpy(connected_ssid, (char*)ssid, sizeof(connected_ssid)-1);
    connected_ssid[sizeof(connected_ssid)-1] = '\0';

    strncpy(connected_password, (char*)password, sizeof(connected_password)-1);
    connected_password[sizeof(connected_password)-1] = '\0';

    save_wifi_credentials_nvs(connected_ssid, connected_password);
    // Clear sensitive data (optional)
    memset(&wifi_config.sta.password, 0, sizeof(wifi_config.sta.password));
}