#ifndef AD9850_H
#define AD9850_H

#include <stdint.h>
#include "sweep_configs.h"

int ad9850_init(void);
int ad9850_set_frequency(uint32_t frequency_out);
int ad9850_sweep_frequency(sweep_config_t sweep_config);
void ad9850_reset(void);
void ad9850_power_down(void);
void ad9850_get_frequency(uint32_t* current_frequency);

#endif