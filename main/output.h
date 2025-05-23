#ifndef OUTPUT_H_
#define OUTPUT_H_

#include <stdint.h>
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "driver/gpio.h"
#include "pin_map.h"        // for OUTPUT_1, OUTPUT_2
#include "esp_log.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Which physical output to drive */
typedef enum {
    OUTPUT_ID_1,    /**< Maps to OUTPUT_1 pin */
    OUTPUT_ID_2,    /**< Maps to OUTPUT_2 pin */
    // add more IDs here if you expand beyond two outputs
} output_id_t;

/** A command to change an output’s state */
typedef struct {
    output_id_t id;   /**< Which output */
    uint8_t      level; /**< 0 = off, 1 = on */
} output_cmd_t;

//output queue
extern QueueHandle_t output_queue;




/**
 * @brief  Send a request to set one of the outputs.
 *
 * Non‑blocking: simply enqueues the command for the output task.
 *
 * @param cmd  Pointer to an output_cmd_t struct (id + desired level).
 */


void output_controller_init(void);
void output_controller_send(const output_cmd_t *cmd);
void OutputTask(void *pvParameters);

#ifdef __cplusplus
}
#endif

#endif /* OUTPUT_H_ */
