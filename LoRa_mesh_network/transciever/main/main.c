//standard c 
#include <stdio.h>
#include <stdint.h>
#include <string.h>
//fRTOS
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
//hal drivers
#include "driver/spi_master.h"
#include "driver/gpio.h"
//esp system headers
#include "esp_log.h"
#include "esp_random.h"
#include "esp_timer.h"
//project libs
#include "mesh_protocol.h"
#include "oled.h"
#include "secrets.h"
#include "packets.h"
#include "smf.h"
#include "keypad.h"

static const char *TAG = "LoRa Project";

#define MAX_DELAY_MS 0xFFFFFFFFUL
#define MAX_TRANSMIT_WAIT_MS 20000 //20 sec

//packet info
#define NODE_ID 1  
#define RECIEVED_PACKET_CACHE_SIZE 8

uint32_t node_sequence_num = 0;

int recieved_packet_cache_index = 0;
packet_id_t recieved_packet_cache[RECIEVED_PACKET_CACHE_SIZE];

lora_packet_t recieved_packet_copy;

//Packet retry buffer
#define RETRY_EVENT_BUFFER_SIZE 8
#define MAX_PACKET_RETRY_COUNT 3
retry_event_t retry_event_buffer[RETRY_EVENT_BUFFER_SIZE];

//LoRa
SemaphoreHandle_t LoRa_mutex = NULL;

//oled stuff
SemaphoreHandle_t oled_mutex = NULL;

uint8_t initial_message[] = "Device On..";

//events
EventGroupHandle_t transciever_events;
#define RELAY_PACKET_EVENT BIT(1)

//Timers
#define PACKET_RETRY_DELAY_US 4000000 //4s

esp_timer_handle_t retry_packet_timer;
void retry_packet_timer_cb(void *arg);

//queues
#define PACKET_QUEUE_SIZE 16 //can change later. this seems reasonable
QueueHandle_t recieved_packet_queue;
QueueHandle_t transmitting_packet_queue;
QueueHandle_t retry_packet_alert_queue;
QueueSetHandle_t transciever_queue_set;

//threads
void recieve_packet_task(void *pvParameters);
void keypad_input_task(void *pvParameters);

//smf setup
static void init_run(void);

static void idle_entry(void);
static void idle_run(void);

static void retry_packet_entry(void);
static void retry_packet_run(void);

static void send_ack_entry(void);
static void send_ack_run(void);

static void send_packet_entry(void);
static void send_packet_run(void);

static void parse_packet_entry(void);
static void parse_packet_run(void);

smf_context_t smf_context;
typedef enum {  
  INIT,
  IDLE,
  RETRY_PACKET,
  SEND_ACK,
  SEND_PACKET,
  PARSE_PACKET,
}app_states_t;

const smf_state_t state_table[] = {
  [INIT]           = SMF_CREATE_STATE(NULL, init_run, NULL),
  [IDLE]           = SMF_CREATE_STATE(idle_entry, idle_run, NULL),
  [RETRY_PACKET]   = SMF_CREATE_STATE(retry_packet_entry, retry_packet_run, NULL),
  [SEND_ACK]       = SMF_CREATE_STATE(send_ack_entry, send_ack_run, NULL),
  [SEND_PACKET]    = SMF_CREATE_STATE(send_packet_entry, send_packet_run, NULL),
  [PARSE_PACKET]   = SMF_CREATE_STATE(parse_packet_entry, parse_packet_run, NULL),
};

void app_main(void) {
  smf_init(&smf_context, state_table, INIT);
  ESP_LOGI(TAG, "smf init complete");

  while (1) {
    smf_run(&smf_context);
    vTaskDelay(pdMS_TO_TICKS(10)); //yield to scheduler
  }
}

//task defs

void recieve_packet_task(void *pvParameters) {
  lora_packet_t recieved_packet; 
  int ret;
  while (1) {
    ret = LoRa_packet_check_and_receive(&recieved_packet);
    if (ret == 0) {
      xQueueSend(recieved_packet_queue, &recieved_packet, 0); //no wait
      //xEventGroupSetBits(transciever_events, RELAY_PACKET_EVENT);
    }
    vTaskDelay(pdMS_TO_TICKS(10)); //delay to let scheuler do other stuff + send packets
  }
}

