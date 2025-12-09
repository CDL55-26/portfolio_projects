#include <stdio.h>
#include <stdint.h>

float detect_dip(uint16_t* sample_buffer, uint32_t *frequency_buffer, uint32_t buffer_size,
                 uint32_t* out_frequency_buffer, float* out_sample_buffer)
{
    uint32_t out_index = 0;
    uint32_t i = 0;

    while (i < buffer_size) {
        uint32_t current_frequency = frequency_buffer[i];
        uint32_t j = i;
        while (j < buffer_size && frequency_buffer[j] == current_frequency)
            j++;  //end of frequency group

        double sum = 0.0;
        for (uint32_t k = i; k < j; k++)
            sum += sample_buffer[k];
        float avg = (float)(sum / (j - i)); //average of frequency group

        out_frequency_buffer[out_index] = current_frequency; //write average and corresponding freq to output buffs
        out_sample_buffer[out_index] = avg;
        out_index++;

        i = j;  //move to next frequency group
    }

    //find min
    float min_val = out_sample_buffer[0];
    uint32_t min_index = 0;
    for (uint32_t k = 1; k < out_index; k++) {
        if (out_sample_buffer[k] < min_val) {
            min_val = out_sample_buffer[k];
            min_index = k;
        }
    }
    
    // Return the frequency, not its index
    return (float)out_frequency_buffer[min_index];
}