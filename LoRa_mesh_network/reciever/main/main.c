//standard c 
#include <stdio.h>
#include <stdint.h>
//fRTOS
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
//hal drivers
#include "driver/spi_master.h"
#include "driver/gpio.h"
//esp system headers
#include "esp_log.h"

//project libs
#include "mesh_protocol.h"
#include "oled.h"

static const char *TAG = "LoRa Project";

void app_main(void) {

    LoRa_device_init();
    ssd1306_init();

    uint8_t my_string[] = "RX...";
    oled_draw_string(my_string);

    uint32_t timeout_ms = 100000;
    LoRa_packet_recieve(timeout_ms);

    ESP_LOGI(TAG,"Packet recieve returned");
    
}
