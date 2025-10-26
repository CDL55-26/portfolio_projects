#ifndef ADF4351_H
#define ADF4351_H

#include <stdint.h>

//structs
typedef struct {
    uint32_t start_frequency; 
    uint32_t stop_frequency;
    uint32_t step_size;
    uint32_t hold_ms; //lets start with 5-10ms as a default
}sweep_configt_t;

int adf4351_init(void); //setup spi, gpio, blast write all 6 registers. Init 40MHz, 100 kHz step
int adf4351_set_frequency(uint32_t frequency); //frequnecy will be in kHz for ease of fractions
//step size also in kHz -> adf4351_set_frequency(40000, 100) = 40 MHz signal with precision of 100 kHz

int adf4351_sweep_frequencies(sweep_configt_t sweep_config);

//extend to frequency sweep, add more helper functions



#endif