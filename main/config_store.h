#ifndef CONFIG_STORE_H
#define CONFIG_STORE_H

#include <stdint.h>
#include "esp_err.h"
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

// === INIT / RESET ===

/**
 * @brief Initialize the configuration storage (must be called after nvs_flash_init()).
 */
void config_store_init(void);

/**
 * @brief Reset all configuration to default values.
 *        Clears all logs, mappings, and resets Unit ID to "Unit ID".
 */
esp_err_t config_store_reset_defaults(void);

// === UNIT ID ===

esp_err_t config_store_set_unit_id(const char *id);
esp_err_t config_store_get_unit_id(char *out_id, size_t max_len);

// === PHONE NUMBER LOGS ===

esp_err_t config_store_add_number(const char *log_name, const char *number);
esp_err_t config_store_remove_number(const char *log_name, const char *number);
esp_err_t config_store_clear_log(const char *log_name);
esp_err_t config_store_list_log(const char *log_name, char *out_buf, size_t max_len);

// === MAPPINGS ===

typedef struct {
    char output[8];       // "OUT1", "OUT2", or "NONE"
    char mode[16];        // e.g., "ON", "OVER", "RANGE", "INSIDE", etc.
    int threshold1;       // e.g., lower bound
    int threshold2;       // e.g., upper bound (optional)
} input_mapping_t;

esp_err_t config_store_set_mapping(const char *input_name, const input_mapping_t *mapping);
esp_err_t config_store_get_mapping(const char *input_name, input_mapping_t *out);

// === VOLTAGE ALARM CONFIG ===

typedef struct {
    int threshold_mv;     // in millivolts
    char output[8];       // "OUT1", "OUT2", or "NONE"
} voltage_alarm_config_t;

esp_err_t config_store_set_voltage_alarm(const voltage_alarm_config_t *cfg);
esp_err_t config_store_get_voltage_alarm(voltage_alarm_config_t *out);

#ifdef __cplusplus
}
#endif

#endif // CONFIG_STORE_H
