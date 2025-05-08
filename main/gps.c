#include "main.h"
#include "gps.h"
#include "pin_map.h"
#include "adc.h"
#include "modem.h"
#include "handler.h"
//#include "inputs.h"
#include "tmp102.h"
#include "output.h"
#include "config_store.h"
#include "nvs_flash.h"
#include "config_store.h"

#define GPS_UART_NUM UART_NUM_2
#define GPS_UART_TX_PIN GPS_TX 
#define GPS_UART_RX_PIN GPS_RX  
#define GPS_UART_BUF_SIZE (1024 * 4)
#define LINE_BUF_SIZE 256

static const char *TAG = "GPS";

static gps_data_t gps_data = {0};
static bool gps_has_fix = false;
SemaphoreHandle_t gps_mutex = NULL;

static double nmea_to_decimal(const char *nmea, const char *direction) {
    double degrees = 0.0, minutes = 0.0;
    char deg_str[4] = {0};
    int deg_len = (direction[0] == 'N' || direction[0] == 'S') ? 2 : 3;

    strncpy(deg_str, nmea, deg_len);
    degrees = atof(deg_str);
    minutes = atof(nmea + deg_len);

    double decimal = degrees + (minutes / 60.0);
    if (direction[0] == 'S' || direction[0] == 'W') decimal *= -1.0;
    return decimal;
}

static void parse_gga(const char *sentence) {
    char *tokens[15] = {0};
    char *buf = strdup(sentence);
    char *token = strtok(buf, ",");
    int i = 0;

    while (token && i < 15) {
        tokens[i++] = token;
        token = strtok(NULL, ",");
    }

    if (i >= 10 && tokens[6] && atoi(tokens[6]) > 0) {
        if (gps_mutex && xSemaphoreTake(gps_mutex, pdMS_TO_TICKS(10))) {
            gps_data.latitude = nmea_to_decimal(tokens[2], tokens[3]);
            gps_data.longitude = nmea_to_decimal(tokens[4], tokens[5]);
            gps_data.altitude = atof(tokens[9]);
            gps_data.satellites_used = atoi(tokens[7]);
            gps_data.hdop = atoi(tokens[8]);
            gps_data.time = atof(tokens[1]);
            gps_has_fix = true;
            xSemaphoreGive(gps_mutex);
        }
    } else {
        if (gps_mutex && xSemaphoreTake(gps_mutex, pdMS_TO_TICKS(10))) {
            gps_has_fix = false;
            xSemaphoreGive(gps_mutex);
        }
    }
    //GPS fix notification
    static bool has_logged_fix = false;

    if (gps_has_lock() == 1 && !has_logged_fix) {
        ESP_LOGI("GPS", "📡 GPS fix acquired!");
        has_logged_fix = true;
    }

    free(buf);
}

static void parse_rmc(const char *sentence) {
    char *tokens[12] = {0};
    char *buf = strdup(sentence);
    char *token = strtok(buf, ",");
    int i = 0;

    while (token && i < 12) {
        tokens[i++] = token;
        token = strtok(NULL, ",");
    }

    if (gps_mutex && xSemaphoreTake(gps_mutex, pdMS_TO_TICKS(10))) {
        gps_has_fix = (i >= 3 && tokens[2] && tokens[2][0] == 'A');
        xSemaphoreGive(gps_mutex);
    }

    free(buf);
}



void gps_task(void *arg) {
    uint8_t *uart_buf = malloc(GPS_UART_BUF_SIZE + 1);  // +1 for null-termination
    char line_buf[LINE_BUF_SIZE] = {0};
    int line_pos = 0;

    if (!uart_buf) {
        ESP_LOGE(TAG, "Failed to allocate UART buffer");
        vTaskDelete(NULL);
        return;
    }

    while (1) {
        int len = uart_read_bytes(GPS_UART_NUM, uart_buf, GPS_UART_BUF_SIZE, pdMS_TO_TICKS(1000));

        if (len > 0) {
            for (int i = 0; i < len; ++i) {
                char c = (char)uart_buf[i];

                if (c == '\n') {
                    line_buf[line_pos] = '\0';  // Null-terminate
                    if (line_pos > 6 && strncmp(line_buf, "$GP", 3) == 0) {
                        //ESP_LOGI(TAG, "NMEA: %s", line_buf);

                        if (strncmp(line_buf, "$GPGGA", 6) == 0) {
                            parse_gga(line_buf);
                            //ESP_LOGW(TAG, "GPS GGA: %s", line_buf);
                        } else if (strncmp(line_buf, "$GPRMC", 6) == 0) {
                            parse_rmc(line_buf);
                            //ESP_LOGW(TAG, "GPS RMC: %s", line_buf);
                        }
                    }
                    line_pos = 0;
                    memset(line_buf, 0, sizeof(line_buf));
                }
                else if (isprint((unsigned char)c) && line_pos < LINE_BUF_SIZE - 1) {
                    line_buf[line_pos++] = c;
                }
                // else: ignore non-printable characters, overflows, or carriage returns
            }
        }
    }

    free(uart_buf);
    vTaskDelete(NULL);
}


void gps_init(void) {
    
    const uart_config_t uart_config = {
        .baud_rate = 9600,
        .data_bits = UART_DATA_8_BITS,
        .parity    = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE
    };
    uart_param_config(GPS_UART_NUM, &uart_config);
    uart_set_pin(GPS_UART_NUM, GPS_UART_TX_PIN, GPS_UART_RX_PIN, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
    uart_driver_install(GPS_UART_NUM, GPS_UART_BUF_SIZE * 2, 0, 0, NULL, 0);

    ESP_LOGW(TAG, "GPS UART initialized");

}

gps_data_t gps_get_data(void) {
    gps_data_t copy = {0};
    if (gps_mutex && xSemaphoreTake(gps_mutex, pdMS_TO_TICKS(100))) {
        copy = gps_data;
        xSemaphoreGive(gps_mutex);
    }
    return copy;
}

bool gps_has_lock(void) {
    bool fix = false;
    if (gps_mutex && xSemaphoreTake(gps_mutex, pdMS_TO_TICKS(100))) {
        fix = gps_has_fix;
        xSemaphoreGive(gps_mutex);
    }
    return fix;
}

void report_gps_status(void) {
    gps_data_t data = gps_get_data();

    if (gps_has_lock()) {
        ESP_LOGI("GPS", "Fix acquired. Lat: %.6f, Lon: %.6f, Alt: %.2f, Sats: %d, HDOP: %.2f",
                data.latitude, data.longitude, data.altitude, data.satellites_used, data.hdop);
                 
    } 
            
    else {
        ESP_LOGI(TAG, "No GPS fix. Waiting for signal...");
        
    }
}

void gps_format_time_hhmm(float nmea_time, char *out, size_t max_len) {
    int hhmmss = (int)nmea_time;

    int hours = hhmmss / 10000;
    int minutes = (hhmmss / 100) % 100;

    snprintf(out, max_len, "%02d:%02d", hours, minutes);
}