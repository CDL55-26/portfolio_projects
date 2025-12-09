#ifndef SI5351_H
#define SI5351_H

#include "sweep_configs.h"
#include <stdint.h>

int si5351_init(void);
int si5351_set_frequency(uint32_t target_frequency);
int si5351_sweep_frequencies(sweep_config_t sweep_config);
uint32_t si5351_get_frequency(void);

#endif
