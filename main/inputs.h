#ifndef INPUT_TASK_H
#define INPUT_TASK_H

#include <string.h>
#include <stdio.h>
#include <stdbool.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_log.h"

#ifdef __cplusplus
extern "C" {
#endif


void InputTask(void *param);

#ifdef __cplusplus
}
#endif

#endif /* INPUT_TASK_H */
