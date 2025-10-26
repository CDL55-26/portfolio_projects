#include <stdint.h>

//def for _trap 
extern void _trap(void);
extern void _scheduler_init(void);

//Inline functions
static inline void set_mtvec(uint64_t value) {
    asm volatile("csrw mtvec, %0" : : "r"(value));
}

static inline void enable_global_interrupts(void) {
    asm volatile("csrs mstatus, %0" : : "r"((uint64_t)1 << 3));
}
static inline void disable_global_interrupts(void) {
    asm volatile("csrc mstatus, %0" : : "r"((uint64_t)1 << 3));
}

static inline uint64_t read_mcause(void) {
    uint64_t value;
    asm volatile("csrr %0, mcause" : "=r"(value));
    return value;
}

static inline void enable_timer_interrupts(void) {
    asm volatile("csrs mie, %0" : : "r"((uint64_t)1 << 7));
}

static inline uint64_t read_mie(void) {
    uint64_t v; __asm__ volatile ("csrr %0, mie" : "=r"(v));
    return v;
}
static inline void write_mie(uint64_t v) {
    __asm__ volatile ("csrw mie, %0" :: "r"(v));
}
