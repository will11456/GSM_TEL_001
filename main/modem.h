#ifndef MODEM_H
#define MODEM_H

#include <stdbool.h>
#include <stddef.h>

/**
 * @brief Initialize UART and modem command queue
 */
void modem_init(void);

/**
 * @brief Main modem task that owns UART and processes SMS/commands
 */
void ModemTask(void *param);

/**
 * @brief Send an AT command via the modem task and wait for response
 *
 * @param cmd Null-terminated AT command string (without \r\n)
 * @param response_buf Buffer to store the modem's response
 * @param buf_len Length of response_buf
 * @return true if "OK" received in response, false if timeout or "ERROR"
 */
bool modem_send_at_cmd(const char *cmd, char *response_buf, size_t buf_len);

bool signal_quality(int *rssi_out, char *rating_buf, size_t buf_len);

/**
 * @brief Send an SMS message via the modem task
 *
 * @param number Null-terminated recipient phone number
 * @param message Message body
 * @return true if SMS sent successfully
 */
bool modem_send_sms(const char *number, const char *message);

#endif // MODEM_H
