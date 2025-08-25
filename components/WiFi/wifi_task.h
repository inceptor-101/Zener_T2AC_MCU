#ifndef WIFI_TASK_H
#define WIFI_TASK_H

#include "../../main/main.h"

// Notification state constants
#define NOT_SET 0
#define SEND_NAP    1
#define SEND_CONNECTION_RESULT    2
#define SEND_CONNECTION_STATUS    3
#define MIN_REQUIRED_MBUF         2
#define NOTIFY_WIFI_CHAR (notification_state >= 1 && notification_state <= 3)
#define NOTIFY (!(((notify_wifi_state || notify_indicate_enabled) && NOTIFY_WIFI_CHAR)))

//Defining the enums
typedef enum{
    BOOT_UP, //0
    STEADY_STATE,
    CHANGING_CONNECTION, //3
    WIFI_SCANNING //4
}WIFI_STATES;

//Defining the functions
extern void wifi_task_core0(void* pvParameters);
extern void initialise_wifi(void);
extern void timer_task_1(void *pvParameters);
extern void wifi_scan(void);
extern void wifi_get_ap(void);
extern void wifi_connect(uint8_t* ssid, uint8_t* password);
extern bool check_internet_status(void);
extern void copy_ssids(uint8_t* from, uint8_t* to, uint8_t len);
extern void wifi_event_handler(void* handler_args, esp_event_base_t base, int32_t id, void* event_data);
extern void communicate_with_BLE(void);

//#####################################################
// Define constants used externally
#define DEFAULT_SCAN_LIST_SIZE 20

// External variables
extern char sub_topic1[65];
extern TaskHandle_t process_MQTT_msg_handler;
extern wifi_ap_record_t ap_info[DEFAULT_SCAN_LIST_SIZE];
extern esp_mqtt_client_handle_t mqtt_client;
extern EventGroupHandle_t wifi_event_group;
extern const int CONNECTED_BIT;
extern const char *ESP_TAG;
extern const char *WIFI_TAG;
extern const char *MQTT_TAG;
extern bool wifiConnected;
extern uint16_t number;
extern uint16_t ap_count;
extern uint8_t scan_task_started;
extern bool could_not_connect;

#endif