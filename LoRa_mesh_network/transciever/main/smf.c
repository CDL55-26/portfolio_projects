#include <stdio.h>
#include <stdint.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_rom_sys.h"
#include "esp_log.h"

#include "smf.h"

void smf_init(smf_context_t* smf_context, const smf_state_t* state_table, int initial_state) {
    smf_context->state_table = state_table;
    smf_context->current_table_index = initial_state;
    smf_context->next_table_index = initial_state; //current and next states are the same until user prompts state change
}

void smf_set_state(smf_context_t* smf_context, int new_state) {
    smf_context->next_table_index = new_state;
}

void smf_run(smf_context_t* smf_context) {
    const smf_state_t* table = smf_context->state_table; //get current state table

    if (smf_context->current_table_index != smf_context->next_table_index) { //states been updated 
        if (table[smf_context->current_table_index].exit != NULL) {
            table[smf_context->current_table_index].exit();
        }

        smf_context->current_table_index = smf_context->next_table_index;

        if (table[smf_context->current_table_index].entry != NULL) {
            table[smf_context->current_table_index].entry();
        }
    }

    if (table[smf_context->current_table_index].run != NULL) {
            table[smf_context->current_table_index].run();
        }
}