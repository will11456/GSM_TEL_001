#include "main.h"
#include "pin_map.h"
#include "adc.h"
#include "modem.h"
#include "handler.h"
#include "inputs.h"
#include "tmp102.h"
#include "output.h"
#include "config_store.h"

#include "modem.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "driver/uart.h"
#include "esp_log.h"
#include <string.h>
#include <stdio.h>
#include <ctype.h>

#define UART_PORT UART_NUM_1
#define UART_BUF_SIZE 256
#define UART_BAUD_RATE 115200

static const char *TAG = "MODEM";

extern TaskHandle_t adcTaskHandle;
extern TaskHandle_t inputTaskHandle;
extern TaskHandle_t handlerTaskHandle;


typedef enum {
    MODEM_CMD_AT,
    MODEM_CMD_SMS
} modem_cmd_type_t;


typedef struct {
    modem_cmd_type_t type;
    union {
        struct {
            char command[64];
            char response[256];
            size_t response_len;
            SemaphoreHandle_t done;
            bool success;
        } at;
        struct {
            char number[32];
            char message[256];
            SemaphoreHandle_t done;
            bool success;
        } sms;
    };
} modem_command_t;

QueueHandle_t modem_cmd_queue = NULL;
QueueHandle_t rx_message_queue = NULL;

static int last_sms_index = -1;

extern void message_parser(const char *sender, const char *message);  // Defined in handler.c

void modem_init(void) {
    const uart_config_t uart_config = {
        .baud_rate = UART_BAUD_RATE,
        .data_bits = UART_DATA_8_BITS,
        .parity    = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };
    uart_driver_install(UART_PORT, UART_BUF_SIZE, 0, 0, NULL, 0);
    uart_param_config(UART_PORT, &uart_config);
    uart_set_pin(UART_PORT, 12, 13, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE); // TX, RX

    modem_cmd_queue = xQueueCreate(8, sizeof(modem_command_t *));
    rx_message_queue = xQueueCreate(8, sizeof(sms_message_t));
}


// === POWER CONTROL FUNCTIONS ===
void sim800c_power_on(void) {
    gpio_set_level(PWR_KEY, 1);
    vTaskDelay(pdMS_TO_TICKS(1000));
    gpio_set_level(PWR_KEY, 0);
    vTaskDelay(pdMS_TO_TICKS(5000));
}


static void clean_sms_message(char *msg)
{
    if (!msg) return;

    // === Trim leading whitespace ===
    char *start = msg;
    while (*start && isspace((unsigned char)*start)) {
        start++;
    }

    if (start != msg) {
        memmove(msg, start, strlen(start) + 1);  // move trimmed string to original buffer
    }

    // === Trim trailing whitespace ===
    size_t len = strlen(msg);
    while (len > 0 && isspace((unsigned char)msg[len - 1])) {
        msg[len - 1] = '\0';
        len--;
    }

    // === Remove trailing "OK" ===
    len = strlen(msg);
    if (len >= 2 && strcmp(&msg[len - 2], "OK") == 0) {
        msg[len - 2] = '\0';

        // Trim again if whitespace remains
        len = strlen(msg);
        while (len > 0 && isspace((unsigned char)msg[len - 1])) {
            msg[len - 1] = '\0';
            len--;
        }
    }
}

//Functions

bool signal_quality(int *rssi_out, char *rating_buf, size_t buf_len)
{
    char response[256] = {0};

    if (!modem_send_at_cmd("AT+CSQ", response, sizeof(response))) {
        if (rating_buf && buf_len) strncpy(rating_buf, "Timeout", buf_len);
        return false;
    }


    char *csq_line = strstr(response, "+CSQ:");
    if (!csq_line) {
        if (rating_buf && buf_len) strncpy(rating_buf, "ParseError", buf_len);
        return false;
    }

    int rssi = -1, ber = -1;
    if (sscanf(csq_line, "+CSQ: %d,%d", &rssi, &ber) != 2) {
        if (rating_buf && buf_len) strncpy(rating_buf, "Invalid", buf_len);
        return false;
    }

    if (rssi_out) *rssi_out = rssi;

    if (rating_buf && buf_len) {
        const char *level =
            (rssi == 99) ? "Unknown" :
            (rssi <= 2)  ? "None" :
            (rssi <= 9)  ? "Poor" :
            (rssi <= 14) ? "Fair" :
            (rssi <= 19) ? "Good" :
            (rssi <= 30) ? "Great" : "Excellent";

        strncpy(rating_buf, level, buf_len - 1);
        rating_buf[buf_len - 1] = '\0';
    }

    return true;
}



