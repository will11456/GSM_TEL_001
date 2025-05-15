#ifndef HANDLER_H
#define HANDLER_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include "esp_err.h"
#include "freertos/queue.h"
#include "esp_err.h"
#include "sms_message.h"


#define SMS_SENDER_LEN 32
#define SMS_MESSAGE_LEN 160

typedef enum {
    OUT_NONE = 0,
    OUT1,
    OUT2
} output_action_t;

typedef enum {
    THRESH_OFF = 0,
    THRESH_LIMIT,
    THRESH_RANGE
} threshold_type_t;

typedef enum {
    COND_NONE = 0,    // only for OFF mode
    COND_OVER,
    COND_UNDER,
    COND_INSIDE,
    COND_OUTSIDE
} condition_t;

typedef struct {
    output_action_t output;
    threshold_type_t type;
    float value1; // LIMIT: threshold, RANGE: lower
    float value2; // RANGE: upper, ignored for LIMIT
    condition_t cond;
} input_monitor_config_t;
void SmsHandlerTask(void *param);

void check_input_conditions(float cur, float alg, float res);

#ifdef __cplusplus
}
#endif

#endif // HANDLER_H