void keypad_input_task(void *pvParameters) {
  char plaintext[128];
  uint8_t plaintext_len;
  uint8_t packet_len = 0; //modified by build packet
  int ret;
  uint8_t target_id;

  lora_packet_t outgoing_packet;
  build_packet_data_t packet_data;

  int retry_event_buffer_index = 0;
      
  while (1) {
    if (keypad_get_input(plaintext, &target_id, sizeof(plaintext)) == ESP_OK) {
      plaintext_len = strlen(plaintext); //get length of string -- null character issues?
      uint8_t* packet = malloc((plaintext_len + 32 + HEADER_LEN) * sizeof(uint8_t));
      if (packet == NULL) {
        ESP_LOGE(TAG, "Malloc failed in keypad task");
        return;
      }
      
      packet_data.message = (uint8_t*)plaintext;
      packet_data.message_len = plaintext_len;
      packet_data.sender_id = NODE_ID;
      packet_data.reciever_id = target_id; //this will need to be variable
      if (target_id == NODE_ID) {
        free(packet);
        oled_mutex_lock(); 
        oled_clear();
        oled_draw_string((uint8_t*)"Invalid Target");
        oled_mutex_unlock();

        continue;
        //don't make or send packet if bad ID.
      }
      packet_data.seq_num = node_sequence_num;
      packet_data.is_ack = 0;


      ret = build_packet(packet_data, packet, &packet_len);
      if (ret == 0) {
        memcpy(outgoing_packet.data, packet, packet_len);
        outgoing_packet.len = packet_len;
        xQueueSend(transmitting_packet_queue, &outgoing_packet, 0);

        retry_event_buffer_index = node_sequence_num % RETRY_EVENT_BUFFER_SIZE; 
        memcpy(retry_event_buffer[retry_event_buffer_index].retry_packet.data, packet, packet_len);
        retry_event_buffer[retry_event_buffer_index].retry_packet.len = packet_len; 
        retry_event_buffer[retry_event_buffer_index].retry_count = MAX_PACKET_RETRY_COUNT;
        retry_event_buffer[retry_event_buffer_index].packet_acked = false;
        retry_event_buffer[retry_event_buffer_index].seq_num = node_sequence_num;
                                      
        node_sequence_num++;
      }
      else {
        ESP_LOGE(TAG, "build packet failed");
      }
      free(packet);
    }
  }
}

//callbacks
void retry_packet_timer_cb(void *arg) {
  retry_alert_t retry_alert = {
    .retry = true,
  };
  xQueueSend(retry_packet_alert_queue, &retry_alert, 0);
}


//state defs
static void init_run(void) {
  ESP_LOGI(TAG, "Device ID: %d", NODE_ID);
  LoRa_device_init();
  LoRa_mutex = xSemaphoreCreateMutex(); //need to lock lora bus
  ssd1306_init();
  
  oled_mutex = xSemaphoreCreateMutex();
  keypad_config_t key_cfg = {
    .row_pins = {ROW1,ROW2,ROW3,ROW4},
    .col_pins = {COL1,COL2,COL3,COL4},
    .oled_mutex = oled_mutex,
    .display_callback = update_oled,
  };
  keypad_init(&key_cfg);
  //maybe config events
 
  transciever_events = xEventGroupCreate();

  recieved_packet_queue = xQueueCreate(PACKET_QUEUE_SIZE, sizeof(lora_packet_t));
  transmitting_packet_queue = xQueueCreate(PACKET_QUEUE_SIZE, sizeof(lora_packet_t));
  retry_packet_alert_queue = xQueueCreate(PACKET_QUEUE_SIZE, sizeof(retry_alert_t));
  //check sizing of queue set **
  
  transciever_queue_set = xQueueCreateSet(3*PACKET_QUEUE_SIZE);
  xQueueAddToSet(recieved_packet_queue, transciever_queue_set);
  xQueueAddToSet(transmitting_packet_queue, transciever_queue_set);
  xQueueAddToSet(retry_packet_alert_queue, transciever_queue_set);

  for (int i = 0; i < RETRY_EVENT_BUFFER_SIZE; i++) {
    retry_event_buffer[i].packet_acked = true; //initialize all to true -> invalid at start
  }

  esp_timer_create_args_t retry_timer_cfg = {
    .callback = retry_packet_timer_cb,
    .name = "Retry packet timer",
  };
  esp_timer_create(&retry_timer_cfg, &retry_packet_timer);
  
  xTaskCreate(recieve_packet_task, "recieve_packet_task", 4096, NULL, 4, NULL);
  xTaskCreate(keypad_input_task, "keypad_input_task", 4096, NULL, 4, NULL);

  esp_timer_start_once(retry_packet_timer, PACKET_RETRY_DELAY_US); 
  /*
   start timer once here. In the future, start timer in retry state 
   only if we are waiting on an ack
   prob not great practice, but whatever 
   */
  
  oled_clear();
  oled_draw_string(initial_message);
  smf_set_state(&smf_context, IDLE);
}