bool modem_send_at_cmd(const char *cmd, char *response_buf, size_t buf_len) {
    modem_command_t *req = malloc(sizeof(modem_command_t));
    if (!req) return false;

    req->type = MODEM_CMD_AT;
    req->at.response_len = buf_len;
    req->at.success = false;
    strncpy(req->at.command, cmd, sizeof(req->at.command) - 1);
    req->at.done = xSemaphoreCreateBinary();
    if (!req->at.done) {
        free(req);
        return false;
    }

    if (!xQueueSend(modem_cmd_queue, &req, pdMS_TO_TICKS(100))) {
        vSemaphoreDelete(req->at.done);
        free(req);
        return false;
    }

    if (!xSemaphoreTake(req->at.done, pdMS_TO_TICKS(3000))) {
        vSemaphoreDelete(req->at.done);
        free(req);
        return false;
    }

    strncpy(response_buf, req->at.response, buf_len - 1);
    response_buf[buf_len - 1] = '\0';

    vSemaphoreDelete(req->at.done);
    free(req);

    return req->at.success;
}


bool modem_send_sms(const char *number, const char *message) {
    // 1) Fetch Unit ID from NVS
    char unit_id[32];
    if (config_store_get_unit_id(unit_id, sizeof(unit_id)) != ESP_OK) {
        // fallback
        strncpy(unit_id, "Unit ID", sizeof(unit_id)-1);
        unit_id[sizeof(unit_id)-1] = '\0';
    }

    // 2) Build the full text with ID header
    //    assume req->sms.message is e.g. 160 or 512 bytes long
    char full_msg[sizeof(((modem_command_t*)0)->sms.message)];
    snprintf(full_msg, sizeof(full_msg), "%s: %s", unit_id, message);

    // 3) Allocate the modem command
    modem_command_t *req = malloc(sizeof(modem_command_t));


    if (req == NULL) {
    ESP_LOGE("QUEUE", "Attempted to queue a NULL pointer!");
    }

    if (!req) return false;

    req->type = MODEM_CMD_SMS;
    req->sms.success = false;

    // 4) Copy number and our full_msg
    strncpy(req->sms.number, number, sizeof(req->sms.number) - 1);
    req->sms.number[sizeof(req->sms.number) - 1] = '\0';

    strncpy(req->sms.message, full_msg, sizeof(req->sms.message) - 1);
    req->sms.message[sizeof(req->sms.message) - 1] = '\0';

    // 5) Queue it and wait for done
    req->sms.done = xSemaphoreCreateBinary();
    if (!req->sms.done) {
        free(req);
        return false;
    }

    //check
    if (!modem_cmd_queue) {
        ESP_LOGE("MODEM", "SMS queue not initialized, can't send!");
        
    }

   
    if (!xQueueSend(modem_cmd_queue, &req, pdMS_TO_TICKS(100))) {
        vSemaphoreDelete(req->sms.done);
        free(req);
        return false;
    }

    if (!xSemaphoreTake(req->sms.done, pdMS_TO_TICKS(12000))) {
        vSemaphoreDelete(req->sms.done);
        free(req);
        return false;
    }

    bool success = req->sms.success;
    vSemaphoreDelete(req->sms.done);
    free(req);
    return success;
}


////////////////////////////////////////////////////


