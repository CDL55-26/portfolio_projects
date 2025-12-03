#include "keypad.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "esp_log.h"
#include <string.h>
#include <ctype.h>

static const char *TAG = "KEYPAD";

// --- Internal Configuration ---
#define SCAN_INTERVAL_MS    20
#define DEBOUNCE_MS         50
#define MUTEX_ACQUIRE_TIMEOUT pdMS_TO_TICKS(5000) // 5s timeout to acquire the OLED lock

// --- Data Structures ---

message_cache_t message_cache = {
  .message_display_index = 0,
  .message_seen_count = 0,
};

static const char *CHAR_MAP[10] = {
    " 0",       // 0
    ".,?!1",    // 1
    "abc2",     // 2
    "def3",     // 3
    "ghi4",     // 4
    "jkl5",     // 5
    "mno6",     // 6
    "pqrs7",    // 7
    "tuv8",     // 8
    "wxyz9"     // 9
};

static const char MATRIX_LAYOUT[4][4] = {
    {'1', '2', '3', 'A'},
    {'4', '5', '6', 'B'},
    {'7', '8', '9', 'C'},
    {'*', '0', '#', 'D'}
};

typedef struct {
    keypad_config_t hw_config;
    QueueHandle_t input_queue;  
    char current_buffer[KEYPAD_MAX_STR_LEN];
    int cursor_pos;
    
    char last_key_raw;          
    int cycle_index;            
    uint32_t last_press_time;   
    bool pending_commit;
    
    bool session_active;
    
    // NEW: Display handling
    SemaphoreHandle_t oled_mutex;
    keypad_display_update_cb_t display_callback;
} keypad_ctx_t;

static keypad_ctx_t s_ctx;

// Helper to update the display (only called when we already hold the lock)
static void update_display(const char *text) {
    if (s_ctx.display_callback) {
        s_ctx.display_callback(text);
    }
}

static char scan_matrix() {
    for (int r = 0; r < 4; r++) {
        gpio_set_level(s_ctx.hw_config.row_pins[r], 0);
        for (int c = 0; c < 4; c++) {
            if (gpio_get_level(s_ctx.hw_config.col_pins[c]) == 0) {
                gpio_set_level(s_ctx.hw_config.row_pins[r], 1);
                return MATRIX_LAYOUT[r][c];
            }
        }
        gpio_set_level(s_ctx.hw_config.row_pins[r], 1);
    }
    return 0; 
}

