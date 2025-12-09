#include <stdio.h>
#include <stdint.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_rom_sys.h"
#include "driver/gpio.h"
#include "esp_adc/adc_continuous.h"
#include "esp_log.h"

#include "adc_driver.h"
#include "ad9850.h"
#include "sweep_configs.h"

static const char *TAG = "adc_driver";

//ADC setup
#define ADC_FRAME_SIZE 256
#define ADC_SAMPLE_TIMEOUT 1000 //1s
#define ADC_SAMPLE_RATE_HZ 1000

static adc_continuous_handle_t adc_handle;

static uint8_t frame_buffer[ADC_FRAME_SIZE];

uint32_t total_samples; //total number of samples in the buffer
static sweep_config_t sweep_buffer_setup_configs;

//Data buffers
static int16_t* sweep_sample_buffer;
static uint32_t* sweep_frequency_buffer;



int adc_init(sweep_config_t* sweep_config) {
    adc_continuous_handle_cfg_t handle_config = {
        .max_store_buf_size = 2048,
        .conv_frame_size = ADC_FRAME_SIZE,
    };
    ESP_ERROR_CHECK(adc_continuous_new_handle(&handle_config, &adc_handle));

    adc_digi_pattern_config_t pattern = {
        .atten = ADC_ATTEN_DB_12, //big attenuation
        .bit_width = ADC_BITWIDTH_12,
        .channel = ADC_CHANNEL_0,    // GPIO0 = A0
        .unit = ADC_UNIT_1,
    };

    adc_continuous_config_t adc_continuous_configs = {
        .sample_freq_hz = ADC_SAMPLE_RATE_HZ,
        .conv_mode = ADC_CONV_SINGLE_UNIT_1,
        .format = ADC_DIGI_OUTPUT_FORMAT_TYPE2, //should also be type 2 for esp32c6
        .pattern_num = 1,
        .adc_pattern = &pattern,
    };

    ESP_ERROR_CHECK(adc_continuous_config(adc_handle, &adc_continuous_configs));

    //Handle buffer setups
    total_samples = (((sweep_config->stop_frequency - sweep_config->start_frequency)/sweep_config->step_size) + 1) * sweep_config->hold_ms;
    
    sweep_sample_buffer = heap_caps_malloc(total_samples * sizeof(int16_t), MALLOC_CAP_DEFAULT);
    sweep_frequency_buffer = heap_caps_malloc(total_samples * sizeof(uint32_t), MALLOC_CAP_DEFAULT);

    if (sweep_sample_buffer == NULL || sweep_frequency_buffer == NULL) {
        ESP_LOGE(TAG, "Malloc failed. Can't allocate buffers");
        return -1;
    }

    memcpy(&sweep_buffer_setup_configs, sweep_config, sizeof(sweep_config_t));

    return 0;
}

static void fill_frequency_buffer(void) {
    if (!sweep_frequency_buffer || total_samples == 0) { 
        ESP_LOGE(TAG, "Can't fill frequency buffer"); 
        return; 
    }

    uint32_t steps = ((sweep_buffer_setup_configs.stop_frequency - sweep_buffer_setup_configs.start_frequency) / sweep_buffer_setup_configs.step_size) + 1;
    uint32_t samples_per_step = (ADC_SAMPLE_RATE_HZ * sweep_buffer_setup_configs.hold_ms) / 1000;

    uint32_t index = 0;
    uint32_t frequency = sweep_buffer_setup_configs.start_frequency;
    
    for (uint32_t s = 0; s < steps; s++, frequency += sweep_buffer_setup_configs.step_size) {
        for (uint32_t k = 0; k < samples_per_step && index < total_samples; k++) {
            sweep_frequency_buffer[index++] = frequency;
        }
    }
}


