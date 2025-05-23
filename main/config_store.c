
#include "main.h"
#include "pin_map.h"
#include "adc.h"
#include "modem.h"
#include "handler.h"
#include "inputs.h"
#include "tmp102.h"
#include "output.h"
#include "config_store.h"

#include <stdint.h>
#include "config_store.h"
#include "nvs_flash.h"
#include "nvs.h"
#include <string.h>
#include <stdio.h>
#include "main.h"

// --- NVS namespace/keys ---
#define NVS_NAMESPACE   "cfg"
#define KEY_UNIT_ID     "unit_id"
#define KEY_VALARM      "valarm_cfg"
#define KEY_CURCFG      "curcfg"
#define KEY_ALGCFG      "algcfg"
#define KEY_RESCFG      "rescfg"
// IN->OUT mapping keys
#define KEY_IN1OUT      "in1_out"
#define KEY_IN2OUT      "in2_out"

// === String Save/Load Helpers ===
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

// === Blob Save/Load for struct configs ===
static esp_err_t save_blob(const char *key, const void *data, size_t len) {
    nvs_handle_t handle;
    ESP_ERROR_CHECK(nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle));
    esp_err_t err = nvs_set_blob(handle, key, data, len);
    if (err == ESP_OK) err = nvs_commit(handle);
    nvs_close(handle);
    return err;
}

static esp_err_t load_blob(const char *key, void *data, size_t len) {
    nvs_handle_t handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READONLY, &handle);
    if (err != ESP_OK) return err;
    size_t size = len;
    err = nvs_get_blob(handle, key, data, &size);
    nvs_close(handle);
    return err;
}

// === INIT / RESET ===
void config_store_init(void) {
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES ||
        err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);
    vTaskDelay(1000 / portTICK_PERIOD_MS);
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

    // Set default configs for CUR/ALG/RES and VALARM to OFF
    input_monitor_config_t def = {0};
    def.type = THRESH_OFF;
    save_blob(KEY_CURCFG, &def, sizeof(def));
    save_blob(KEY_ALGCFG, &def, sizeof(def));
    save_blob(KEY_RESCFG, &def, sizeof(def));
    save_blob(KEY_VALARM, &def, sizeof(def));

    // Default IN1/IN2->NONE
    config_store_set_input_output("IN1", OUT_NONE);
    config_store_set_input_output("IN2", OUT_NONE);

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
        return ESP_OK;
    }
    if (strlen(log_buffer) + strlen(number) + 2 >= sizeof(log_buffer)) {
        return ESP_ERR_NO_MEM;
    }
    if (strlen(log_buffer) > 0) strcat(log_buffer, ",");
    strcat(log_buffer, number);
    return save_string(log_key, log_buffer);
}

esp_err_t config_store_remove_number(const char *log_name, const char *number) {
    snprintf(log_key, sizeof(log_key), "log_%s", log_name);
    if (load_string(log_key, log_buffer, sizeof(log_buffer)) != ESP_OK) return ESP_FAIL;
    char *start = strstr(log_buffer, number);
    if (!start) return ESP_OK;
    char *end = start + strlen(number);
    if (*end == ',') end++;
    else if (start != log_buffer) start--;
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

// === CUR/ALG/RES/VALARM persistent config ===
esp_err_t config_store_save_cur_config(const input_monitor_config_t *cfg) {
    return save_blob(KEY_CURCFG, cfg, sizeof(*cfg));
}
esp_err_t config_store_save_alg_config(const input_monitor_config_t *cfg) {
    return save_blob(KEY_ALGCFG, cfg, sizeof(*cfg));
}
esp_err_t config_store_save_res_config(const input_monitor_config_t *cfg) {
    return save_blob(KEY_RESCFG, cfg, sizeof(*cfg));
}
esp_err_t config_store_save_valarm_config(const input_monitor_config_t *cfg) {
    return save_blob(KEY_VALARM, cfg, sizeof(*cfg));
}

esp_err_t config_store_load_cur_config(input_monitor_config_t *cfg) {
    size_t len = sizeof(*cfg);
    return load_blob(KEY_CURCFG, cfg, len);
}
esp_err_t config_store_load_alg_config(input_monitor_config_t *cfg) {
    size_t len = sizeof(*cfg);
    return load_blob(KEY_ALGCFG, cfg, len);
}
esp_err_t config_store_load_res_config(input_monitor_config_t *cfg) {
    size_t len = sizeof(*cfg);
    return load_blob(KEY_RESCFG, cfg, len);
}
esp_err_t config_store_load_valarm_config(input_monitor_config_t *cfg) {
    size_t len = sizeof(*cfg);
    return load_blob(KEY_VALARM, cfg, len);
}

// === IN1/IN2 -> OUTPUT mapping ===
esp_err_t config_store_set_input_output(const char *input, output_action_t out) {
    const char *key = (strcmp(input, "IN1") == 0 ? KEY_IN1OUT : KEY_IN2OUT);
    nvs_handle_t h;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &h);
    if (err != ESP_OK) return err;
    err = nvs_set_u8(h, key, (uint8_t)out);
    if (err == ESP_OK) err = nvs_commit(h);
    nvs_close(h);
    return err;
}

esp_err_t config_store_get_input_output(const char *input, output_action_t *out) {
    const char *key = (strcmp(input, "IN1") == 0 ? KEY_IN1OUT : KEY_IN2OUT);
    nvs_handle_t h;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READONLY, &h);
    if (err != ESP_OK) return err;
    uint8_t v;
    err = nvs_get_u8(h, key, &v);
    nvs_close(h);
    if (err == ESP_ERR_NVS_NOT_FOUND) {
        *out = OUT_NONE;
        return ESP_OK;
    }
    *out = (output_action_t)v;
    return err;
}

