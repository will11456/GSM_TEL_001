#include "pin_map.h"
#include "adc.h"




//I2C Interface Setup
#define I2C_MASTER_NUM I2C_NUM_0 // I2C port number for master dev
#define I2C_MASTER_TX_BUF_DISABLE 0 // I2C master do not need buffer
#define I2C_MASTER_RX_BUF_DISABLE 0 // I2C master do not need buffer
#define I2C_MASTER_FREQ_HZ 100000   // I2C master clock frequency

#define WRITE_BIT I2C_MASTER_WRITE // I2C master write
#define READ_BIT  I2C_MASTER_READ  // I2C master read
#define ACK_CHECK_EN 0x1           // I2C master will check ack from slave

//Physical Constants
#define SHUNT_420_RESISTANCE    250.0      //resistance of shunt resistor on the 4-20mA inputs
#define COUNT_TO_VOLTS_CAL      6.144      //Calibration constant for counts to volts in ADC
#define ANALOG_INPUT_MULTIPLIER 2.5        // Conversion factor for ADC input to voltage
#define RES_INPUT_MULTIPLIER    5.657

//Logging Tag
static const char* TAG = "ADC";


// Initialize I2C
esp_err_t i2c_master_init(void) {
    i2c_config_t conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = SDA_PIN, // SDA pin
        .scl_io_num = SCL_PIN, // SCL pin
        .master.clk_speed = I2C_MASTER_FREQ_HZ,
    };
    esp_err_t err = i2c_param_config(I2C_MASTER_NUM, &conf);
    if (err != ESP_OK) return err;

    return i2c_driver_install(I2C_MASTER_NUM, conf.mode,
                              I2C_MASTER_RX_BUF_DISABLE,
                              I2C_MASTER_TX_BUF_DISABLE, 0);
}


//Functions
void ads1115_init(uint8_t address) {
    i2c_master_init();
}


uint16_t convert_counts_to_volts(uint16_t counts)
{
    uint16_t voltage = 1000.0 * (counts / 32768.0) * COUNT_TO_VOLTS_CAL;
    return voltage;
}


int16_t ads1115_read_single_ended(uint8_t address, uint8_t channel) {
    uint16_t config = ADS1115_CONFIG_OS_SINGLE |
                      ADS1115_CONFIG_PGA_4_096V |
                      ADS1115_CONFIG_MODE_SINGLE |
                      ADS1115_CONFIG_DR_128SPS;

    switch (channel) {
        case 0:
            config |= ADS1115_CONFIG_MUX_SINGLE_0;
            break;
        case 1:
            config |= ADS1115_CONFIG_MUX_SINGLE_1;
            break;
        case 2:
            config |= ADS1115_CONFIG_MUX_SINGLE_2;
            break;
        case 3:
            config |= ADS1115_CONFIG_MUX_SINGLE_3;
            break;
        default:
            return 0;
    }

    uint8_t write_buf[3];
    write_buf[0] = ADS1115_CONFIG_REG;
    write_buf[1] = config >> 8;
    write_buf[2] = config & 0xFF;

    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (address << 1) | WRITE_BIT, ACK_CHECK_EN);
    i2c_master_write(cmd, write_buf, 3, ACK_CHECK_EN);
    i2c_master_stop(cmd);
    esp_err_t ret = i2c_master_cmd_begin(I2C_MASTER_NUM, cmd, 1000 / portTICK_PERIOD_MS);
    i2c_cmd_link_delete(cmd);

    if (ret != ESP_OK) {
        return 0;
    }

    // Delay for conversion
    vTaskDelay(pdMS_TO_TICKS(20));

    uint8_t read_buf[2];
    cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (address << 1) | WRITE_BIT, ACK_CHECK_EN);
    i2c_master_write_byte(cmd, ADS1115_CONVERSION_REG, ACK_CHECK_EN);
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (address << 1) | READ_BIT, ACK_CHECK_EN);
    i2c_master_read(cmd, read_buf, 2, I2C_MASTER_LAST_NACK);
    i2c_master_stop(cmd);
    ret = i2c_master_cmd_begin(I2C_MASTER_NUM, cmd, 1000 / portTICK_PERIOD_MS);
    i2c_cmd_link_delete(cmd);

    if (ret != ESP_OK) {
        return 0;
    }

    int16_t result = (read_buf[0] << 8) | read_buf[1];
    return result;
}


int16_t read_ads1115_channel(uint8_t address, uint8_t channel) {
    int16_t counts = ads1115_read_single_ended(address, channel);
    if (counts == 0) {
        ESP_LOGE(TAG, "Failed to read channel %d", channel);
    }
    return counts;
}


//MAIN READ FUNCTIONS

uint16_t read_analog_inputs(void) 
{
    
    uint16_t analog_1_counts =  ads1115_read_single_ended(ADC1,0);
    
    analog_1_counts = analog_1_counts * ANALOG_INPUT_MULTIPLIER; //acount for the potential divider

    uint16_t analog_voltage_1 = convert_counts_to_volts(analog_1_counts);

    uint16_t voltage_msg_1 = analog_voltage_1 + 4;



    //filter for obscure values
    if (voltage_msg_1 < 10){
        voltage_msg_1 = 0;
    }
    
    return voltage_msg_1;
    //ESP_LOGI(TAG, "Analog Input mV: %d", voltage_msg_1);
}



uint16_t read_4_20_inputs(void)
{
    uint16_t counts_4_20_in = read_ads1115_channel(ADC1, 3);
    
    uint16_t voltage_4_20_mv_in = convert_counts_to_volts(counts_4_20_in);
    
    float current = 100 * voltage_4_20_mv_in / SHUNT_420_RESISTANCE;
    uint16_t current_int = current;

    return current_int;
    //ESP_LOGI(TAG, "c1: %d", current_int);

}

uint16_t read_res_inputs(void)
{
    uint16_t analog_1_counts =  ads1115_read_single_ended(ADC1,1);

    uint16_t analog_voltage_1 = convert_counts_to_volts(analog_1_counts);

    uint16_t resistance = (8427/ RES_INPUT_MULTIPLIER) * analog_voltage_1 / (23100 - analog_voltage_1); 

    uint16_t res_cal = 0.0093 * pow(resistance,2) +0.6919* resistance +10.026;

    if (res_cal > 320)
    {
        res_cal = 999;
    }

    ESP_LOGW(TAG, "res v: %d", res_cal ); 

    return resistance;
    
}



//MAIN ADC TASK 

void ADCTask(void *pvParameter) 
{
    ads1115_init(ADC1);
    
    uint16_t an;
    uint16_t cur;
    uint16_t res;

    while (1) {

        an =  read_analog_inputs();
        vTaskDelay(pdMS_TO_TICKS(50));
        cur =  read_4_20_inputs();
        vTaskDelay(pdMS_TO_TICKS(50));
        res = read_res_inputs();
        vTaskDelay(pdMS_TO_TICKS(50));
        ESP_LOGI(TAG, "Analog: %d  Current: %d  Resist: %d",an, cur, res); 


    }
}