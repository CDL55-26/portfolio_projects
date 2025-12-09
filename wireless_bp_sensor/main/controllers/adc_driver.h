#ifndef ADC_DRIVER
#define ADC_DRIVER

#include <stdint.h>
#include "ad9850.h"

/*
ADC_BUFFER_SIZE =  ( ((stop_frequency - start_frequency) ) / step_size) + 1 ) * hold_ms
ex. (((25000-10000)/500 + 1) * 100 = 3100 samples
*/
#define ADC_SAMPLE_RATE_HZ 1000 //1ms in us 

int adc_init(sweep_config_t* sweep_config);
int adc_sample_sweep(void);
void adc_get_data(uint32_t** frequency_buffer, int16_t** sample_buffer, uint32_t* sample_count);

//****** TEST function for sampling: generates arbitrary data and puts into the sample and freq buffers
void test_adc_get_data(uint32_t **frequency_buffer, uint16_t **sample_buffer); 

#endif