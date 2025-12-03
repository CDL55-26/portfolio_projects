#ifndef OLED_H
#define OLED_H

void ssd1306_init(void);
void oled_draw_char(uint8_t x, uint8_t y, uint8_t c);
void oled_draw_string(uint8_t* str);
void oled_clear(void);
void update_oled(const char* text); //helper for keypad routine
void oled_mutex_lock(void);
void oled_mutex_unlock(void);
void oled_print_message(const uint8_t* original_message, uint8_t sender_id);

#endif
