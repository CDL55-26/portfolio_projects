//standard c 
#include <stdio.h>
#include <stdint.h>
#include <string.h>
//uart
#include "driver/uart.h"
#include "esp_vfs_dev.h"
//fRTOS
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
//hal drivers
#include "driver/spi_master.h"
#include "driver/gpio.h"
//esp system headers
#include "esp_log.h"
//LoRa
#include "mesh_protocol.h"
//secrets
#include "secrets.h"
//packets
#include "packets.h"

static const char *TAG = "LoRa Project";

#define NODE_ID 1
#define TEMP_TARGET_ID 3

#define HEADER_LEN 6

void app_main(void) {
    const uart_port_t uart_num = UART_NUM_0;
    const int buf_size = 256;

    uart_driver_install(uart_num, buf_size, 0, 0, NULL, 0);
    esp_vfs_dev_uart_use_driver(uart_num);
    
    int ret;
    LoRa_device_init();

    uint8_t plaintext[128] = {0};
    fgets((char*)plaintext, sizeof(plaintext), stdin);

    uint8_t plaintext_len = (uint8_t)strlen((char *)plaintext);
    
    uint8_t packet[plaintext_len + 32 + HEADER_LEN]; //worst case packet size 
    uint8_t packet_len = 0;//init to zero

    build_packet_data_t packet_data = {
        .message = plaintext,
        .message_len = plaintext_len,
        .sender_id = NODE_ID,
        .reciever_id = TEMP_TARGET_ID
    };

    ret = build_packet(packet_data, packet, &packet_len);
    if (ret != 0) {
        ESP_LOGE(TAG, "build packet failed");
    }


    uint32_t timeout_ms = 1000;

    while (1) {
        ret = LoRa_packet_send(packet, packet_len, timeout_ms);

        if (ret != 0) {
            ESP_LOGI(TAG,"packet send failed");
            return;
        }

        ESP_LOGI(TAG,"packet send success");
        vTaskDelay(pdMS_TO_TICKS(2000)); // delay 1 second btwn
    }
    
}
