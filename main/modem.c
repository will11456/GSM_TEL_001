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

#define MODEM_RESP_BUF 256
#define MODEM_CMD_QUEUE_LEN 8
QueueHandle_t modem_cmd_queue = NULL;

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
bool modem_send_cmd(const char *cmd, char *response_buf, size_t buf_len, int timeout_ms)
{
    modem_cmd_t req = {
        .response_len = buf_len
    };
    strncpy(req.command, cmd, sizeof(req.command) - 1);
    memset(response_buf, 0, buf_len);
    req.response[0] = '\0';
    req.done = xSemaphoreCreateBinary();

    if (!req.done) return false;

    if (!xQueueSend(modem_cmd_queue, &req, pdMS_TO_TICKS(100))) {
        vSemaphoreDelete(req.done);
        return false;
    }

    if (xSemaphoreTake(req.done, pdMS_TO_TICKS(timeout_ms))) {
        strncpy(response_buf, req.response, buf_len - 1);
        vSemaphoreDelete(req.done);
        return req.success;
    }

    vSemaphoreDelete(req.done);
    return false;
}



void sim800_send_cmd(const char *cmd)
{
    modem_cmd_t req = {
        .response_len = 1  // minimal buffer to satisfy structure
    };
    strncpy(req.command, cmd, sizeof(req.command) - 1);
    req.response[0] = '\0';
    req.done = xSemaphoreCreateBinary();

    if (!req.done) return;

    // Send the command to ModemTask
    if (xQueueSend(modem_cmd_queue, &req, pdMS_TO_TICKS(100))) {
        // Wait briefly just to let it process, but ignore outcome
        xSemaphoreTake(req.done, pdMS_TO_TICKS(300));
    }

    vSemaphoreDelete(req.done);
}


// === INIT SMS RECEIVING MODE ===
void sim800_setup_sms()
{
    sim800_send_cmd("ATE0");   // disable echo
    vTaskDelay(pdMS_TO_TICKS(300));

    sim800_send_cmd("AT+CMGF=1");  // text mode
    vTaskDelay(pdMS_TO_TICKS(500));

    sim800_send_cmd("AT+CMGDA=\"DEL ALL\"");  // delete all
    vTaskDelay(pdMS_TO_TICKS(500));

    sim800_send_cmd("AT+CNMI=2,1,0,0,0");  // push SMS immediately
    vTaskDelay(pdMS_TO_TICKS(500));

    uart_flush_input(SIM800L_UART_PORT);  // just in case

    ESP_LOGI(TAG, "GSM Modem initialized and ready to receive SMS messages.");
}



//Signal quality function
bool get_signal_quality(int *rssi_out, char *rating, size_t rating_len)
{
    if (!modem_cmd_queue || !uart_mutex) return false;

    modem_cmd_t req = {
        .response_len = sizeof(req.response),
        .done = xSemaphoreCreateBinary(),
        .success = false
    };

    if (!req.done) return false;

    strncpy(req.command, "AT+CSQ", sizeof(req.command) - 1);

    if (!xQueueSend(modem_cmd_queue, &req, pdMS_TO_TICKS(100))) {
        vSemaphoreDelete(req.done);
        return false;
    }

    if (!xSemaphoreTake(req.done, pdMS_TO_TICKS(2000))) {
        vSemaphoreDelete(req.done);
        return false;
    }

    vSemaphoreDelete(req.done);

    if (!req.success) return false;

    // Parse "+CSQ: <rssi>,<ber>"
    int rssi = -1, ber = -1;
    if (sscanf(req.response, "+CSQ: %d,%d", &rssi, &ber) != 2) {
        return false;
    }

    if (rssi_out) *rssi_out = rssi;

    if (rating && rating_len > 0) {
        const char *level =
            (rssi == 99) ? "Unknown" :
            (rssi <= 2)  ? "None" :
            (rssi <= 9)  ? "Poor" :
            (rssi <= 14) ? "Fair" :
            (rssi <= 19) ? "Good" :
            (rssi <= 30) ? "Great" : "Excellent";

        snprintf(rating, rating_len, "%s", level);
    }

    return true;
}










// === SEND SMS FUNCTION ===
void send_sms(const char *number, const char *message)
{
    char buf[512];

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
    vTaskDelay(pdMS_TO_TICKS(3000));

    
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

    if (uart_mutex == NULL) {
        uart_mutex = xSemaphoreCreateMutex();
    }

    if (modem_cmd_queue == NULL) {
        modem_cmd_queue = xQueueCreate(8, sizeof(modem_cmd_t));
    }

    sim800c_init();
    sim800c_power_on();
    vTaskDelay(pdMS_TO_TICKS(5000));

    sim800_setup_sms();
    flush_rx();

    uint8_t data[SIM800L_UART_BUF_SIZE];
    modem_cmd_t req;

    while (1) {
        
        // === Process AT Command Requests ===
        if (xQueueReceive(modem_cmd_queue, &req, 0) == pdTRUE) {
            uart_flush_input(SIM800L_UART_PORT);

            char full_cmd[80];
            snprintf(full_cmd, sizeof(full_cmd), "%s\r\n", req.command);
            safe_uart_write(full_cmd, strlen(full_cmd));

            char buf[256] = {0};
            int total_wait = 0;
            req.success = false;

            while (total_wait < 2000) {
                int len = uart_read_bytes(SIM800L_UART_PORT, (uint8_t*)buf, sizeof(buf) - 1, pdMS_TO_TICKS(100));
                if (len > 0) {
                    buf[len] = '\0';

                    if (strstr(buf, "OK")) {
                        req.success = true;
                    }
                    strncpy(req.response, buf, req.response_len - 1);
                    if (strstr(buf, "OK") || strstr(buf, "ERROR")) break;
                }
                total_wait += 100;
            }

            uart_flush_input(SIM800L_UART_PORT);
            xSemaphoreGive(req.done);
            continue; // Skip SMS handling this cycle
        }

        // === SMS Receive Handling ===
        int len = uart_read_bytes(SIM800L_UART_PORT, data, SIM800L_UART_BUF_SIZE - 1, pdMS_TO_TICKS(1000));
        if (len > 0) {
            data[len] = '\0';

            char *cmti = strstr((char *)data, "+CMTI:");
            if (cmti) {
                if (sscanf(cmti, "+CMTI: \"SM\",%d", &last_sms_index) == 1 && last_sms_index >= 0) {
                    char cmd[32];
                    snprintf(cmd, sizeof(cmd), "AT+CMGR=%d", last_sms_index);
                    sim800_send_cmd(cmd);
                    vTaskDelay(pdMS_TO_TICKS(1000));
                }
            }

            char *cmgr = strstr((char *)data, "+CMGR:");
            if (cmgr) {
                char sender[32] = {0};
                char message[160] = {0};
                sscanf(cmgr, "+CMGR: \"REC UNREAD\",\"%[^\"]", sender);
                char *msg_start = strchr(cmgr, '\n');
                if (msg_start) {
                    msg_start += 1;
                    strncpy(message, msg_start, sizeof(message) - 1);
                    message_parser(sender, message);
                }
            }
        }
    }
}
