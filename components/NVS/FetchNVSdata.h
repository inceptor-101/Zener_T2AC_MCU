
extern void load_first_time_config_from_nvs(void);
extern void load_security_setting_from_nvs(void);
extern void load_server_settings_from_nvs(void);
extern void load_config_from_nvs(void);
extern esp_err_t load_wifi_credentials_nvs(char *ssid, size_t ssid_size, char *password, size_t pass_size);