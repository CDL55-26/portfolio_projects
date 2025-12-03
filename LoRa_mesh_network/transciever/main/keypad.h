#pragma once

#include "esp_err.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h" 

#ifdef __cplusplus
extern "C" {
#endif

// Configuration for buffer size and timing
#define KEYPAD_MAX_STR_LEN      128     
#define KEYPAD_CYCLE_TIMEOUT_MS 1000    

#define ROW1 14
#define ROW2 12
#define ROW3 13
#define ROW4 15
#define COL1 2
#define COL2 0
#define COL3 4
#define COL4 25


#define STORED_MESSAGE_SIZE 3
typedef struct {
  char message_array[STORED_MESSAGE_SIZE][256];
  int message_display_index; 
  int message_seen_count; 
}message_cache_t;

/**
 * @brief Function pointer type for display updates.
 */
typedef void (*keypad_display_update_cb_t)(const char *text);

/**
 * @brief Configuration structure for keypad initialization
 */
typedef struct {
    gpio_num_t row_pins[4]; // Output pins (Rows 1-4)
    gpio_num_t col_pins[4]; // Input pins (Cols 1-4)
    
    // NEW: Shared resource management
    SemaphoreHandle_t oled_mutex; 
    keypad_display_update_cb_t display_callback; 
} keypad_config_t; // <--- CRITICAL: Ensure this semicolon exists!

/**
 * @brief Initialize the keypad driver.
 */
esp_err_t keypad_init(const keypad_config_t *config);

/**
 * @brief Blocking call to wait for a complete user string.
 */
esp_err_t keypad_get_input(char *out_buffer, uint8_t* node_id, size_t max_len);

void update_message_cache(const char* new_message); 

#ifdef __cplusplus
}
#endif
