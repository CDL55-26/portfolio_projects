#include <string.h>

#include "esp_log.h"
#include "esp_check.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_vendor.h"
#include "esp_lcd_panel_ssd1306.h"
#include "esp_lcd_io_i2c.h"  
#include "font8x8_basic.h"        

#include "driver/i2c_master.h" 

#include "oled.h"      
#include "keypad.h"

#define TAG "oled"

#define I2C_SDA             21
#define I2C_SCL             22
#define I2C_PORT            0
#define I2C_FREQ_HZ         400000
#define OLED_ADDR           0x3C
#define OLED_RST_GPIO       -1        

extern SemaphoreHandle_t oled_mutex; 

static esp_lcd_panel_handle_t panel;

//Helper functions
static void transpose_8x8(uint8_t dst[8], const uint8_t src[8]);

void ssd1306_init(void) {
    ESP_LOGI(TAG, "starting ssd1306 int");
    i2c_master_bus_config_t bus_cfg = {
        .clk_source = I2C_CLK_SRC_DEFAULT, //lets esp-idf choose a default clk for i2c
        .i2c_port = I2C_PORT, //using port 0
        .sda_io_num = I2C_SDA,
        .scl_io_num = I2C_SCL,
        .glitch_ignore_cnt = 7, //glitch filter, could prob remove
        .flags = {
            .enable_internal_pullup = true //add internal pullups
        },
    };
    i2c_master_bus_handle_t i2c_bus;
    ESP_ERROR_CHECK(i2c_new_master_bus(&bus_cfg, &i2c_bus)); //init i2c master

    esp_lcd_panel_io_i2c_config_t io_cfg = { // i/o protocol layer for i2c master -> oled display
        .dev_addr = OLED_ADDR,
        .scl_speed_hz = I2C_FREQ_HZ,
        .control_phase_bytes = 1,     
        .dc_bit_offset = 6,           
        .lcd_cmd_bits = 8,
        .lcd_param_bits = 8,
        
        
    };
    esp_lcd_panel_io_handle_t io;
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_i2c(i2c_bus, &io_cfg, &io));

    esp_lcd_panel_dev_config_t panel_cfg = {//driver for oled. driver -> io -> i2c bus -> display
        .bits_per_pixel = 1,
        .reset_gpio_num = OLED_RST_GPIO, 
    };
    ESP_ERROR_CHECK(esp_lcd_new_panel_ssd1306(io, &panel_cfg, &panel));

    
    ESP_ERROR_CHECK(esp_lcd_panel_reset(panel));
    ESP_ERROR_CHECK(esp_lcd_panel_init(panel));
    ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(panel, true));
    ESP_ERROR_CHECK(esp_lcd_panel_mirror(panel, true, true));

    
    oled_clear();

    ESP_LOGI(TAG, "ssd1306 initialized successfully");
}

static void transpose_8x8(uint8_t dst[8], const uint8_t src[8]) {
    for (int x = 0; x < 8; x++) {
        uint8_t col = 0;
        for (int y = 0; y < 8; y++) {
            // grab bit x of row y and move it into bit y of the column
            col |= ((src[y] >> x) & 0x01) << y;
        }
        dst[x] = col;
    }
}

void oled_clear(void) {
    uint8_t fb_black[128 * 64 / 8];
    memset(fb_black, 0x00, sizeof(fb_black)); 
    esp_lcd_panel_draw_bitmap(panel, 0, 0, 128, 64, fb_black);
}

void oled_draw_char(uint8_t x, uint8_t y, uint8_t c) {
    if (c > 127) return;             // skip unsupported chars
    const uint8_t *glyph = font8x8_basic[(uint8_t)c];
    uint8_t rotated_glyph[8];

    transpose_8x8(rotated_glyph, glyph);
    // Send this 8x8 block to the display
    esp_lcd_panel_draw_bitmap(panel, x, y, x + 8, y + 8, rotated_glyph);
}

void oled_draw_string(uint8_t* str) {
    uint8_t x = 0;
    uint8_t y = 0;
    uint8_t c = 0;

    while (str[c] != '\0' && c <= 128) {
        if (y <= 64) {
            oled_draw_char(x, y, str[c]);
            x += 8;
            if (x == 128) {
                x = 0;
                y+=8;
            }

        }
        c++;
    }
}

void update_oled(const char* text) { //small helper to pass to keypad routine
  oled_clear();
  oled_draw_string((uint8_t*)text);
}

void oled_mutex_lock(void) {
  if (xSemaphoreTake(oled_mutex, portMAX_DELAY) == pdTRUE) {
    return;
  }
}

void oled_mutex_unlock(void) {
  xSemaphoreGive(oled_mutex);
}

void oled_print_message(const uint8_t* original_message, uint8_t sender_id) {
  uint8_t id_message[] = "SID:X - ";
  size_t id_message_len = strlen((char*)id_message);
  size_t original_message_len = strlen((char*)original_message);
  //space for null at end
  size_t full_message_length = original_message_len + id_message_len + 1; 

  uint8_t full_message[full_message_length];

  strcpy((char*)full_message, (char*)id_message);
  strcpy((char*)(full_message + id_message_len), (char*)original_message);

  //replace X with node id
  full_message[4] = sender_id + 48;// convert to ascii

  update_message_cache((char*)full_message);

  oled_clear();
  oled_draw_string(full_message);
}
