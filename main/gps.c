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
#include "sms_sender.h"

#define GPS_UART_NUM UART_NUM_2
#define GPS_UART_TX_PIN GPS_TX 
#define GPS_UART_RX_PIN GPS_RX  
#define GPS_UART_BUF_SIZE (1024 * 2)

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
            gps_data.satellites = atoi(tokens[7]);
            gps_has_fix = true;
            xSemaphoreGive(gps_mutex);
        }
    } else {
        if (gps_mutex && xSemaphoreTake(gps_mutex, pdMS_TO_TICKS(10))) {
            gps_has_fix = false;
            xSemaphoreGive(gps_mutex);
        }
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
    uint8_t *data = malloc(GPS_UART_BUF_SIZE);
    char line[256] = {0};
    int line_pos = 0;

    while (1) {
        int len = uart_read_bytes(GPS_UART_NUM, data, GPS_UART_BUF_SIZE - 1, pdMS_TO_TICKS(1000));
        if (len > 0) {
            for (int i = 0; i < len; ++i) {
                if (data[i] == '\n' || data[i] == '\r') {
                    if (line_pos > 6) {
                        if (strncmp(line, "$GPGGA", 6) == 0) parse_gga(line);
                        else if (strncmp(line, "$GPRMC", 6) == 0) parse_rmc(line);
                    }
                    line_pos = 0;
                    memset(line, 0, sizeof(line));
                } else if (line_pos < sizeof(line) - 1) {
                    line[line_pos++] = data[i];
                }
            }
        }
    }

    free(data);
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
