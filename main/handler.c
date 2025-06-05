#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include "main.h"
#include "pin_map.h"
#include "adc.h"
#include "modem.h"
#include "handler.h"
#include "inputs.h"
#include "tmp102.h"
#include "output.h"
#include "config_store.h"
#include "gps.h"

static const char* TAG = "HANDLER";

extern QueueHandle_t rx_message_queue;

input_monitor_config_t cur_config;
input_monitor_config_t alg_config;
input_monitor_config_t res_config;
static input_monitor_config_t valarm_config;

static bool already_triggered_cur = false;
static bool already_triggered_alg = false;
static bool already_triggered_res = false;
static bool already_triggered_valarm = false;

void restore_input_configs_from_flash(void){
    if (config_store_load_cur_config(&cur_config) != ESP_OK) {
        // First boot or corrupt, set defaults
        memset(&cur_config, 0, sizeof(cur_config));
        cur_config.type = THRESH_OFF;
        config_store_save_cur_config(&cur_config);
    }

    // Load ALG config
    if (config_store_load_alg_config(&alg_config) != ESP_OK) {
        memset(&alg_config, 0, sizeof(alg_config));
        alg_config.type = THRESH_OFF;
        config_store_save_alg_config(&alg_config);
    }

    // Load RES config
    if (config_store_load_res_config(&res_config) != ESP_OK) {
        memset(&res_config, 0, sizeof(res_config));
        res_config.type = THRESH_OFF;
        config_store_save_res_config(&res_config);
    }

    // Load VALARM config
    if (config_store_load_valarm_config(&valarm_config) != ESP_OK) {
        memset(&valarm_config, 0, sizeof(valarm_config));
        valarm_config.type = THRESH_OFF;
        config_store_save_valarm_config(&valarm_config);
    }

}

//Input naming helper function
void get_input_display(const char *input, char *buf, size_t bufsize) {
    char name[32] = {0};
    if (config_store_get_input_name(input, name, sizeof(name)) == ESP_OK && strlen(name)) {
        snprintf(buf, bufsize, "%s [%s]", input, name);
    } else {
        snprintf(buf, bufsize, "%s", input);
    }
}

// Utility function
static bool should_trigger(float value, const input_monitor_config_t *cfg) {
    switch(cfg->type) {
        case THRESH_OFF:
            return false;
        case THRESH_LIMIT:
            if (cfg->cond == COND_OVER)  return value > cfg->value1;
            if (cfg->cond == COND_UNDER) return value < cfg->value1;
            break;
        case THRESH_RANGE:
            if (cfg->cond == COND_INSIDE)  return (value >= cfg->value1 && value <= cfg->value2);
            if (cfg->cond == COND_OUTSIDE) return (value < cfg->value1 || value > cfg->value2);
            break;
        default:
            break;
    }
    return false;
}


