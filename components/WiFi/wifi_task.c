#include "wifi_task.h"
// Custom Data Structures 
WIFI_STATES curr_wifi_state = BOOT_UP;
bool could_not_connect = false;
static bool wifi_scanning_timeout = false;       // Controls whether retry is allowed
static TimerHandle_t wifi_retry_timer;       // FreeRTOS timer handle
static bool scanning_beautify = false;
static bool timer_started = false;

//For timer utility
static void retry_timer_callback(TimerHandle_t wifi_retry_timer){
    wifi_scanning_timeout = true;
}

static void timer_supported_retry_connection(uint8_t* ssid, uint8_t* password){
    //WiFi driver in ESP32 has a default retries interval of 2s 
    if ((!wifi_connected)  && (!could_not_connect)){
        //connected_ssid and connected_password have been defaulted to nvs fetched
        wifi_connect(ssid, password);
        could_not_connect = false;
    }

    //This code has been commented due to the switch to the event driven instead of timer based retrials 

    // else if((!wifi_connected) && (could_not_connect)){
    //     //starting a timer to restart a connection request
    //     wifi_retry_allowed = false;
    //     xTimerStart(wifi_retry_timer, 0); //Instantaneously start the timer
    // }
}

//Main Task
void wifi_task_core0(void* pvParameters) {
    //Creating the custom 
    //Commenting it out to make sure the use of the event driven wifi task 
    wifi_retry_timer = xTimerCreate(
        "wifi connection retry timer", 
        pdMS_TO_TICKS(60*1000),   // After 1 minute
        pdFALSE, //No auto reloading
        NULL,
        retry_timer_callback
    );

    while (1) {
        // ---------- Check BLE Commands ----------

        if (WifiService.LAP == ENABLE) {
            curr_wifi_state = WIFI_SCANNING;
            WifiService.LAP = DISABLE;
        } 
        else if (WifiService.GAP == ENABLE) {
            wifi_get_ap();
            WifiService.GAP = DISABLE;
        }

        // ---------- State Machine ----------
        switch (curr_wifi_state) {
            case BOOT_UP: {
                ESP_LOGI("WiFi", "In the boot up mode");

                timer_supported_retry_connection((uint8_t*)connected_ssid, (uint8_t*)connected_password);
                if (wifi_connected) {
                    curr_wifi_state = STEADY_STATE;
                }
                break;
            }

            case STEADY_STATE: {
                ESP_LOGI("WiFi", "Connection established");
                //Checking the internet status
                check_internet_status();
                if (!wifi_connected) {
                    curr_wifi_state = BOOT_UP;
                }
                break;
            }

            case CHANGING_CONNECTION: {
                
                //We can enter this case only when scanning is complete 
                ESP_LOGI("WIFI", "Starting the procedure for changing connection");
                timer_supported_retry_connection((uint8_t*)ap_info[WifiService.SSID_No - 1].ssid, (uint8_t*)WifiService.password_received);
                if (wifi_connected){
                    curr_wifi_state = STEADY_STATE;
                }
                break;
            }

            case WIFI_SCANNING: {
                if (wifi_connected){
                    esp_wifi_disconnect(); //Disconnecting if already connected to the wifi
                }

                //Setting the timeout for the current scanning state of the wifi
                if (wifi_scanning_timeout){
                    wifi_scanning_timeout = false;
                    ESP_LOGI("WIFI", "Could not find the APs in scan.... Doing BOOTUP");
                    curr_wifi_state = BOOT_UP;
                    timer_started = false;
                    break;
                }
                if (!timer_started){
                    //If timeout has not happened yet 
                    xTimerStart(wifi_retry_timer, 0); // setting the timeout, timeout does 
                                                    // not restart on repetitive calling 
                                                    // unless actualy time out happens 
                    timer_started = true;
                }
                                                  

                //Main logic
                if (scan_task_started == 0) {
                    //If condition will be true at the before start of scan
                    if (ap_count < 1){   
                        ESP_LOGI("WIFI", "Starting WiFi scan");                            
                        wifi_scan();
                        scan_task_started = 1;
                    }
                    //This else condition will be true after completion of the scan
                    else{
                        //Reinitialising the scanned variables 
                        ap_count = 0;
                        number = 0;   
                        ESP_LOGI("WIFI", "Found APs...But still in scanning mode looking for changing connection command");           
                        if (WifiService.CAP == ENABLE) {
                            ESP_LOGI("WIFI", "Found APs... and got command for changing connection");
                            curr_wifi_state = CHANGING_CONNECTION;
                            WifiService.CAP = DISABLE;
                        }         
                    }
                }else{
                    //Scanning logic
                    if (scanning_beautify == false){
                        printf("\nScanning for APs...");
                        scanning_beautify = true;
                    }
                    else {
                        printf("\nScanning...");
                    }
                }

                break;
            }
        }
        //Making the communication with BLE if needed 
        communicate_with_BLE();
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
