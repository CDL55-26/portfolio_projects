#ifndef WIFI_MODULE_H
#define WIFI_MODULE_H

#include <stdint.h>
#include "esp_err.h"
#include "sweep_configs.h"

//function pointer for wifi event control callback
typedef void (*wifi_remote_command_cb_t)(const char *cmd);
void wifi_register_cmd_callback(wifi_remote_command_cb_t cb);

void wifi_post_resonant_frequencies(const resonant_frequency_tuple *buffer, uint16_t count);
esp_err_t wifi_init(void);

esp_err_t wifi_ws_start(void);
void wifi_ws_send_datapoint(uint32_t freq, int16_t sample);
void wifi_ws_send_resonance(uint32_t freq, int16_t sample);

#endif
