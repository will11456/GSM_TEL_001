#ifndef HANDLER_H
#define HANDLER_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include "esp_err.h"
#include "freertos/queue.h"
#include "esp_err.h"


#define SMS_SENDER_LEN 32
#define SMS_MESSAGE_LEN 160

typedef struct {
    char sender[SMS_SENDER_LEN];     // Phone number of sender
    char message[SMS_MESSAGE_LEN];   // Raw message text
} sms_message_t;

/**
 * @brief The queue used by the modem to push incoming SMS messages.
 *        Must be created by the main application before tasks are started.
 */
extern QueueHandle_t rx_message_queue;

/**
 * @brief Task that listens for SMS commands and processes them.
 *        Matches messages beginning with "CMD:", parses them, executes logic,
 *        and responds via SMS using send_sms().
 *
 * @param param Not used.
 */
void SmsHandlerTask(void *param);

#ifdef __cplusplus
}
#endif

#endif // HANDLER_H