void check_input_conditions(float cur, float alg, float res, float battery_volts) {
    char reply[128];
    char numbers[256];

    // ——— CUR (Current) ———
    bool trigger_cur = (cur_config.type != THRESH_OFF) && should_trigger(cur, &cur_config);
    if (trigger_cur && !already_triggered_cur) {
        ESP_LOGW(TAG, "CUR condition met: %.2f mA", cur / 100.0);
        if (config_store_list_log("CUR", numbers, sizeof(numbers)) == ESP_OK && *numbers) {
            for (char *token = strtok(numbers, ","); token; token = strtok(NULL, ",")) {
                char input_disp[48];
                get_input_display("CUR", input_disp, sizeof(input_disp));
                snprintf(reply, sizeof(reply), "ALERT: %s Triggered! Value: %.2f mA", input_disp, cur / 100.0);
                modem_send_sms(token, reply);
            }
        }
        if (cur_config.output == OUT1) {
            output_controller_send(&(output_cmd_t){ .id = OUTPUT_ID_1, .level = 1 });
        } else if (cur_config.output == OUT2) {
            output_controller_send(&(output_cmd_t){ .id = OUTPUT_ID_2, .level = 1 });
        }
        already_triggered_cur = true;
    }
    else if (!trigger_cur && already_triggered_cur) {
        // clear CUR output
        if (cur_config.output == OUT1) {
            output_controller_send(&(output_cmd_t){ .id = OUTPUT_ID_1, .level = 0 });
        } else if (cur_config.output == OUT2) {
            output_controller_send(&(output_cmd_t){ .id = OUTPUT_ID_2, .level = 0 });
        }
        already_triggered_cur = false;
    }

    // ——— ALG (Analog Voltage) ———
    bool trigger_alg = (alg_config.type != THRESH_OFF) && should_trigger(alg, &alg_config);
    if (trigger_alg && !already_triggered_alg) {
        ESP_LOGW(TAG, "ALG condition met: %.2f V", alg / 1000.0);
        if (config_store_list_log("ALG", numbers, sizeof(numbers)) == ESP_OK && *numbers) {
            for (char *token = strtok(numbers, ","); token; token = strtok(NULL, ",")) {
                char input_disp[48];
                get_input_display("ALG", input_disp, sizeof(input_disp));
                snprintf(reply, sizeof(reply), "ALERT: %s Triggered! Value: %.2f V", input_disp, alg / 1000.0);
                modem_send_sms(token, reply);
            }
        }
        if (alg_config.output == OUT1) {
            output_controller_send(&(output_cmd_t){ .id = OUTPUT_ID_1, .level = 1 });
        } else if (alg_config.output == OUT2) {
            output_controller_send(&(output_cmd_t){ .id = OUTPUT_ID_2, .level = 1 });
        }
        already_triggered_alg = true;
    }
    else if (!trigger_alg && already_triggered_alg) {
        // clear ALG output
        if (alg_config.output == OUT1) {
            output_controller_send(&(output_cmd_t){ .id = OUTPUT_ID_1, .level = 0 });
        } else if (alg_config.output == OUT2) {
            output_controller_send(&(output_cmd_t){ .id = OUTPUT_ID_2, .level = 0 });
        }
        already_triggered_alg = false;
    }

    // ——— RES (Resistive) ———
    bool trigger_res = (res_config.type != THRESH_OFF) && should_trigger(res, &res_config);
    if (trigger_res && !already_triggered_res) {
        ESP_LOGW(TAG, "RES condition met: %.0f Ohm", res);
        if (config_store_list_log("RES", numbers, sizeof(numbers)) == ESP_OK && *numbers) {
            for (char *token = strtok(numbers, ","); token; token = strtok(NULL, ",")) {
                char input_disp[48];
                get_input_display("RES", input_disp, sizeof(input_disp));
                snprintf(reply, sizeof(reply), "ALERT: %s Triggered! Value: %.0f Ohm", input_disp, res);
                modem_send_sms(token, reply);
            }
        }
        if (res_config.output == OUT1) {
            output_controller_send(&(output_cmd_t){ .id = OUTPUT_ID_1, .level = 1 });
        } else if (res_config.output == OUT2) {
            output_controller_send(&(output_cmd_t){ .id = OUTPUT_ID_2, .level = 1 });
        }
        already_triggered_res = true;
    }
    else if (!trigger_res && already_triggered_res) {
        // clear RES output
        if (res_config.output == OUT1) {
            output_controller_send(&(output_cmd_t){ .id = OUTPUT_ID_1, .level = 0 });
        } else if (res_config.output == OUT2) {
            output_controller_send(&(output_cmd_t){ .id = OUTPUT_ID_2, .level = 0 });
        }
        already_triggered_res = false;
    }

    // ——— VALARM (Battery Voltage) ———
    bool trigger_val = (valarm_config.type != THRESH_OFF) && should_trigger(battery_volts, &valarm_config);
    if (trigger_val && !already_triggered_valarm) {
        ESP_LOGW(TAG, "VALARM triggered: %.2f V", battery_volts);
        if (config_store_list_log("VALARM", numbers, sizeof(numbers)) == ESP_OK && *numbers) {
            for (char *token = strtok(numbers, ","); token; token = strtok(NULL, ",")) {
                snprintf(reply, sizeof(reply), "Voltage Alarm Triggered: %.2f V", battery_volts);
                modem_send_sms(token, reply);
            }
        }
        if (valarm_config.output == OUT1) {
            output_controller_send(&(output_cmd_t){ .id = OUTPUT_ID_1, .level = 1 });
        } else if (valarm_config.output == OUT2) {
            output_controller_send(&(output_cmd_t){ .id = OUTPUT_ID_2, .level = 1 });
        }
        already_triggered_valarm = true;
    }
    else if (!trigger_val && already_triggered_valarm) {
        ESP_LOGI(TAG, "VALARM cleared: %.2f V", battery_volts);
        if (valarm_config.output == OUT1) {
            output_controller_send(&(output_cmd_t){ .id = OUTPUT_ID_1, .level = 0 });
        } else if (valarm_config.output == OUT2) {
            output_controller_send(&(output_cmd_t){ .id = OUTPUT_ID_2, .level = 0 });
        }
        already_triggered_valarm = false;
    }
}


