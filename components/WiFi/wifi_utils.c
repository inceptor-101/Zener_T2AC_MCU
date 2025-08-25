#include "esp_wifi.h"
#include "esp_event.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_log.h"
#include <string.h>
#include "driver/gpio.h"
#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "host/ble_hs.h"
#include "host/util/util.h"
#include "console/console.h"
#include "services/gap/ble_svc_gap.h"
#include "../src/ble_hs_hci_priv.h"
#include <inttypes.h>
#include <esp_http_client.h>
#include "nvs.h"
#include "nvs_flash.h"
//Including user defined library
#include "../../main/main.h"

NetworkStruct Networks[20];

//Defining Variables to Store Topics that Charger Has Subscribed to 
char sub_topic1[65];

//Handler for Notify Task
TaskHandle_t process_MQTT_msg_handler = NULL;
wifi_ap_record_t ap_info[DEFAULT_SCAN_LIST_SIZE];
esp_mqtt_client_handle_t mqtt_client = NULL;

/* FreeRTOS event group to signal when we are connected & ready to make a request */
EventGroupHandle_t wifi_event_group;

/* The event group allows multiple bits for each event,
   but we only care about one event - are we connected
   to the AP with an IP? */
const int CONNECTED_BIT = BIT0;
const char *ESP_TAG = "ESP32";
const char *WIFI_TAG = "WiFi";
const char *MQTT_TAG = "MQTT";
bool wifiConnected = false;

typedef struct {
    char topic[60];
    char data[500];
} mqtt_message_t;

void wifi_get_ap(){
    memset(ap_info_display, 0, sizeof(ap_info_display));    // Initialising the structure to store the access points
    esp_wifi_sta_get_ap_info(ap_info_display);

    // Required the SSID, RSSI and Channel of the access point fetched 
    ESP_LOGI(WIFI_TAG, "Displaying connected to SSID....");    
    ESP_LOGI(WIFI_TAG, "SSID \t\t%s", ap_info_display[0].ssid);
    ESP_LOGI(WIFI_TAG, "RSSI \t\t%d", ap_info_display[0].rssi);
    ESP_LOGI(WIFI_TAG, "Channel \t\t%d\n", ap_info_display[0].primary);

    if(ap_info_display[0].ssid[0] != 0){
        WiFiServiceLocal.connection_status = WIFI_CONNECTED;
    }else{
        WiFiServiceLocal.connection_status = WIFI_DISCONNECTED;
    }
    
    //For communicating the same to the mobile app via BLE
    notification_state = SEND_CONNECTION_STATUS;
    notify_wifi_state = true;
    notify_indicate_enabled = true;
    printf("Value of notify_wfi_state 05 %d \n",notify_wifi_state);
    printf("Value of notify_wfi_chr 05 %d \n",NOTIFY_WIFI_CHAR);
    printf("Value of notification_state 05 %d \n",notification_state);
}

void copy_ssids(uint8_t* from, uint8_t* to, uint8_t len){
    for(uint8_t i=0; i<len; i++){
        to[i] = from[i];
    }
}

bool check_internet_status() { // Doing the google ping for the internet connection checking 
    esp_err_t err = ESP_OK;
    esp_http_client_config_t config = { // adding of counter is needed asap
        .url = "http://www.google.com",
    };

    esp_http_client_handle_t client = esp_http_client_init(&config);
    esp_http_client_set_method(client, HTTP_METHOD_GET);
    err = esp_http_client_perform(client);
    internet_connected = ((err == ESP_OK) ? true : false);
    esp_http_client_cleanup(client);
    return internet_connected;
}

// This function is deprecated and will be removed 
void timer_task_1(void *pvParameters) {
    while (true) {
        // Wait for notification from timer callback
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

        // Check internet status if Wi-Fi is connected
        if(wifiConnected)
            check_internet_status();

        // Handle Wi-Fi service states
        if (WifiService.LAP == ENABLE) {
            wifi_scan();
            WifiService.LAP = DISABLE;
        } else if (WifiService.CAP == ENABLE) {
            wifi_connect(ap_info[WifiService.SSID_No - 1].ssid, WifiService.password_received);
            WifiService.CAP = DISABLE;
        } else if (WifiService.GAP == ENABLE) {
            wifi_get_ap();
            WifiService.GAP = DISABLE;
        }
        
        // 🔽 Add a small delay at the end to yield CPU just in case
        vTaskDelay(10 / portTICK_PERIOD_MS);
    }
}
