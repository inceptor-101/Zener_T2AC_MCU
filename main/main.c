/*
    Author - Dhruv Joshi
    Date - 2024-01-01
    Description - This code initializes a GPTimer to toggle a GPIO pin every 1 ms.
                 It also creates a CAN task that runs on core 1, which can be used for CAN communication.

    Also this code is based on the ESP-IDF framework and uses FreeRTOS for task management and creatinon of 
    timer tasks.
*/

#include "main.h"
#include "esp_log.h"
#include "esp_timer.h"
#define NOT_FAULTY(status)  ((status) != FAULTED)

TaskHandle_t can_task_handle = NULL; // Will store handle of CAN task
TaskHandle_t main_task_handle = NULL;// Will store handle of main task which is for intelligence 
TaskHandle_t timer_task_1_handler = NULL;
SemaphoreHandle_t mutex_handle;

connector_status_t status = AVAILABLE;

uint16_t global_conn_handle = BLE_HS_CONN_HANDLE_NONE;
bool wifiInitialised = false;
bool wifi_connected = false;    
uint8_t isr_counter =0;

//Global Variables to Store the SSID and Password
char connected_ssid[32];     // Max SSID length is 32 + 1 for null
char connected_password[32]; // Max password length is 64 + 1 for null

//++++++++ Declaration Of Variables for CAN Communication ++++++++++
LLC_Data_t Data_Sensed_LLC;  //This takes data from LLC and Stores it in this structure
LLC_Data_t Data_Sensed_LLC_IPC; //This takes data from LLC and Stores it in this structure for IPC Communication
LLC_Data_t Data_Sensed_LLC_Core0; //This takes data from LLC and Stores it in this structure for Core 1 IPC Communication

//We have initialized the structure with some Dummy Values
firstTimeData_t firstTimeConfigData = {
    .deviceType = TYPE2AC,
    .num_connector = 2,
    .connector_types = {6, 6},  
    .noOfPhases = SINGLE_PHASE_AC,
    .noOfPCM = 0,
    .dynamicPowerSharing = DISABLED,
    .ftcSettingFlagDone = 0,
    .serialNumber = "ZT-Ph1T2AC"
};

securityConfig_t securitySetting = {
    .chargerMode = OFFLINE,
    .ftuMode = ENABLED,
    .offline_pin = "486486"
};

connectionConfiguration_t connectionConfiguration = {
    .url_str = "192.168.***.***",
    .host_str = "ocpp.zenergize.data.com",
    .port_str = "300",
    .ocpp_security = 0,
    .ocpp_version = 1
};

chargerConfiguration_t chargerConfiguration = {
    .authenticationFlag = 1
};

WiFiServiceStruct WifiService ={
    .LAP = DISABLE,
    .CAP = DISABLE,
    .GAP = DISABLE,
    .SSID_No = 0,
    .password_length = 0,
    .password_received = {},
};

//Variables for calculating Connector Statuses in intelligent code
connector_status_t noOfConnectors[2];

bool determine_authentication_status();
bool determine_faulty_connector();
bool determine_unavailable_connector();

/**
 * @brief GPTimer callback (ISR context)
 * 
 * This function runs every 1 ms.
 * - Toggles a GPIO (1 ms high, 1 ms low → 2 ms period)
 * - Notifies CAN task (runs on core 0)
 */
static bool IRAM_ATTR core_timer_callback(gptimer_handle_t timer, 
                                     const gptimer_alarm_event_data_t *edata, 
                                     void *user_ctx)
{
    static bool level = false;
    static uint32_t counter100ms = 0;  

    // Notify CAN task (ISR-safe API)
    vTaskNotifyGiveFromISR(can_task_handle, NULL);

    // Count up to 100 ms
    counter100ms++;
    if (counter100ms >= 100) {
        counter100ms = 0;
        // Notify another task (ISR-safe API)
        vTaskNotifyGiveFromISR(main_task_handle, NULL);
    }

    return true; // Keep the alarm running (auto-reload)
}

static bool IRAM_ATTR timer_group0_isr(gptimer_handle_t timer, const gptimer_alarm_event_data_t *edata, void *user_data){
    // Increment the counter each time the interrupt is called
    isr_counter++;
    // return false to indicate no need to yield at the end of ISR

    if (timer_task_1_handler != NULL) {
        xTaskNotifyGive(timer_task_1_handler);
    }
    return false;
}

