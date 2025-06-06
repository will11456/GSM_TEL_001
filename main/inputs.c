#include "main.h"
#include "pin_map.h"
#include "adc.h"
#include "modem.h"
#include "handler.h"
#include "inputs.h"
#include "tmp102.h"
#include "output.h"
#include "config_store.h"

#define IN1_GPIO    INPUT_1
#define IN2_GPIO    INPUT_2

static const char *TAG = "INPUTS";

// previous input states
static bool prev_in1;
static bool prev_in2;

// current and previous mapping from inputs to outputs
static output_action_t in1_out_cfg;
static output_action_t in2_out_cfg;
static output_action_t prev_in1_out_cfg;
static output_action_t prev_in2_out_cfg;

// static buffers for SMS and logs
static char numbers_buf[256];
static char msg_buf[128];

/**
 * Configure IN1/IN2 as inputs, latch initial states and mappings,
 * and activate outputs if inputs are already active.
 */
void digital_inputs_init(void)
{
    gpio_config_t io_conf = {
        .pin_bit_mask   = (1ULL<<IN1_GPIO) | (1ULL<<IN2_GPIO),
        .mode           = GPIO_MODE_INPUT,
        .pull_up_en     = GPIO_PULLUP_ENABLE,
        .pull_down_en   = GPIO_PULLDOWN_DISABLE,
        .intr_type      = GPIO_PIN_INTR_DISABLE
    };
    gpio_config(&io_conf);

    // latch initial physical state (inverted for opto-isolation)
    prev_in1 = !gpio_get_level(IN1_GPIO);
    prev_in2 = !gpio_get_level(IN2_GPIO);

    // load initial mapping from NVS
    if (config_store_get_input_output("IN1", &in1_out_cfg) != ESP_OK) {
        in1_out_cfg = OUT_NONE;
    }
    if (config_store_get_input_output("IN2", &in2_out_cfg) != ESP_OK) {
        in2_out_cfg = OUT_NONE;
    }
    // remember previous mapping for change detection
    prev_in1_out_cfg = in1_out_cfg;
    prev_in2_out_cfg = in2_out_cfg;

    // for each input active on boot: drive output and send SMS
    if (prev_in1) {
        // drive configured output
        if (in1_out_cfg == OUT1) {
            output_controller_send(&(output_cmd_t){ .id = OUTPUT_ID_1, .level = 1 });
        } else if (in1_out_cfg == OUT2) {
            output_controller_send(&(output_cmd_t){ .id = OUTPUT_ID_2, .level = 1 });
        }
        // send SMS using same format as edge
        if (config_store_list_log("IN1", numbers_buf, sizeof(numbers_buf)) == ESP_OK && *numbers_buf) {
            for (char *tok = strtok(numbers_buf, ","); tok; tok = strtok(NULL, ",")) {
                char input_disp[48];
                get_input_display("IN1", input_disp, sizeof(input_disp));
                snprintf(msg_buf, sizeof(msg_buf), "%s Activated", input_disp);
                ESP_LOGI(TAG, "%s Activated", input_disp);
                modem_send_sms(tok, msg_buf);
            }
        }
    }

    // do the same for IN2
    if (prev_in2) {
                if (in2_out_cfg == OUT1) {
            output_controller_send(&(output_cmd_t){ .id = OUTPUT_ID_1, .level = 1 });
        } else if (in2_out_cfg == OUT2) {
            output_controller_send(&(output_cmd_t){ .id = OUTPUT_ID_2, .level = 1 });
        }
        if (config_store_list_log("IN2", numbers_buf, sizeof(numbers_buf)) == ESP_OK && *numbers_buf) {
            for (char *tok = strtok(numbers_buf, ","); tok; tok = strtok(NULL, ",")) {
                char input_disp[48];
                get_input_display("IN2", input_disp, sizeof(input_disp));
                snprintf(msg_buf, sizeof(msg_buf), "%s Activated", input_disp);
                ESP_LOGI(TAG, "%s Activated", input_disp);
                modem_send_sms(tok, msg_buf);
            }
        }
    }
}

/**
 * Periodically called to detect mapping changes and edges on IN1/IN2,
 * send SMS logs, and drive the configured outputs.
 */
