#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_random.h"
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

#define CAD_TIMEOUT_MS 100
#define SX127X_REG_IRQ_FLAGS        0x12
#define SX127X_IRQ_CAD_DETECTED_MASK 0x01
#define SX127X_IRQ_CAD_DONE_MASK     0x04

#define MAX_BACKOFF_MS 200
#define MAX_RETRY_COUNT 10

static const char *TAG = "mesh_protocol";

extern SemaphoreHandle_t LoRa_mutex; //declared in main.c

static sx127x sx127x_device;

static volatile uint8_t rx_done_flag = 0;

static volatile lora_packet_t recieved_packet;

//static uint8_t retry_list_max_len; 

//Function defs
static int wait_tx_done(uint32_t timeout_ms);
static bool channel_busy_wait(void);
static void LoRa_packet_recieve_cb(void *ctx, uint8_t *data, uint16_t len);

void LoRa_device_init(void) {
    ESP_LOGI(TAG, "Initializing SPI bus...");

    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << DIO0),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE, // SX127x drives this pin active high
        .pull_down_en = GPIO_PULLDOWN_ENABLE, // Keep it low when radio is off
        .intr_type = GPIO_INTR_DISABLE
    };
    ESP_ERROR_CHECK(gpio_config(&io_conf));

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
    
    sx127x_rx_set_callback(LoRa_packet_recieve_cb, &sx127x_device, &sx127x_device);
    ESP_ERROR_CHECK(sx127x_set_opmod(SX127X_MODE_RX_CONT, SX127X_MODULATION_LORA, &sx127x_device));

    ESP_LOGI(TAG, "LoRA device init ok.");

}

static bool channel_busy_wait(void) {
    ESP_ERROR_CHECK(sx127x_set_opmod(SX127X_MODE_CAD, SX127X_MODULATION_LORA, &sx127x_device));
    
    uint32_t waited = 0;
    sx127x_mode_t mode;
    sx127x_modulation_t mod;

    while (waited < CAD_TIMEOUT_MS) {
        if (sx127x_get_opmod(&sx127x_device, &mode, &mod) == SX127X_OK) {
            if ((mode & 0x07) != SX127X_MODE_CAD) {
                break;
            }
        }
        vTaskDelay(pdMS_TO_TICKS(1));
        waited += 1;
    }
    
    uint8_t irq_flags;
    ESP_ERROR_CHECK(sx127x_read_register(SX127X_REG_IRQ_FLAGS, (shadow_spi_device_t *)&sx127x_device.spi_device, &irq_flags));
    
    uint8_t clear_mask = SX127X_IRQ_CAD_DONE_MASK | SX127X_IRQ_CAD_DETECTED_MASK;
    ESP_ERROR_CHECK(sx127x_write_register(SX127X_REG_IRQ_FLAGS, clear_mask, (shadow_spi_device_t *)&sx127x_device.spi_device));
    
    sx127x_set_opmod(SX127X_MODE_STANDBY, SX127X_MODULATION_LORA, &sx127x_device);

    return (irq_flags & SX127X_IRQ_CAD_DETECTED_MASK);
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
    int retry_count = 0;
    uint32_t backoff_ms;
    LoRa_mutex_lock();
    while (retry_count < MAX_RETRY_COUNT) {
      if (!channel_busy_wait()) {
        break;
      }
      else {
        backoff_ms = esp_random() % MAX_BACKOFF_MS;
        backoff_ms = backoff_ms < 10 ? 10 : backoff_ms;
        retry_count++;

        ESP_LOGI(TAG, "Channel busy, retrying.");
        vTaskDelay(pdMS_TO_TICKS(backoff_ms));
      }
    }
    if (retry_count >= MAX_RETRY_COUNT) {
      ESP_LOGE(TAG, "Maximum packet send retrys");
      LoRa_mutex_unlock();
      return -1;
    }

    ESP_ERROR_CHECK(sx127x_lora_reset_fifo(&sx127x_device));
    ESP_ERROR_CHECK(sx127x_lora_tx_set_for_transmission(data, len, &sx127x_device));
    ESP_ERROR_CHECK(sx127x_set_opmod(SX127X_MODE_TX, SX127X_MODULATION_LORA, &sx127x_device));

    int ret = wait_tx_done(timeout_ms);
    ESP_ERROR_CHECK(sx127x_set_opmod(SX127X_MODE_RX_CONT, SX127X_MODULATION_LORA, &sx127x_device)); //put back into recieving mode
    LoRa_mutex_unlock();
    ESP_LOGI(TAG, "Packet transmitted");
    return ret;
}

static void LoRa_packet_recieve_cb(void *ctx, uint8_t *data, uint16_t len) {
  if (len > 0) {
      memcpy((void* )recieved_packet.data, data, len);
      recieved_packet.len = len;
  } 
  else {
      ESP_LOGW(TAG,"Packet recieved wrong formatting");
  }
    
  rx_done_flag = 1;
}


int LoRa_packet_check_and_receive(lora_packet_t* new_recieved_packet) {
    int result = -1;

    if (gpio_get_level(DIO0) == 0) {
        return -1;
    }

    LoRa_mutex_lock();

    rx_done_flag = 0;
    sx127x_handle_interrupt(&sx127x_device);

    if (rx_done_flag) {
        *new_recieved_packet = recieved_packet;
        result = 0;
        ESP_LOGI(TAG, "RX Packet received");
    }

    LoRa_mutex_unlock();

    return result;
}


//mutex helper function for controlling lora/spi bus
void LoRa_mutex_lock(void) {
  if (xSemaphoreTake(LoRa_mutex, portMAX_DELAY) == pdTRUE) {
    return;
  }
}

void LoRa_mutex_unlock(void) {
  xSemaphoreGive(LoRa_mutex);
}
/*
int retry_event_list_init(retry_event_t* list, uint8_t max_length) {
  retry_list_max_len = max_length;

  list = malloc(sizeof(retry_event_t));
  if (list == 0) {
    ESP_LOGI(TAG, "retry list malloc failed");
    return -1;
  }
  list->next = NULL; //init to null 
  return 0;
}

void retry_event_list_add_event(retry_event_t* list, retry_event_t* new_event) {
  retry_event_t* front = list;
  uint8_t itteration_count = 0;

  while (front->next != NULL && itteration_count < retry_list_max_len) { 
    front = front->next;
    itteration_count++;
  }
  if (itteration_count >= retry_list_max_len) {
    retry_event_t* event_to_remove = list->next;
    list->next = event_to_remove->next;
    free(event_to_remove);
  } 
  front->next = new_event;
}
*/
