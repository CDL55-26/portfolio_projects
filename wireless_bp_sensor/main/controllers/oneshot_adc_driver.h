#ifndef ONESHOT_ADC_DRIVER_H
#define ONESHOT_ADC_DRIVER_H

#include <stdint.h>

typedef struct {
  int16_t sample_data;
  uint32_t sample_frequency; 
}oneshot_adc_datapoint;

int oneshot_adc_init(void);
int oneshot_adc_get_datapoint(oneshot_adc_datapoint* data_point);
void oneshot_adc_filter_datapoint(oneshot_adc_datapoint* original_datapoint, 
                                  oneshot_adc_datapoint* smoothed_datapoint); 
void oneshot_adc_clear_filter(void); 


#endif
