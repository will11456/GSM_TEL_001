#include "main.h"
#include "pin_map.h"
#include "adc.h"
#include "modem.h"
#include "handler.h"
#include "inputs.h"
#include "tmp102.h"
#include "output.h"
#include "config_store.h"





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


void EnableModemRail(void)
{
    gpio_set_level(RAIL_4V_EN, 1);
    ESP_LOGW(TAG, "4V Rail Enabled");
}


void app_main(void)
{
    //Initialize GPIO
    GPIOInit();

    //initialize I2C
    i2c_master_init();


    //Enable 4V rail for modem
    EnableModemRail();

    //init flash and logs
    // esp_err_t ret = nvs_flash_init();
    // if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
    //     ESP_ERROR_CHECK(nvs_flash_erase());
    //     ret = nvs_flash_init();
    // }
    // ESP_ERROR_CHECK(ret);
    // init_logs();

    vTaskDelay(1000 / portTICK_PERIOD_MS);

    //create mutexs
    uart_mutex = xSemaphoreCreateMutex();

    //create queues
    output_queue = xQueueCreate(10, sizeof(output_cmd_t));         //create the output queue
    rx_message_queue = xQueueCreate(10, sizeof(sms_message_t));    // create SMS queue



    //Start Tasks
    xTaskCreate(OutputTask, "OutputTask", 2048, NULL, 5, NULL);
    xTaskCreate(ADCTask, "read_ads1115_task", 2048*8, NULL, 10, NULL);
    xTaskCreate(ModemTask, "modem_task", 2048*8, NULL, 11, NULL);
    xTaskCreate(SmsHandlerTask, "SmsHandlerTask", 4096, NULL, 5, NULL);
    xTaskCreate(InputTask, "InputTask", 2048, NULL, 5, NULL);
    xTaskCreate(tmp102_task, "tmp102_task", 2048, NULL, 5, NULL);



    vTaskDelay(10000 / portTICK_PERIOD_MS);

    //send_sms("07852709248", "Hi from ESP32 using ESP-IDF!");


}
