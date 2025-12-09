#include <stdint.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_rom_sys.h"
#include "driver/gpio.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_log.h"
#include "sweep_configs.h"
#include "oneshot_adc_driver.h"
#include "si5351.h"

static const char *TAG = "oneshot_adc_driver";


adc_oneshot_unit_handle_t oneshot_adc_handle;

#define FILTER_BUFFER_SIZE 4
oneshot_adc_datapoint filter_buffer[FILTER_BUFFER_SIZE];
uint32_t filter_buffer_count = 0;

int oneshot_adc_init(void) {
  esp_err_t ret;

  adc_oneshot_unit_init_cfg_t oneshot_adc_config = {
    .unit_id = ADC_UNIT_1,
    .ulp_mode = ADC_ULP_MODE_DISABLE,
  };
  ret = adc_oneshot_new_unit(&oneshot_adc_config , &oneshot_adc_handle);
  
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "ADC oneshot new unit failed");
    return -1;
  }


  adc_oneshot_chan_cfg_t config = {
    .atten = ADC_ATTEN_DB_12,         
    .bitwidth = ADC_BITWIDTH_12,     
  };
  
  ret = adc_oneshot_config_channel(oneshot_adc_handle, ADC_CHANNEL_0, &config);
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "ADC oneshot channel config failed");
    return -1;
  }

  ESP_LOGI(TAG, "ADC init complete"); 
  return 0;
}

int oneshot_adc_get_datapoint(oneshot_adc_datapoint* data_point) {
  esp_err_t ret;

  int sample_data;
  uint32_t sample_frequency;
  
  ret = adc_oneshot_read(oneshot_adc_handle, ADC_CHANNEL_0, &sample_data);
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "ADC oneshot read failed");
    return -1;
  }

  sample_frequency = si5351_get_frequency();
 
  //ESP_LOGI(TAG, "Original Data Point: %d", sample_data);
  data_point->sample_data = (uint16_t)sample_data;
  data_point->sample_frequency = sample_frequency;

  return 0;
}

void oneshot_adc_filter_datapoint(oneshot_adc_datapoint* original_datapoint, 
                                  oneshot_adc_datapoint* smoothed_datapoint) {
  
  //ESP_LOGI(TAG, "Smoothed Point: %d", original_datapoint->sample_data);
  //if buffer not full yet, just return unsmoothed, and fill
  if (filter_buffer_count < FILTER_BUFFER_SIZE - 1) {
    memcpy(smoothed_datapoint, original_datapoint, sizeof(oneshot_adc_datapoint)); 
    filter_buffer[filter_buffer_count] = *smoothed_datapoint;
    filter_buffer_count++;

    return;
  }
  /*
   Might want to tweak this and do a weighted average? 
   Might need to mess around with adding averaged vs raw sample to buffer
   */
  int32_t sample_data_sum = original_datapoint->sample_data;
  for (int index = 0; index < FILTER_BUFFER_SIZE; index++) {
    sample_data_sum += filter_buffer[index].sample_data;
  }
  //buffer size + current sample
  smoothed_datapoint->sample_data = sample_data_sum / (FILTER_BUFFER_SIZE + 1); 
  smoothed_datapoint->sample_frequency = original_datapoint->sample_frequency;

  filter_buffer[filter_buffer_count % FILTER_BUFFER_SIZE] = *original_datapoint; 
  filter_buffer_count++;
}

void oneshot_adc_clear_filter(void) {
  filter_buffer_count = 0;
  //just reset the count. values in buffer will be stale and get replaced
}