void ModemTask(void *param) {

    modem_command_t *req = NULL;
    uint8_t buf[512];

    sim800c_power_on();
    vTaskDelay(pdMS_TO_TICKS(5000));

    // Modem initialization
    uart_flush_input(UART_PORT);
    uart_write_bytes(UART_PORT, "ATE0\r\n", 6);
    vTaskDelay(pdMS_TO_TICKS(300));
    uart_write_bytes(UART_PORT, "AT+CMGF=1\r\n", 11);
    vTaskDelay(pdMS_TO_TICKS(300));
    uart_write_bytes(UART_PORT, "AT+CMGDA=\"DEL ALL\"\r\n", 21);
    vTaskDelay(pdMS_TO_TICKS(300));
    uart_write_bytes(UART_PORT, "AT+CNMI=2,1,0,0,0\r\n", 21);
    vTaskDelay(pdMS_TO_TICKS(300));
    uart_flush_input(UART_PORT);

    vTaskDelay(pdMS_TO_TICKS(2000)); // Wait for the modem to stabilize

    ESP_LOGW(TAG, "Modem init: Complete");
    xTaskNotifyGive(adcTaskHandle);
    xTaskNotifyGive(inputTaskHandle);
    xTaskNotifyGive(handlerTaskHandle);


        
    while (1) {
        if (xQueueReceive(modem_cmd_queue, &req, 0)) {
            uart_flush_input(UART_PORT);

            if (req->type == MODEM_CMD_AT) {
                char full_cmd[80];
                snprintf(full_cmd, sizeof(full_cmd), "%s\r\n", req->at.command);
                uart_write_bytes(UART_PORT, full_cmd, strlen(full_cmd));

                size_t offset = 0;
                int timeout_ms = 3000;
                int waited = 0;

                req->at.success = false;

                while (waited < timeout_ms && offset < sizeof(buf) - 1) {
                    int len = uart_read_bytes(UART_PORT, buf + offset, sizeof(buf) - 1 - offset, pdMS_TO_TICKS(100));
                    if (len > 0) {
                        offset += len;
                        buf[offset] = '\0';

                        if (strstr((char*)buf, "OK")) {
                            req->at.success = true;
                            break;
                        }
                        if (strstr((char*)buf, "ERROR")) {
                            break;
                        }
                    }
                    waited += 100;
                }

                strncpy(req->at.response, (char*)buf, req->at.response_len - 1);
                xSemaphoreGive(req->at.done);
            }

            if (req->type == MODEM_CMD_SMS) {
                char buf[256] = {0};
                size_t offset = 0;

                // 1. Ensure text mode
                uart_write_bytes(UART_PORT, "AT+CMGF=1\r\n", 11);
                vTaskDelay(pdMS_TO_TICKS(300));
                uart_flush_input(UART_PORT);

                // 2. Send AT+CMGS
                char cmd[64];
                snprintf(cmd, sizeof(cmd), "AT+CMGS=\"%s\"\r\n", req->sms.number);
                uart_write_bytes(UART_PORT, cmd, strlen(cmd));

                // 3. Wait for '>' prompt (modem ready for SMS text)
                int got_prompt = 0;
                for (int waited = 0; waited < 5000; waited += 100) {
                    int len = uart_read_bytes(UART_PORT, buf, sizeof(buf) - 1, pdMS_TO_TICKS(100));
                    if (len > 0 && memchr(buf, '>', len)) {
                        got_prompt = 1;
                        break;
                    }
                }
                if (!got_prompt) {
                    req->sms.success = false;
                    xSemaphoreGive(req->sms.done);
                    continue;
                }

                // 4. Send message + Ctrl+Z
                uart_write_bytes(UART_PORT, req->sms.message, strlen(req->sms.message));
                uart_write_bytes(UART_PORT, "\x1A", 1);  // Ctrl+Z to send

                // 5. Wait for OK/ERROR (up to ~10 seconds)
                offset = 0;
                int waited = 0;
                req->sms.success = false;
                memset(buf, 0, sizeof(buf));
                while (waited < 12000 && offset < sizeof(buf) - 1) {
                    int len = uart_read_bytes(UART_PORT, buf + offset, sizeof(buf) - 1 - offset, pdMS_TO_TICKS(100));
                    if (len > 0) {
                        offset += len;
                        buf[offset] = '\0';
                        //ESP_LOGW(TAG, "buf: %s", buf);
                        if (strstr((char*)buf, "OK")) {
                            //ESP_LOGW(TAG, "got OK");
                            req->sms.success = true;
                            break;
                        }
                        if (strstr((char*)buf, "ERROR")) {
                            //ESP_LOGE(TAG, "got ERROR");
                            break;
                        }
                    }
                    waited += 100;
                }
                vTaskDelay(pdMS_TO_TICKS(250));
                xSemaphoreGive(req->sms.done);
            }

            continue;
        }

        // Handle unsolicited responses
        int len = uart_read_bytes(UART_PORT, buf, sizeof(buf)-1, pdMS_TO_TICKS(100));
        if (len > 0) {
            buf[len] = 0;

            char *cmti = strstr((char*)buf, "+CMTI:");
            if (cmti) {
                if (sscanf(cmti, "+CMTI: \"SM\",%d", &last_sms_index) == 1 && last_sms_index >= 0) {
                    char cmd[32];
                    snprintf(cmd, sizeof(cmd), "AT+CMGR=%d\r\n", last_sms_index);
                    uart_write_bytes(UART_PORT, cmd, strlen(cmd));
                    vTaskDelay(pdMS_TO_TICKS(1000));
                }
            }

            char *cmgr = strstr((char*)buf, "+CMGR:");
            if (cmgr) {
                char sender[32] = {0}, message[160] = {0};
                sscanf(cmgr, "+CMGR: \"REC UNREAD\",\"%[^\"]", sender);
                char *msg_start = strchr(cmgr, '\n');
                if (msg_start) {
                    msg_start++;
                    strncpy(message, msg_start, sizeof(message) - 1);
                    sms_message_t sms;
                    strncpy(sms.sender, sender, sizeof(sms.sender));
                    strncpy(sms.message, message, sizeof(sms.message));
                    clean_sms_message(sms.message);
                    xQueueSend(rx_message_queue, &sms, portMAX_DELAY);
                }
            }
        }
    }
}

