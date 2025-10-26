#include <stdint.h>
#include "riscv.h" 
#include "uart.h"
#include "task.h"
#include "trap.h"


// A global variable to hold our system tick count
volatile uint64_t g_ticks = 0;

void handle_timer_interrupt(void) {
    // 1. Get a pointer to the 64-bit MTIMECMP register
    // (keep your structure; we’ll use 32-bit halves below)
    // NOTE: remove this if your compiler warns about unused; it’s not needed anymore.
    // volatile uint64_t *mtimecmp = (uint64_t *)CLINT_MTIMECMP;

    // 2. Read the current value of MTIME
    volatile uint64_t *mtime = (uint64_t *)CLINT_MTIME;
    uint64_t current_mtime = *mtime;

    // 3. Set the next interrupt time. For a 100 Hz tick on a 10 MHz clock:
    uint64_t next_interrupt_time = current_mtime + 100000ULL;

    // 4. Write the new value to MTIMECMP to schedule the next interrupt
    // --- BEGIN: minimal safe update to avoid MTIP staying asserted ---
    uint64_t mie_save = read_mie();                                  // NEW
    write_mie(mie_save & ~(1ULL << 7));                               // NEW (clear MTIE)

    volatile uint32_t *cmp_lo = (uint32_t *)(CLINT_MTIMECMP + 0);     // NEW
    volatile uint32_t *cmp_hi = (uint32_t *)(CLINT_MTIMECMP + 4);     // NEW
    uint32_t lo = (uint32_t)(next_interrupt_time & 0xFFFFFFFFu);      // NEW
    uint32_t hi = (uint32_t)(next_interrupt_time >> 32);              // NEW

    *cmp_hi = 0xFFFFFFFFu;   // push compare far in future → MTIP deasserts    // NEW
    *cmp_lo = lo;            // program low word                                 // NEW
    *cmp_hi = hi;            // program high word                                // NEW

    write_mie(mie_save);     // restore MTIE (and any other mie bits)            // NEW
    // --- END: minimal safe update ---

    // 5. Finally, increment our system's tick counter
    if ((g_ticks % 100) == 0) {
        // optional while debugging: printing in ISR can stall; comment out if needed
        // uart_puts("\ninterrupt\n");
    }
    g_ticks++;
}

uint64_t* trap_handler(uint64_t* old_sp) {
    uint64_t cause = read_mcause();

    // Check if it's a timer interrupt
    if ((cause & (1ULL << 63)) && (cause & 0xFF) == 7) {
        
        //reset timer + counter++
        handle_timer_interrupt();

        //save sp of previosuly executing task
        tasks[current_task_idx].stack_pointer = old_sp;

        //run scheduler to determine next task
        scheduler(); 
        /*in the furtuer should either return sp or error code*/

        return tasks[current_task_idx].stack_pointer;
    }
    uart_puts("error with trap\n");
    return old_sp; //shouldnt hit ever for now
}
