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
const char *WIFI_TAG = "Wi-Fi";
const char *MQTT_TAG = "MQTT";
bool wifiConnected = false;

typedef struct {
    char topic[60];
    char data[500];
} mqtt_message_t;

void log_error_if_nonzero(const char *message, int error_code)
{
    if (error_code != 0) {
        ESP_LOGE(ESP_TAG, "Last error %s: 0x%x", message, error_code);
    }
}

void wifi_event_handler(void* handler_args, esp_event_base_t base, int32_t id, void* event_data)
{
    ESP_LOGI(ESP_TAG, "WiFi event_handler: %s:%" PRIi32, base, id);

    if(base == WIFI_EVENT) {
        switch(id) {
        case WIFI_EVENT_STA_START:
            //Here the Wifi Has been initialised 
            wifiInitialised = true;
            ESP_LOGI(WIFI_TAG,"Wi-Fi in STA Mode started");
            break;
        case WIFI_EVENT_STA_CONNECTED:
            ESP_LOGI(WIFI_TAG,"Wi-Fi connected");
            WiFiServiceLocal.connection_result = WIFI_CONNECTED;
            wifi_connected=true;  //Used for MQTT
            notification_state = SEND_CONNECTION_RESULT;
            notify_indicate_enabled = true;
            notify_wifi_state = true;
            break;
        case WIFI_EVENT_STA_DISCONNECTED:
            /* This is a workaround as ESP32 WiFi libs don't currently
            auto-reassociate. */
            ESP_LOGI(WIFI_TAG,"Wi-Fi lost connection...");
            wifiConnected = false;
            if(WiFiServiceLocal.retry_num<5)
            {
                esp_wifi_connect();WiFiServiceLocal.retry_num++;ESP_LOGI(WIFI_TAG,"Wi-Fi trying to reconnect...");
            }
            if(WiFiServiceLocal.retry_num == 5){
                WiFiServiceLocal.connection_result = WIFI_DISCONNECTED;
                notification_state = SEND_CONNECTION_RESULT;
                notify_indicate_enabled = true;
                notify_wifi_state = true;
            }
            break;
        default:
            break;
        }
    }
    else if(base == IP_EVENT) {
        switch(id) {
        case IP_EVENT_STA_GOT_IP:
            {
                ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
                ESP_LOGI(WIFI_TAG, "got ip:" IPSTR, IP2STR(&event->ip_info.ip));
                xEventGroupSetBits(wifi_event_group, CONNECTED_BIT);
                wifiConnected = true;
            }
        }
    }
}

/// @brief Initializes the ESP32 Wi-Fi in Station (STA) mode.
/// @param None
void initialise_wifi(void)
{
    //Creates an event group used for inter-task signaling (e.g., signaling when Wi-Fi is connected or disconnected).
    wifi_event_group = xEventGroupCreate();
    //Initializes the TCP/IP stack (esp-netif) — mandatory before using any networking features.
    ESP_ERROR_CHECK(esp_netif_init());
    //Sets up the default event loop to handle system events like WIFI_EVENT and IP_EVENT.
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    //Creates a default Wi-Fi station interface. This is where the ESP32 will connect to a Wi-Fi network.
    esp_netif_create_default_wifi_sta();    
    //Register handler for Wi-Fi and IP events
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL, NULL));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
                                                        IP_EVENT_STA_GOT_IP,
                                                        &wifi_event_handler,
                                                        NULL,
                                                        NULL));

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK( esp_wifi_init(&cfg) );
    //Choose storage for Wi-Fi credentials
    ESP_ERROR_CHECK( esp_wifi_set_storage(WIFI_STORAGE_RAM));
    ESP_ERROR_CHECK( esp_wifi_set_mode(WIFI_MODE_STA) );
    ESP_ERROR_CHECK( esp_wifi_start() );
}

void wifi_get_ap(){
    memset(ap_info_display, 0, sizeof(ap_info_display));
    esp_wifi_sta_get_ap_info(ap_info_display);
    ESP_LOGI(WIFI_TAG, "Displaying connected to SSID....");    
    ESP_LOGI(WIFI_TAG, "SSID \t\t%s", ap_info_display[0].ssid);
    ESP_LOGI(WIFI_TAG, "RSSI \t\t%d", ap_info_display[0].rssi);
    ESP_LOGI(WIFI_TAG, "Channel \t\t%d\n", ap_info_display[0].primary);

    if(ap_info_display[0].ssid[0] != 0)
        WiFiServiceLocal.connection_status = WIFI_CONNECTED;
    else
        WiFiServiceLocal.connection_status = WIFI_DISCONNECTED;
    
    notification_state = SEND_CONNECTION_STATUS;
    notify_wifi_state = true;
    notify_indicate_enabled = true;
    printf("Value of notify_wfi_state 05 %d \n",notify_wifi_state);
    printf("Value of notify_wfi_chr 05 %d \n",NOTIFY_WIFI_CHAR);
    printf("Value of notification_state 05 %d \n",notification_state);
}

