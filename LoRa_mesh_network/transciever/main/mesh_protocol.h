//Functions
#ifndef MESH_PROTOCOL_H
#define MESH_PROTOCOL_H

#include "packets.h"
#include <stdbool.h>
#include <stdint.h>

typedef struct {
  lora_packet_t retry_packet;
  int retry_count;
  bool packet_acked;
  uint32_t seq_num;
}retry_event_t;

typedef struct {
  bool retry; //this is kind of garbage, just need to signal to retry, data dont matter
}retry_alert_t;

void LoRa_device_init(void);
int LoRa_packet_send(const uint8_t *data, size_t len, uint32_t timeout_ms);
int LoRa_packet_check_and_receive(lora_packet_t* new_recieved_packet); 
void LoRa_mutex_lock(void); 
void LoRa_mutex_unlock(void); 


#endif