void check_digital_inputs(void)
{
    

    // read current inverted levels (active low)
    bool cur1 = !gpio_get_level(IN1_GPIO);
    bool cur2 = !gpio_get_level(IN2_GPIO);

    // reload mapping so SMS command changes apply immediately
    if (config_store_get_input_output("IN1", &in1_out_cfg) != ESP_OK) {
        in1_out_cfg = OUT_NONE;
    }
    if (config_store_get_input_output("IN2", &in2_out_cfg) != ESP_OK) {
        in2_out_cfg = OUT_NONE;
    }

    // handle mapping change for IN1
    if (in1_out_cfg != prev_in1_out_cfg) {
        // turn off previous output if it was on
        if (prev_in1 && prev_in1_out_cfg == OUT1) {
            output_controller_send(&(output_cmd_t){ .id = OUTPUT_ID_1, .level = 0 });
        } else if (prev_in1 && prev_in1_out_cfg == OUT2) {
            output_controller_send(&(output_cmd_t){ .id = OUTPUT_ID_2, .level = 0 });
        }
        // turn on new output if input still active
        if (prev_in1 && in1_out_cfg == OUT1) {
            output_controller_send(&(output_cmd_t){ .id = OUTPUT_ID_1, .level = 1 });
        } else if (prev_in1 && in1_out_cfg == OUT2) {
            output_controller_send(&(output_cmd_t){ .id = OUTPUT_ID_2, .level = 1 });
        }
        prev_in1_out_cfg = in1_out_cfg;
    }

    // handle mapping change for IN2
    if (in2_out_cfg != prev_in2_out_cfg) {
        if (prev_in2 && prev_in2_out_cfg == OUT1) {
            output_controller_send(&(output_cmd_t){ .id = OUTPUT_ID_1, .level = 0 });
        } else if (prev_in2 && prev_in2_out_cfg == OUT2) {
            output_controller_send(&(output_cmd_t){ .id = OUTPUT_ID_2, .level = 0 });
        }
        if (prev_in2 && in2_out_cfg == OUT1) {
            output_controller_send(&(output_cmd_t){ .id = OUTPUT_ID_1, .level = 1 });
        } else if (prev_in2 && in2_out_cfg == OUT2) {
            output_controller_send(&(output_cmd_t){ .id = OUTPUT_ID_2, .level = 1 });
        }
        prev_in2_out_cfg = in2_out_cfg;
    }

    // IN1: edge detection
    if (cur1 != prev_in1) {

        // drive output on edge
        if (in1_out_cfg == OUT1) {
            output_controller_send(&(output_cmd_t){ .id=OUTPUT_ID_1, .level=cur1 });
        } else if (in1_out_cfg == OUT2) {
            output_controller_send(&(output_cmd_t){ .id=OUTPUT_ID_2, .level=cur1 });
        }


        if (config_store_list_log("IN1", numbers_buf, sizeof(numbers_buf)) == ESP_OK && *numbers_buf) {
            for (char *tok = strtok(numbers_buf, ","); tok; tok = strtok(NULL, ",")) {
                char input_disp[48];
                get_input_display("IN1", input_disp, sizeof(input_disp));
                snprintf(msg_buf, sizeof(msg_buf), "%s %s", input_disp, cur1 ? "Activated" : "Deactivated");
                ESP_LOGI(TAG, "%s %s", input_disp, cur1 ? "Activated" : "Deactivated");
                modem_send_sms(tok, msg_buf);
            }
        }
        
        prev_in1 = cur1;
    }

    // IN2: edge detection
    if (cur2 != prev_in2) {

        if (in2_out_cfg == OUT1) {
            output_controller_send(&(output_cmd_t){ .id=OUTPUT_ID_1, .level=cur2 });
        } else if (in2_out_cfg == OUT2) {
            output_controller_send(&(output_cmd_t){ .id=OUTPUT_ID_2, .level=cur2 });
        }

        
        if (config_store_list_log("IN2", numbers_buf, sizeof(numbers_buf)) == ESP_OK && *numbers_buf) {
            for (char *tok = strtok(numbers_buf, ","); tok; tok = strtok(NULL, ",")) {
                char input_disp[48];
                get_input_display("IN2", input_disp, sizeof(input_disp));
                snprintf(msg_buf, sizeof(msg_buf), "%s %s", input_disp, cur2 ? "Activated" : "Deactivated");
                ESP_LOGI(TAG, "%s %s", input_disp, cur2 ? "Activated" : "Deactivated");
                modem_send_sms(tok, msg_buf);
            }
        }
        
        prev_in2 = cur2;
    }
}

void InputTask(void *pvParameter)
{

    ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
    ESP_LOGW(TAG, "Input Task Started"); 

    vTaskDelay(pdMS_TO_TICKS(10000)); // Allow other tasks to initialize

    digital_inputs_init();
    ESP_LOGW(TAG, "Digital Inputs: Complete");

    //Latch the _real_ initial state
    prev_in1 = !gpio_get_level(IN1_GPIO);
    prev_in2 = !gpio_get_level(IN2_GPIO);

    while (1) {
        check_digital_inputs();
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}
