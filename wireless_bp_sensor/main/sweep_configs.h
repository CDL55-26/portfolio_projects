#ifndef SWEEP_CONFIGS
#define SWEEP_CONFIGS

#include <stdint.h>

typedef struct {
    uint32_t start_frequency; 
    uint32_t stop_frequency;
    uint32_t step_size;
    uint32_t hold_ms; //lets start with 5-10ms as a default
}sweep_config_t;

typedef struct {
  uint32_t frequency;
  uint16_t frequency_index;
}resonant_frequency_tuple;


#endif
