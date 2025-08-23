#include "../../main/main.h"

static const char *FETCH_NVS = "Fetch_NVS";
#define WIFI_NAMESPACE "wifi_creds"

/**
 * Fetch Wi-Fi credentials from NVS
 */
esp_err_t load_wifi_credentials_nvs(char *ssid, size_t ssid_size, char *password, size_t pass_size) {
    nvs_handle_t nvs_handle;
    esp_err_t err;

    err = nvs_open(WIFI_NAMESPACE, NVS_READONLY, &nvs_handle);
    if (err != ESP_OK) {
        ESP_LOGE(FETCH_NVS, "Error opening NVS handle: %s", esp_err_to_name(err));
        return err;
    }

    err = nvs_get_str(nvs_handle, "ssid", ssid, &ssid_size);
    if (err != ESP_OK) {
        ESP_LOGE(FETCH_NVS, "Failed to get SSID: %s", esp_err_to_name(err));
        nvs_close(nvs_handle);
        return err;
    }

    err = nvs_get_str(nvs_handle, "password", password, &pass_size);
    if (err != ESP_OK) {
        ESP_LOGE(FETCH_NVS, "Failed to get password: %s", esp_err_to_name(err));
        nvs_close(nvs_handle);
        return err;
    }

    ESP_LOGI(FETCH_NVS, "Fetched SSID: %s", ssid);
    ESP_LOGI(FETCH_NVS, "Fetched Password: %s", password);

    nvs_close(nvs_handle);
    return ESP_OK;
}

void load_first_time_config_from_nvs(void) {
    nvs_handle_t nvs_handle;
    esp_err_t err = nvs_open("FTC", NVS_READONLY, &nvs_handle);
    if (err != ESP_OK) {
        ESP_LOGE(FETCH_NVS, "Failed to open NVS: %s", esp_err_to_name(err));
        return;
    }

    // Read the flag first
    err = nvs_get_i8(nvs_handle, "firstFlag", (int8_t *)&firstTimeConfigData.ftcSettingFlagDone);
    if (err != ESP_OK) {
        ESP_LOGW(FETCH_NVS, "Flag not found, using defaults");
        nvs_close(nvs_handle);
        return;   // Nothing more to read
    }

    // If flag is not set (not 1), then skip reading other fields
    if (firstTimeConfigData.ftcSettingFlagDone != 1) {
        ESP_LOGI(FETCH_NVS, "First-time config flag not set. Using defaults.");
        nvs_close(nvs_handle);
        return;
    }

    // --- If flag == 1, then load other values ---
    nvs_get_i8(nvs_handle, "firstDevice",   (int8_t *)&firstTimeConfigData.deviceType);
    nvs_get_i8(nvs_handle, "firstNumConn",  (int8_t *)&firstTimeConfigData.num_connector);
    nvs_get_i8(nvs_handle, "firstConnec",   (int8_t *)&firstTimeConfigData.connector_types);
    nvs_get_i8(nvs_handle, "firstPhase",    (int8_t *)&firstTimeConfigData.noOfPhases);
    nvs_get_i8(nvs_handle, "firstPCM",      (int8_t *)&firstTimeConfigData.noOfPCM);
    nvs_get_i8(nvs_handle, "firstDPS",      (int8_t *)&firstTimeConfigData.dynamicPowerSharing);
    nvs_get_i8(nvs_handle, "firstConfM",    (int8_t *)&firstTimeConfigData.configMonth);
    nvs_get_i8(nvs_handle, "firstConfY",    (int8_t *)&firstTimeConfigData.configYear);

    // Strings
    size_t required_size;

    required_size = sizeof(firstTimeConfigData.totalRatedPowerValue);
    nvs_get_str(nvs_handle, "firstTRP", firstTimeConfigData.totalRatedPowerValue, &required_size);

    required_size = sizeof(firstTimeConfigData.time);
    nvs_get_str(nvs_handle, "firstTime", firstTimeConfigData.time, &required_size);

    required_size = sizeof(firstTimeConfigData.date);
    nvs_get_str(nvs_handle, "firstDate", firstTimeConfigData.date, &required_size);

    required_size = sizeof(firstTimeConfigData.serialNumber);
    nvs_get_str(nvs_handle, "firstSeri", firstTimeConfigData.serialNumber, &required_size);

    nvs_close(nvs_handle);
}

