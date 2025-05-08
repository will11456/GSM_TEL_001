#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include "main.h"
#include <stdint.h>

#include <stdint.h>
#include <stddef.h>
#include "main.h"
#include "pin_map.h"
#include "adc.h"
#include "modem.h"
#include "handler.h"
#include "inputs.h"
#include "tmp102.h"
#include "output.h"
#include "config_store.h"


//Logging Tag
static const char* TAG = "HANDLER";

extern QueueHandle_t rx_message_queue;

void send_reply(const char *to_number, const char *message)
{

    char unit_id[32];
    // Try to fetch the stored Unit ID; default to "Unit ID" on error
    if (config_store_get_unit_id(unit_id, sizeof(unit_id)) != ESP_OK) {
        strncpy(unit_id, "Unit ID", sizeof(unit_id)-1);
        unit_id[sizeof(unit_id)-1] = '\0';
    }

    // Build the full reply: "<UnitID>: <msg>"
    char buffer[512];
    snprintf(buffer, sizeof(buffer), "%s: %s", unit_id, message);


    if (!to_number || !message) return;

    modem_send_sms(to_number, buffer);

}

static void trim(char *str) {
    // Trim leading
    while (*str && isspace((unsigned char)*str)) str++;
    // Trim trailing
    char *end = str + strlen(str) - 1;
    while (end > str && isspace((unsigned char)*end)) *end-- = '\0';
}

static void parse_command(const sms_message_t *sms) {
    char *cmd = sms->message;
    char response[512];
    char arg1[32], arg2[32], arg3[32], arg4[32];

    if (strncasecmp(cmd, "CMD:", 4) != 0) return;
    cmd += 4;
    trim(cmd);

    if (sscanf(cmd, "SETID %31[^\n]", arg1) == 1) {
        config_store_set_unit_id(arg1);
        snprintf(response, sizeof(response), "ID set to %s", arg1);
        send_reply(sms->sender, response);
        return;
    }

    if (strcasecmp(cmd, "SIGNAL") == 0) {
        int rssi = 0;
        char quality[16];
    
        if (signal_quality(&rssi, quality, sizeof(quality))) {
            char reply[64];
            snprintf(reply, sizeof(reply), "Signal RSSI: %d (%s)", rssi, quality);
            send_reply(sms->sender, reply);
        } else {
            send_reply(sms->sender, "Failed to get signal quality.");
        }
        return;
    }

    if (sscanf(cmd, "ADDNUM %31s %31s", arg1, arg2) == 2) {
        config_store_add_number(arg1, arg2);
        snprintf(response, sizeof(response), "Number %s added to %s", arg2, arg1);
        send_reply(sms->sender, response);
        return;
    }

    if (sscanf(cmd, "CLEAR %31s", arg1) == 1) {
        config_store_clear_log(arg1);
        snprintf(response, sizeof(response), "Cleared %s log", arg1);
        send_reply(sms->sender, response);
        return;
    }

    if (sscanf(cmd, "LIST %31s", arg1) == 1) {
        char list[256] = {0};
        if (config_store_list_log(arg1, list, sizeof(list)) == ESP_OK) {
            snprintf(response, sizeof(response), "%s log: %s", arg1, strlen(list) ? list : "(empty)");
        } else {
            snprintf(response, sizeof(response), "Error reading %s log", arg1);
        }
        send_reply(sms->sender, response);
        return;
    }

    if (strcasecmp(cmd, "RESET") == 0) {
        config_store_reset_defaults();
        send_reply(sms->sender, "Factory settings restored");
        return;
    }

    if (strcasecmp(cmd, "TEMP") == 0) {
        
        float temp_c;
        
        if (tmp102_read_celsius(&temp_c) == ESP_OK) {
            ESP_LOGI(TAG, "Temperature: %.2f°C", temp_c);
            snprintf(response, sizeof(response), "Temperature: %.2f C", temp_c);

        } 
        else {
            ESP_LOGE(TAG, "Error reading temperature");
            snprintf(response, sizeof(response), "TEMP sensor error");
        }

        // <— use your ID‑prefixing helper so SMS goes out
        send_reply(sms->sender, response);
        return;
    }

    if (strcasecmp(cmd, "BATTV") == 0) {
        float v = get_battery_voltage(); // should be defined in adc/sensor module
        snprintf(response, sizeof(response), "Battery: %.2f V", v);
        ESP_LOGI(TAG, "Battery voltage: %.2f V", v);
        send_reply(sms->sender, response);
        return;
    }

    if (sscanf(cmd, "OUT1 %31s", arg1) == 1) {
        output_cmd_t o = { .id = OUTPUT_ID_1, .level = strcasecmp(arg1, "ACTIVATE") == 0 };
        output_controller_send(&o);
        snprintf(response, sizeof(response), "OUT1 %s", o.level ? "ON" : "OFF");
        send_reply(sms->sender, response);
        return;
    }

    if (sscanf(cmd, "OUT2 %31s", arg1) == 1) {
        output_cmd_t o = { .id = OUTPUT_ID_2, .level = strcasecmp(arg1, "ACTIVATE") == 0 };
        output_controller_send(&o);
        snprintf(response, sizeof(response), "OUT2 %s", o.level ? "ON" : "OFF");
        send_reply(sms->sender, response);
        return;
    }

    if (sscanf(cmd, "%31s %31s %31s %31s", arg1, arg2, arg3, arg4) >= 2) {
        input_mapping_t map = {0};
        strncpy(map.output, arg2, sizeof(map.output)-1);
        if (strcasecmp(arg1, "VALARM") == 0 && sscanf(arg2, "%d", &map.threshold1) == 1) {
            voltage_alarm_config_t vcfg = {
                .threshold_mv = map.threshold1,
            };
            strncpy(vcfg.output, arg3, sizeof(vcfg.output)-1);
            config_store_set_voltage_alarm(&vcfg);
            snprintf(response, sizeof(response), "Voltage alarm set: %dmV → %s", vcfg.threshold_mv, vcfg.output);
            send_reply(sms->sender, response);
            return;
        }

        if (strcasecmp(arg1, "IN1") == 0 || strcasecmp(arg1, "IN2") == 0 ||
            strcasecmp(arg1, "CUR") == 0 || strcasecmp(arg1, "ALG") == 0 || strcasecmp(arg1, "RES") == 0) {
            strncpy(map.mode, arg3, sizeof(map.mode)-1);
            map.threshold1 = atoi(arg4);
            config_store_set_mapping(arg1, &map);
            snprintf(response, sizeof(response), "%s mapped to %s [%s %d]", arg1, map.output, map.mode, map.threshold1);
            send_reply(sms->sender, response);
            return;
        }
    }

    send_reply(sms->sender, "Unknown or invalid command");
}

void SmsHandlerTask(void *param) {
    sms_message_t sms;
    while (1) {
        if (xQueueReceive(rx_message_queue, &sms, portMAX_DELAY)) {
            ESP_LOGW("SMS", "Received SMS from %s: %s", sms.sender, sms.message);
            parse_command(&sms);
        }
    }
}
