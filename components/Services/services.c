#include <string.h>                         // for strlen(), memcmp()
#include <assert.h>                         // for assert()
#include <stdint.h>                         // for uint8_t, uint16_t, etc.
#include <stdbool.h>                        // for bool type
#include "nimble/ble.h"                     // for BLE definitions
#include "host/ble_hs.h"                    // for ble_hs_mbuf_from_flat, BLE_GATT_ACCESS_OP_WRITE_CHR, BLE_ATT_ERR_*
#include "host/ble_gatt.h"                  // for ble_gatt_access_ctxt
#include "services/gatt/ble_svc_gatt.h"     // (optional but common for GATT service macros)
#include "nvs_flash.h"
#include "nvs.h"
#include "../../main/main.h"

#define FIRST_TIME_TAG      "First_Time"
#define SECURITY_TAG        "Security_Config"
static const char *BLE_TAG = "Service";

static uint16_t extract_uuid16_from_thrpt_uuid128(const ble_uuid_t *uuid)
{
    const uint8_t *u8ptr;
    uint16_t uuid16;

    u8ptr = BLE_UUID128(uuid)->value;
    uuid16 = u8ptr[12];
    uuid16 |= (uint16_t)u8ptr[13] << 8;
    return uuid16;
}

void print_bytes_(const uint8_t *bytes, int len)
{
    int i;
    for (i = 0; i < len; i++) {
        ESP_LOGI("Recv data", "%u", bytes[i]);
    }
}

static uint32_t extract_value(uint8_t* data, int start, int length) {
    char buffer[12] = {0};  // Enough for uint32_t ASCII strings
    memcpy(buffer, &data[start], length);
    return atoi(buffer);
}

// Notifications Function 
void send_firstTime_config_status_notification(uint16_t conn_handle, uint16_t chr_val_handle, uint8_t command_id, uint8_t status) {
    uint8_t notify_data[2];
    notify_data[0] = command_id;
    notify_data[1] = status;
    printf("The status of firstTime Config is %d \n", status);    

    struct os_mbuf *om = ble_hs_mbuf_from_flat(notify_data, sizeof(notify_data));
    if (!om) {
        ESP_LOGE(BLE_TAG, "Failed to allocate mbuf for config notification");
        return;
    }

    int rc = ble_gatts_notify_custom(conn_handle, chr_val_handle, om);
    if (rc != 0) {
        ESP_LOGE(BLE_TAG, "Notification failed for handle 0x%04X: rc=%d", chr_val_handle, rc);
    } else {
        ESP_LOGI(BLE_TAG, "Notified: Cmd=0x%02X, Status=%d on handle 0x%04X", command_id, status, chr_val_handle);
    }
}

void send_config_status_indication(uint16_t conn_handle, uint16_t chr_val_handle, uint8_t command_id, uint8_t status){
    uint8_t indicate_data[2];
    indicate_data[0] = command_id;
    indicate_data[1] = status;

    struct os_mbuf *ind = ble_hs_mbuf_from_flat(indicate_data, sizeof(indicate_data));
    if (!ind) {
        ESP_LOGE(BLE_TAG, "Failed to allocate mbuf for config");
        return;
    }

    int rc = ble_gatts_indicate_custom(conn_handle, chr_val_handle, ind);
    if (rc != 0) {
        ESP_LOGE(BLE_TAG, "Indication failed for handle 0x%04X: rc=%d", chr_val_handle, rc);
    } else {
        ESP_LOGI(BLE_TAG, "Indicated: Cmd=0x%02X, Status=%d on handle 0x%04X", command_id, status, chr_val_handle);
    }
}

void send_security_setting_notification(uint16_t conn_handle, uint16_t chr_val_handle, uint8_t command_id, uint8_t status) {
    uint8_t notify_data[2];
    notify_data[0] = command_id;
    notify_data[1] = status;

    struct os_mbuf *om = ble_hs_mbuf_from_flat(notify_data, sizeof(notify_data));
    if (!om) {
        ESP_LOGE(BLE_TAG, "Failed to allocate mbuf for security setting notification");
        return;
    }

    int rc = ble_gatts_notify_custom(conn_handle, chr_val_handle, om);
    if (rc != 0) {
        ESP_LOGE(BLE_TAG, "Notification failed for handle 0x%04X: rc=%d", chr_val_handle, rc);
    } else {
        ESP_LOGI(BLE_TAG, "Security setting notification sent: Cmd=0x%02X, Status=%d on handle 0x%04X", command_id, status, chr_val_handle);
    }
}