void load_security_setting_from_nvs(void) {
    nvs_handle_t nvs_handle;
    esp_err_t err = nvs_open("security", NVS_READONLY, &nvs_handle);
    if (err != ESP_OK) {
        ESP_LOGE("NVS", "Failed to open NVS: %s", esp_err_to_name(err));
        return;
    }

    size_t required_size;

    // Read the offline pin
    required_size = sizeof(securitySetting.offline_pin);
    err = nvs_get_str(nvs_handle, "offline_pin", securitySetting.offline_pin, &required_size);
    if (err == ESP_OK) {
        //ESP_LOGI("NVS", "Offline Pin: %s (len=%d)", securitySetting.offline_pin, strlen(securitySetting.offline_pin));
    } else {
        ESP_LOGE("NVS", "Failed to read offline pin: %s", esp_err_to_name(err));
    }

    //Read the FTU Mode Settings 
    err = nvs_get_u8(nvs_handle, "ftuMode", &securitySetting.ftuMode);
    if (err == ESP_OK) {    
        //ESP_LOGI("NVS", "FTU Mode: %d", securitySetting.ftuMode);
    } else {
        ESP_LOGE("NVS", "Failed to read FTU Mode: %s", esp_err_to_name(err));
    }

    //Read the Charger Mode 
    err = nvs_get_u8(nvs_handle, "chargerMode", &securitySetting.chargerMode);
    if (err == ESP_OK) {    
        //ESP_LOGI("NVS", "chargerMode: %d", securitySetting.chargerMode);
    } else {
        ESP_LOGE("NVS", "Failed to read chargerMode : %s", esp_err_to_name(err));
    }

    nvs_close(nvs_handle);
}

void load_server_settings_from_nvs(void)
{
    nvs_handle_t nvs_handle;
    esp_err_t err = nvs_open("server", NVS_READONLY, &nvs_handle);
    if (err != ESP_OK) {
        ESP_LOGE("NVS", "Failed to open NVS: %s", esp_err_to_name(err));
        return;
    }

    size_t required_size;
    
    // Load URL
    required_size = sizeof(connectionConfiguration.url_str);
    err = nvs_get_str(nvs_handle, "url", connectionConfiguration.url_str, &required_size);
    if (err != ESP_OK) {
        ESP_LOGE("NVS", "Failed to read URL: %s", esp_err_to_name(err));
    }

    // Load Host
    required_size = sizeof(connectionConfiguration.host_str);
    err = nvs_get_str(nvs_handle, "host", connectionConfiguration.host_str, &required_size);
    if (err != ESP_OK) {
        ESP_LOGE("NVS", "Failed to read Host: %s", esp_err_to_name(err));
    }

    // Load Port
    required_size = sizeof(connectionConfiguration.port_str);
    err = nvs_get_str(nvs_handle, "port", connectionConfiguration.port_str, &required_size);
    if (err != ESP_OK) {
        ESP_LOGE("NVS", "Failed to read Port: %s", esp_err_to_name(err));
    }

    // Load ocpp_security
    err = nvs_get_u8(nvs_handle, "OcppSecurity", &connectionConfiguration.ocpp_security);
    if (err != ESP_OK) {
        ESP_LOGE("NVS", "Failed to read OcppSecurity: %s", esp_err_to_name(err));
    }

    // Load ocpp_version
    err = nvs_get_u8(nvs_handle, "OcppVersion", &connectionConfiguration.ocpp_version);
    if (err != ESP_OK) {
        ESP_LOGE("NVS", "Failed to read OcppVersion: %s", esp_err_to_name(err));
    }

    // Load Tariff
    required_size = sizeof(connectionConfiguration.tariff_str);
    err = nvs_get_str(nvs_handle, "tariff", connectionConfiguration.tariff_str, &required_size);
    if (err != ESP_OK) {
        ESP_LOGE("NVS", "Failed to read Tariff: %s", esp_err_to_name(err));
    }
    nvs_close(nvs_handle);
}

void load_config_from_nvs(void)
{
    nvs_handle_t nvs_handle;
    esp_err_t err = nvs_open("storage", NVS_READONLY, &nvs_handle);
    if (err != ESP_OK) return;

    //Meter Type 
    nvs_get_u8(nvs_handle, "AuthFlag", &chargerConfiguration.authenticationFlag);
    
    nvs_close(nvs_handle);
}

