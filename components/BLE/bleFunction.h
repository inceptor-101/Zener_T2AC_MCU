
#include "../../main/main.h"

#define UUID_DECLARE(uuid16)                            \
((const ble_uuid_t *) (&(ble_uuid128_t) BLE_UUID128_INIT( \
    0x45, 0x23, 0xe1, 0xd1, 0x6f, 0x0a, 0x9e, 0xbc, \
    0xf1, 0x42, 0x3b, 0x4d, \
    (uint8_t)((uuid16) & 0xff), (uint8_t)(((uuid16) >> 8) & 0xff), 0x00, 0x00 \
)))

//1WiFi Service  
#define WIFI_SERVICE          0x0001
//Wifi Characterstics 
#define DATA_EXCHANGE_CHR     0x0011
//Command byte for Wi-Fi Service
#define  START_SCAN_DATA_FRAME              0x01
#define  CONNECT_AP_DATA_FRAME              0x02
#define  PASSWORD_DATA_FRAME                0x03
#define  CONNECTION_RESULT_DATA_FRAME       0x04
#define  CONNECTION_STATUS_DATA_FRAME       0x05

//2) Restricted Access Service 
#define RESTRICTED_ACCESS_SERVICE           0x0002
//Restricted Access Characterstics 
#define SERVER_SETTINGS_CHR                 0x0015
#define FIRST_TIME_CHR                      0x0020
#define SECURITY_SETTING_CHR                0x0021  
#define RFID_SETTING_CHR                    0x0022

//Command Byte for Restricted Access Service - Basic Configuration 
#define SET_AUTH                            0x01
//Command Byte for Restricted Access Service - Basic Configuration 
#define SET_DATE_TIME                       0x01
#define SET_SERIAL_TARIFF                   0x02
#define SET_PASSWORD                        0x03
#define SET_CHARGER_MODE_PMUSTATUS_TYPE     0x04
#define SET_OFFLINE_OTP                     0x05
#define SET_HOME_PARAMETER                  0x06
#define SET_GUN_PLUGIN_PARAMETER            0x07
#define SET_RFID                            0x08
//Command Byte for Restricted Access Service - Charger Configuration 
#define SET_METERTYPE_COMMAND               0x01
#define SET_SYSTEM_COMMAND                  0x02
#define SET_GENERAL_COMMAND                 0x03
//Command Byte for Restricted Access Service - Connection Configuration
#define SET_URL_COMMAND                     0x01
#define SET_WEBSOCKET_COMMAND               0x02
//Command Byte for Restricted Access Service - Limit Settings 
#define SET_LIMITS                          0x01
//Command Byte for Restricted Access Service - Energy Data
#define REQUEST_DATA                        0x01
#define REQUEST_DAILY                       0x00
#define REQUEST_MONTHLY                     0x01
#define REQUEST_YEARLY                      0x02
//Command Byte for Restricted Access Services - Fault Diagnosis
#define REQUEST_FAULT                       0x01 
//Command Byte for Restricted Access Service - Session Logs
#define GET_TOTAL_SESSION_LOGS              0x01
#define GET_SESSION_LOGS                    0x02
#define CONNECTOR_A                         0x01
#define CONNECTOR_B                         0x02
// Restricted Access Service - Session Logs
#define MAX_SESSION_LOGS                    50
#define SESSION_LOG_CHUNK_SIZE              5
//Command Byte for Restricted Access Service - First Time Configuration
#define SET_FIRST_TIME_DATA                 0x01
//Command Byte for Restricted Acces Service - Security Settings
#define SET_SECURITY_SETTING                0x01
//3) BootUp Service
#define BOOT_UP_SERVICE                     0x0003
//BootUp Characteristics
#define  START_CONFIG_CHR                   0x0021
//Command bytes of BootUp Charcateristics
#define GET_CONFIG_FLAG                     0x01

//3) Charging Session Service
#define CHARGING_SESSION_SERVICE            0x0004
//Characterstics of Charging Session Service 
#define CHARGING_CHR                        0x0022
//Command Bytes of Charging Session Service
#define START_CHARGING                      0x01
#define CHARGING_PIN                        0x02 
#define REQUEST_STATUS                      0x03
#define STOP_CHARGING                       0x04

//Wifi relatedconstants
#define DEFAULT_SCAN_LIST_SIZE              20
#define MAX_PASS_LEN_ONE_DATA_FRAME         18
#define WIFI_CONNECTED                       1
#define WIFI_DISCONNECTED                    0

extern uint16_t notify_wifi_handle;

//Extern Variables for Restricted Access Service 
extern uint16_t notify_auth_handle;
extern uint16_t notify_basic_Config_handle;
extern uint16_t notify_charger_Config_handle;
extern uint16_t notify_connection_Config_handle;
extern uint16_t notify_energy_Data_handle;
extern uint16_t notify_fault_Diagnosis_handle;
extern uint16_t notify_limit_Setting_handle;
extern uint16_t notify_session_Log_handle;
extern uint16_t notify_first_time_handle;
extern uint16_t notify_security_setting_handle;

extern void ble_init(void);
extern uint8_t notification_state;
extern bool notify_wifi_state;
extern bool notify_indicate_enabled;
extern wifi_ap_record_t ap_info_display[1];
extern uint8_t ssid_index;
extern bool notify_ready;
extern uint16_t conn_handle;
