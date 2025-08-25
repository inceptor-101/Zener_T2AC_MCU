#include "wifi_task.h"

void wifi_event_handler(void* handler_args, esp_event_base_t base, int32_t event_occured, void* event_data)
{
    if(base == WIFI_EVENT) {
        switch(event_occured) {

            case WIFI_EVENT_STA_START:{
                ESP_LOGI(WIFI_TAG,"WiFi Initialised");  //WiFi has been initialised
                wifi_initialised = true;
                break;
            }
                
            case WIFI_EVENT_STA_CONNECTED:{
                ESP_LOGI(WIFI_TAG,"Successfully connected to Wi-Fi");
                //wifi_connected=true;  //Used for MQTT

                // For figuring out the reason for the disconnection
                wifi_event_sta_disconnected_t* reason_struct = (wifi_event_sta_disconnected_t*) event_data;

                //In the reason struct we can access the reason behind the disconnection 
                //Used for sending data over BLE
                notification_state = SEND_CONNECTION_RESULT;
                notify_indicate_enabled = true;
                notify_wifi_state = true;

                break;
            }

            case WIFI_EVENT_SCAN_DONE:{
                ESP_ERROR_CHECK(esp_wifi_scan_get_ap_num(&ap_count));
                ESP_ERROR_CHECK(esp_wifi_scan_get_ap_records(&number, ap_info));


                ESP_LOGI(WIFI_TAG, "APs Scanned = %u. Check out them below...", ap_count); // For testing purposes 

                for (int i = 0; (i < DEFAULT_SCAN_LIST_SIZE) && (i < ap_count); i++) 
                {
                    copy_ssids(ap_info[i].ssid, Networks[i].ssid, 20); //Maintaing a copy of fetched APs
                    ESP_LOGI(WIFI_TAG, "SSID \t\t%s", Networks[i].ssid);
                    ESP_LOGI(WIFI_TAG, "RSSI \t\t%d", ap_info[i].rssi);
                    ESP_LOGI(WIFI_TAG, "Channel \t\t%d\n", ap_info[i].primary);
                }

                // WiFiServiceLocal.NAP_count -> Number of access points found during scan
                WiFiServiceLocal.NAP_count = ((ap_count > 20) ? 20 : (uint8_t)ap_count);

                //Flags used the send the above data via BLE to the mobile application
                notification_state = SEND_NAP;
                notify_indicate_enabled = true;
                notify_wifi_state = true;
                ESP_LOGI(WIFI_TAG, "Enabled variables for sending APs via BLE....");

                // Getting ready for the next scan 
                scan_task_started = 0;
            }

            case WIFI_EVENT_STA_DISCONNECTED:{
                wifi_connected = false;
                ESP_LOGI(WIFI_TAG,"Could not connect to WiFi");
                WiFiServiceLocal.connection_result = WIFI_DISCONNECTED;
                could_not_connect = true;

                //Used for the BLE configuration
                notification_state = SEND_CONNECTION_RESULT;
                notify_indicate_enabled = true;
                notify_wifi_state = true;
                break;
            }

            case WIFI_EVENT_STA_BEACON_TIMEOUT: {
                wifi_connected = false;
                ESP_LOGI(WIFI_TAG,"Beacon timeout, connection lost");
                WiFiServiceLocal.connection_result = WIFI_DISCONNECTED;
                could_not_connect = true;

                //Used for the BLE configuration
                notification_state = SEND_CONNECTION_RESULT;
                notify_indicate_enabled = true;
                notify_wifi_state = true;
                break;                
            }

            default:{
                break;
            }
        }
    }
    else if(base == IP_EVENT) {
        ESP_LOGI("IP","Successfully got the IP");
        switch(event_occured) {
            case IP_EVENT_STA_GOT_IP:{
                ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
                ESP_LOGI("IP", "Got IP address:" IPSTR, IP2STR(&event->ip_info.ip));
                xEventGroupSetBits(wifi_event_group, CONNECTED_BIT);
                WiFiServiceLocal.connection_result = WIFI_CONNECTED;
                wifi_connected = true;  //At this instant I have got the IP address
            }
            case IP_EVENT_STA_LOST_IP:{
                ESP_LOGI(WIFI_TAG,"Could not get IP");
                WiFiServiceLocal.connection_result = WIFI_DISCONNECTED;
                wifi_connected = false;  //At this instant I have got the IP address                
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

    //Setting up networking layers
    ESP_ERROR_CHECK(esp_netif_init());
    //Sets up the default event loop to handle system events like WIFI_EVENT and IP_EVENT.
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    //Creates a default Wi-Fi station interface. This is where the ESP32 will connect to a Wi-Fi network.
    esp_netif_create_default_wifi_sta();   
     
    //Register handler for Wi-Fi and IP events
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT, 
                                                        ESP_EVENT_ANY_ID, 
                                                        &wifi_event_handler, 
                                                        NULL, 
                                                        NULL));

    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
                                                        ESP_EVENT_ANY_ID,
                                                        &wifi_event_handler,
                                                        NULL,
                                                        NULL));

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK( esp_wifi_init(&cfg) );
    ESP_ERROR_CHECK( esp_wifi_set_storage(WIFI_STORAGE_RAM));
    ESP_ERROR_CHECK( esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK( esp_wifi_start() );
}