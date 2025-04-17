#include <stdint.h>
#include "esp_err.h"


#include "main.h"
#include "pin_map.h"
#include "adc.h"
#include "modem.h"
#include "handler.h"
#include "inputs.h"
#include "tmp102.h"
#include "config_store.h"



#define I2C_MASTER_NUM              I2C_NUM_0
#define I2C_MASTER_FREQ_HZ          100000
#define I2C_MASTER_TX_BUF_LEN       0
#define I2C_MASTER_RX_BUF_LEN       0

#define TMP102_ADDR                 0x4B
#define TMP102_REG_TEMPERATURE      0x00

static const char *TAG = "TMP102";


// read raw temperature, convert to °C
esp_err_t tmp102_read_celsius(float *out_celsius)
{
    uint8_t data[2];
    // write register pointer, then read 2 bytes
    esp_err_t err = i2c_master_write_read_device(
        I2C_MASTER_NUM,
        TMP102_ADDR,
        (const uint8_t[]){ TMP102_REG_TEMPERATURE }, 1,
        data, 2,
        pdMS_TO_TICKS(1000)
    );
    if (err != ESP_OK) {
        return err;
    }

    // TMP102 returns 12-bit two's‑complement in the high 12 bits of the 16‑bit word
    int16_t raw = ((int16_t)data[0] << 8) | data[1];
    raw >>= 4;                // discard the lowest 4 bits
    if (raw & 0x800) {        // sign extend negative values
        raw |= 0xF000;
    }
    *out_celsius = raw * 0.0625f;  // each LSB = 0.0625°C
    return ESP_OK;
}

// the FreeRTOS task that loops and logs the temperature
void tmp102_task(void *pvParameters)
{

    float temp_c;
    for (;;) {
        if (tmp102_read_celsius(&temp_c) == ESP_OK) {
            //ESP_LOGI(TAG, "Temperature: %.2f°C", temp_c);
        } else {
            ESP_LOGE(TAG, "Error reading temperature");
        }
        vTaskDelay(pdMS_TO_TICKS(30000));
    }
}

