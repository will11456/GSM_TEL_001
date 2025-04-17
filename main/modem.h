#ifndef MODEM_H
#define MODEM_H

#include "freertos/queue.h"
#include "driver/uart.h"
#include <string.h>
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "esp_log.h"
#include <ctype.h> 

#define SIM800L_UART_PORT      UART_NUM_2      // UART port number for SIM800L
#define SIM800L_UART_BUF_SIZE  1024            // UART buffer size
#define SIM800L_BAUD_RATE      9600            // UART baud rate

// SMS message structure used for inter-task communication.
typedef struct {
    char sender[32];
    char message[160];
} sms_message_t;

// Global queue for received SMS messages.
// This variable is defined in modem.c.
extern QueueHandle_t rx_message_queue;

// Mutex for UART operations  
extern SemaphoreHandle_t uart_mutex; 

// Function prototypes for modem operations.
void sim800c_init(void);
void sim800_setup_sms(void);
void sim800c_power_on(void);
void sim800c_power_off(void);
void send_sms(const char *number, const char *message);
void ModemTask(void *param);

#endif // MODEM_H