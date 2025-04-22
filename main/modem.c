#include "main.h"
#include "pin_map.h"
#include "adc.h"
#include "modem.h"
#include "handler.h"
#include "inputs.h"
#include "tmp102.h"
#include "output.h"
#include "config_store.h"
#include "sms_sender.h"

//Logging Tag
static const char* TAG = "MODEM";

// Global variable to store last received SMS index.
static int last_sms_index = -1;

// Global mutex for UART writes.
SemaphoreHandle_t uart_mutex = NULL;

// Global queue for incoming SMS messages.
QueueHandle_t rx_message_queue = NULL;

// === INITIALIZE SIM800C UART ===
void sim800c_init(void) {
    const uart_config_t uart_config = {
        .baud_rate = SIM800L_BAUD_RATE,
        .data_bits = UART_DATA_8_BITS,
        .parity    = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };

    ESP_ERROR_CHECK(uart_driver_install(SIM800L_UART_PORT, SIM800L_UART_BUF_SIZE, 0, 0, NULL, 0));
    ESP_ERROR_CHECK(uart_param_config(SIM800L_UART_PORT, &uart_config));
    ESP_ERROR_CHECK(uart_set_pin(SIM800L_UART_PORT, MODEM_TX, MODEM_RX, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));
    gpio_set_direction(PWR_KEY, GPIO_MODE_OUTPUT);
}

// === POWER CONTROL FUNCTIONS ===
void sim800c_power_on(void) {
    gpio_set_level(PWR_KEY, 1);
    vTaskDelay(pdMS_TO_TICKS(1000));
    gpio_set_level(PWR_KEY, 0);
    vTaskDelay(pdMS_TO_TICKS(5000));
}

void sim800c_power_off(void) {
    gpio_set_level(PWR_KEY, 1);
    vTaskDelay(pdMS_TO_TICKS(1200));
    gpio_set_level(PWR_KEY, 0);
    vTaskDelay(pdMS_TO_TICKS(5000)); // Wait for the module to power off
}

// === SAFE UART WRITE (protected by mutex) ===
static void safe_uart_write(const char *data, size_t len)
{
    xSemaphoreTake(uart_mutex, portMAX_DELAY);
    uart_write_bytes(SIM800L_UART_PORT, data, len);
    xSemaphoreGive(uart_mutex);
}

// === SEND RAW AT COMMAND ===
void sim800_send_cmd(const char *cmd)
{
    safe_uart_write(cmd, strlen(cmd));
    safe_uart_write("\r\n", 2);
}

// === INIT SMS RECEIVING MODE ===
void sim800_setup_sms()
{
    sim800_send_cmd("ATE0");   // disable local echo of AT commands
    vTaskDelay(pdMS_TO_TICKS(300));

    sim800_send_cmd("AT+CMGF=1");           // text mode
    vTaskDelay(pdMS_TO_TICKS(500));

    // deliver full SMS (header + body) as soon as it arrives
    sim800_send_cmd("AT+CNMI=2,1,0,0,0");    
    vTaskDelay(pdMS_TO_TICKS(500));

    ESP_LOGI(TAG, "SMS mode: text + push new SMS to UART");
}



// === SEND SMS FUNCTION ===
void send_sms(const char *number, const char *message)
{
    char buf[256];

    // 1. Set text mode
    sim800_send_cmd("AT+CMGF=1");
    vTaskDelay(pdMS_TO_TICKS(300));

    // 2. Flush UART input (get rid of any previous junk)
    uart_flush_input(SIM800L_UART_PORT);

    // 3. Send the SMS command
    snprintf(buf, sizeof(buf), "AT+CMGS=\"%s\"\r\n", number);
    safe_uart_write(buf, strlen(buf));

    // 4. Wait a safe amount of time for the SIM800 to prepare (NO wait for '>')
    vTaskDelay(pdMS_TO_TICKS(1000));  // ← longer delay to be extra safe

    // 5. Send the actual message followed by Ctrl+Z
    safe_uart_write(message, strlen(message));
    safe_uart_write("\x1A", 1);

    // 6. Log that we're sending (for debug)
    ESP_LOGI("MODEM", "📤 SMS to %s: %s", number, message);

    // 7. Wait up to 10s for the module to process and send
    vTaskDelay(pdMS_TO_TICKS(10000));

    // 8. Flush UART again to discard trailing OK or +CMGS
    uart_flush_input(SIM800L_UART_PORT);
}


