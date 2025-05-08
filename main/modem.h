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

#define SIM800L_UART_PORT      UART_NUM_1      // UART port number for SIM800L
#define SIM800L_UART_BUF_SIZE  2048           // UART buffer size
#define SIM800L_BAUD_RATE      115200            // UART baud rate

#include "sms_message.h"

typedef struct {
    char command[64];           // AT command (no \r\n needed)
    char response[256];         // Response will be written here
    size_t response_len;        // Max buffer length
    SemaphoreHandle_t done;     // Semaphore to signal completion
    bool success;               // Set true if OK received
} modem_cmd_t;

// Global queue for received SMS messages.
// This variable is defined in modem.c.
extern QueueHandle_t rx_message_queue;
extern QueueHandle_t modem_cmd_queue;

// Mutex for UART operations  
extern SemaphoreHandle_t uart_mutex; 

// Function prototypes for modem operations.
void sim800c_init(void);
void sim800_setup_sms(void);
void sim800c_power_on(void);
void sim800c_power_off(void);
void sim800_delete_all_sms(void);

//void modem_send_cmd(const char *cmd);
bool get_signal_quality(int *rssi_out, char *rating, size_t rating_len);

void send_sms(const char *number, const char *message);
void ModemTask(void *param);

#endif // MODEM_H