static void idle_entry(void) {
  ESP_LOGI(TAG, "Entered Idle");
}
static void idle_run(void) {
  QueueSetMemberHandle_t activated = xQueueSelectFromSet(transciever_queue_set, portMAX_DELAY);

  if (activated == transmitting_packet_queue) {
    smf_set_state(&smf_context, SEND_PACKET);
  }
  else if (activated == recieved_packet_queue) {
    smf_set_state(&smf_context, PARSE_PACKET);
  }

  else if (activated == retry_packet_alert_queue) {
    smf_set_state(&smf_context, RETRY_PACKET);
  }

}

static void retry_packet_entry(void) {
  ESP_LOGI(TAG, "Entered retry packet");
}
static void retry_packet_run(void) {
  retry_alert_t alert;
  bool waiting_for_ack = false;
  
  bool slot_processed[RETRY_EVENT_BUFFER_SIZE] = {0}; 

  xQueueReceive(retry_packet_alert_queue, &alert, portMAX_DELAY);

  for (int event = 0; event < RETRY_EVENT_BUFFER_SIZE; event++) {
    if (slot_processed[event]) {
      continue;
    }

    if (retry_event_buffer[event].packet_acked == false && retry_event_buffer[event].retry_count > 0) {
      uint32_t old_seq = retry_event_buffer[event].seq_num;
      uint32_t new_seq = node_sequence_num;
      int new_index = new_seq % RETRY_EVENT_BUFFER_SIZE;

      if (new_index != event) {
        memcpy(&retry_event_buffer[new_index], &retry_event_buffer[event], sizeof(retry_event_t));
        retry_event_buffer[event].packet_acked = true;
        retry_event_buffer[event].retry_count = 0;
      }

      memcpy((retry_event_buffer[new_index].retry_packet.data + 2), &new_seq, sizeof(uint32_t));
      
      retry_event_buffer[new_index].seq_num = new_seq;
      retry_event_buffer[new_index].retry_count--;
      
      slot_processed[new_index] = true;

      LoRa_packet_send(
        retry_event_buffer[new_index].retry_packet.data, 
        retry_event_buffer[new_index].retry_packet.len, 
        MAX_TRANSMIT_WAIT_MS
      );
      
      ESP_LOGI(TAG, "Packet retried. Old Seq: %d -> New Seq: %d (Slot %d->%d)", 
               old_seq, new_seq, event, new_index);
      
      node_sequence_num++;
      waiting_for_ack = true;
    }
    else if (retry_event_buffer[event].packet_acked == false && retry_event_buffer[event].retry_count == 0) {
      ESP_LOGW(TAG, "Packet gave up. Seq ID: %d", retry_event_buffer[event].seq_num);
      retry_event_buffer[event].packet_acked = true;
    }
  }
  
  if (waiting_for_ack == true) {
    esp_timer_stop(retry_packet_timer);
    esp_timer_start_once(retry_packet_timer, PACKET_RETRY_DELAY_US);
  }

  smf_set_state(&smf_context, IDLE);
}


static void send_packet_entry(void) {
  ESP_LOGI(TAG, "Entering send packet");
}
static void send_packet_run(void) {
  if (xEventGroupWaitBits(transciever_events, RELAY_PACKET_EVENT, pdTRUE, pdFALSE, 0)) {
    uint32_t random_delay = (esp_random() % 400) + 100; 
    vTaskDelay(pdMS_TO_TICKS(random_delay));
    LoRa_packet_send(recieved_packet_copy.data, recieved_packet_copy.len, MAX_TRANSMIT_WAIT_MS);
  } //check if its a relay. if not, move on

  else {
    lora_packet_t transmitting_packet;
    xQueueReceive(transmitting_packet_queue, &transmitting_packet, portMAX_DELAY);
    LoRa_packet_send(transmitting_packet.data, transmitting_packet.len, MAX_TRANSMIT_WAIT_MS);
    //only start timer if outoging packet
    esp_timer_stop(retry_packet_timer);
    esp_timer_start_once(retry_packet_timer, PACKET_RETRY_DELAY_US); //restart timer each new packet sent

  }
  smf_set_state(&smf_context, IDLE);
}

static void send_ack_entry(void) {
  ESP_LOGI(TAG, "Entered send ack");
}

