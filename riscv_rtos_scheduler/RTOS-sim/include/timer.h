//timer.h
#pragma once
#define CLINT_BASE 0x2000000

// Register offsets from the base address
#define CLINT_MTIMECMP (CLINT_BASE + 0x4000)
#define CLINT_MTIME    (CLINT_BASE + 0xBFF8)

void timer_init(void);