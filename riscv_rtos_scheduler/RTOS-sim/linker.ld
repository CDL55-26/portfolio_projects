/*linker script for GNU linker */

/*Give name to memory block with rwx permissions*/
MEMORY {
    RAM (rwx) : ORIGIN = 0x80000000, LENGTH = 128M
}

/*First instruction executed*/
ENTRY(_start)

SECTIONS {
  . = ORIGIN(RAM);

  /* Put the very first instructions here */
  .text : {
    KEEP(*(.text.entry)) /*Place .text code (instructions) into RAM, starting with the entry code*/
    *(.text*)
  } > RAM

  .rodata : { *(.rodata*) } > RAM /*Do same with read only data and data*/
  .data   : { *(.data*)   } > RAM

  /* BSS is zero-initialized RAM that we clear at boot */
  .bss : {
    __bss_start = .;
    *(.bss*)
    *(COMMON)
    __bss_end = .;
  } > RAM

  /* Put the stack at the top of RAM (grow downward) */
  . = ALIGN(16);

  /*label name for top of stack*/
  _stack_top = ORIGIN(RAM) + LENGTH(RAM);
}


