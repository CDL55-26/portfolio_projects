// util.c
#include <stdint.h>

extern uint8_t __bss_start, __bss_end; //read these symbols from the ld file

void zero_bss(void) { //getting called from start.S
    uint8_t *p = &__bss_start; //zero each byte from start to end of the global data
    while (p < &__bss_end) {
        *p++ = 0;
    }
}