///@brief Initializes a general-purpose timer and creates a task for timer operations to be used in WiFi
static void timer_initialize() {
    // configMINIMAL_STACK_SIZE * 6 ensures enough stack size for the task
    xTaskCreatePinnedToCore(timer_task_1, "timer_task_1", configMINIMAL_STACK_SIZE * 6, NULL, 1, &timer_task_1_handler, 1);
    
    ESP_LOGI("TIMER", "Create timer handle");
    gptimer_handle_t gptimer = NULL;
    gptimer_config_t timer_config = {
        .clk_src = GPTIMER_CLK_SRC_DEFAULT,
        .direction = GPTIMER_COUNT_UP,
        .resolution_hz = 1000000, // 1MHz, 1 tick = 1us
    };
    ESP_ERROR_CHECK(gptimer_new_timer(&timer_config, &gptimer));

    gptimer_event_callbacks_t cbs = {
        .on_alarm = timer_group0_isr,
    };
    ESP_ERROR_CHECK(gptimer_register_event_callbacks(gptimer, &cbs, NULL));

    ESP_LOGI("TIMER", "Enable timer");
    ESP_ERROR_CHECK(gptimer_enable(gptimer));

    ESP_LOGI("TIMER", "Start timer with 1ms alarm interval");
    gptimer_alarm_config_t alarm_config = {
        .alarm_count = 100000, // period = 100ms (100000us)
        .flags.auto_reload_on_alarm = true,
    };
    ESP_ERROR_CHECK(gptimer_set_alarm_action(gptimer, &alarm_config));
    ESP_ERROR_CHECK(gptimer_start(gptimer));
}

/**
 * @brief Main task
 * 
 * Waits for notifications from the ISR every 1 ms.
 * Here, you can place CAN processing code.
 */
static void main_task(void *arg)
{
    static uint32_t main_tick_count = 0; // Counter for number of notifications received

    while (1) {
        // Block until ISR sends notification
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        
        //Mutex to take data from IPC into the main task
        if(xSemaphoreTake(mutex_handle, pdMS_TO_TICKS(0)) == pdTRUE) {
            // ---- Critical Section ----
            memcpy(&Data_Sensed_LLC_Core0, &Data_Sensed_LLC_IPC, sizeof(Data_Sensed_LLC_IPC));
            // ---- End Critical Section ----
            xSemaphoreGive(mutex_handle);
        } else {
            ESP_LOGW("MUTEX", "Timeout: Couldn't take mutex for IPC copy");
        }

        chargerConfiguration.authenticationFlag = determine_authentication_status();

        //Logic For Intelligence goes here 


    }
}

/**
 * @brief Configure GPIO for waveform output
 */
static void gpio_init(void)
{
 gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << EMERGENCY_STOP_PRESS),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .intr_type = GPIO_INTR_DISABLE  // No ISR
    };
    gpio_config(&io_conf);

    // 2. Configure INTERNET_LED as output
    gpio_config_t output_conf1 = {
        .pin_bit_mask = (1ULL << INTERNET_LED),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE  // Not used for output
    };
    gpio_config(&output_conf1);

    // Optional: Set initial level LOW or HIGH
    gpio_set_level(INTERNET_LED, 0);

    // 3. Configure SUPPLY_LED as output
    gpio_config_t output_conf2 = {
        .pin_bit_mask = (1ULL << SUPPLY_LED),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE  // Not used for output
    };
    gpio_config(&output_conf2);

    // Optional: Set initial level LOW or HIGH
    gpio_set_level(SUPPLY_LED, 0);  

    // 4. Configure  as output
    gpio_config_t output_conf3 = {
        .pin_bit_mask = (1ULL << FAULT_GUNB_LED),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE  // Not used for output
    };
    gpio_config(&output_conf3);

    // Optional: Set initial level LOW or HIGH
    gpio_set_level(FAULT_GUNB_LED, 0);  

        // 5. Configure SUPPLY_LED as output
    gpio_config_t output_conf4 = {
        .pin_bit_mask = (1ULL << CS_GUNB_LED),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE  // Not used for output
    };
    gpio_config(&output_conf4);

    // Optional: Set initial level LOW or HIGH
    gpio_set_level(CS_GUNB_LED, 0);  

        // 6. Configure SUPPLY_LED as output
    gpio_config_t output_conf5 = {
        .pin_bit_mask = (1ULL << FAULT_GUNA_LED),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE  // Not used for output
    };
    gpio_config(&output_conf5);

    // Optional: Set initial level LOW or HIGH
    gpio_set_level(FAULT_GUNA_LED, 0);  

        // 7. Configure SUPPLY_LED as output
    gpio_config_t output_conf6 = {
        .pin_bit_mask = (1ULL << CS_GUNA_LED),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE  // Not used for output
    };
    gpio_config(&output_conf6);

    // Optional: Set initial level LOW or HIGH
    gpio_set_level(CS_GUNA_LED, 0);  // Set LED ON initially
}

