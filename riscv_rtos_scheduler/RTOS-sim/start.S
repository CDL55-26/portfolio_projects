    .section .text.entry
    .globl _start           #make the start label globally available
    .extern _stack_top      #read in the stack label from the linker
    .extern zero_bss        #make the zero_bss C function avail
    .extern main
        

_start:
    la   sp, _stack_top       #load RAM_start + RAM_length into sp
    call zero_bss             #psuedo instruction to call c function
    call main
1: #risc-v syntax for creating short loops. Can j 1b to go to most recent 1:
    
    wfi #wait for interrupt <- spin gracefully
    j    1b

