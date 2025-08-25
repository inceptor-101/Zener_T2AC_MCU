#ifndef MAIN_H
#define MAIN_H

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "freertos/queue.h"
#include "driver/gpio.h"
#include "driver/gptimer.h"
#include "driver/twai.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "nvs_flash.h"
#include "mqtt_client.h"
#include "stdlib.h"
#include "nvs.h"
#include "string.h"
#include "esp_mac.h"
#include "host/ble_gatt.h"
#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "host/ble_hs.h"
#include "services/gap/ble_svc_gap.h"
#include "services/gatt/ble_svc_gatt.h"
#include <esp_http_client.h>
#include "esp_wifi.h"
#include "esp_event.h"
#include <stdint.h>
#include <stdbool.h>

#include "../components/canLib/canFunction.h"
#include "../components/BLE/bleFunction.h"
#include "../components/Services/services.h"
#include "../components/WiFi/wifi_task.h"
#include "../components/NVS/FetchNVSdata.h"
#include "../components/NVS/SaveNVSdata.h"

#define EMERGENCY_STOP_PRESS GPIO_NUM_2
#define INTERNET_LED GPIO_NUM_21
#define SUPPLY_LED GPIO_NUM_22
#define CS_GUNB_LED GPIO_NUM_5
#define FAULT_GUNB_LED GPIO_NUM_4
#define CS_GUNA_LED GPIO_NUM_26
#define FAULT_GUNA_LED GPIO_NUM_25

#define TIMER_RES_HZ     1000000        //The timer counts in microseconds (1 MHz resolution) So i am using 1 tick = 1microsecond resolution
#define TIMER_PERIOD_US  1000           // Alarm every 1000 µs = 1 ms

extern TaskHandle_t can_task_handle;
extern TaskHandle_t main_task_handle;
extern SemaphoreHandle_t mutex_handle;

extern uint16_t global_conn_handle;
extern bool wifi_initialised;
extern bool wifi_connected;
extern bool internet_connected;

extern char connected_ssid[32];
extern char connected_password[32];

//Enums Definition
typedef uint8_t faultStatusEnum;
enum{
    CLEARED = 1,
    PERSISTANT = 2
};
typedef enum {
    AVAILABLE = 0,
    UNAVAILABLE,
    FAULTED,
    CHARGING,
    FINISHING,
    PREPARING
} connector_status_t;

typedef uint8_t faultDescriptionEnum;
enum{
    NO_FAULT = 1,
    EMERGENCY_STOP = 2
    //More to be Added 
};

typedef uint8_t cpStateEnum;
enum{
    CP_STATE_A = 0,
    CP_STATE_B,
    CP_STATE_C,
    CP_STATE_D,
    CP_STATE_E,
    CP_STATE_F
};

typedef enum {
    EVSE_STATE_NOT_READY = 1,
    EVSE_STATE_READY     = 2
} evseStateEnum;

typedef uint8_t brandEnum;
enum{
	PYRAMID_ELECTRONICS = 1,
	ELECTROWAVES_ELECTRONICS = 2,
	Ez_PYRAMID = 3,
	VOLTIC = 4,
	CHARGESOL = 5,
	ZENERGIZE = 6	
};

typedef uint8_t powerInputTypeEnum;
enum{
    AC_ = 1,
    DC_ = 2
};

typedef uint8_t communicationStatusEnum;
enum{
    DISCONNECTED = 1,
    CONNECTED = 2
};

typedef uint8_t statusEnum;
enum{
    DISABLED = 1,
    ENABLED = 2
};

typedef uint8_t meterTypeEnum;
enum{
    ACTUAL = 1,
    VIRTUAL = 2
};

typedef uint8_t totalRatedPowerEnum;
enum{
    CHARGER_POWER_3_3_KW = 1,
    CHARGER_POWER_7_4_KW = 2,   
    CHARGER_POWER_11_KW  = 3,    
    CHARGER_POWER_22_KW  = 4      
};

typedef uint8_t chargerModeEnum;
enum{
    OFFLINE = 1,
    ONLINE = 2
};

typedef uint8_t chargingTypeEnum;
enum{
    HOME = 1,
    PUBLIC = 2
};

typedef uint8_t deviceTypeEnum;
enum{
    DC = 1,
    TYPE2AC = 2,
    AC001 = 3,
    GTSI = 4
};

typedef uint8_t connectorTypesEnum;
enum{
    CCS = 1,
    GBT,
    CHAdeMO,
    CHAoJi,
    DC001,
    Type2AC
};

typedef uint8_t noOfPhasesEnum;
enum{
    SINGLE_PHASE_AC = 1,
    THREE_PHASE_AC = 2
};

typedef uint8_t monthEnum;
enum{
    JAN = 1,
    FEB,
    MARCH,
    APRIL,
    MAY,
    JUNE,
    JULY,
    AUGUST,
    SEPTEMBER,
    OCTOBER,
    NOVEMBER,
    DECEMBER
};

typedef uint8_t webSocketTypeEnum;
enum{
    NON_SECURED = 0,
    SECURED
};

typedef uint8_t ocppVersionEnum;
enum{
    OCPP_1_6 = 1,
    OCPP_2_0
};