static void process_key_logic(char raw_key) {
    uint32_t now = xTaskGetTickCount() * portTICK_PERIOD_MS;
    
    // --- IDLE STATE LOGIC ---
    if (!s_ctx.session_active) {
        if (raw_key == '*') {
            // Try to acquire the OLED lock immediately
            if (xSemaphoreTake(s_ctx.oled_mutex, MUTEX_ACQUIRE_TIMEOUT) == pdTRUE) {
                // Lock acquired, now we can start the session
                s_ctx.session_active = true;
                
                // Clear state
                s_ctx.cursor_pos = 0;
                memset(s_ctx.current_buffer, 0, KEYPAD_MAX_STR_LEN);
                s_ctx.pending_commit = false;
                s_ctx.last_key_raw = 0;
                
                // Provide visual feedback that typing has started
                update_display("[Typing]...");
                ESP_LOGI(TAG, "Session START: OLED lock acquired.");
            } else {
                ESP_LOGW(TAG, "Failed to acquire OLED lock. Cannot start typing session.");
            }
        }

        if (raw_key == 'D') {
            if (xSemaphoreTake(s_ctx.oled_mutex, MUTEX_ACQUIRE_TIMEOUT) == pdTRUE) {
              // Lock acquired, now we can start the session
                          
              if (message_cache.message_display_index < message_cache.message_seen_count - 1) {
                message_cache.message_display_index++;
                update_display(message_cache.message_array[message_cache.message_display_index]);
                ESP_LOGI(TAG, "Cycled back. index: %d", message_cache.message_display_index);
              }

              xSemaphoreGive(s_ctx.oled_mutex); // <--- RELEASE THE LOCK
            } 
            else {
              ESP_LOGW(TAG, "Failed to acquire OLED lock. Cannot cycle message");
            }
        }

        if (raw_key == 'C') {
            if (xSemaphoreTake(s_ctx.oled_mutex, MUTEX_ACQUIRE_TIMEOUT) == pdTRUE) {
              // Lock acquired, now we can start the session
                          
              if (message_cache.message_display_index > 0) {
                message_cache.message_display_index--;
                update_display(message_cache.message_array[message_cache.message_display_index]);
                ESP_LOGI(TAG, "Cycled up. index: %d", message_cache.message_display_index);
              }
              
              xSemaphoreGive(s_ctx.oled_mutex); // <--- RELEASE THE LOCK
            } 
            else {
              ESP_LOGW(TAG, "Failed to acquire OLED lock. Cannot cycle message");
            }
        }
        if (raw_key == 'B') {
          if (xSemaphoreTake(s_ctx.oled_mutex, MUTEX_ACQUIRE_TIMEOUT) == pdTRUE) {
            // Lock acquired, now we can start the session
            //just display most recently viewed message
            if (message_cache.message_seen_count > 0) {
              update_display(message_cache.message_array[message_cache.message_display_index]);
              ESP_LOGI(TAG, "Restored message. index: %d", message_cache.message_display_index);
            }

            else {
              update_display("No Messages");
            }

            xSemaphoreGive(s_ctx.oled_mutex); // <--- RELEASE THE LOCK
          } 
          else {
            ESP_LOGW(TAG, "Failed to acquire OLED lock. Cannot cycle message");
          }
        }


        return;
    }

    // --- ACTIVE TYPING LOGIC ---

    // 1. Handle '#' (ENTER / SEND)
    if (raw_key == '#') {
        s_ctx.current_buffer[s_ctx.cursor_pos] = '\0';
        
        // Send message to main task queue
        if (xQueueSend(s_ctx.input_queue, s_ctx.current_buffer, 0) != pdTRUE) {
            ESP_LOGW(TAG, "Input queue full, dropped message");
        } 

        // Update display one last time and indicate message sent
        update_display(s_ctx.current_buffer); // Display finalized message momentarily
        vTaskDelay(pdMS_TO_TICKS(500)); // Show it for half a second before releasing the lock
        
        // Reset state and RELEASE THE LOCK
        s_ctx.session_active = false; 
        xSemaphoreGive(s_ctx.oled_mutex); // <--- RELEASE THE LOCK
        
        ESP_LOGI(TAG, "Message Sent. OLED lock released.");
        return;
    }

    // 2. Handle '*' (BACKSPACE / DELETE)
    if (raw_key == '*') {
        if (s_ctx.pending_commit) {
            s_ctx.current_buffer[s_ctx.cursor_pos] = 0;
            s_ctx.pending_commit = false;
            s_ctx.last_key_raw = 0;
        } else if (s_ctx.cursor_pos > 0) {
            s_ctx.cursor_pos--;
            s_ctx.current_buffer[s_ctx.cursor_pos] = 0;
        }
        update_display(s_ctx.current_buffer);
        return;
    }

    // 3. Handle A, B, C, D (Plain characters) & 4. Numeric Keys (0-9)
    if ((raw_key >= 'A' && raw_key <= 'D') || (raw_key >= '0' && raw_key <= '9')) {
        
        if (raw_key >= 'A' && raw_key <= 'D') {
            // Logic for A/B/C/D
            if (s_ctx.pending_commit) { s_ctx.cursor_pos++; s_ctx.pending_commit = false; }
            if (s_ctx.cursor_pos < KEYPAD_MAX_STR_LEN - 1) {
                s_ctx.current_buffer[s_ctx.cursor_pos] = raw_key;
                s_ctx.cursor_pos++;
                s_ctx.current_buffer[s_ctx.cursor_pos] = 0;
            }
            s_ctx.last_key_raw = 0; 

        } else {
            // Logic for 0-9 (T9)
            int num_idx = raw_key - '0';
            const char *cycles = CHAR_MAP[num_idx];
            int cycle_len = strlen(cycles);

            bool is_continuation = (raw_key == s_ctx.last_key_raw) && 
                                   (now - s_ctx.last_press_time < KEYPAD_CYCLE_TIMEOUT_MS) &&
                                   s_ctx.pending_commit;

            if (is_continuation) {
                s_ctx.cycle_index = (s_ctx.cycle_index + 1) % cycle_len;
                s_ctx.current_buffer[s_ctx.cursor_pos] = cycles[s_ctx.cycle_index];
                s_ctx.last_press_time = now;
            } else {
                if (s_ctx.pending_commit) {
                    s_ctx.cursor_pos++;
                    if (s_ctx.cursor_pos >= KEYPAD_MAX_STR_LEN - 1) {
                        s_ctx.cursor_pos = KEYPAD_MAX_STR_LEN - 2; 
                    }
                }
                
                s_ctx.cycle_index = 0;
                s_ctx.current_buffer[s_ctx.cursor_pos] = cycles[0];
                s_ctx.last_key_raw = raw_key;
                s_ctx.last_press_time = now;
                s_ctx.pending_commit = true;
            }
        }
        
        // After any successful character change, update the display!
        update_display(s_ctx.current_buffer);
    }
}


static void keypad_scan_task(void *arg) {
    char current_key = 0;
    int stable_count = 0;
    bool key_handled = false;

    while (1) {
        char raw = scan_matrix();

        // Debounce
        if (raw == current_key && raw != 0) {
            stable_count++;
        } else {
            stable_count = 0;
            current_key = raw;
            key_handled = false; 
        }

        // Trigger logic
        if (stable_count >= (DEBOUNCE_MS / SCAN_INTERVAL_MS) && !key_handled) {
            if (current_key != 0) {
                process_key_logic(current_key);
            }
            key_handled = true;
        }
        
        // Time-based commit logic: Only applies if we hold the lock (session_active)
        if (s_ctx.session_active && s_ctx.pending_commit && (xTaskGetTickCount() * portTICK_PERIOD_MS - s_ctx.last_press_time > KEYPAD_CYCLE_TIMEOUT_MS)) {
             s_ctx.pending_commit = false;
             s_ctx.cursor_pos++;
             if (s_ctx.cursor_pos < KEYPAD_MAX_STR_LEN) {
                 s_ctx.current_buffer[s_ctx.cursor_pos] = 0;
             }
             s_ctx.last_key_raw = 0;
             
             // Commit also updates the display
             update_display(s_ctx.current_buffer);
        }

        vTaskDelay(pdMS_TO_TICKS(SCAN_INTERVAL_MS));
    }
}

