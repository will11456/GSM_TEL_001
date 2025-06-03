#pragma once
#include "esp_err.h"
#include <stddef.h>
#include "handler.h"  // for input_monitor_config_t, output_action_t, etc

#ifdef __cplusplus
extern "C" {
#endif

typedef input_monitor_config_t valarm_config_t;

void config_store_init(void);

esp_err_t config_store_reset_defaults(void);

esp_err_t config_store_set_unit_id(const char *id);
esp_err_t config_store_get_unit_id(char *out_id, size_t max_len);

// set/get which output IN1/IN2 should drive
esp_err_t config_store_set_input_output(const char *input, output_action_t out);
esp_err_t config_store_get_input_output(const char *input, output_action_t *out);

esp_err_t config_store_add_number(const char *log_name, const char *number);
esp_err_t config_store_remove_number(const char *log_name, const char *number);
esp_err_t config_store_clear_log(const char *log_name);
esp_err_t config_store_list_log(const char *log_name, char *out_buf, size_t max_len);

// CUR/ALG/RES configs
esp_err_t config_store_save_cur_config(const input_monitor_config_t *cfg);
esp_err_t config_store_save_alg_config(const input_monitor_config_t *cfg);
esp_err_t config_store_save_res_config(const input_monitor_config_t *cfg);
esp_err_t config_store_save_valarm_config(const valarm_config_t *cfg);

esp_err_t config_store_load_cur_config(input_monitor_config_t *cfg);
esp_err_t config_store_load_alg_config(input_monitor_config_t *cfg);
esp_err_t config_store_load_res_config(input_monitor_config_t *cfg);
esp_err_t config_store_load_valarm_config(valarm_config_t *cfg);

// Serial number management
esp_err_t config_store_set_serial(const char *serial);
esp_err_t config_store_get_serial(char *out_serial, size_t max_len);






#ifdef __cplusplus
}
#endif