// Helper functions as described earlier

output_action_t parse_output(const char *s) {
    if (strcasecmp(s, "OUT1") == 0) return OUT1;
    if (strcasecmp(s, "OUT2") == 0) return OUT2;
    return OUT_NONE;
}

threshold_type_t parse_type(const char *s) {
    if (strcasecmp(s, "OFF") == 0)   return THRESH_OFF;
    if (strcasecmp(s, "LIMIT") == 0) return THRESH_LIMIT;
    if (strcasecmp(s, "RANGE") == 0) return THRESH_RANGE;
    return THRESH_OFF;
}

condition_t parse_condition(const char *s) {
    if (strcasecmp(s, "OVER") == 0)    return COND_OVER;
    if (strcasecmp(s, "UNDER") == 0)   return COND_UNDER;
    if (strcasecmp(s, "INSIDE") == 0)  return COND_INSIDE;
    if (strcasecmp(s, "OUTSIDE") == 0) return COND_OUTSIDE;
    return COND_NONE;
}


void send_reply(const char *to_number, const char *message)
{
    if (!to_number || !message) return;
    modem_send_sms(to_number, message);
    ESP_LOGI(TAG, "Reply sent to %s: %s", to_number, message);
}


static void trim(char *str) {
    // Trim leading
    while (*str && isspace((unsigned char)*str)) str++;
    // Trim trailing
    char *end = str + strlen(str) - 1;
    while (end > str && isspace((unsigned char)*end)) *end-- = '\0';
}

// ---- NEW CUR/ALG/RES handler ----
// Assumes: input_type = "CUR" or "ALG" or "RES"
// params:  points to string after <INPUT> (e.g. "OUT1 LIMIT 2.5 OVER")
// sender:  the phone number to reply to

