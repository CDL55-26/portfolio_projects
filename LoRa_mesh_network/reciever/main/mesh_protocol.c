#include <stdio.h>
#include <stdint.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/spi_master.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "mesh_protocol.h"
#include "esp_utils.h"
#include "secrets.h"
#include "packets.h"
#include "oled.h"

#define US_LORA_f 915000000
#define LORA_TX_DBM 17

#define HEADER_LEN 6

#define NODE_ID 3 //bad place, change later

static const char *TAG = "mesh_protocol";

static sx127x sx127x_device;

static volatile uint8_t rx_done_flag = 0;

//Function defs
static int wait_tx_done(uint32_t timeout_ms);
static void LoRa_packet_recieve_cb(void *ctx, uint8_t *data, uint16_t len);

void LoRa_device_init(void) {
    ESP_LOGI(TAG, "Initializing SPI bus...");
    
    spi_device_handle_t spi_device;
    sx127x_init_spi(&spi_device);

    sx127x_util_reset(); //pulls reset up and down

    ESP_ERROR_CHECK(sx127x_create(spi_device, &sx127x_device));
    ESP_LOGI(TAG, "sx127x device created successfully");


    ESP_ERROR_CHECK(sx127x_set_opmod(SX127X_MODE_STANDBY, SX127X_MODULATION_LORA, &sx127x_device));
    ESP_ERROR_CHECK(sx127x_set_frequency(US_LORA_f, &sx127x_device));
    ESP_ERROR_CHECK(sx127x_lora_reset_fifo(&sx127x_device));
    ESP_ERROR_CHECK(sx127x_rx_set_lna_boost_hf(true, &sx127x_device));
    ESP_ERROR_CHECK(sx127x_rx_set_lna_gain(SX127X_LNA_GAIN_G4, &sx127x_device));
    ESP_ERROR_CHECK(sx127x_lora_set_bandwidth(SX127X_BW_125000, &sx127x_device)); //125 kHz bandwidth
    ESP_ERROR_CHECK(sx127x_lora_set_spreading_factor(SX127X_SF_9, &sx127x_device)); //what sample uses
    ESP_ERROR_CHECK(sx127x_lora_set_syncword(18, &sx127x_device)); //default for ind projects
    ESP_ERROR_CHECK(sx127x_set_preamble_length(8, &sx127x_device)); //8 sync symbols

    //ESP_ERROR_CHECK(sx127x_tx_set_pa_config(SX127X_PA_PIN_BOOST, supported_power_levels[current_power_level], &sx127x_device));
    sx127x_tx_header_t header = {
        .enable_crc = true,
        .coding_rate = SX127X_CR_4_5};
    ESP_ERROR_CHECK(sx127x_lora_tx_set_explicit_header(&header, &sx127x_device));
    ESP_ERROR_CHECK(sx127x_tx_set_pa_config(SX127X_PA_PIN_BOOST, LORA_TX_DBM, &sx127x_device));

    ESP_LOGI(TAG, "LoRA device init ok.");

}

static int wait_tx_done(uint32_t timeout_ms) {
  uint32_t waited = 0;
  sx127x_mode_t mode;
  sx127x_modulation_t mod;

  while (waited < timeout_ms) {
    if (sx127x_get_opmod(&sx127x_device, &mode, &mod) == SX127X_OK) {
      if ((mode & 0x07) != SX127X_MODE_TX) {  //TX = DONE
        return 0;
      }
    } else {
      return -1;
    }
    vTaskDelay(pdMS_TO_TICKS(2));
    waited += 2;
  }
  return -1; //timeout 
}

int LoRa_packet_send(const uint8_t *data, size_t len, uint32_t timeout_ms) {
    if (!data || len == 0) return -1;

    ESP_ERROR_CHECK(sx127x_lora_reset_fifo(&sx127x_device));
    ESP_ERROR_CHECK(sx127x_lora_tx_set_for_transmission(data, len, &sx127x_device));
    ESP_ERROR_CHECK(sx127x_set_opmod(SX127X_MODE_TX, SX127X_MODULATION_LORA, &sx127x_device));

    int ret = wait_tx_done(timeout_ms);
    ESP_ERROR_CHECK(sx127x_set_opmod(SX127X_MODE_STANDBY, SX127X_MODULATION_LORA, &sx127x_device));
    return ret;
}

static void LoRa_packet_recieve_cb(void *ctx, uint8_t *data, uint16_t len) {
  if (len > 0) {
        uint8_t plaintext[len-HEADER_LEN-16]; //largest the plain text could be
        packet_header_t header;

        int ret;
        ret = parse_packet(data, len, &header, plaintext);
        if (ret != 0) {
          ESP_LOGE(TAG, "Error parsing packet");
          rx_done_flag = 1;
          return;
        }
        if (header.reciever_id == NODE_ID ) {
          ESP_LOGI(TAG, "Packet parsed succesfully");
          oled_clear();
          oled_draw_string(plaintext);
        }
        else {
          ESP_LOGI(TAG, "Packet parsed succesfully, not intended target");
        }
        
        rx_done_flag = 1;
    } 
    else {
        ESP_LOGW(TAG,"Packet recieved wrong formatting");
    }
}

int LoRa_packet_recieve(uint32_t timeout_ms) {
  rx_done_flag = 0;
  uint32_t waited = 0;

  sx127x_rx_set_callback(LoRa_packet_recieve_cb, &sx127x_device, &sx127x_device);
  ESP_ERROR_CHECK(sx127x_set_opmod(SX127X_MODE_RX_CONT, SX127X_MODULATION_LORA, &sx127x_device));

  ESP_LOGI(TAG, "Listening for packets ...");

  while (!rx_done_flag && waited < timeout_ms) {
      sx127x_handle_interrupt(&sx127x_device);
      vTaskDelay(pdMS_TO_TICKS(10));
      waited += 10;
    }

  ESP_ERROR_CHECK(sx127x_set_opmod(SX127X_MODE_STANDBY, SX127X_MODULATION_LORA, &sx127x_device)); //go back to standby

  if (rx_done_flag) {
      ESP_LOGI(TAG,"RX Packet received successfully");
      return 0;
  } 
  else {
      ESP_LOGW(TAG,"RX Timeout waiting for packet");
      return -1; 
  }
}
