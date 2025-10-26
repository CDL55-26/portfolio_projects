#include <stdint.h>

#define CLINT_BASE 0x2000000

// Register offsets from the base address
#define CLINT_MTIMECMP (CLINT_BASE + 0x4000)
#define CLINT_MTIME    (CLINT_BASE + 0xBFF8)

void handle_timer_interrupt(void);
uint64_t* trap_handler(uint64_t* old_sp);