void handle_input_config(const char *input_type, const char *params, const char *sender)
{
    char out_str[8] = {0}, type_str[16] = {0}, arg3[16] = {0}, arg4[16] = {0}, arg5[16] = {0};
    char reply[160];
    int n = sscanf(params, "%7s %15s %15s %15s %15s", out_str, type_str, arg3, arg4, arg5);

    input_monitor_config_t config = {0};

    // --- Output ---
    if      (strcasecmp(out_str, "OUT1") == 0) config.output = OUT1;
    else if (strcasecmp(out_str, "OUT2") == 0) config.output = OUT2;
    else if (strcasecmp(out_str, "NONE") == 0) config.output = OUT_NONE;
    else {
        snprintf(reply, sizeof(reply), "Invalid output '%s'. Use OUT1, OUT2, or NONE.", out_str);
        send_reply(sender, reply);
        return;
    }

    // --- OFF ---
    if (strcasecmp(type_str, "OFF") == 0 && n >= 2) {
        config.type = THRESH_OFF;
        config.value1 = config.value2 = 0;
        config.cond = COND_NONE;
        snprintf(reply, sizeof(reply), "%s monitoring disabled.", input_type);
    }
    // --- LIMIT ---
    else if (strcasecmp(type_str, "LIMIT") == 0 && n >= 4) {
        config.type = THRESH_LIMIT;
        config.value1 = atof(arg3);
        if      (strcasecmp(arg4, "OVER") == 0)  config.cond = COND_OVER;
        else if (strcasecmp(arg4, "UNDER") == 0) config.cond = COND_UNDER;
        else {
            snprintf(reply, sizeof(reply), "Invalid condition '%s'. Use OVER or UNDER.", arg4);
            send_reply(sender, reply);
            return;
        }
        snprintf(reply, sizeof(reply), "%s: %s, LIMIT %.2f %s -> %s", input_type, out_str, config.value1, arg4, out_str);
    }
    // --- RANGE ---
    else if (strcasecmp(type_str, "RANGE") == 0 && n >= 5) {
        config.type = THRESH_RANGE;
        config.value1 = atof(arg3);
        config.value2 = atof(arg4);
        if      (strcasecmp(arg5, "INSIDE") == 0)  config.cond = COND_INSIDE;
        else if (strcasecmp(arg5, "OUTSIDE") == 0) config.cond = COND_OUTSIDE;
        else {
            snprintf(reply, sizeof(reply), "Invalid condition '%s'. Use INSIDE or OUTSIDE.", arg5);
            send_reply(sender, reply);
            return;
        }
        snprintf(reply, sizeof(reply), "%s: %s, RANGE %.2f-%.2f %s -> %s", input_type, out_str, config.value1, config.value2, arg5, out_str);
    }
    // --- Error ---
    else {
        snprintf(reply, sizeof(reply), "Invalid command format for %s. See: %s OUT1 OFF, %s OUT1 LIMIT <v> OVER, %s OUT1 RANGE <lo> <hi> INSIDE", input_type, input_type, input_type, input_type);
        send_reply(sender, reply);
        return;
    }

    // --- Save to flash ---
    if      (strcasecmp(input_type, "CUR") == 0) { config_store_save_cur_config(&config); cur_config = config; }
    else if (strcasecmp(input_type, "ALG") == 0) { config_store_save_alg_config(&config); alg_config = config; }
    else if (strcasecmp(input_type, "RES") == 0) { config_store_save_res_config(&config); res_config = config; }

    send_reply(sender, reply);
}

static void handle_valarm_config(const char *params, const char *sender)
{
    char out_str[8]={0}, val_str[16]={0}, reply[128];
    int n = sscanf(params, "%7s %15s", out_str, val_str);
    if (n < 2) {
        send_reply(sender, "Usage: VALARM <OUT1|OUT2|NONE> <VOLTAGE>");
        return;
    }

    // parse output
    valarm_config.output = parse_output(out_str);
    if (valarm_config.output == OUT_NONE && strcasecmp(out_str,"NONE")!=0) {
        snprintf(reply, sizeof(reply),
                 "Invalid output '%s'. Use OUT1, OUT2 or NONE.", out_str);
        send_reply(sender, reply);
        return;
    }

    // configure as a simple UNDER limit
    valarm_config.type  = THRESH_LIMIT;
    valarm_config.cond  = COND_UNDER;
    valarm_config.value1 = atof(val_str);
    valarm_config.value2 = 0;

    // persist
    config_store_save_valarm_config(&valarm_config);

    // ack
    snprintf(reply, sizeof(reply),
             "VALARM: %s if < %.2fV",
             out_str, valarm_config.value1);
    send_reply(sender, reply);
}


