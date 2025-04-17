#ifndef SMS_SENDER_H_
#define SMS_SENDER_H_

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"

/// One SMS send request
typedef struct {
    char number[32];
    char text[160];
} sms_request_t;

/**
 * @brief Start the SMS‐sender task. Call this once from app_main().
 */
void start_sms_sender(void);

/**
 * @brief Enqueue an SMS to be sent as soon as possible.
 * @return true if queued successfully, false if queue is full.
 */
bool send_sms_async(const char *number, const char *text);

#endif // SMS_SENDER_H_
