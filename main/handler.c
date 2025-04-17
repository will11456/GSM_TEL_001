#include "main.h"
#include "pin_map.h"
#include "adc.h"
#include "modem.h"
#include "handler.h"
#include "inputs.h"
#include "tmp102.h"
#include "output.h"


#define MAX_NUMBER_LOG    10
#define PHONE_NUMBER_LEN  16

static const char *TAG = "HANDLER";

// NVS namespace and keys for persistent storage.
#define NVS_NAMESPACE     "numbers"
#define KEY_INPUT1        "INPUT1"
#define KEY_INPUT2        "INPUT2"
// (Other keys can be defined later for CURRENT, ANALOG, etc.)

//--- Data structure for a number log ---//
typedef struct {
    char numbers[MAX_NUMBER_LOG][PHONE_NUMBER_LEN];
    int count;
} number_log_t;

//--- Global logs for INPUT1 and INPUT2 ---//
static number_log_t log_input1   = { .count = 0 };
static number_log_t log_input2   = { .count = 0 };

//--- Helper function: Get pointer to log based on name ---//
static number_log_t *get_log(const char *logName) {
    if (strcasecmp(logName, "INPUT1") == 0) {
        return &log_input1;
    } else if (strcasecmp(logName, "INPUT2") == 0) {
        return &log_input2;
    }
    return NULL;
}

//--- Persistent Storage Helper Functions ---//

// Save a given log to NVS as a comma-separated string.
static esp_err_t save_log(number_log_t *log, const char *key) {
    char buffer[256] = {0};

    for (int i = 0; i < log->count; i++) {
        strncat(buffer, log->numbers[i], sizeof(buffer) - strlen(buffer) - 1);
        if (i < log->count - 1) {
            strncat(buffer, ",", sizeof(buffer) - strlen(buffer) - 1);
        }
    }

    nvs_handle_t handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Error opening NVS handle (key: %s)", key);
        return err;
    }
    err = nvs_set_str(handle, key, buffer);
    if (err == ESP_OK) {
        err = nvs_commit(handle);
    }
    nvs_close(handle);
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "Saved log (%s): %s", key, buffer);
    } else {
        ESP_LOGE(TAG, "Failed to save log (%s)", key);
    }
    return err;
}

// Load a log from NVS (a comma-separated string) into the provided log structure.
static esp_err_t load_log(number_log_t *log, const char *key) {
    nvs_handle_t handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READONLY, &handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Error opening NVS handle for key %s", key);
        return err;
    }

    // Get required size for the string.
    size_t required_size = 0;
    err = nvs_get_str(handle, key, NULL, &required_size);
    if (err == ESP_ERR_NVS_NOT_FOUND) {
        // Key not found. Log will remain empty.
        nvs_close(handle);
        log->count = 0;
        return ESP_OK;
    }
    if(err != ESP_OK){
        nvs_close(handle);
        return err;
    }
    char *buffer = malloc(required_size);
    if(!buffer) {
        nvs_close(handle);
        return ESP_ERR_NO_MEM;
    }
    err = nvs_get_str(handle, key, buffer, &required_size);
    nvs_close(handle);
    if (err == ESP_OK) {
        // Clear the current log.
        log->count = 0;
        char *token = strtok(buffer, ",");
        while(token && log->count < MAX_NUMBER_LOG) {
            // Trim trailing whitespace from token.
            size_t len = strlen(token);
            while(len > 0 && isspace((unsigned char)token[len - 1])) {
                token[len - 1] = '\0';
                len--;
            }
            strncpy(log->numbers[log->count], token, PHONE_NUMBER_LEN - 1);
            log->numbers[log->count][PHONE_NUMBER_LEN - 1] = '\0';
            log->count++;
            token = strtok(NULL, ",");
        }
        ESP_LOGI(TAG, "Loaded log (%s) with %d numbers", key, log->count);
    }
    free(buffer);
    return err;
}

//--- Initialize logs by loading from NVS ---
void init_logs(void) {
    esp_err_t err;
    err = load_log(&log_input1, KEY_INPUT1);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to load INPUT1 log: %s", esp_err_to_name(err));
    }
    err = load_log(&log_input2, KEY_INPUT2);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to load INPUT2 log: %s", esp_err_to_name(err));
    }
}

//--- Helper functions for managing the number log ---//

// Adds a phone number to the given log, and then saves the log to flash.
static void add_number_to_log(number_log_t *log, const char *number, const char *key) {
    if (!log) return;
    if (log->count < MAX_NUMBER_LOG) {
        strncpy(log->numbers[log->count], number, PHONE_NUMBER_LEN - 1);
        log->numbers[log->count][PHONE_NUMBER_LEN - 1] = '\0';
        log->count++;
        ESP_LOGI(TAG, "Added number %s to log", number);
        save_log(log, key);
    } else {
        ESP_LOGW(TAG, "Log is full. Could not add %s", number);
    }
}