// Flush any received bytes
static void flush_rx(void) {
    uart_flush_input(SIM800L_UART_PORT);
}


// === MESSAGE PARSER ===
// This function processes the SMS content after reading, removes trailing "OK", and
// then posts the trimmed SMS details on the rx_message_queue. It logs only a notification.
void message_parser(const char *sender, const char *message)
{
    // Log a notification (without printing the full message text).
    ESP_LOGI("MODEM", "SMS received from %s", sender);

    // Create an sms_message_t instance.
    sms_message_t sms;
    strncpy(sms.sender, sender, sizeof(sms.sender) - 1);
    sms.sender[sizeof(sms.sender) - 1] = '\0';
    strncpy(sms.message, message, sizeof(sms.message) - 1);
    sms.message[sizeof(sms.message) - 1] = '\0';

    // --- Trim Leading Whitespace ---
    char *start = sms.message;
    while(*start && isspace((unsigned char)*start)) {
        start++;
    }
    if(start != sms.message) {
        memmove(sms.message, start, strlen(start) + 1);
    }

    // --- Trim Trailing Whitespace ---
    size_t len = strlen(sms.message);
    while(len > 0 && isspace((unsigned char)sms.message[len - 1])) {
        sms.message[len - 1] = '\0';
        len--;
    }

    // --- Remove Trailing "OK" if Present ---
    // Check if the message ends with "OK" after trimming whitespace.
    if (len >= 2 && strcmp(sms.message + len - 2, "OK") == 0) {
        sms.message[len - 2] = '\0';
        // Trim any whitespace that might now trail after removing "OK".
        len = strlen(sms.message);
        while(len > 0 && isspace((unsigned char)sms.message[len - 1])) {
            sms.message[len - 1] = '\0';
            len--;
        }
    }

    // Post the SMS message to the queue.
    if (rx_message_queue != NULL) {
        xQueueSend(rx_message_queue, &sms, portMAX_DELAY);
    }

    // Delete the SMS from the module if we have its index.
    if (last_sms_index >= 0) {
        char del_cmd[32];
        snprintf(del_cmd, sizeof(del_cmd), "AT+CMGD=%d", last_sms_index);
        sim800_send_cmd(del_cmd);
        last_sms_index = -1; // Reset index after deletion.
    }
}


// === MODEM TASK ===
void ModemTask(void *param) {
    // Create the UART mutex if it hasn't been created.
    if (uart_mutex == NULL) {
        uart_mutex = xSemaphoreCreateMutex();
    }


    sim800c_init();
    sim800c_power_on();
    vTaskDelay(pdMS_TO_TICKS(5000));

    
    
    sim800_setup_sms();

    flush_rx();

    uint8_t data[SIM800L_UART_BUF_SIZE];

    while (1) {
        int len = uart_read_bytes(SIM800L_UART_PORT, data, SIM800L_UART_BUF_SIZE - 1, pdMS_TO_TICKS(1000));
        if (len > 0) {
            data[len] = '\0';
            //ESP_LOGI("MODEM", "Received data: %s", (char *)data);
            // 1. Handle new SMS notification: +CMTI: "SM",index
            char *cmti = strstr((char *)data, "+CMTI:");
            if (cmti) {
                if (sscanf(cmti, "+CMTI: \"SM\",%d", &last_sms_index) == 1 && last_sms_index >= 0) {
                    char cmd[32];
                    snprintf(cmd, sizeof(cmd), "AT+CMGR=%d", last_sms_index);
                    sim800_send_cmd(cmd);
                    vTaskDelay(pdMS_TO_TICKS(1000)); // Allow time for the response
                }
            }

            // 2. Handle SMS read result: +CMGR: ...
            char *cmgr = strstr((char *)data, "+CMGR:");
            if (cmgr) {
                char sender[32] = {0};
                char message[160] = {0};

                // Extract sender number.
                sscanf(cmgr, "+CMGR: \"REC UNREAD\",\"%[^\"]", sender);

                // Find the message text start (assumes it follows a newline).
                char *msg_start = strchr(cmgr, '\n');
                if (msg_start) {
                    msg_start += 1;  // Skip the newline.
                    strncpy(message, msg_start, sizeof(message) - 1);

                    // Process the SMS.
                    message_parser(sender, message);
                }
            }
        }
    }
}
