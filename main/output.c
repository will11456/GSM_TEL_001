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



static const char *TAG = "OUTPUT";
QueueHandle_t output_queue;

// Mapping from output_id_t to actual GPIO pin:
static const gpio_num_t output_pins[] = {
    [OUTPUT_ID_1] = OUTPUT_1,
    [OUTPUT_ID_2] = OUTPUT_2,
};

void OutputTask(void *pvParameters)
{   

    output_cmd_t cmd;
    // Configure all output pins:
    for (int i = 0; i < sizeof(output_pins)/sizeof(output_pins[0]); i++) {
        gpio_set_direction(output_pins[i], GPIO_MODE_OUTPUT);
        gpio_set_level(output_pins[i], 0);
    }

    while (1) {
        if (xQueueReceive(output_queue, &cmd, portMAX_DELAY) == pdTRUE) {
            if (cmd.id < (sizeof(output_pins)/sizeof(output_pins[0]))) {
                gpio_set_level(output_pins[cmd.id], cmd.level);
                ESP_LOGI(TAG, "Output %d -> %s",
                         cmd.id + 1, cmd.level ? "ON" : "OFF");
            } else {
                ESP_LOGW(TAG, "Invalid output ID: %d", cmd.id);
            }
        }
    }
}

void output_controller_send(const output_cmd_t *cmd)
{
    if (output_queue) {
        xQueueSend(output_queue, cmd, 0);
    }
}