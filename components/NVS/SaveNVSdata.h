extern void save_firstTimeConfig_to_nvs(void);
extern void save_security_setting_to_nvs(void);
extern void save_server_settings_to_nvs(void);
extern void save_config_to_nvs(void);
extern esp_err_t save_wifi_credentials_nvs(const char *ssid, const char *password);