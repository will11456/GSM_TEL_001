#include "main.h"
#include "pin_map.h"
#include "adc.h"



static const char* TAG = "MAIN";




void GPIOInit(void)
{

    ESP_LOGW(TAG, "Initializing GPIO");

    //Reset all GPIO pins to default state
    gpio_reset_pin(SCL_PIN);
    gpio_reset_pin(SDA_PIN);

    gpio_reset_pin(INPUT_1);
    gpio_reset_pin(INPUT_2);
   
    gpio_reset_pin(OUTPUT_1);
    gpio_reset_pin(OUTPUT_2);
    
    gpio_reset_pin(MODEM_RX);
    gpio_reset_pin(MODEM_TX);

    gpio_reset_pin(PWR_KEY);
    gpio_reset_pin(RAIL_4V_EN);
    
    gpio_reset_pin(TEST_PAD_1);
    gpio_reset_pin(TEST_PAD_2);
    gpio_reset_pin(TEST_PAD_3);
    gpio_reset_pin(TEST_PAD_4);

    //Set pull-up resistors for input pins
    gpio_set_pull_mode(TEST_PAD_1, GPIO_PULLDOWN_ENABLE);
    gpio_set_pull_mode(TEST_PAD_2, GPIO_PULLDOWN_ENABLE);
    gpio_set_pull_mode(TEST_PAD_3, GPIO_PULLDOWN_ENABLE);
    gpio_set_pull_mode(TEST_PAD_4, GPIO_PULLDOWN_ENABLE);
    

    //Set GPIO pin modes
    gpio_set_direction(INPUT_1, GPIO_MODE_INPUT);
    gpio_set_direction(INPUT_2, GPIO_MODE_INPUT);

    gpio_set_direction(OUTPUT_1, GPIO_MODE_OUTPUT);
    gpio_set_direction(OUTPUT_2, GPIO_MODE_OUTPUT);
    
    gpio_set_direction(PWR_KEY, GPIO_MODE_OUTPUT);
    gpio_set_direction(RAIL_4V_EN, GPIO_MODE_OUTPUT);


    gpio_set_direction(TEST_PAD_1, GPIO_MODE_INPUT);
    gpio_set_direction(TEST_PAD_2, GPIO_MODE_INPUT);
    gpio_set_direction(TEST_PAD_3, GPIO_MODE_INPUT);
    gpio_set_direction(TEST_PAD_4, GPIO_MODE_INPUT);
    
    gpio_set_level(OUTPUT_1, 0);
    gpio_set_level(OUTPUT_2, 0);

    ESP_LOGW(TAG, "GPIO Init Done");

}





void app_main(void)
{
    //Initialize GPIO
    GPIOInit();

    //Start Tasks
    xTaskCreate(ADCTask, "read_ads1115_task", 2048*8, NULL, 10, NULL);

    //while (1){

        // //Toggle GPIO pin 1
        // gpio_set_level(OUTPUT_1,1);
        // vTaskDelay(1000 / portTICK_PERIOD_MS);
        // gpio_set_level(OUTPUT_1,0);
        // vTaskDelay(1000 / portTICK_PERIOD_MS);

        // //Toggle GPIO pin 2
        // gpio_set_level(OUTPUT_2,1);
        // vTaskDelay(1000 / portTICK_PERIOD_MS);
        // gpio_set_level(OUTPUT_2,0);
        // vTaskDelay(1000 / portTICK_PERIOD_MS);

        //Read inputs
        // ESP_LOGW(TAG, "Input 1: %d", gpio_get_level(INPUT_1));
        // ESP_LOGW(TAG, "Input 2: %d", gpio_get_level(INPUT_2));
        // vTaskDelay(300 / portTICK_PERIOD_MS);


    //}
}