/**
 * @brief Configure and start GPTimer
 *
 * 1. Creates a new general-purpose timer instance.
 * 2. Configures the timer to count up with the resolution of 1 tick = 1 microseconds.
 * 3. Registers an ISR callback function to run when the timer alarm triggers.
 * 4. Enables the timer hardware.
 * 5. Sets an alarm to trigger every 1 ms (auto-reload enabled) using the value of TIMER_PERIOD_US.
  *    This means the timer will generate an interrupt every 1000 ticks (1 ms).
  *    The ISR toggles a GPIO pin and notifies the CAN task.
 * 6. Starts the timer so it begins counting and generating interrupts.
 */
static void gptimer_init_and_start(gptimer_handle_t *timer,
                                   uint32_t period_us,
                                   gptimer_alarm_cb_t callback)
{
    gptimer_config_t timer_config = {
        .clk_src = GPTIMER_CLK_SRC_DEFAULT, // Uses APB clock
        .direction = GPTIMER_COUNT_UP,
        .resolution_hz = 1000000,           // 1 tick = 1 us
    };
    ESP_ERROR_CHECK(gptimer_new_timer(&timer_config, timer));

    gptimer_event_callbacks_t cbs = {
        .on_alarm = callback,
    };
    ESP_ERROR_CHECK(gptimer_register_event_callbacks(*timer, &cbs, NULL));

    ESP_ERROR_CHECK(gptimer_enable(*timer));

    gptimer_alarm_config_t alarm_config = {
        .alarm_count = period_us,
        .reload_count = 0,
        .flags.auto_reload_on_alarm = true,
    };
    ESP_ERROR_CHECK(gptimer_set_alarm_action(*timer, &alarm_config));
    ESP_ERROR_CHECK(gptimer_start(*timer));
}


// ---------- minimal helper implementations (illustrative) ----------

/**
 * @brief Determine authentication status of the connector
 * 
 * Logic as per system design:
 * - If charger is OFFLINE:
 *      - If FTU (Free-To-Use) mode is ENABLED -> Authentication always succeeds
 *      - If FTU mode is DISABLED -> Local authentication required (RFID/PIN logic here)
 * - If charger is ONLINE:
 *      - If OCPP server grants access (remoteStart OR authorizeConfirmation) -> Authentication succeeds
 *      - Otherwise -> Authentication fails
 * 
 * @return true  -> Authentication successful
 * @return false -> Authentication failed
 */
bool determine_authentication_status()
{
    // if(securitySetting.chargerMode == OFFLINE)
    // {
    //     if(securitySetting.ftuMode == ENABLED)
    //     {
    //         //FTU Mode is Enabled, Hence Authentication is always Successful - 1
    //         return true;
    //     }
    //     else
    //     {
    //         //FTU Mode is Disabled, Hence authentication is required - 0
    //         //RFID or PIN based authentication Logic goes here
    //         return false;
    //     }
    // }
    // else
    // {
    //     //Online Mode, Hence authentication is required
    //     if(remoteStartStatusFromOCPPServer == 1 || authorizeConfirmationAcceptedFromOCPPServer == 1)
    //     {
    //         //Authentication is Successful
    //         return true;
    //     }
    //     else{
    //         // localGracefulTerminationByUser
    //         // localGracefulTerminationByEV
    //         // faultyTermination
    //         // remoteStopFromOCPPServer
    //         return false;
    //     }
    //     return false;
    // }
    return true;    // Just for testing 
}

bool determine_faulty_connector()
{
    return true;    // Just for testing
}