esp_err_t keypad_init(const keypad_config_t *config) {
    if (!config || !config->oled_mutex || !config->display_callback) {
        ESP_LOGE(TAG, "Invalid configuration: Mutex or callback missing.");
        return ESP_ERR_INVALID_ARG;
    }

    s_ctx.hw_config = *config;
    s_ctx.cursor_pos = 0;
    s_ctx.pending_commit = false;
    s_ctx.session_active = false; 
    s_ctx.oled_mutex = config->oled_mutex;
    s_ctx.display_callback = config->display_callback;
    memset(s_ctx.current_buffer, 0, KEYPAD_MAX_STR_LEN);

    // GPIO configuration (omitted for brevity, assume same as V1)

    // 1. Rows (Outputs)
    gpio_config_t row_conf = {
        .mode = GPIO_MODE_OUTPUT, .pull_up_en = 0, .pull_down_en = 0, .intr_type = GPIO_INTR_DISABLE, .pin_bit_mask = 0
    };
    for(int i=0; i<4; i++) row_conf.pin_bit_mask |= (1ULL << config->row_pins[i]);
    gpio_config(&row_conf);
    for(int i=0; i<4; i++) gpio_set_level(config->row_pins[i], 1);

    // 2. Cols (Inputs)
    gpio_config_t col_conf = {
        .mode = GPIO_MODE_INPUT, .pull_up_en = 1, .pull_down_en = 0, .intr_type = GPIO_INTR_DISABLE, .pin_bit_mask = 0
    };
    for(int i=0; i<4; i++) col_conf.pin_bit_mask |= (1ULL << config->col_pins[i]);
    gpio_config(&col_conf);

    // 3. Queue
    s_ctx.input_queue = xQueueCreate(5, KEYPAD_MAX_STR_LEN);
    if (s_ctx.input_queue == NULL) return ESP_FAIL;

    // 4. Task
    xTaskCreate(keypad_scan_task, "keypad_task", 4096, NULL, 5, NULL);

    ESP_LOGI(TAG, "Keypad initialized (IDLE - Waiting for '*' to start)");
    return ESP_OK;
}
 
esp_err_t keypad_get_input(char *out_buffer, uint8_t* node_id, size_t max_len) {
    // ... same as before
    if (!s_ctx.input_queue || !out_buffer) return ESP_ERR_INVALID_STATE;

    /*
     right now copying temp_buf into out_buffer
     keep same copying, find strlen of out_buffer
     grab target id as out_buffer[len - 1]
     then out_buffer[len - 1] = 0;
     should handle errors/bad id here while I still have the lock?
     maybe just parse silently and turn on an LED if bad id
     */
    char temp_buf[KEYPAD_MAX_STR_LEN];
    int message_len;
    int temp_node_id;

    if (xQueueReceive(s_ctx.input_queue, temp_buf, portMAX_DELAY) == pdTRUE) {
        strncpy(out_buffer, temp_buf, max_len);
        out_buffer[max_len - 1] = 0;

        message_len = strlen(out_buffer);
        temp_node_id = atoi(&out_buffer[message_len - 1]);
        ESP_LOGI(TAG, "temp node id: %d", temp_node_id);
        if (temp_node_id < 1 || temp_node_id > 9) {
          *node_id = -1;
          ESP_LOGE(TAG, "Bad target id");
          return ESP_FAIL;
        }
        *node_id = (uint8_t)temp_node_id;
        out_buffer[message_len -1] = 0; //yank id and replace with zero to indicate terminator
        return ESP_OK;
    }
    return ESP_FAIL;
}

void update_message_cache(const char* new_message) {
  if (message_cache.message_seen_count == 0) {
    strcpy(message_cache.message_array[0], new_message);
  }
  else if (message_cache.message_seen_count < STORED_MESSAGE_SIZE && message_cache.message_seen_count > 0) {
    for (int i = message_cache.message_seen_count - 1; i >= 0; i--) {
      strcpy(message_cache.message_array[i+1], message_cache.message_array[i]);
    }
    strcpy(message_cache.message_array[0], new_message);
  }
  else {
    for (int i = STORED_MESSAGE_SIZE - 1; i >= 0; i--) {
      strcpy(message_cache.message_array[i+1], message_cache.message_array[i]);
    }
    strcpy(message_cache.message_array[0], new_message);
  }

  if (message_cache.message_seen_count < STORED_MESSAGE_SIZE) {
    message_cache.message_seen_count++;
  }
  message_cache.message_display_index = 0; //reset display index after each new addition
}
