//Functions
void LoRa_device_init(void);
int LoRa_packet_send(const uint8_t *data, size_t len, uint32_t timeout_ms);
int LoRa_packet_recieve(uint32_t timeout_ms);