// sms_message.h
#ifndef SMS_MESSAGE_H
#define SMS_MESSAGE_H

#include <stdint.h>

#define SMS_SENDER_LEN  32
#define SMS_MESSAGE_LEN 160

/**
 * @brief Structure for passing an incoming SMS from the modem to your handler.
 */
typedef struct sms_message_s {
    char sender[SMS_SENDER_LEN];     /**< Null‑terminated E.164 number */
    char message[SMS_MESSAGE_LEN];   /**< Null‑terminated text */
} sms_message_t;

#endif // SMS_MESSAGE_H