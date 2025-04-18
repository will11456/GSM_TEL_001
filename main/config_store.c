#include <stdint.h>
#include "config_store.h"
#include "nvs_flash.h"
#include "nvs.h"
#include <string.h>
#include <stdio.h>


#include "main.h"
#include "pin_map.h"
#include "adc.h"
#include "modem.h"
#include "handler.h"
#include "inputs.h"
#include "tmp102.h"
#include "output.h"
#include "config_store.h"


#define NVS_NAMESPACE "cfg"

#define KEY_UNIT_ID     "unit_id"
#define KEY_VALARM      "valarm_cfg"

static esp_err_t save_string(const char *key, const char *value) {
    nvs_handle_t handle;
    ESP_ERROR_CHECK(nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle));
    esp_err_t err = nvs_set_str(handle, key, value);
    if (err == ESP_OK) {
        err = nvs_commit(handle);
    }
    nvs_close(handle);
    return err;
}

static esp_err_t load_string(const char *key, char *out, size_t max_len) {
    nvs_handle_t handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READONLY, &handle);
    if (err != ESP_OK) return err;
    err = nvs_get_str(handle, key, out, &max_len);
    nvs_close(handle);
    return err;
}

// === INIT / RESET ===

void config_store_init(void) {
    // NVS already initialized by app
}

esp_err_t config_store_reset_defaults(void) {
    nvs_handle_t handle;
    ESP_ERROR_CHECK(nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle));
    esp_err_t err = nvs_erase_all(handle);
    if (err == ESP_OK) {
        err = nvs_commit(handle);
        if (err == ESP_OK) {
            err = save_string(KEY_UNIT_ID, "Unit ID");
        }
    }
    nvs_close(handle);
    return err;
}

// === UNIT ID ===

esp_err_t config_store_set_unit_id(const char *id) {
    return save_string(KEY_UNIT_ID, id);
}

esp_err_t config_store_get_unit_id(char *out_id, size_t max_len) {
    return load_string(KEY_UNIT_ID, out_id, max_len);
}

// === PHONE NUMBER LOGS ===

static char log_key[32];
static char log_buffer[512];

esp_err_t config_store_add_number(const char *log_name, const char *number) {
    snprintf(log_key, sizeof(log_key), "log_%s", log_name);
    if (load_string(log_key, log_buffer, sizeof(log_buffer)) != ESP_OK) {
        log_buffer[0] = '\0';
    }

    if (strstr(log_buffer, number)) {
        return ESP_OK; // already present
    }

    if (strlen(log_buffer) + strlen(number) + 2 >= sizeof(log_buffer)) {
        return ESP_ERR_NO_MEM;
    }

    if (strlen(log_buffer) > 0) {
        strcat(log_buffer, ",");
    }
    strcat(log_buffer, number);
    return save_string(log_key, log_buffer);
}

esp_err_t config_store_remove_number(const char *log_name, const char *number) {
    snprintf(log_key, sizeof(log_key), "log_%s", log_name);
    if (load_string(log_key, log_buffer, sizeof(log_buffer)) != ESP_OK) return ESP_FAIL;

    char *start = strstr(log_buffer, number);
    if (!start) return ESP_OK; // not found

    char *end = start + strlen(number);
    if (*end == ',') end++; // skip comma
    else if (start != log_buffer) start--; // remove comma before

    memmove(start, end, strlen(end) + 1);
    return save_string(log_key, log_buffer);
}

esp_err_t config_store_clear_log(const char *log_name) {
    snprintf(log_key, sizeof(log_key), "log_%s", log_name);
    return save_string(log_key, "");
}

esp_err_t config_store_list_log(const char *log_name, char *out_buf, size_t max_len) {
    snprintf(log_key, sizeof(log_key), "log_%s", log_name);
    return load_string(log_key, out_buf, max_len);
}

// === MAPPINGS ===

esp_err_t config_store_set_mapping(const char *input_name, const input_mapping_t *mapping) {
    snprintf(log_key, sizeof(log_key), "map_%s", input_name);
    snprintf(log_buffer, sizeof(log_buffer), "%s,%s,%d,%d",
             mapping->output, mapping->mode, mapping->threshold1, mapping->threshold2);
    return save_string(log_key, log_buffer);
}

esp_err_t config_store_get_mapping(const char *input_name, input_mapping_t *out) {
    snprintf(log_key, sizeof(log_key), "map_%s", input_name);
    if (load_string(log_key, log_buffer, sizeof(log_buffer)) != ESP_OK) return ESP_FAIL;
    memset(out, 0, sizeof(input_mapping_t));
    sscanf(log_buffer, "%7[^,],%15[^,],%d,%d",
           out->output, out->mode, &out->threshold1, &out->threshold2);
    return ESP_OK;
}

// === VOLTAGE ALARM ===

esp_err_t config_store_set_voltage_alarm(const voltage_alarm_config_t *cfg) {
    snprintf(log_buffer, sizeof(log_buffer), "%d,%s", cfg->threshold_mv, cfg->output);
    return save_string(KEY_VALARM, log_buffer);
}

esp_err_t config_store_get_voltage_alarm(voltage_alarm_config_t *out) {
    if (load_string(KEY_VALARM, log_buffer, sizeof(log_buffer)) != ESP_OK) return ESP_FAIL;
    sscanf(log_buffer, "%d,%7s", &out->threshold_mv, out->output);
    return ESP_OK;
}
