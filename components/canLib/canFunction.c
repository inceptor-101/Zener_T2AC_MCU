#include "CanFunction.h"
#include "freertos/task.h"
#include "freertos/timers.h"
#include "driver/gpio.h"
#include "driver/twai.h"
#include "esp_log.h"

#define TX_GPIO_NUM 32                                      // TWAI TX Pin
#define RX_GPIO_NUM 33                                      // TWAI RX Pin

int counterAck = 0;

static char *tag = "CAN";
twai_general_config_t g_config = TWAI_GENERAL_CONFIG_DEFAULT(TX_GPIO_NUM, RX_GPIO_NUM, TWAI_MODE_NORMAL);
twai_timing_config_t t_config = TWAI_TIMING_CONFIG_500KBITS(); //500 kbps
twai_filter_config_t f_config = TWAI_FILTER_CONFIG_ACCEPT_ALL();

void twai_setup() {
    g_config.tx_queue_len = 50;
	g_config.rx_queue_len = 50;
    ESP_ERROR_CHECK(twai_driver_install(&g_config, &t_config, &f_config));
    ESP_ERROR_CHECK(twai_start());
    ESP_LOGI(tag, "TWAI driver installed and started.");
}

void send_can_message(uint32_t id, uint8_t *data, size_t len){
    	twai_message_t message;
    message.extd = 0;
    message.identifier = id;  
    message.extd = 0;
    message.data_length_code = len;
    message.rtr = 0;
    
    // Copy data into the message structure
    for (size_t i = 0; i < len; i++) {
        message.data[i] = data[i];
    }
    
    if (twai_transmit(&message, pdMS_TO_TICKS(0)) == ESP_OK) {

		if(data[0] == 3){
			counterAck++;
		}
        //ESP_LOGW("EXAMPLE_TAG", "Message Transmitted: ID=0x%" PRIx32 ", DLC=%d", message.identifier, message.data_length_code);
    } else {
        //ESP_LOGW(EXAMPLE_TAG, "Failed to transmit message.");
    }
}

void send_evse_state_to_llc()
{
    uint8_t data[8] = {0};
    uint8_t evse_state = 1;

    data[0] = 0x01;         // Sequence number = 1
    data[1] = evse_state;   // Byte 2 = EVSE State (0 or 1)

    data[2] = 32; // Gun per phase current (32A)
    data[3] = 1; // Power Input Type (1 for Type 2 AC)
    data[4] = 1; //Authentication Flag

    for (int i = 5; i < 8; i++) {
        data[i] = 0xFF;
    }
    send_can_message(CAN_ID_TO_LLC, data, 8);  // Replace with actual CAN ID
}

void receive_can_message() {
	twai_message_t rx_msg;
    esp_err_t err = twai_receive(&rx_msg, pdMS_TO_TICKS(0));
    //ESP_LOGI("Inside receive","Inside Received");
    if (err == ESP_OK)
    {
        switch(rx_msg.identifier)
        {
            case LLC_to_MCU:
            {

                uint8_t seq = rx_msg.data[0];  // Sequence number
                switch (seq)
                {
                    case 1:
                        Data_Sensed_LLC.phaseA_voltage = ((rx_msg.data[1] << 8) | rx_msg.data[2]) / 10.0f;
                        Data_Sensed_LLC.phaseB_voltage = ((rx_msg.data[3] << 8) | rx_msg.data[4]) / 10.0f;
                        Data_Sensed_LLC.phaseC_voltage = ((rx_msg.data[5] << 8) | rx_msg.data[6]) / 10.0f;
                        Data_Sensed_LLC.temperature = rx_msg.data[7];  // Temperature in Celsius
                        Data_Sensed_LLC.llcDataStatus.seq1_received = true;
                        seq = 0;
                        break;

                    case 2:
                        Data_Sensed_LLC.phaseA_current = ((rx_msg.data[1] << 8) | rx_msg.data[2]) / 10.0f;
                        Data_Sensed_LLC.phaseB_current = ((rx_msg.data[3] << 8) | rx_msg.data[4]) / 10.0f;
                        Data_Sensed_LLC.phaseC_current = ((rx_msg.data[5] << 8) | rx_msg.data[6]) / 10.0f;
                    
                        //Extract only 1-bit (LSB) from byte 7
                        uint8_t statusByte = rx_msg.data[7];
                        //ESP_LOGI("CAN", "Received Status Byte: 0x%02X", statusByte);
                        Data_Sensed_LLC.chargingStatus = statusByte & 0x01;
                        //ESP_LOGI("CAN", "Charging Status: %d", Data_Sensed_LLC.chargingStatus);
                        Data_Sensed_LLC.llcDataStatus.seq2_received = true;
                        seq = 0;
                        break;

                    case 3:
                        Data_Sensed_LLC.rcmu            = (rx_msg.data[1]) / 10.0f;
                        Data_Sensed_LLC.ne_voltage      = (rx_msg.data[2]) / 10.0f;
                        Data_Sensed_LLC.cp_state        = rx_msg.data[3];
                        Data_Sensed_LLC.duty_cycle      = rx_msg.data[4];
                        Data_Sensed_LLC.connector_state = rx_msg.data[5];
                        Data_Sensed_LLC.instantaneous_power = ((rx_msg.data[6] << 8) | rx_msg.data[7]);
                        //ESP_LOGI(tag, "Power: %.2f W", Data_Sensed_LLC.instantaneous_power);
                        Data_Sensed_LLC.llcDataStatus.seq3_received = true;
                        seq = 0;
                        break;

                    case 4:
                    //data coming in 2 bytes
                        Data_Sensed_LLC.energyPerMinute = ((rx_msg.data[1] << 8) | rx_msg.data[2]) / 100.0f; // Convert to float
                        //ESP_LOGI(tag, "Energy Per Minute: %.2f Wh", Data_Sensed_LLC.energyPerMinute);
                        Data_Sensed_LLC.llcDataStatus.seq4_received = true;
                        break;

                    default:
                            break;
                }
                break;
            }
            default:
                break;
        }
    }
}

/**
 * @brief CAN task
 * 
 * Waits for notifications from the ISR every 1 ms.
 * Here, you can place CAN processing code.
 */
void can_task(void *arg)
{
    while (1) {
        // Block until ISR sends notification
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

        //Receive CAN message
        receive_can_message();

        //Mutex to be Added for The IPC Communication
         if(xSemaphoreTake(mutex_handle, pdMS_TO_TICKS(0)) == pdTRUE) {
            // ---- Critical Section ----
            memcpy(&Data_Sensed_LLC_IPC, &Data_Sensed_LLC, sizeof(Data_Sensed_LLC));
            // ---- End Critical Section ----
            xSemaphoreGive(mutex_handle);
        } else {
            ESP_LOGW("MUTEX", "Timeout: Couldn't take mutex for IPC copy");
        }
    }
}