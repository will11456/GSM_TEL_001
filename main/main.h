#ifndef APP_MAIN_H
#define APP_MAIN_H

#include <stdint.h>
#include <esp_err.h>
#include <stdio.h>
#include "driver/gpio.h"
#include "driver/i2c.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "freertos/semphr.h"
#include <esp_log.h>
#include "esp_system.h"


// CPU configuration
#ifndef APP_CPU_NUM
#define APP_CPU_NUM PRO_CPU_NUM
#endif




//
// Function Declarations
void GPIOInit(void);
void app_main(void);

#endif // APP_MAIN_H