void send_config_status_notification(uint16_t conn_handle, uint16_t chr_val_handle, uint8_t command_id, uint8_t status) {
    uint8_t notify_data[2];
    notify_data[0] = command_id;
    notify_data[1] = status;
    printf("The status of chargerConfig is %d \n", status);    

    struct os_mbuf *om = ble_hs_mbuf_from_flat(notify_data, sizeof(notify_data));
    if (!om) {
        ESP_LOGE(BLE_TAG, "Failed to allocate mbuf for config notification");
        return;
    }

    int rc = ble_gatts_notify_custom(conn_handle, chr_val_handle, om);
    if (rc != 0) {
        ESP_LOGE(BLE_TAG, "Notification failed for handle 0x%04X: rc=%d", chr_val_handle, rc);
    } else {
        ESP_LOGI(BLE_TAG, "Notified: Cmd=0x%02X, Status=%d on handle 0x%04X", command_id, status, chr_val_handle);
    }
}

// -------- Services Implementation --------
int wifi_svr(uint16_t conn_handle, uint16_t attr_handle, struct ble_gatt_access_ctxt *ctxt, void *arg)
{
    uint16_t uuid16;
    int rc;

    uuid16 = extract_uuid16_from_thrpt_uuid128(ctxt->chr->uuid);
    assert(uuid16 != 0);

    switch (uuid16) 
    {
    case DATA_EXCHANGE_CHR:
        if (ctxt->op == BLE_GATT_ACCESS_OP_WRITE_CHR) {
            print_bytes_(ctxt->om->om_data, ctxt->om->om_len);
            if(ctxt->om->om_data[0] == START_SCAN_DATA_FRAME){
                ESP_LOGI("","Scanning started...");
                WifiService.LAP = ENABLE;
            }
            else if(ctxt->om->om_data[0] == CONNECT_AP_DATA_FRAME){
                ESP_LOGI("Received","CONNECT_AP");
                WifiService.SSID_No = ctxt->om->om_data[1];
                ESP_LOGI("","SSIDNO : %u", WifiService.SSID_No);
            }
            else if(ctxt->om->om_data[0] == PASSWORD_DATA_FRAME){
                ESP_LOGI(BLE_TAG,"Password Data Received");

                // Get password length dynamically
                uint16_t password_len = ctxt->om->om_len - 1;
                WifiService.password_length = password_len;

                // Copy the password from byte index 1 onwards
                memcpy(WifiService.password_received, &ctxt->om->om_data[1], password_len);
                WifiService.password_received[password_len] = '\0'; // Null terminate

                ESP_LOGI(BLE_TAG, "Complete password received (%u bytes): %s", 
                    password_len, WifiService.password_received);
                WifiService.CAP = ENABLE;
        }
            else if(ctxt->om->om_data[0] == CONNECTION_STATUS_DATA_FRAME){
                WifiService.GAP  = ENABLE;
            }
        }   
        else if (ctxt->op == BLE_GATT_ACCESS_OP_READ_CHR) {  
            ESP_LOGI("Read Operation","SSID");  
            rc = os_mbuf_append(ctxt->om, Networks[ssid_index].ssid,
                                sizeof(Networks[ssid_index].ssid));
            ssid_index++;
            return rc == 0 ? 0 : BLE_ATT_ERR_INSUFFICIENT_RES;
            if (rc != 0) {
                return BLE_ATT_ERR_INSUFFICIENT_RES;
            } 
        }

        return 0;

    default:
        assert(0);
        return BLE_ATT_ERR_UNLIKELY;
    }
}