// ---- Main parser ----
static void parse_command(const sms_message_t *sms) {
    char *cmd = sms->message;
    char response[512];
    char arg1[32], arg2[32], arg3[32], arg4[32];

    if (strncasecmp(cmd, "CMD:", 4) != 0) return;
    cmd += 4;
    trim(cmd);

    // CUR/ALG/RES config command (first token)
    if (strncasecmp(cmd, "CUR ", 4) == 0) { handle_input_config("CUR", cmd + 4, sms->sender); return; }
    if (strncasecmp(cmd, "ALG ", 4) == 0) { handle_input_config("ALG", cmd + 4, sms->sender); return; }
    if (strncasecmp(cmd, "RES ", 4) == 0) { handle_input_config("RES", cmd + 4, sms->sender); return; }

    // Everything below here is your existing commands, unchanged.
    if (sscanf(cmd, "SETID %31[^\n]", arg1) == 1) {
        config_store_set_unit_id(arg1);
        snprintf(response, sizeof(response), "ID set to %s", arg1);
        send_reply(sms->sender, response);
        return;
    }
    
    // VALARM config (single‐threshold UNDER alarm)
        if (strncasecmp(cmd, "VALARM ", 7) == 0) {
            handle_valarm_config(cmd + 7, sms->sender);
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
            snprintf(response, sizeof(response), "%s log is empty", arg1);
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
        send_reply(sms->sender, response);
        return;
    }

    if (strcasecmp(cmd, "LOCATION") == 0) {
        gps_data_t gps_data = gps_get_data();
        if (!gps_has_lock()) {
            send_reply(sms->sender, "No GPS Fix");
            return;
        } else {
            char time_str[6];
            gps_format_time_hhmm(gps_data.time, time_str, sizeof(time_str));    
            snprintf(response, sizeof(response), "GPS Location at %s GMT  http://map.google.com/?q=%.6f,%.6f", time_str, gps_data.latitude, gps_data.longitude); 
            send_reply(sms->sender, response);
            return;
        }
    }
    
    if (strcasecmp(cmd, "GPS") == 0) {
        gps_data_t gps_data = gps_get_data();
        char time_str[6];
        gps_format_time_hhmm(gps_data.time, time_str, sizeof(time_str));    
        snprintf(response, sizeof(response), "GPS: %s GMT Lat: %.6f Lon: %.6f Alt: %.2f m HDOP: %.2f Sats: %d", time_str, gps_data.latitude, gps_data.longitude, gps_data.altitude, gps_data.hdop, gps_data.satellites_used);
        send_reply(sms->sender, response);
        return;
    }

    if (strcasecmp(cmd, "BATTV") == 0) {
        float v = battery_volts; // should be defined in adc/sensor module
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

    if (sscanf(cmd, "IN1 %7s", arg1) == 1) {
        output_action_t out = parse_output(arg1);
            if (out == OUT_NONE && strcasecmp(arg1,"NONE")!=0) {
                send_reply(sms->sender, "Usage: IN1 OUT1|OUT2|NONE");
            } 
            else {
                config_store_set_input_output("IN1", out);
                char buf[128];
                char input_disp[48];
                get_input_display("IN1", input_disp, sizeof(input_disp));               
                snprintf(buf, sizeof(buf), "%s will drive %s", input_disp, arg1);
                send_reply(sms->sender, buf);
            }
            return;
    }

    if (sscanf(cmd, "IN2 %7s", arg1) == 1) {
        output_action_t out = parse_output(arg1);
        if (out == OUT_NONE && strcasecmp(arg1,"NONE")!=0) {
            send_reply(sms->sender, "Usage: IN2 OUT1|OUT2|NONE");
        } else {
            config_store_set_input_output("IN2", out);
            char buf[128];
            char input_disp[48];
            get_input_display("IN2", input_disp, sizeof(input_disp));
            snprintf(buf, sizeof(buf), "%s will drive %s", input_disp, arg1);
            send_reply(sms->sender, buf);
        }
        return;
    }

if (sscanf(cmd, "WRITESERIAL%31s", arg1) == 1) {
    config_store_set_serial(arg1);
    snprintf(response, sizeof(response), "Serial number set to %s", arg1);
    send_reply(sms->sender, response);
    return;
}

if (strcasecmp(cmd, "READSERIAL") == 0) {
    char serial[32] = {0};
    if (config_store_get_serial(serial, sizeof(serial)) == ESP_OK && strlen(serial) > 0) {
        snprintf(response, sizeof(response), "Serial number: %s", serial);
    } else {
        snprintf(response, sizeof(response), "Serial number not set");
    }
    send_reply(sms->sender, response);
    return;
}

if (sscanf(cmd, "NAME %31s %31[^\n]", arg1, arg2) == 2) {
    if (config_store_set_input_name(arg1, arg2) == ESP_OK) {
        snprintf(response, sizeof(response), "%s name set to %s", arg1, arg2);
    } else {
        snprintf(response, sizeof(response), "Invalid input for NAME: %s", arg1);
    }
    send_reply(sms->sender, response);
    return;
}



    // Any unknown or invalid command
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
