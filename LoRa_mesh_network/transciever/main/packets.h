#ifndef PACKETS_H
#define PACKETS_H

#include <stdint.h>
#define HEADER_LEN 7

typedef struct __attribute__((packed)) { //dont let compiler pad
    uint8_t sender_id;
    uint8_t reciever_id;
    uint32_t seq_num;
    uint8_t is_ack;
} packet_header_t;

typedef struct {
    uint8_t* message;
    uint8_t message_len;
    uint8_t sender_id;
    uint8_t reciever_id;
    uint32_t seq_num;
    uint8_t is_ack;
}build_packet_data_t;

typedef struct {
 uint8_t data[256];
 uint16_t len;
}lora_packet_t;

typedef struct {
  uint8_t node_id;
  uint32_t seq_num;
}packet_id_t;

int build_packet(build_packet_data_t packet_data, uint8_t* packet, uint8_t* packet_len);
int parse_packet(uint8_t* packet, uint16_t len, packet_header_t* header, uint8_t* plaintext);

#endif