int restricted_access_svr(uint16_t conn_handle, uint16_t attr_handle, struct ble_gatt_access_ctxt *ctxt, void *arg)
{
    uint16_t uuid16;
    int rc;
    uint8_t payload[185] = {0};
    uuid16 = extract_uuid16_from_thrpt_uuid128(ctxt->chr->uuid);
    assert(uuid16 != 0);

    switch (uuid16)
    {
        case SERVER_SETTINGS_CHR:
        {
            if (ctxt->op == BLE_GATT_ACCESS_OP_READ_CHR)
            {
                //Mobile App performs the Read from ESP32 
                uint8_t index = 0;
                ESP_LOGI("Connect_Config_TAG", "Reading SERVER_SETTINGS_CHR for conn_handle=%d", conn_handle);
                
                // URL 
                uint8_t url_len = strlen(connectionConfiguration.url_str);
                ESP_LOGI("Connect_Config_TAG", "URL: %s (len=%d)", connectionConfiguration.url_str, url_len);
                payload[index++] = url_len;
                memcpy(&payload[index], connectionConfiguration.url_str, url_len);
                index += url_len;

                //Host 
                uint8_t host_len = strlen(connectionConfiguration.host_str);
                ESP_LOGI("Connect_Config_TAG", "Host: %s (len=%d)", connectionConfiguration.host_str, host_len);
                payload[index++] = host_len;
                memcpy(&payload[index], connectionConfiguration.host_str, host_len);
                index += host_len;

                //Port 
                uint8_t port_len = strlen(connectionConfiguration.port_str);
                ESP_LOGI("Connect_Config_TAG", "Port: %s (len=%d)", connectionConfiguration.port_str, port_len);
                payload[index++] = port_len;
                memcpy(&payload[index], connectionConfiguration.port_str, port_len);
                index += port_len;

                //Websocket Connection
                uint8_t websocket_len = 1;
                ESP_LOGI("Connect_Config_TAG", "Ocpp Secure Status: %d", connectionConfiguration.ocpp_security);
                payload[index++] = websocket_len;
                payload[index++] = connectionConfiguration.ocpp_security;
                
                //OCPP Version
                uint8_t websocketVersion_len = 1;
                ESP_LOGI("Connect_Config_TAG", "Ocpp Version: %d", connectionConfiguration.ocpp_version);
                payload[index++] = websocketVersion_len;
                payload[index++] = connectionConfiguration.ocpp_version;

                //Tariff Length
                uint8_t tariff_len = strlen(connectionConfiguration.tariff_str);
                ESP_LOGI("Connect_Config_TAG", "Tariff: %s (len=%d)", connectionConfiguration.tariff_str, tariff_len);
                payload[index++] = tariff_len; 
                memcpy(&payload[index], connectionConfiguration.tariff_str, tariff_len);
                index += tariff_len;

                //Send the payload
                rc = os_mbuf_append(ctxt->om, payload, index);

                return rc == 0 ? 0 : BLE_ATT_ERR_INSUFFICIENT_RES;
            }
            else if (ctxt->op == BLE_GATT_ACCESS_OP_WRITE_CHR)
            {
                //Handle updates(SET commands based on your first byte inside ctxt->om->om_data)
                if (ctxt->om->om_len < 1)
                {
                    return BLE_ATT_ERR_INVALID_ATTR_VALUE_LEN;  // Indicate that this is invalid case and returns
                }
                uint8_t cmd = ctxt->om->om_data[0];
                switch (cmd)
                {
                    case SET_URL_COMMAND:{
                    if (ctxt->om->om_len >= 6) // Minimum length check
                    {
                        // write URL string
                        uint8_t url_len = ctxt->om->om_data[1];
                        if (url_len + 2 > ctxt->om->om_len) {
                            ESP_LOGE("BLE_PARSE", "URL too short: url_len=%d, om_len=%d", url_len, ctxt->om->om_len);
                            send_config_status_notification(conn_handle, notify_connection_Config_handle, SET_URL_COMMAND, 0);
                            break;
                        }
                        memcpy(connectionConfiguration.url_str, &ctxt->om->om_data[2], url_len);
                        connectionConfiguration.url_str[url_len] = '\0';
                        ESP_LOGW("Debug1","The value of Connection URL is %s",connectionConfiguration.url_str);

                        // Host string
                        uint8_t host_len_index = 2 + url_len;
                        if (host_len_index >= ctxt->om->om_len) {
                            ESP_LOGE("BLE_PARSE", "Host length index out of bounds: host_len_index=%d, om_len=%d", host_len_index, ctxt->om->om_len);
                            send_config_status_notification(conn_handle, notify_connection_Config_handle, SET_URL_COMMAND, 0);
                            break;
                        }

                        uint8_t host_len = ctxt->om->om_data[host_len_index];
                        if (host_len_index + 1 + host_len > ctxt->om->om_len) {
                            ESP_LOGE("BLE_PARSE", "Host string too short: host_len=%d, host_len_index=%d, om_len=%d", host_len, host_len_index, ctxt->om->om_len);
                            send_config_status_notification(conn_handle, notify_connection_Config_handle, SET_URL_COMMAND, 0);
                            break;
                        }

                        memcpy(connectionConfiguration.host_str, &ctxt->om->om_data[host_len_index + 1], host_len);
                        connectionConfiguration.host_str[host_len] = '\0';
                        ESP_LOGW("Debug1","The value of Connection Host is %s",connectionConfiguration.host_str);

                        // Port string
                        uint8_t port_len_index = host_len_index + 1 + host_len;
                        if (port_len_index >= ctxt->om->om_len) {
                            ESP_LOGE("BLE_PARSE", "Port length index out of bounds: port_len_index=%d, om_len=%d", port_len_index, ctxt->om->om_len);
                            send_config_status_notification(conn_handle, notify_connection_Config_handle, SET_URL_COMMAND, 0);
                            break;
                        }

                        uint8_t port_len = ctxt->om->om_data[port_len_index];
                        if (port_len_index + 1 + port_len > ctxt->om->om_len) {
                            ESP_LOGE("BLE_PARSE", "Port string too short: port_len=%d, port_len_index=%d, om_len=%d", port_len, port_len_index, ctxt->om->om_len);
                            send_config_status_notification(conn_handle, notify_connection_Config_handle, SET_URL_COMMAND, 0);
                            break;
                        }

                        memcpy(connectionConfiguration.port_str, &ctxt->om->om_data[port_len_index + 1], port_len);
                        connectionConfiguration.port_str[port_len] = '\0';
                        ESP_LOGW("Debug1","The value of Connection Port is %s",connectionConfiguration.port_str);

                        // OCPP security
                        uint8_t ocpp_security_len_index = port_len_index + 1 + port_len;
                        if (ocpp_security_len_index >= ctxt->om->om_len) {
                            ESP_LOGE("BLE_PARSE", "OCPP sec len index out of bounds: idx=%d, om_len=%d", ocpp_security_len_index, ctxt->om->om_len);
                            send_config_status_notification(conn_handle, notify_connection_Config_handle, SET_URL_COMMAND, 0);
                            break;
                        }

                        uint8_t ocpp_security_len = ctxt->om->om_data[ocpp_security_len_index];
                        connectionConfiguration.ocpp_security = ctxt->om->om_data[ocpp_security_len_index + 1];
                        ESP_LOGW("Debug1","The value of Connection Configuration in OCPP Security is %d",connectionConfiguration.ocpp_security);

                        // OCPP version
                        uint8_t ocpp_version_len_index = ocpp_security_len_index + 1 + ocpp_security_len;
                        if (ocpp_version_len_index >= ctxt->om->om_len) {
                            ESP_LOGE("BLE_PARSE", "OCPP version len index out of bounds: idx=%d, om_len=%d", ocpp_version_len_index, ctxt->om->om_len);
                            send_config_status_notification(conn_handle, notify_connection_Config_handle, SET_URL_COMMAND, 0);
                            break;
                        }

                        uint8_t ocpp_version_len = ctxt->om->om_data[ocpp_version_len_index];
                        if (ocpp_version_len != 1 || ocpp_version_len_index + 1 >= ctxt->om->om_len) {
                            ESP_LOGE("BLE_PARSE", "Invalid OCPP version len: len=%d, idx=%d, om_len=%d", ocpp_version_len, ocpp_version_len_index, ctxt->om->om_len);
                            send_config_status_notification(conn_handle, notify_connection_Config_handle, SET_URL_COMMAND, 0);
                            break;
                        }
                        connectionConfiguration.ocpp_version = ctxt->om->om_data[ocpp_version_len_index + 1];
                        ESP_LOGW("Debug1","The value of Connection Configuration in OCPP Version is %d",connectionConfiguration.ocpp_version);


                        //tariff string
                        uint8_t tariff_len_index = ocpp_version_len_index + 1 + ocpp_version_len;
                        if (tariff_len_index >= ctxt->om->om_len) {
                            ESP_LOGE("BLE_PARSE", "Tariff len index out of bounds: idx=%d, om_len=%d", tariff_len_index, ctxt->om->om_len);
                            send_config_status_notification(conn_handle, notify_connection_Config_handle, SET_URL_COMMAND, 0);
                            break;
                        }
                        uint8_t tariff_len = ctxt->om->om_data[tariff_len_index];
                        if (tariff_len_index + 1 > ctxt->om->om_len) {
                            ESP_LOGE("BLE_PARSE", "Tariff string too short: tariff_len=%d, tariff_len_index=%d, om_len=%d", tariff_len, tariff_len_index, ctxt->om->om_len);
                            send_config_status_notification(conn_handle, notify_connection_Config_handle, SET_URL_COMMAND, 0);
                            break;
                        }
                        memcpy(connectionConfiguration.tariff_str, &ctxt->om->om_data[tariff_len_index + 1], tariff_len);
                        connectionConfiguration.tariff_str[tariff_len] = '\0';
                        ESP_LOGW("Debug1","The value of Tarrif Amount is %s",connectionConfiguration.tariff_str);

                        //firstTimeBootUpFlag = 1;
                        save_server_settings_to_nvs();
                        send_config_status_notification(conn_handle, notify_connection_Config_handle, SET_URL_COMMAND, 1);
                    } 
                    else 
                    {
                        ESP_LOGE("BLE_PARSE", "Payload too short: om_len=%d", ctxt->om->om_len);
                        send_config_status_notification(conn_handle, notify_connection_Config_handle, SET_URL_COMMAND, 0);
                    }
                    break;
                    }
                        
                    default:
                        return BLE_ATT_ERR_REQ_NOT_SUPPORTED;
                }
                return 0; // success
            }
            break;
        }

        case FIRST_TIME_CHR:
        {
            //Read can be implemented here 
            if (ctxt->op == BLE_GATT_ACCESS_OP_READ_CHR)
            {
                load_first_time_config_from_nvs();
                uint8_t index = 0;
                // Log reading
                ESP_LOGI(FIRST_TIME_TAG, "Reading FIRST_TIME_CHR for conn_handle=%d", conn_handle);
                
                //Device Type
                uint8_t deviceType_len = 1;
                ESP_LOGI(FIRST_TIME_TAG, "Read Device Type : %d", firstTimeConfigData.deviceType);
                payload[index++] = deviceType_len;
                payload[index++] = firstTimeConfigData.deviceType;

                //Time 
                uint8_t time_len = strlen(firstTimeConfigData.time);
                ESP_LOGI(FIRST_TIME_TAG, "Read Time: %s (len=%d)", firstTimeConfigData.time, time_len);
                payload[index++] = time_len;
                memcpy(&payload[index], firstTimeConfigData.time, time_len);
                index += time_len;

                //Data
                uint8_t Date_len = 10;
                ESP_LOGI(FIRST_TIME_TAG, "Date: %s (len=%d)", firstTimeConfigData.date, Date_len);
                payload[index++] = Date_len;
                memcpy(&payload[index], firstTimeConfigData.date, Date_len);
                index += Date_len;

                //Connector Type

                payload[index++] = firstTimeConfigData.num_connector;   // Length (1 or 2)
                for (int i = 0; i < firstTimeConfigData.num_connector; i++) {
                payload[index++] = firstTimeConfigData.connector_types[i];
                ESP_LOGI(FIRST_TIME_TAG, "Connector[%d]: %d", i, firstTimeConfigData.connector_types[i]);
            }

                //Phases
                uint8_t phases_len = 1;
                ESP_LOGI(FIRST_TIME_TAG, "No of Phases: %d", firstTimeConfigData.noOfPhases);
                payload[index++] = phases_len;
                payload[index++] = firstTimeConfigData.noOfPhases;

                //PCM
                uint8_t pcm_len = 1;
                ESP_LOGI(FIRST_TIME_TAG, "No of PCM: %d", firstTimeConfigData.noOfPCM);
                payload[index++] = pcm_len;
                payload[index++] = firstTimeConfigData.noOfPCM;

                //Dynamic Power Sharing 
                uint8_t dps_len = 1;
                ESP_LOGI(FIRST_TIME_TAG, "Dynamic Power Sharing: %d", firstTimeConfigData.dynamicPowerSharing);
                payload[index++] = dps_len;
                payload[index++] = firstTimeConfigData.dynamicPowerSharing;        

                //Config Month
                uint8_t month_len = 1;
                ESP_LOGI(FIRST_TIME_TAG, "Config Month: %d", firstTimeConfigData.configMonth);
                payload[index++] = month_len;
                payload[index++] = firstTimeConfigData.configMonth;        

                //Config Year
                uint8_t year_len = 1;
                ESP_LOGI(FIRST_TIME_TAG, "Config Year: %d", firstTimeConfigData.configYear);
                payload[index++] = year_len;
                payload[index++] = firstTimeConfigData.configYear;        

                //Total Rated Power Enum
                uint8_t trp_len = strlen(firstTimeConfigData.totalRatedPowerValue);
                ESP_LOGI(FIRST_TIME_TAG, "Total Rated Power: %s (len=%d)", firstTimeConfigData.totalRatedPowerValue, trp_len);
                payload[index++] = trp_len;
                memcpy(&payload[index], firstTimeConfigData.totalRatedPowerValue, trp_len);
                index += trp_len;

                //Serial Number
                uint8_t serial_len = strlen(firstTimeConfigData.serialNumber);
                ESP_LOGI(FIRST_TIME_TAG, "Serial Number: %s (len=%d)", firstTimeConfigData.serialNumber, serial_len);
                payload[index++] = serial_len;
                memcpy(&payload[index], firstTimeConfigData.serialNumber, serial_len);
                index += serial_len;

                //Send the payload
                rc = os_mbuf_append(ctxt->om, payload, index);
                return rc == 0 ? 0 : BLE_ATT_ERR_INSUFFICIENT_RES;
            }
            //Write can be implemented here
            if (ctxt->op == BLE_GATT_ACCESS_OP_WRITE_CHR)
            {
                //Handle updates (SET commands based on your first byte inside ctxt->om->om_data)
                if (ctxt->om->om_len < 1)
                {
                    return BLE_ATT_ERR_INVALID_ATTR_VALUE_LEN;
                }

                uint8_t status = 0x00;                  //Default to failure
                uint8_t cmd = ctxt->om->om_data[0];

                switch(cmd)
                {
                    case SET_FIRST_TIME_DATA:                             // SET First Time
                    {
                        int index1 = 1;
                        if (ctxt->om->om_len >= 2)
                        {
                            // Receive Device Type
                            uint8_t len1 = ctxt->om->om_data[index1++];
                            if (len1 != 1){
                                ESP_LOGE("First-Time","Receive Device Type CASE FAILED len1");
                                goto code_failure;
                            }
                            firstTimeConfigData.deviceType = ctxt->om->om_data[index1++];
                            ESP_LOGI("First-Time","firstTimeConfigData.deviceType %d",firstTimeConfigData.deviceType);

                            //Read Time 
                            uint8_t time_len = ctxt->om->om_data[index1++];
                            ESP_LOGI("First-Time", "Parsed time_len = %d", time_len);
                            memcpy(firstTimeConfigData.time, &ctxt->om->om_data[index1], time_len);
                            firstTimeConfigData.time[time_len] = '\0';
                            ESP_LOGI("First-Time", "Parsed time is %s", firstTimeConfigData.time);
                            index1 += time_len;
                            
                            //Read Date 
                            uint8_t date_len = ctxt->om->om_data[index1++];
                            ESP_LOGI("First-Time", "Parsed date_len = %d", date_len);
                            if (index1 + date_len > ctxt->om->om_len) {
                                ESP_LOGE("First-Time", "Date length overflow! index1: %d, date_len: %d, total: %d",
                                        index1, date_len, ctxt->om->om_len);
                                goto code_failure;
                            }
                            memcpy(firstTimeConfigData.date, &ctxt->om->om_data[index1], date_len);
                            firstTimeConfigData.date[date_len] = '\0';
                            ESP_LOGI("First-Time", "Parsed date = %s", firstTimeConfigData.date);
                            index1 += date_len;
                            //ESP_LOG_BUFFER_HEX_LEVEL("ByteDump", &ctxt->om->om_data[index1], 30, ESP_LOG_INFO);

                            // Connector Types (can be 1 or 2)
                            uint8_t conn_len = ctxt->om->om_data[index1++];
                            if (conn_len < 1 || conn_len > 2) {
                                ESP_LOGE("First-Time","Invalid number of connectors: %d", conn_len);
                                goto code_failure;
                            }
                            firstTimeConfigData.num_connector = conn_len;

                            for (int i = 0; i < conn_len; i++) {
                                if (index1 >= ctxt->om->om_len) {
                                    ESP_LOGE("First-Time","Connector type overflow");
                                    goto code_failure;
                                }
                                firstTimeConfigData.connector_types[i] = ctxt->om->om_data[index1++];
                                ESP_LOGI("First-Time","Connector[%d] = %u", i, firstTimeConfigData.connector_types[i]);
}
                            
                            //Phases Types
                            uint8_t len5 = ctxt->om->om_data[index1++];
                            if (len5 != 1){ 
                                ESP_LOGE("First-Time","Phases Types CASE FAILED ");
                                goto code_failure;
                            }
                            firstTimeConfigData.noOfPhases = ctxt->om->om_data[index1++];
                            ESP_LOGI("First-Time","firstTimeConfigData.noOfPhases %d",firstTimeConfigData.noOfPhases);

                            //No of Power Converter Modules Types
                            uint8_t len6 = ctxt->om->om_data[index1++];
                            if (len6 != 1){ 
                                ESP_LOGE("First-Time","No of Power Converter Modules CASE FAILED ");
                                goto code_failure;
                            }
                            firstTimeConfigData.noOfPCM = ctxt->om->om_data[index1++];
                            ESP_LOGI("First-Time","firstTimeConfigData.noOfPCM %d",firstTimeConfigData.noOfPCM);

                            //Dynamic Power Sharing Types
                            uint8_t len7 = ctxt->om->om_data[index1++];
                            if (len7 != 1){  
                                goto code_failure;
                            }
                            firstTimeConfigData.dynamicPowerSharing = ctxt->om->om_data[index1++];
                            ESP_LOGI("First-Time","firstTimeConfigData.dynamicPowerSharing %d",firstTimeConfigData.dynamicPowerSharing);

                            //No of configMonth Types
                            uint8_t len8 = ctxt->om->om_data[index1++];
                            if (len8 != 1){  
                                goto code_failure;
                            }
                            firstTimeConfigData.configMonth = ctxt->om->om_data[index1++];
                            ESP_LOGI("First-Time","firstTimeConfigData.configMonth %d",firstTimeConfigData.configMonth);

                            uint8_t len9 = ctxt->om->om_data[index1++];
                            if (len9 != 1){  
                                goto code_failure;
                            }
                            firstTimeConfigData.configYear = ctxt->om->om_data[index1++];
                            ESP_LOGI("First-Time","firstTimeConfigData.configYear %d",firstTimeConfigData.configYear);

                            // Read Total Rated Power (enum)
                            uint8_t trp_len = ctxt->om->om_data[index1++];
                            ESP_LOGI("First-Time", "Parsed trp_len = %d", trp_len);
                            memcpy(firstTimeConfigData.totalRatedPowerValue, &ctxt->om->om_data[index1], trp_len);
                            firstTimeConfigData.totalRatedPowerValue[trp_len] = '\0';
                            ESP_LOGI("First-Time", "Parsed Serial Number = %s", firstTimeConfigData.totalRatedPowerValue);
                            index1 += trp_len;

                            //Use mutex here
                            // if (xSemaphoreTake(mutex_handle, (TickType_t)0) == pdTRUE) {
                            //     memcpy(firstTimeConfigData_to_LLC.totalRatedPowerValue,firstTimeConfigData.totalRatedPowerValue,sizeof(firstTimeConfigData.totalRatedPowerValue));
                            //     xSemaphoreGive(mutex_handle);
                            // } else {
                            //     ESP_LOGW("MUTEX", "Failed to acquire mutex");
                            // }
                            
                            //Read Serial Number 
                            uint8_t serial_len = ctxt->om->om_data[index1++];
                            ESP_LOGI("First-Time", "Parsed serial_len = %d", serial_len);
                            memcpy(firstTimeConfigData.serialNumber, &ctxt->om->om_data[index1], serial_len);
                            firstTimeConfigData.serialNumber[serial_len] = '\0';
                            ESP_LOGI("First-Time", "Parsed Serial Number = %s", firstTimeConfigData.serialNumber);
                            index1 += serial_len;
                            firstTimeConfigData.ftcSettingFlagDone = 1;
                            
                            save_firstTimeConfig_to_nvs();
                            //save_config_to_nvs(); 
                            send_firstTime_config_status_notification(conn_handle, notify_first_time_handle, SET_FIRST_TIME_DATA, 1);
                            break;

                        code_failure:
                            send_firstTime_config_status_notification(conn_handle, notify_first_time_handle, SET_FIRST_TIME_DATA, 0);
                            break;
                        }
                        else {
                            send_firstTime_config_status_notification(conn_handle, notify_first_time_handle, SET_FIRST_TIME_DATA, 0);
                        }
                        break;
                    } 
                }
            }
            break;
        }
        
        case SECURITY_SETTING_CHR:
        {
            //Read is being Implemented here
            if (ctxt->op == BLE_GATT_ACCESS_OP_READ_CHR)
            {
                load_security_setting_from_nvs();
                uint8_t index = 0;
                // Log reading
                ESP_LOGI(SECURITY_TAG, "\033[95mReading SECURITY_SETTING_CHR for conn_handle=%d\033[0m", conn_handle);
                
                //Offline Pin
                uint8_t offlinePinlen = strlen(securitySetting.offline_pin);
                ESP_LOGI(SECURITY_TAG, "\033[95mRead Offline Pin : %s with length = %d\033[0m", securitySetting.offline_pin, offlinePinlen);
                payload[index++] = offlinePinlen;
                memcpy(&payload[index], securitySetting.offline_pin, offlinePinlen);
                index += offlinePinlen;

                //FTU Mode
                uint8_t ftuModeLen = 1;
                ESP_LOGI(SECURITY_TAG, "\033[95mFTU Mode: %d\033[0m", securitySetting.ftuMode);
                payload[index++] = ftuModeLen;      
                payload[index++] = securitySetting.ftuMode;

                //Charger Status 
                uint8_t chargerModelen = 1;
                ESP_LOGI(SECURITY_TAG, "\033[95mCharger Mode: %d\033[0m", securitySetting.chargerMode);
                payload[index++] = chargerModelen;      
                payload[index++] = securitySetting.chargerMode;

                //Send the payload
                rc = os_mbuf_append(ctxt->om, payload, index);
                return rc == 0 ? 0 : BLE_ATT_ERR_INSUFFICIENT_RES;
            }
            //Write can be implemented here
            if (ctxt->op == BLE_GATT_ACCESS_OP_WRITE_CHR)
            {
                //Handle updates (SET commands based on your first byte inside ctxt->om->om_data)
                if (ctxt->om->om_len < 1)
                    return BLE_ATT_ERR_INVALID_ATTR_VALUE_LEN;
                
                uint8_t status = 0x00;                  //Default to failure
                uint8_t cmd = ctxt->om->om_data[0];

                switch(cmd)
                {
                    case SET_SECURITY_SETTING:                             // SET First Time
                    {
                        int index1 = 1;
                        if (ctxt->om->om_len >= 2)
                        {
                            //Receive Offline pin
                            uint8_t offline_pin_len = ctxt->om->om_data[index1++];
                            if (offline_pin_len < 1){
                                ESP_LOGE(SECURITY_TAG,"Received Offline Pin Length CASE FAILED");
                                goto security_failure;
                            }
                            ESP_LOGI(SECURITY_TAG, "Parsed offline_pin_len = %d", offline_pin_len);
                            memcpy(securitySetting.offline_pin, &ctxt->om->om_data[index1], offline_pin_len);
                            securitySetting.offline_pin[offline_pin_len] = '\0';
                            ESP_LOGI(SECURITY_TAG, "\033[95mOffline Pin is %s\033[0m", securitySetting.offline_pin);
                            index1 += offline_pin_len;

                            //FTU Mode
                            uint8_t ftu_mode_len = ctxt->om->om_data[index1++];
                            if (ftu_mode_len != 1){ 
                                ESP_LOGE(SECURITY_TAG,"FTU Mode Length CASE FAILED");
                                goto security_failure;
                            }
                            ESP_LOGI(SECURITY_TAG, "Parsed ftu_mode_len = %d", ftu_mode_len);
                            securitySetting.ftuMode = ctxt->om->om_data[index1++];
                            ESP_LOGI(SECURITY_TAG, "\033[95msecuritySetting.ftu_mode %d\033[0m", securitySetting.ftuMode);

                            //Authentication Flag would be set to 1 if FTU Mode is enabled
                            if (securitySetting.ftuMode == ENABLED) {
                                chargerConfiguration.authenticationFlag = 1; // Enable authentication
                                save_config_to_nvs();
                                ESP_LOGI(SECURITY_TAG, "Authentication enabled for conn_handle=%d", conn_handle);
                            }
                            else if (securitySetting.ftuMode == DISABLED)
                            {
                                chargerConfiguration.authenticationFlag = 0; // Disable authentication
                                ESP_LOGE("AUTH_DEBUG", "In SERVICES TASK authontication flag is zero\n");
                                save_config_to_nvs();
                                ESP_LOGI(SECURITY_TAG, "Authentication Disabled for conn_handle=%d", conn_handle);
                            }

                            //Charger Mode 
                            uint8_t charger_mode_len = ctxt->om->om_data[index1++];
                            if (charger_mode_len != 1){ 
                                ESP_LOGE(SECURITY_TAG,"Charger Mode Length CASE FAILED");
                                goto security_failure;
                            }
                            ESP_LOGI(SECURITY_TAG, "Parsed charger_mode_len = %d", charger_mode_len);
                            securitySetting.chargerMode = ctxt->om->om_data[index1++];
                            ESP_LOGI(SECURITY_TAG, "\033[95msecuritySetting.chargerMode %d\033[0m", securitySetting.chargerMode);
                            
                            // if(securitySetting.chargerMode == ONLINE){
                            //     esp_restart();
                            // }

                            //Saving the Settings to NVS
                            save_security_setting_to_nvs();
                            send_security_setting_notification(conn_handle, notify_security_setting_handle, SET_SECURITY_SETTING, 1);
                            break;

                        security_failure:
                            send_security_setting_notification(conn_handle, notify_security_setting_handle, SET_SECURITY_SETTING, 0);
                            break;
                        }
                        else {
                            send_security_setting_notification(conn_handle, notify_security_setting_handle, SET_SECURITY_SETTING, 0);
                        }
                        break;
                    } 
                }
            }
            break;
        }
        
        default:
            break;
        }
        return 0;
}
