#ifndef HANDLER_H
#define HANDLER_H

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "string.h"
#include "ctype.h"
#include "stdlib.h"       // For malloc/free
#include "nvs_flash.h"
#include "nvs.h"



/**
 * @brief Task to process incoming SMS messages.
 *
 * This task listens for SMS messages from a shared queue (rx_message_queue)
 * and, when an SMS message begins with "CMD:", it processes the command.
 * Supported commands include:
 *    CMD:ADD <LOG> <number>   - Add a phone number to the specified log.
 *    CMD:CLEAR <LOG>          - Clear the specified number log.
 *    CMD:LIST <LOG>           - List phone numbers in the specified log.
 *
 * A confirmation SMS is sent back to the sender after processing each command.
 *
 * @param param Not used.
 */
void SmsHandlerTask(void *param);

/**
 * @brief Sends an SMS message to all phone numbers stored in the specified log.
 *
 * @param msg      The message to send.
 * @param logName  A string identifying which number log to use.
 *                 (For example: "INPUT1" or "INPUT2")
 */
void send_sms_to_all_by_log(const char *msg, const char *logName);

/**
 * @brief Initializes the persistent logs.
 *
 * This function loads the saved number logs (for example, INPUT1 and INPUT2)
 * from flash storage (using NVS) into the corresponding global structures.
 * Call this function after nvs_flash_init() in your app_main() so that saved logs persist over power cycles.
 */
void init_logs(void);

#ifdef __cplusplus
}
#endif

#endif /* HANDLER_H */