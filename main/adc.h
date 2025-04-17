#ifndef ADS1115_H
#define ADS1115_H



// I2C address options for ADS1115
#define ADC1 0x48 // ADDR pin connected to GND


// ADS1115 Register Addresses
#define ADS1115_CONVERSION_REG 0x00
#define ADS1115_CONFIG_REG     0x01

// ADS1115 Configuration Register Bits
#define ADS1115_CONFIG_OS_SINGLE (0x8000) // Start single conversion
#define ADS1115_CONFIG_MUX_DIFF_0_1 (0x0000) // Differential P = AIN0, N = AIN1
#define ADS1115_CONFIG_MUX_SINGLE_0 (0x4000) // Single-ended AIN0
#define ADS1115_CONFIG_MUX_SINGLE_1 (0x5000) // Single-ended AIN1
#define ADS1115_CONFIG_MUX_SINGLE_2 (0x6000) // Single-ended AIN2
#define ADS1115_CONFIG_MUX_SINGLE_3 (0x7000) // Single-ended AIN3
#define ADS1115_CONFIG_PGA_4_096V  (0x0000) // +/-4.096V range
#define ADS1115_CONFIG_MODE_SINGLE (0x0100) // Single-shot mode
#define ADS1115_CONFIG_DR_128SPS   (0x0080) // 128 samples per second

// Function prototypes
esp_err_t i2c_master_init(void);
void ads1115_init(uint8_t address);
int16_t ads1115_read_single_ended(uint8_t address, uint8_t channel);
int16_t read_ads1115_channel(uint8_t address, uint8_t channel);
uint16_t convert_counts_to_volts(uint16_t counts);
uint16_t read_analog_inputs(void);
uint16_t get_battery_voltage(void);
void ADCTask(void *pvParameter);

#endif // ADS1115_H