void wifi_connect(uint8_t* ssid, uint8_t* password) {
    // Validate inputs
    if (ssid == NULL || password == NULL) {
        ESP_LOGE("WiFi", "SSID or password is NULL!");
        return;
    }

    // Debug: Print received SSID and password
    ESP_LOGI("WiFi", "Attempting to connect to:");
    ESP_LOGI("WiFi", "SSID: %s",ssid);
    ESP_LOGI("WiFi", "Password: %s",password);

    ESP_LOG_BUFFER_HEXDUMP("WiFi", password, strlen((char*)password), ESP_LOG_INFO); // Hex dump for non-printable chars

    // Initialize Wi-Fi config
    wifi_config_t wifi_config = {0};

    // Copy SSID (ensure null-termination)
    strncpy((char*)wifi_config.sta.ssid, (char*)ssid, sizeof(wifi_config.sta.ssid) - 1);
    wifi_config.sta.ssid[sizeof(wifi_config.sta.ssid) - 1] = '\0';

    // Copy password (ensure null-termination)
    strncpy((char*)wifi_config.sta.password, (char*)password, sizeof(wifi_config.sta.password) - 1);
    wifi_config.sta.password[sizeof(wifi_config.sta.password) - 1] = '\0';

    // Disconnect first (if already connected)
    esp_wifi_disconnect();

    // Set new config
    esp_err_t ret = esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config);
    if (ret != ESP_OK) {
        ESP_LOGE("WiFi", "Failed to set config: %s", esp_err_to_name(ret));
        return;
    }

    // Connect
    ret = esp_wifi_connect();
    if (ret != ESP_OK) {
        ESP_LOGE("WiFi", "Failed to connect: %s", esp_err_to_name(ret));
    } else {
        ESP_LOGI("WiFi", "Connection attempt started");
    }

    //Copy to global variables
    strncpy(connected_ssid, (char*)ssid, sizeof(connected_ssid)-1);
    connected_ssid[sizeof(connected_ssid)-1] = '\0';

    strncpy(connected_password, (char*)password, sizeof(connected_password)-1);
    connected_password[sizeof(connected_password)-1] = '\0';

    save_wifi_credentials_nvs(connected_ssid, connected_password);
    // Clear sensitive data (optional)
    memset(&wifi_config.sta.password, 0, sizeof(wifi_config.sta.password));
}

void copy_ssids(uint8_t* from, uint8_t* to, uint8_t len){
    for(uint8_t i=0; i<len; i++){
        to[i] = from[i];
    }
}

void wifi_scan(void)
{
    //static const char *TAG = "scan";
    ssid_index = 0;
    uint16_t number = DEFAULT_SCAN_LIST_SIZE;
    uint16_t ap_count = 0;
    memset(ap_info, 0, sizeof(ap_info));
    esp_wifi_scan_start(NULL, true);
    ESP_ERROR_CHECK(esp_wifi_scan_get_ap_num(&ap_count));
    ESP_ERROR_CHECK(esp_wifi_scan_get_ap_records(&number, ap_info));
    ESP_LOGI(WIFI_TAG, "Total APs scanned = %u", ap_count);
    if(ap_count > 20)
        WiFiServiceLocal.NAP_count = 20;
    else
        WiFiServiceLocal.NAP_count = (uint8_t)ap_count;
    notification_state = SEND_NAP;
    notify_indicate_enabled = true;
    notify_wifi_state = true;
    printf("Value of notify_wfi_state D %d \n",notify_wifi_state);
    printf("Value of notify_wfi_chr D %d \n",NOTIFY_WIFI_CHAR);
    //printf("Value of Notification State %d \n",notification_state);
    for (int i = 0; (i < DEFAULT_SCAN_LIST_SIZE) && (i < ap_count); i++) 
    {
        printf(" Debug1 \n");
        copy_ssids(ap_info[i].ssid, Networks[i].ssid, 20);
        ESP_LOGI(WIFI_TAG, "SSID \t\t%s", Networks[i].ssid);
        ESP_LOGI(WIFI_TAG, "RSSI \t\t%d", ap_info[i].rssi);
        ESP_LOGI(WIFI_TAG, "Channel \t\t%d\n", ap_info[i].primary);
    }
}

bool check_internet_status() {
    esp_err_t err = ESP_OK;
    esp_http_client_config_t config = {
        .url = "http://www.google.com",
    };

    esp_http_client_handle_t client = esp_http_client_init(&config);
    esp_http_client_set_method(client, HTTP_METHOD_GET);
    err = esp_http_client_perform(client);
    if (err == ESP_OK) {
        internet_connected = true;
        //gpio_set_level(INTERNET_LED, 1); // Turn on LED if internet is connected
        //ESP_LOGI(WIFI_TAG, "Internet is connected");
    } else {
        internet_connected = false;
        //gpio_set_level(INTERNET_LED, 0);
        //ESP_LOGE(WIFI_TAG, "Internet is disconnected");
    }
    esp_http_client_cleanup(client);
    return internet_connected;
}

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
        
        // 🔽 Add a small delay at the end to yield CPU just in case
        vTaskDelay(10 / portTICK_PERIOD_MS);
    }
}
