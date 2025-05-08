#include "sms_sender.h"
#include "modem.h"    


// Depth of your SMS queue
#define SMS_QUEUE_DEPTH 8

static QueueHandle_t sms_queue = NULL;

void sms_sender_task(void *pv) {
    sms_request_t req;
    while (xQueueReceive(sms_queue, &req, portMAX_DELAY)) {
        ESP_LOGW("SMS", "Sending SMS to %s: %s", req.number, req.text);
        send_sms(req.number, req.text);
        // small delay between SMS to give the module breathing room
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

void start_sms_sender(void) {
    sms_queue = xQueueCreate(SMS_QUEUE_DEPTH, sizeof(sms_request_t));
    configASSERT(sms_queue);
    xTaskCreate(sms_sender_task, "SMS_SENDER", 4096, NULL, 5, NULL);
}

bool send_sms_async(const char *number, const char *text) {
    if (!sms_queue) return false;
    sms_request_t req;
    memset(&req, 0, sizeof(req));
    strncpy(req.number, number, sizeof(req.number)-1);
    strncpy(req.text,   text,   sizeof(req.text)-1);
    return xQueueSend(sms_queue, &req, 0) == pdTRUE;
}