int adc_sample_sweep(void) {
    uint32_t total_samples_read = 0;
    uint32_t actual_bytes_read = 0;

    ESP_LOGI(TAG,"Starting sampling");
    ESP_ERROR_CHECK(adc_continuous_start(adc_handle)); //this needs to be tight. Could have inconsitencies with frequency buffer

    while (total_samples_read < total_samples) {
        
        esp_err_t ret = (adc_continuous_read(adc_handle, frame_buffer, ADC_FRAME_SIZE, &actual_bytes_read, ADC_SAMPLE_TIMEOUT)); //blocks
        if (ret == ESP_OK) {

            int samples_in_this_frame = actual_bytes_read / sizeof(adc_digi_output_data_t);
            for (int i = 0; i < samples_in_this_frame; i++) {
                int global_sample_index = total_samples_read + i;
                if (global_sample_index >= total_samples) {
                    break; // Buffer is full
                }

                adc_digi_output_data_t* adc_struct_output = (adc_digi_output_data_t*)&frame_buffer[i * sizeof(uint32_t)]; //uint32 for TYPE 2, uint16 for TYPE1
                sweep_sample_buffer[global_sample_index] = (int16_t)adc_struct_output->val;
            }
            total_samples_read += samples_in_this_frame;
        }
        else if (ret == ESP_ERR_TIMEOUT) {
            ESP_LOGW("adc_driver", "ADC Read Timeout!");
            return -1;
        }
        else {
            ESP_LOGE("adc_driver", "ADC Read Error: %s", esp_err_to_name(ret));
           return -1; // Exit loop on error
        }
    }

    ESP_ERROR_CHECK(adc_continuous_stop(adc_handle));

    ESP_LOGI(TAG, "Finished Sampling");

    fill_frequency_buffer(); //fill after we sample. Want to keep sampling and sweeping as tightly coupled as possible 

    return 0;
}


void adc_get_data(uint32_t** frequency_buffer, int16_t** sample_buffer, uint32_t* sample_count) { //used to transfer control of buffers to dip detector
    *frequency_buffer = sweep_frequency_buffer;
    *sample_buffer = sweep_sample_buffer;

    *sample_count = total_samples;
}

void adc_free_buffers(void) {
    free(sweep_sample_buffer);
    free(sweep_frequency_buffer);
}


// // ****************************************************************************** TESTING.....
// #include <stdlib.h>
// #include <math.h>
// #include <stdint.h>
// #include <time.h>

// #define START_FREQ_KHZ      10000     // 10 MHz
// #define STOP_FREQ_KHZ       25000     // 25 MHz
// #define STEP_KHZ            1000       // 0.5 MHz = 500 kHz
// #define SAMPLES_PER_FREQ    100

// #define NUM_FREQ_STEPS    (((STOP_FREQ_KHZ - START_FREQ_KHZ) / STEP_KHZ) + 1)

// void test_adc_get_data(uint32_t **frequency_buffer, uint16_t **sample_buffer)
// {
//     *frequency_buffer = sweep_frequency_buffer;
//     *sample_buffer = sweep_sample_buffer;

//     const double dip_center_khz = 18500.0;   // dip at 18.5 MHz
//     const double sigma_khz = 500.0;          // width of dip in kHz
//     const double min_val = 700.0;            // bottom of dip (ADC counts)
//     const double max_val = 800.0;            // baseline top (ADC counts)

//     srand(k_cycle_get_32());

//     uint32_t index = 0;

//     for (uint32_t f_khz = START_FREQ_KHZ; f_khz <=STOP_FREQ_KHZ; f_khz += STEP_KHZ) {

//         // Reverse Gaussian — deep dip around 18.5 MHz (18500 kHz)
//         double x = (double)f_khz;
//         double gaussian = exp(-0.5 * pow((x - dip_center_khz) / sigma_khz, 2.0));
//         double avg_value = max_val - (max_val - min_val) * gaussian;

//         for (uint32_t s = 0; s < SAMPLES_PER_FREQ; s++) {
//             // Add small random noise (±5 ADC counts)
//             double noisy = avg_value + ((rand() % 11) - 5);
//             if (noisy < 0) noisy = 0;
//             if (noisy > 1023) noisy = 1023;  // 10-bit ADC limit

//             sweep_frequency_buffer[index] = f_khz;      // store directly in kHz
//             sweep_sample_buffer[index] = (uint16_t)noisy;
//             index++;
//         }
//     }

//     // Sanity check (optional)
//     if (index != ADC_BUFFER_LENGTH) {
//         printk("Warning: filled %u samples, expected %u\n", index, ADC_BUFFER_LENGTH);
//     }
// }


        