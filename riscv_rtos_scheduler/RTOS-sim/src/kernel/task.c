#include "task.h"
#include "uart.h" // Or your printing utility
#include "timer.h"

#define MSTATUS_MPP_M (3ULL << 11)
#define MSTATUS_MPIE (1ULL << 7)

TCB_t tasks[2];

uint32_t current_task_idx = 0; //actual definition for current_task_index


uint64_t task1_stack[256];
uint64_t task2_stack[256]; //tells linker to reserve 32 bytes in RAM

void task_init(void) {
    // --- Initialize Context for Task 1 ---
    uint64_t *sp1 = &task1_stack[256] - 33; // Calculate base of the 33-word frame

    // Zero out the entire frame for a clean state
    for (int i = 0; i < 33; i++) {
        sp1[i] = 0;
    }

    // Set mstatus to enable interrupts upon return from trap
    sp1[32] = MSTATUS_MPP_M | MSTATUS_MPIE;
    // Set mepc to the task's entry point
    sp1[31] = (uint64_t)task1;

    // Save the final stack pointer to the Task Control Block
    tasks[0].stack_pointer = sp1;

    // --- Initialize Context for Task 2 ---
    uint64_t *sp2 = &task2_stack[256] - 33;

    for (int i = 0; i < 33; i++) {
        sp2[i] = 0;
    }

    sp2[32] = MSTATUS_MPP_M | MSTATUS_MPIE;
    sp2[31] = (uint64_t)task2;
    tasks[1].stack_pointer = sp2;
}

// The first task function
void task1(void) {
   while (1) {
        uart_puts("A");
        for (volatile int i = 0; i < 1000000; i++);
   }    
}

// The second task function
void task2(void) {
    while (1) {
        uart_puts("B");
        for (volatile int i = 0; i < 1000000; i++); //delay
    }
}

void scheduler(void) {
    current_task_idx = (current_task_idx + 1) % 2; //just toggle index
}
