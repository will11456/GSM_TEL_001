#ifndef TMP102_H_
#define TMP102_H_


#include "esp_err.h"
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/i2c.h"
#include "esp_log.h"


#ifdef __cplusplus
extern "C" {
#endif

esp_err_t tmp102_read_celsius(float *out_celsius);
void tmp102_task(void *pvParameters);



#ifdef __cplusplus
}
#endif

#endif /* TMP102_H_ */
