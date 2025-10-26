// main.c
#include "uart.h"
#include "timer.h"
#include "riscv.h"
#include "task.h"

// In src/main.c

int main(void) {
    uart_init();
    timer_init(); 
    uart_puts("Checkpoint 1: Drivers Initialized\n");

    task_init();
    uart_puts("Checkpoint 2: Tasks Initialized\n");

    set_mtvec((uint64_t)_trap);
    uart_puts("Checkpoint 3: MTVEC Set\n");
    
    enable_timer_interrupts();
    uart_puts("Checkpoint 4: Timer Interrupts Enabled\n");

    enable_global_interrupts();
    uart_puts("Checkpoint 5: Global Interrupts Enabled\n");

    // 5. Start the scheduler (this call never returns)
    _scheduler_init(); // Or scheduler_start(), whichever name you chose

    // This code should now be unreachable
    uart_puts("ERROR: Scheduler Returned!\n");
    while(1);
}