// Main Function
void app_main(void)
{
    /**
     * @section NVS
     * @brief Non-Volatile Storage Initialization
     *
     * NVS is mandatory for:
     * - Wi-Fi (storing SSID/password)
     * - BLE (bonding, pairing info, keys, calibration)
     * - Application data
     */
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());   /**< Erase NVS if corrupted or outdated */
        ESP_ERROR_CHECK(nvs_flash_init());    /**< Re-initialize NVS */
    }
    ble_init();  // ✅ BLE always runs (pinned to Core 0)

    //Load the First Time Configuration From NVS
    load_first_time_config_from_nvs();

    if (firstTimeConfigData.ftcSettingFlagDone != 1)
    {
        /**
         * 🚨 Case 1: Charger is NOT CONFIGURED
         *
         * - Do NOT initialize any peripherals (CAN, GPIO, timers, etc.)
         * - Only start BLE GATT Server so the mobile app can perform "First Time Configuration"
         * - Charger will remain in BLE mode until configuration is completed
         */
        ESP_LOGW("CONFIG", "First Time Configuration not done. Staying in BLE mode...");
    }
    else
    {
        /**
         * ✅ Case 2: Charger is CONFIGURED
         *
         * - Retrieve charger configuration from NVS (permanent memory)
         * - Possible parameters:
         *      - Single Gun or Dual Gun
         *      - Single Phase or Three Phase
         *      - Online or Offline
         *      - Rated Power
         *      - FTU enabled in Offline Mode
         */
        ESP_LOGW("CONFIG", "First Time Configuration done. Initializing peripherals...");
        load_security_setting_from_nvs();   // Read all saved config data

        // ✅ CAN setup (communication with LLC, MCU, etc.)
        twai_setup();

        // ✅ Initialize GPIO pin(s) used for toggling (Emergency Stop, LEDs)
        gpio_init();

        //Create a mutex for LLC Data IPC
        mutex_handle = xSemaphoreCreateMutex();
        if (mutex_handle == NULL) {
            ESP_LOGE("MUTEX", "Failed to create LLC Data mutex");
        }

        /**
         * 🔄 Task Assignments Across Cores
         *
         * - CAN Task → pinned to Core 1
         * - (Future) RFID Task → planned on Core 1
         * - Main Application Task → pinned to Core 0
         * - BLE Host Task → already pinned to Core 0 inside ble_init()
         */
        xTaskCreatePinnedToCore(can_task, "can_task", 2048, NULL, 5, &can_task_handle, 1);
        xTaskCreatePinnedToCore(main_task, "main_task", 4096, NULL, 5, &main_task_handle, 0);

        /**
         * 🕒 Timer Setup
         * Update Dhruv - Use only 1 timer 
         * Core Timer → 1 ms period (high-frequency tasks, e.g., real-time sensing)      
         */
        gptimer_handle_t core_timer;
        gptimer_init_and_start(&core_timer, 1000, core_timer_callback);      // 1 ms
        /**
         * Offline Mode Configuration (retrieved from NVS):
         * - FTU mode or Authenticated mode
         * - Offline current & power limit per connector (defaults = max)
         * - RFID-A, RFID-B, Offline PIN (defaults = empty/null)
         * - Rated Power
         *
         * Online Mode Configuration (retrieved from NVS):
         * - Internet + OCPP connection establishment
         * - SSID (default empty)
         * - Password (default empty)
         * - connectionURL (default empty)
         * - Host (default empty)
         * - Port Number (default empty)
         * - WebSocket connection type (default empty)
         *
         * These values guide how CAN, MQTT, Wi-Fi, and Charging Logic will initialize.
         */
        if(securitySetting.chargerMode == ONLINE) {
            ESP_LOGI("MODE", "Operating in ONLINE mode");   // Logging for the online mode cnfg

            // Loading the default wifi credentials from the non volatile storage 
            esp_err_t fetched_default_wifi_creds = load_wifi_credentials_nvs(connected_ssid, sizeof(connected_ssid), connected_password, sizeof(connected_password));

            // If successfully fetched 
            if(fetched_default_wifi_creds == ESP_OK) {  
                ESP_LOGI("WiFi", "SSID: %s", connected_ssid);   // Printing the fetched SSID
                ESP_LOGI("WiFi", "Password: %s", connected_password);   // Printing the fetched  password 
            // If failed to fetch 
            } else {
                ESP_LOGW("WiFi", "No credentials found in NVS");    // Could not access the creds
            }
            
            // Loading the OCPP port and related parameters from the nvs flash memory 
            load_server_settings_from_nvs();

            initialise_wifi();
            // We will initialise a wifi task which is pinned to core 0
            xTaskCreatePinnedToCore(
                wifi_task_core0, 
                "WiFi-Task", 
                4096, NULL, 
                0, 
                NULL, 
                0
            ); 
            // timer_initialize();
        }else if(securitySetting.chargerMode == OFFLINE) {
            ESP_LOGI("MODE", "Operating in OFFLINE mode");
            if(securitySetting.ftuMode == ENABLED) {
                ESP_LOGI("MODE", "FTU Mode is ENABLED");
            } else {
                ESP_LOGI("MODE", "FTU Mode is DISABLED, Hence we need authentication Mode");
            }
        }
    }
}
