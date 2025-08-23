
#include "../../main/main.h"

char *SAVE_TAG = "Save_NVS";
#define WIFI_NAMESPACE "wifi_creds"

/**
 * @brief Save Wi-Fi credentials (SSID and Password) into NVS
 * 
 * @param ssid Wi-Fi SSID string
 * @param password Wi-Fi Password string
 * @return esp_err_t ESP_OK if saved successfully, error code otherwise
 */
esp_err_t save_wifi_credentials_nvs(const char *ssid, const char *password)
{
    nvs_handle_t nvs_handle;
    esp_err_t err;

    // Open NVS storage with read-write mode
    err = nvs_open(WIFI_NAMESPACE, NVS_READWRITE, &nvs_handle);
    if (err != ESP_OK) {
        ESP_LOGE(SAVE_TAG, "Error opening NVS: %s", esp_err_to_name(err));
        return err;
    }

    // Save SSID
    err = nvs_set_str(nvs_handle, "ssid", ssid);
    if (err != ESP_OK) {
        ESP_LOGE(SAVE_TAG, "Failed to write SSID: %s", esp_err_to_name(err));
        nvs_close(nvs_handle);
        return err;
    }

    // Save Password
    err = nvs_set_str(nvs_handle, "password", password);
    if (err != ESP_OK) {
        ESP_LOGE(SAVE_TAG, "Failed to write Password: %s", esp_err_to_name(err));
        nvs_close(nvs_handle);
        return err;
    }

    // Commit changes
    err = nvs_commit(nvs_handle);
    if (err != ESP_OK) {
        ESP_LOGE(SAVE_TAG, "Failed to commit NVS: %s", esp_err_to_name(err));
    } else {
        ESP_LOGI(SAVE_TAG, "Wi-Fi credentials saved successfully (SSID: %s)", ssid);
        ESP_LOGI(SAVE_TAG, "Wi-Fi credentials saved successfully (Password: %s)", password);
    }

    nvs_close(nvs_handle);
    return err;
}

//Function to Save First Time Configuration into NVS 
void save_firstTimeConfig_to_nvs(void)
{
    nvs_handle_t nvs_handle;
    esp_err_t err = nvs_open("FTC", NVS_READWRITE, &nvs_handle);
    if (err != ESP_OK) 
        return;
    
    //First Time Characterstics
    nvs_set_i8(nvs_handle, "firstDevice", firstTimeConfigData.deviceType);
    nvs_set_str(nvs_handle, "firstTime", firstTimeConfigData.time);
    nvs_set_str(nvs_handle, "firstDate", firstTimeConfigData.date);
    // Save number of connectors
    nvs_set_i8(nvs_handle, "firstNumConn", firstTimeConfigData.num_connector);
    // Save connector types array
    nvs_set_blob(nvs_handle, "firstConnec", firstTimeConfigData.connector_types, firstTimeConfigData.num_connector);
    nvs_set_i8(nvs_handle, "firstPhase", firstTimeConfigData.noOfPhases);
    nvs_set_i8(nvs_handle, "firstPCM", firstTimeConfigData.noOfPCM);
    nvs_set_i8(nvs_handle, "firstDPS", firstTimeConfigData.dynamicPowerSharing);
    nvs_set_i8(nvs_handle, "firstConfM", firstTimeConfigData.configMonth);
    nvs_set_i8(nvs_handle, "firstConfY", firstTimeConfigData.configYear);
    nvs_set_str(nvs_handle, "firstTRP", firstTimeConfigData.totalRatedPowerValue);
    nvs_set_str(nvs_handle, "firstSeri", firstTimeConfigData.serialNumber);
    nvs_set_i8(nvs_handle, "firstFlag", firstTimeConfigData.ftcSettingFlagDone);

    // Commit changes
    err = nvs_commit(nvs_handle);
    if (err != ESP_OK) {
        ESP_LOGE(SAVE_TAG, "Error committing FTC settings to NVS: %s", esp_err_to_name(err));
    }
    else {
        ESP_LOGI(SAVE_TAG, "FTC settings saved successfully");
        ESP_LOGW(SAVE_TAG, "FTC Flag: %d", firstTimeConfigData.ftcSettingFlagDone);
    }
    // Close the NVS handle
    nvs_close(nvs_handle);
    return;
}

// New Functions Created for NVS Security Settings
void save_security_setting_to_nvs(void){
    nvs_handle_t nvs_handle;
    esp_err_t err = nvs_open("security", NVS_READWRITE, &nvs_handle);
    if (err != ESP_OK) return;

    // Save security settings
    nvs_set_str(nvs_handle, "offline_pin", securitySetting.offline_pin);

    nvs_set_u8(nvs_handle, "ftuMode", securitySetting.ftuMode);
    nvs_set_u8(nvs_handle, "chargerMode", securitySetting.chargerMode);

    // Commit changes
    err = nvs_commit(nvs_handle);
    if (err != ESP_OK) {
        ESP_LOGE(SAVE_TAG, "Failed to commit security settings: %s", esp_err_to_name(err));
    }

    nvs_close(nvs_handle);
}

void save_server_settings_to_nvs(void)
{
    nvs_handle_t nvs_handle;
    esp_err_t err = nvs_open("server", NVS_READWRITE, &nvs_handle);
    if (err != ESP_OK) return;

    // Save server settings
    nvs_set_str(nvs_handle, "url", connectionConfiguration.url_str);
    nvs_set_str(nvs_handle, "host", connectionConfiguration.host_str);
    nvs_set_str(nvs_handle, "port", connectionConfiguration.port_str);
    nvs_set_str(nvs_handle, "tariff", connectionConfiguration.tariff_str);
    nvs_set_u8(nvs_handle, "OcppSecurity", connectionConfiguration.ocpp_security);
    nvs_set_u8(nvs_handle, "OcppVersion", connectionConfiguration.ocpp_version);

    // Commit changes
    err = nvs_commit(nvs_handle);
    if (err != ESP_OK) {
        ESP_LOGE(SAVE_TAG, "Error committing server settings to NVS: %s", esp_err_to_name(err));
    }
    else{
        ESP_LOGI(SAVE_TAG, "Saved to the NVS");
    }

    // Close the NVS handle
    nvs_close(nvs_handle);
}

void save_config_to_nvs(void)
{
    nvs_handle_t nvs_handle;
    esp_err_t err = nvs_open("storage", NVS_READWRITE, &nvs_handle);
    if (err != ESP_OK) return;
    
    //Charger Configuration
    nvs_set_u8(nvs_handle, "AuthFlag", chargerConfiguration.authenticationFlag);
    
    nvs_commit(nvs_handle);
    nvs_close(nvs_handle);
}