
#include "main.h"
#include "pin_map.h"
#include "adc.h"
#include "modem.h"
#include "handler.h"
#include "inputs.h"
#include "tmp102.h"
#include "output.h"
#include "config_store.h"




static const char *TAG = "INPUTS";

void InputTask(void *pvParameter)
{
    // Configure both input pins as inputs.
    gpio_set_direction(INPUT_1, GPIO_MODE_INPUT);
    gpio_set_direction(INPUT_2, GPIO_MODE_INPUT);

    // Initialize last known levels.
    int last_input1 = gpio_get_level(INPUT_1);
    int last_input2 = gpio_get_level(INPUT_2);

    for (;;) {
        // Read current pin levels.
        int current_input1 = gpio_get_level(INPUT_1);
        int current_input2 = gpio_get_level(INPUT_2);

        // Detect a rising edge for INPUT_1.
        if (current_input1 == 1 && last_input1 == 0) {
            ESP_LOGI(TAG, "Input 1 activated");
            send_sms_to_all_by_log("Input 1 activated", "INPUT1");
            // Debounce delay.
            vTaskDelay(pdMS_TO_TICKS(1000));
        }

        // Detect a rising edge for INPUT_2.
        if (current_input2 == 1 && last_input2 == 0) {
            ESP_LOGI(TAG, "Input 2 activated");
            send_sms_to_all_by_log("Input 2 activated", "INPUT2");
            // Debounce delay.
            vTaskDelay(pdMS_TO_TICKS(1000));
        }

        // Update the last known levels.
        last_input1 = current_input1;
        last_input2 = current_input2;

        // Polling delay.
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}