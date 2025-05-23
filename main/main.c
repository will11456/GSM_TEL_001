#include <stdint.h>
#include <stddef.h>



#include "main.h"
#include "gps.h"
#include "pin_map.h"
#include "adc.h"
#include "modem.h"
#include "handler.h"
#include "inputs.h"
#include "tmp102.h"
#include "output.h"
#include "config_store.h"
#include "nvs_flash.h"
#include "config_store.h"


static const char* TAG = "MAIN";

TaskHandle_t adcTaskHandle = NULL;
TaskHandle_t modemTaskHandle = NULL;
TaskHandle_t inputTaskHandle = NULL;



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
    
    gpio_reset_pin(TEST_PAD_2);
    gpio_reset_pin(TEST_PAD_4);

    //Set pull-up resistors for input pins
    gpio_set_pull_mode(TEST_PAD_2, GPIO_PULLDOWN_ENABLE);
    gpio_set_pull_mode(TEST_PAD_4, GPIO_PULLDOWN_ENABLE);
    

    //Set GPIO pin modes
    gpio_set_direction(INPUT_1, GPIO_MODE_INPUT);
    gpio_set_direction(INPUT_2, GPIO_MODE_INPUT);

    gpio_set_direction(OUTPUT_1, GPIO_MODE_OUTPUT);
    gpio_set_direction(OUTPUT_2, GPIO_MODE_OUTPUT);
    
    gpio_set_direction(PWR_KEY, GPIO_MODE_OUTPUT);
    gpio_set_direction(RAIL_4V_EN, GPIO_MODE_OUTPUT);


    gpio_set_direction(TEST_PAD_2, GPIO_MODE_INPUT);
    gpio_set_direction(TEST_PAD_4, GPIO_MODE_INPUT);
    
    gpio_set_level(OUTPUT_1, 0);
    gpio_set_level(OUTPUT_2, 0);

    ESP_LOGW(TAG, "GPIO Init Done");

}


void EnableModemRail(void)
{
    gpio_set_level(RAIL_4V_EN, 0);
    vTaskDelay(500 / portTICK_PERIOD_MS); // Wait for 1 second to stabilize the rail
    gpio_set_level(RAIL_4V_EN, 1);
    ESP_LOGW(TAG, "4V Rail Enabled");
}



void app_main(void)
{


    //create mutexs
    gps_mutex = xSemaphoreCreateMutex();

    //Initialize GPIO
    GPIOInit();

    //Initialize Output Controller
    output_controller_init();

    //initialize I2C
    i2c_master_init();

    //Enable 4V rail for modem
    EnableModemRail();

    //init modem and UART1
    modem_init();

    //Init NVS
    config_store_init();

    //retrieve config
    restore_input_configs_from_flash();

    //init GPS and UART2
    gps_init();



    //Start Tasks
    xTaskCreate(OutputTask, "OutputTask", 2*2048, NULL, 6, NULL);
    xTaskCreate(ADCTask, "read_ads1115_task", 2048*8, NULL, 3, &adcTaskHandle);
    xTaskCreate(InputTask, "InputTask", 2048*2, NULL, 5, &inputTaskHandle);
    xTaskCreate(ModemTask, "modem_task", 2048*8, NULL, 1, NULL);
    xTaskCreate(SmsHandlerTask, "SmsHandlerTask", 4096, NULL, 2, NULL);
    xTaskCreate(tmp102_task, "tmp102_task", 2048, NULL, 10, NULL);
    xTaskCreate(gps_task, "gps_task", 4096, NULL, 7, NULL);




    vTaskDelay(10000 / portTICK_PERIOD_MS);

    // while(1){
        
    //     report_gps_status();
        
    //     vTaskDelay(1000 / portTICK_PERIOD_MS); // Delay for 1 second before checking again
    // }

}
