#pragma once
#include <stddef.h>

//Macros
#define SMF_CREATE_STATE(_entry, _run, _exit) \
    { .entry = (_entry), .run = (_run), .exit = (_exit)} 

//function pointer called smf_function_t. functions return void and take void* as arg
typedef void (*smf_function_t)(void);

typedef struct {

    smf_function_t entry;
    smf_function_t run;
    smf_function_t exit;

}smf_state_t;

typedef struct {
    const smf_state_t* state_table;
    int current_table_index;
    int next_table_index;
}smf_context_t;

//API
void smf_init(smf_context_t* smf_context, const smf_state_t* state_table, int initial_state);
void smf_set_state(smf_context_t* smf_context, int new_state);
void smf_run(smf_context_t* smf_context);

#ifndef BIT
#define BIT(n) (1UL << (n))
#endif