typedef uint8_t connectorIdEnum;
enum{
    CONNECTORA = 1,
    CONNECTORB = 2
};

typedef uint8_t StopReasonAC;
enum {
    reasonStopButton = 1,                 // User pressed physical stop button
    reasonEarthFault = 2,                 // Earth leakage or ground fault detected
    reasonSmokeDetected = 3,              // Smoke/fire sensor triggered
    reasonDoorOpen = 4,                   // Cabinet door opened (if applicable)
    reasonGridFailure = 6,                // Grid power lost or unstable
    reasonHardwareTrip = 15,              // Internal hardware protection triggered
    reasonThermalFault = 19,              // Over-temperature detected
    reasonSpuriousFault = 20,             // Unclassified or unexpected fault
    reasonCANAFailed = 21,                // Internal CAN bus A communication failure (optional)
    reasonCANBFailed = 22,                // Internal CAN bus B communication failure (optional)
    reasonEV = 23,                        // EV signaled error or disconnected unexpectedly
    reasonMobileApp = 26,                 // Charging stopped from mobile application
    reasonRFID = 27,                      // RFID card used to stop charging
    reasonResetReceivedFromServer = 28,   // Remote stop/reset command via OCPP
    reasonChargingComplete = 30,          // EV requested stop or SOC full
    reasonEMButton = 31,                  // Emergency stop button pressed
    reasonIDTagDeauthorized = 32,         // Authorization revoked during session
    reasonChargingAmountLimit = 33,       // kWh or time limit reached
    unknownError = 34,                    // Catch-all for undefined errors
    tcpConnectionClosed = 102,            // TCP session (e.g., OCPP) closed unexpectedly
    ethLinkDownDetected = 104             // Ethernet cable unplugged or network down
};

// Macro for WiFi Service
#define DISABLE 1
#define ENABLE  2

//Variables Declaration 
typedef struct
{
    bool seq1_received;
    bool seq2_received;
    bool seq3_received;
    bool seq4_received; //Added fro Sequence 4
} LLC_DataStatus_t;
extern LLC_DataStatus_t llcDataStatus;

//Structure for CAN reception
typedef struct
{
    // Sequence 1
    float phaseA_voltage;
    float phaseB_voltage;
    float phaseC_voltage;
    uint8_t temperature;
    uint8_t soc; // Temperature in Celsius

    // Sequence 2
    float phaseA_current;
    float phaseB_current;
    float phaseC_current;
    uint8_t chargingStatus;

    // Sequence 3
    float rcmu;
    float ne_voltage;
    uint8_t cp_state;
    float duty_cycle;
    uint8_t connector_state;
    float instantaneous_power;
    float energyPerMinute;
    
    // Data status flags
    LLC_DataStatus_t llcDataStatus;

}LLC_Data_t;
extern LLC_Data_t Data_Sensed_LLC;
extern LLC_Data_t Data_Sensed_LLC_IPC;
extern LLC_Data_t Data_Sensed_LLC_Core0;

//Structure for First Time configuration from BLE 
typedef struct
{
  deviceTypeEnum deviceType ;
  uint8_t num_connector;
  connectorTypesEnum connector_types[2];
  noOfPhasesEnum noOfPhases;
  uint8_t noOfPCM;
  char totalRatedPowerValue[5];
  char serialNumber[26];
  char time[10];
  char date[10];     
  statusEnum dynamicPowerSharing;
  monthEnum configMonth;
  uint8_t configYear;
  uint8_t ftcSettingFlagDone; // Flag to indicate if first-time configuration is done
}firstTimeData_t;
extern firstTimeData_t firstTimeConfigData;

//Structure for Security Setting from BLE 
typedef struct {
    char offline_pin[40]; // Pin for offline mode
    statusEnum ftuMode; // FTU mode status
    chargerModeEnum chargerMode; // Charger mode (Offline/Online)
} securityConfig_t;
extern securityConfig_t securitySetting;

typedef struct{
    char url_str[300];
    char host_str[300];
    char port_str[10];
    webSocketTypeEnum ocpp_security; 
    ocppVersionEnum ocpp_version;
    char tariff_str[10];
}connectionConfiguration_t;
extern connectionConfiguration_t connectionConfiguration;

typedef struct{
    uint8_t gun_per_phase_current;
    uint8_t authenticationFlag; 
}chargerConfiguration_t;
extern chargerConfiguration_t chargerConfiguration;

//WiFi Service Structure  
typedef struct {
    uint8_t ssid[70];                     /**< SSID of AP */
} NetworkStruct;
extern NetworkStruct Networks[20];

typedef struct {
    uint8_t LAP : 2;
    uint8_t CAP : 2;
    uint8_t GAP : 2;
    uint8_t SSID_No : 5;
    uint8_t password_length : 7;
    uint8_t password_received[100];
} WiFiServiceStruct;
extern WiFiServiceStruct WifiService;

typedef struct {
    uint8_t NAP_count  : 5;
    uint8_t retry_num : 3;
    uint8_t connection_result : 3;
    uint8_t connection_status : 3;

} WiFiServiceLocalStruct;
extern WiFiServiceLocalStruct WiFiServiceLocal;


#endif