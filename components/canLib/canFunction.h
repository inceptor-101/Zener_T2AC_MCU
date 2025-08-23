#ifndef CAN_FUNCTION_H
#define CAN_FUNCTION_H

#include "../../main/main.h" 

#define TX_GPIO_NUM         32                                      // TWAI TX Pin
#define RX_GPIO_NUM         33                                      // TWAI RX Pin

//CAN Id
#define CAN_ID_TO_LLC       0x31
#define LLC_to_MCU          0x21

//Function prototypes
extern void can_task(void *pvParameters);
extern void twai_setup();

#endif // CAN_FUNCTION_H