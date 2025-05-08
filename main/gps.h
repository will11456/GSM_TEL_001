#ifndef GPS_H
#define GPS_H

#include <stdbool.h>
#include "freertos/queue.h"
#include "driver/uart.h"
#include <string.h>
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "esp_log.h"
#include <ctype.h> 
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "driver/uart.h"
#include "esp_log.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>   // for isprint()

// Mutex for GPS operations  
extern SemaphoreHandle_t gps_mutex; 

typedef struct {
    double time;
    double latitude;
    double longitude;
    double altitude;
    double hdop;
    int satellites_used;
} gps_data_t;

void gps_init(void);
gps_data_t gps_get_data(void);
bool gps_has_lock(void);
void report_gps_status(void);
void gps_task(void *arg);  
void gps_format_time_hhmm(float nmea_time, char *out, size_t max_len);


#endif // GPS_H