static void send_ack_run(void) {
  uint8_t* data = recieved_packet_copy.data;
  uint16_t len  = recieved_packet_copy.len;

  uint8_t plaintext[len-HEADER_LEN-16]; //largest the plain text could be
  packet_header_t header;

  int ret;
  ret = parse_packet(data, len, &header, plaintext);
  if (ret != 0) {
    ESP_LOGE(TAG, "Error parsing packet in ack");
  }

  uint8_t ack_message[] = "Recieved";
  uint8_t ack_message_len = strlen((char*)ack_message); //get length of string -- null character issues?
  
  uint8_t packet_len;
  uint8_t* packet = malloc((ack_message_len + 32 + HEADER_LEN) * sizeof(uint8_t));
  if (packet == NULL) {
    ESP_LOGE(TAG, "Malloc failed in send_ack_run task");
    return;
  }
  build_packet_data_t packet_data = {
    .message = (uint8_t*)ack_message,
    .message_len = ack_message_len,
    .sender_id = NODE_ID,
    .reciever_id = header.sender_id, //grab sender id of packet we recieved
    .seq_num = header.seq_num,
    .is_ack = 1, 
  };  
  lora_packet_t ack_packet;
  ret = build_packet(packet_data, packet, &packet_len);
  if (ret == 0) {
    memcpy(ack_packet.data, packet, packet_len);
    ack_packet.len = packet_len;
    ESP_LOGI(TAG, "Sent ack packet");
    LoRa_packet_send(ack_packet.data, ack_packet.len, MAX_TRANSMIT_WAIT_MS);
    
    node_sequence_num++;
  }
  else {
    ESP_LOGE(TAG, "build packet failed");
  }
  free(packet);
  smf_set_state(&smf_context, IDLE);
}

static void parse_packet_entry(void) {
  ESP_LOGI(TAG, "Entered parse packet");
}
static void parse_packet_run(void) {
  lora_packet_t recieved_packet;
  uint8_t* data;
  uint16_t len;

  xQueueReceive(recieved_packet_queue, &recieved_packet, portMAX_DELAY); 
  recieved_packet_copy = recieved_packet; //make copy incase we have to relay or ack
                                          
  data = recieved_packet.data;
  len  = recieved_packet.len;

  uint8_t plaintext[len-HEADER_LEN-16]; //largest the plain text could be
  packet_header_t header;

  int ret;
  ret = parse_packet(data, len, &header, plaintext);
  if (ret != 0) {
    ESP_LOGE(TAG, "Error parsing packet");
  }

  packet_id_t new_packet_id = {
    .node_id = header.sender_id,
    .seq_num = header.seq_num,
  }; 
  for (int i = 0; i < RECIEVED_PACKET_CACHE_SIZE; i++) {
    if (recieved_packet_cache[i].node_id == new_packet_id.node_id 
                && recieved_packet_cache[i].seq_num == new_packet_id.seq_num) {
      ESP_LOGI(TAG, "Packet already cached");
      smf_set_state(&smf_context, IDLE); //if we've seen packet, go to idle
      return;
    }
  }

  recieved_packet_cache[recieved_packet_cache_index % RECIEVED_PACKET_CACHE_SIZE] = new_packet_id;
  recieved_packet_cache_index++;
    if (header.reciever_id == NODE_ID ) {
      ESP_LOGI(TAG, "Packet parsed succesfully");
      ESP_LOGI(TAG, "Sender id: %d packet num: %d", header.sender_id, header.seq_num);
    
    if (header.is_ack != 0 && retry_event_buffer[header.seq_num % RETRY_EVENT_BUFFER_SIZE].seq_num == header.seq_num) {
      retry_event_buffer[header.seq_num % RETRY_EVENT_BUFFER_SIZE].packet_acked = true;
      ESP_LOGI(TAG, "Ack packet");
      smf_set_state(&smf_context, IDLE);
      return;
    } 
    else if (header.is_ack != 0) {
      ESP_LOGI(TAG, "Ack packet, not matched in cache");
      smf_set_state(&smf_context, IDLE);
      return;
    }

    oled_mutex_lock(); 
    oled_print_message(plaintext, header.sender_id);
    oled_mutex_unlock();

    smf_set_state(&smf_context, SEND_ACK);
  }
  else if (header.sender_id != NODE_ID) {
    ESP_LOGI(TAG, "Packet parsed succesfully, not intended target");
    ESP_LOGI(TAG, "target id: %d", header.reciever_id);
    xEventGroupSetBits(transciever_events, RELAY_PACKET_EVENT);
    smf_set_state(&smf_context, SEND_PACKET); //remove relay logic for a sec
  }
  else {
    ESP_LOGI(TAG, "Packet parsed, packet sent by this node");
    smf_set_state(&smf_context, IDLE);
  }
}

