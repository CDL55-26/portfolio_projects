#ifndef ADF4351_H
#define ADF4351_H

#include <stdint.h>
#include "sweep_configs.h"

//structs
typedef struct {
    uint8_t div;
    uint8_t ps;
    uint16_t INT;
    uint16_t pfd;
    uint32_t current_freq_kHz;
    uint32_t r4;
    uint32_t r2;
    uint32_t r1;
    uint32_t r0;
}adf4351_status_t;

int adf4351_init(void); //setup spi, gpio, blast write all 6 registers. Init 40MHz, 100 kHz step
int adf4351_set_frequency(uint32_t frequency); //frequnecy will be in kHz for ease of fractions
//step size also in kHz -> adf4351_set_frequency(40000, 100) = 40 MHz signal with precision of 100 kHz

int adf4351_sweep_frequencies(sweep_config_t sweep_config);

//extend to frequency sweep, add more helper functions

#endif