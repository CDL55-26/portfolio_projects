#include "packets.h"
#include "secrets.h"
#include "string.h"
#include "esp_log.h"

static const char *TAG = "packets";

#define HEADER_LEN 6 //6 bytes
#define MAX_MESSAGE_LEN 162 //bytes

static uint32_t node_sequence_num = 0;

int build_packet(build_packet_data_t packet_data, uint8_t* packet, uint8_t* packet_len) {
    packet_header_t header;
    uint8_t* message = packet_data.message;
    uint8_t message_len = packet_data.message_len;
    if (message_len > MAX_MESSAGE_LEN) {
        message_len = MAX_MESSAGE_LEN;
    }

    header.sender_id = packet_data.sender_id;
    header.reciever_id = packet_data.reciever_id;
    header.seq_num = node_sequence_num;

    uint8_t iv_cipher[message_len + 32]; //32 = 16 byte IV + worst case 16 bytes of padding
    uint8_t iv_cipher_len;

    int ret;
    ret = encrypt_message(message, message_len, iv_cipher, &iv_cipher_len);
    if (ret != 0) {
        ESP_LOGE(TAG, "Packet encrypt failed");
        return ret;
    }

    *packet_len = iv_cipher_len + HEADER_LEN;

    memcpy(packet, &header, HEADER_LEN);
    memcpy(packet+HEADER_LEN, iv_cipher, iv_cipher_len);

    node_sequence_num++;
    ESP_LOGI(TAG, "Packet built");

    return 0;
}

int parse_packet(uint8_t* packet, uint16_t len, packet_header_t* header, uint8_t* plaintext) {
    uint8_t packet_len = (uint8_t) len;
    uint8_t iv_cipher_len = packet_len - HEADER_LEN;
    
    memcpy(header, packet, HEADER_LEN); //copy header bytes in header struct

    int ret;
    ret = decrypt_message(packet+HEADER_LEN, iv_cipher_len, plaintext);
    if (ret != 0) {
        ESP_LOGE(TAG, "Decrypt failed while parsing packet");
        return ret;
    }

    return 0;
}