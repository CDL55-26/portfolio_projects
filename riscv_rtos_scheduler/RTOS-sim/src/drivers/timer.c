#include <stdint.h>
#include "riscv.h" 
#include "timer.h"

#define TIMER_INTERVAL 100000




void timer_init(void) {
    volatile uint64_t *mtimecmp = (uint64_t *)CLINT_MTIMECMP;
    volatile uint64_t *mtime = (uint64_t *)CLINT_MTIME;
    uint64_t current_mtime = *mtime;

    //calculate time for first int
    uint64_t first_interrupt_time = current_mtime + TIMER_INTERVAL;

    //write to mtimecmp
    *mtimecmp = first_interrupt_time;

   
}