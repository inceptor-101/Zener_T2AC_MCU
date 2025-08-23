#include "../../main/main.h"

// Notification state constants
#define NOT_SET 0
#define SEND_NAP    1
#define SEND_CONNECTION_RESULT    2
#define SEND_CONNECTION_STATUS    3
#define MIN_REQUIRED_MBUF         2

#define NOTIFY_WIFI_CHAR (notification_state >= 1 && notification_state <= 3)
#define NOTIFY (!(((notify_wifi_state || notify_indicate_enabled) && NOTIFY_WIFI_CHAR)))

extern void timer_task_1(void *pvParameters);
extern void initialise_wifi(void);