// Clears the given log and saves the empty log to flash.
static void clear_number_log(number_log_t *log, const char *key) {
    if (!log) return;
    log->count = 0;
    ESP_LOGI(TAG, "Log cleared");
    save_log(log, key);
}

// Builds a comma-separated list from a log.
static void list_number_log(number_log_t *log, char *buffer, size_t buffer_len) {
    if (!log) {
        snprintf(buffer, buffer_len, "Log not found");
        return;
    }
    buffer[0] = '\0';
    for (int i = 0; i < log->count; i++) {
        strncat(buffer, log->numbers[i], buffer_len - strlen(buffer) - 1);
        if (i < log->count - 1) {
            strncat(buffer, ", ", buffer_len - strlen(buffer) - 1);
        }
    }
}

//--- Exposed function: Send SMS to all numbers in a specified log ---//
void send_sms_to_all_by_log(const char *msg, const char *logName) {
    number_log_t *log = get_log(logName);
    if (log) {
        for (int i = 0; i < log->count; i++) {
            send_sms(log->numbers[i], msg);
        }
    }
}

//--- Command parser ---//
// The command formats supported are:
//   ADD <LOG> <number>
//   CLEAR <LOG>
//   LIST <LOG>
// This function parses the command (after the "CMD:" prefix) and sends a confirmation back to the sender.
static void parse_command(const char *cmdStr, const char *sender) {
    char cmdBuf[128];
    strncpy(cmdBuf, cmdStr, sizeof(cmdBuf) - 1);
    cmdBuf[sizeof(cmdBuf) - 1] = '\0';

    // Tokenize the command.
    char *token = strtok(cmdBuf, " \t");
    if (!token) {
        send_sms(sender, "Error: No command provided");
        return;
    }

    if (strcasecmp(token, "ADD") == 0) {
        char *logName = strtok(NULL, " \t");
        char *number = strtok(NULL, " \t");
        if (!logName || !number) {
            send_sms(sender, "Error: Format: ADD <LOG> <number>");
            return;
        }
        number_log_t *log = get_log(logName);
        if (!log) {
            send_sms(sender, "Error: Unknown log type");
            return;
        }
        // Determine the corresponding NVS key.
        const char *key = NULL;
        if (strcasecmp(logName, "INPUT1") == 0) {
            key = KEY_INPUT1;
        } else if (strcasecmp(logName, "INPUT2") == 0) {
            key = KEY_INPUT2;
        }
        if (!key) {
            send_sms(sender, "Error: Log type unsupported for persistence");
            return;
        }
        add_number_to_log(log, number, key);
        char reply[64];
        snprintf(reply, sizeof(reply), "Added %s to %s log", number, logName);
        send_sms(sender, reply);
    }
    else if (strcasecmp(token, "CLEAR") == 0) {
        char *logName = strtok(NULL, " \t");
        if (!logName) {
            send_sms(sender, "Error: Format: CLEAR <LOG>");
            return;
        }
        number_log_t *log = get_log(logName);
        if (!log) {
            send_sms(sender, "Error: Unknown log type");
            return;
        }
        const char *key = NULL;
        if (strcasecmp(logName, "INPUT1") == 0) {
            key = KEY_INPUT1;
        } else if (strcasecmp(logName, "INPUT2") == 0) {
            key = KEY_INPUT2;
        }
        if (!key) {
            send_sms(sender, "Error: Log type unsupported for persistence");
            return;
        }
        clear_number_log(log, key);
        char reply[64];
        snprintf(reply, sizeof(reply), "Cleared %s log", logName);
        send_sms(sender, reply);
    }
    else if (strcasecmp(token, "LIST") == 0) {
        char *logName = strtok(NULL, " \t");
        if (!logName) {
            send_sms(sender, "Error: Format: LIST <LOG>");
            return;
        }
        number_log_t *log = get_log(logName);
        if (!log) {
            send_sms(sender, "Error: Unknown log type");
            return;
        }
        char list[256];
        list_number_log(log, list, sizeof(list));
        char reply[512];
        snprintf(reply, sizeof(reply), "Numbers in %s log: %s", logName, list);
        send_sms(sender, reply);
    }
    else {
        send_sms(sender, "Error: Unknown command");
    }
}

//--- SMS Handler Task ---//
// This task receives SMS messages from rx_message_queue. For messages beginning with "CMD:"
// it parses the command and executes it.
void SmsHandlerTask(void *param) {
    sms_message_t sms;
    for (;;) {
        if (xQueueReceive(rx_message_queue, &sms, portMAX_DELAY) == pdTRUE) {
            if (strncasecmp(sms.message, "CMD:", 4) == 0) {
                const char *command = sms.message + 4;
                parse_command(command, sms.sender);
            } else {
                ESP_LOGI(TAG, "Received non-command SMS from %s", sms.sender);
            }
        }
    }
}
