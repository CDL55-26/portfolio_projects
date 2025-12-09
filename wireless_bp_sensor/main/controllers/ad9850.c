#include "ad9850.h"
#include "sweep_configs.h"
#include <stdio.h>
#include <stdint.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_rom_sys.h"
#include "rom/ets_sys.h"
#include "driver/gpio.h"
#include "esp_log.h"

static const char *TAG = "ad9850";

//HL dev board 
#define FREF 125000ULL
#define FOUT_MAX 40000 //40 MHz 

#define CALC_TUNING_WORD(kHz) ((uint32_t)(((uint64_t)(kHz) << 32) / (FREF)))//kHz * 2^32 / 125000kHz
#define TUNING_WORD_BITS 32
#define CONTROL_WORD_BITS 8


//Frequency static
static uint32_t ad9850_frequency = 0;

//gpio setup
#define PIN_HIGH 1
#define PIN_LOW 0

#define CLK_PIN 17
#define FQUD_PIN 19
#define DAT_PIN 20
#define RST_PIN 18


//helper functions
static void write_bit(uint8_t bit);
static void write_word(uint32_t tuning_word, uint8_t bit_count);


int ad9850_init(void) {
    gpio_config_t gpio_conf = {
        .pin_bit_mask = ((1ULL << CLK_PIN) | (1ULL << FQUD_PIN) | (1ULL << DAT_PIN) | (1ULL << RST_PIN)),
        .mode = GPIO_MODE_OUTPUT,
     };
     gpio_config(&gpio_conf);
   
    ad9850_reset(); //reset on init
    ad9850_power_down();
    
    return 0;
}

void ad9850_reset(void) {
    gpio_set_level(RST_PIN, PIN_HIGH);
    ets_delay_us(5); //wait 5 us;
    gpio_set_level(RST_PIN, PIN_LOW);

    gpio_set_level(FQUD_PIN, PIN_HIGH); //data sheet recommends to pulse fqud after reset to bring into serial ready state
    ets_delay_us(5); 
    gpio_set_level(FQUD_PIN, PIN_LOW);
}

void ad9850_power_down(void) {
    uint32_t tuning_word = 0; //set freq to zero on power down
    write_word(tuning_word, TUNING_WORD_BITS);

    uint8_t control_word = 0x20; //set power down bit to 1, everything else zero
    write_word(control_word, CONTROL_WORD_BITS);

    gpio_set_level(FQUD_PIN, PIN_HIGH); //latch new frequency word 
    ets_delay_us(1);
    gpio_set_level(FQUD_PIN, PIN_LOW);

    ESP_LOGI(TAG," AD9850 power down complete");
}

int ad9850_set_frequency(uint32_t frequency_out) {
    if (frequency_out > FOUT_MAX) {
        ESP_LOGE(TAG,"Frequency out of bounds");
        return -1;
    }
    uint32_t tuning_word = CALC_TUNING_WORD(frequency_out);
    write_word(tuning_word, TUNING_WORD_BITS);

    uint8_t control_word = 0; //no phase change or power off ***REVIST if issues
    write_word(control_word, CONTROL_WORD_BITS);

    gpio_set_level(FQUD_PIN, PIN_HIGH); //latch new frequency word 
    ets_delay_us(1);
    gpio_set_level(FQUD_PIN, PIN_LOW);

    ad9850_frequency = frequency_out;
    return 0;

}

int ad9850_sweep_frequency(sweep_config_t sweep_config) {
    uint32_t current_frequency = sweep_config.start_frequency;
    
    int ret;
    while (current_frequency <= sweep_config.stop_frequency) {
        
        ret = ad9850_set_frequency(current_frequency);
        if (ret != 0) {
            ESP_LOGE(TAG,"Frequency set failed during sweep");
            return -1;
        }

        current_frequency += sweep_config.step_size;
        vTaskDelay(pdMS_TO_TICKS(sweep_config.hold_ms));
    }

    ESP_LOGI(TAG, "Frequency sweep done");
    return 0;
}

void ad9850_get_frequency(uint32_t* current_frequency) {
    *current_frequency = ad9850_frequency; //user pointer -> current static global frequency
}

static void write_bit(uint8_t bit) { //uint8 because we just need 1 bit
    gpio_set_level(DAT_PIN, bit);
    ets_delay_us(1);

    gpio_set_level(CLK_PIN, PIN_HIGH);
    ets_delay_us(1); //wait 50 ns for chip to latch
    gpio_set_level(CLK_PIN, PIN_LOW);

}

static void write_word(uint32_t tuning_word, uint8_t bit_count) {
    for (int shift_index = 0; shift_index < bit_count; shift_index++) {
        uint8_t isolated_bit = (tuning_word >> shift_index) & 0x01;
        write_bit(isolated_bit);
    }
}
