#include <stdint.h>
#include "riscv.h"

/*
 * Memory-Mapped I/O (MMIO) Address for the UART Controller.
 * In QEMU's 'virt' machine, the ns16550 UART is located at this base address.
 */
#define UART0_BASE 0x10000000UL

/*
 * ====================================================================
 * UART Register Offsets and Bit Field Definitions
 * ====================================================================
 * These are offsets from UART0_BASE.
 */

/* Register Offsets */
#define UART_RBR 0 /* Receiver Buffer Register (read-only) */
#define UART_THR 0 /* Transmitter Holding Register (write-only) */
#define UART_IER 1 /* Interrupt Enable Register */
#define UART_FCR 2 /* FIFO Control Register (write-only) */
#define UART_LCR 3 /* Line Control Register */
#define UART_LSR 5 /* Line Status Register (read-only) */

/*
 * When the DLAB bit in LCR is set, these two registers become accessible
 * at offsets 0 and 1, temporarily replacing RBR/THR and IER.
 */
#define UART_DLL 0 /* Divisor Latch LSB */
#define UART_DLM 1 /* Divisor Latch MSB */

/* Bit Field Masks for Line Control Register (LCR) */
#define LCR_DLAB 0x80 /* Divisor Latch Access Bit. Set to 1 to access DLL/DLM. */
#define LCR_8N1  0x03 /* Sets the line protocol to 8 data bits, No parity, 1 stop bit. */

/* Bit Field Masks for FIFO Control Register (FCR) */
#define FCR_FIFO_EN  0x01 /* Enable the FIFOs for receive and transmit. */
#define FCR_CLEAR_RX 0x02 /* Clears all bytes in the receiver FIFO. */
#define FCR_CLEAR_TX 0x04 /* Clears all bytes in the transmitter FIFO. */

/* Bit Field Masks for Line Status Register (LSR) */
#define LSR_THRE 0x20 /* Transmitter Holding Register Empty. Set when ready for the next character. */

/*
 * ====================================================================
 * Low-Level Memory Access Helpers
 * ====================================================================
 */

/**
 * @brief Writes an 8-bit value to a specific memory-mapped address.
 *
 * @param addr The 64-bit memory address.
 * @param value The 8-bit value to write.
 *
 * The 'volatile' keyword is crucial here. It tells the compiler not to
 * optimize this memory access away, because the value at this address
 * can be changed by hardware at any time.
 */
static inline void mmio_write8(uint64_t addr, uint8_t value) {
    *(volatile uint8_t*)addr = value;
}

/**
 * @brief Reads an 8-bit value from a specific memory-mapped address.
 * @param addr The 64-bit memory address.
 * @return The 8-bit value read from the address.
 */
static inline uint8_t mmio_read8(uint64_t addr) {
    return *(volatile uint8_t*)addr;
}

/**
 * @brief Writes a value to a specific UART register.
 * @param reg_offset The offset of the register from UART0_BASE.
 * @param value The value to write.
 */
static inline void uart_write_reg(int reg_offset, uint8_t value) {
    mmio_write8(UART0_BASE + reg_offset, value);
}

/**
 * @brief Reads a value from a specific UART register.
 * @param reg_offset The offset of the register from UART0_BASE.
 * @return The value read from the register.
 */
static inline uint8_t uart_read_reg(int reg_offset) {
    return mmio_read8(UART0_BASE + reg_offset);
}

/*
 * ====================================================================
 * Public UART Functions
 * ====================================================================
 */

/**
 * @brief Initializes the UART controller.
 */
void uart_init(void) {
    // 1. Disable all interrupts from the UART.
    uart_write_reg(UART_IER, 0x00);

    // 2. Set the Divisor Latch Access Bit (DLAB) to 1.
    // This allows access to the Divisor Latch registers (DLL, DLM)
    // to configure the baud rate.
    uart_write_reg(UART_LCR, LCR_DLAB);

    // 3. Set the baud rate divisor.
    // Baud Rate = Clock Speed / (16 * Divisor).
    // A divisor of 1 gives the fastest rate, which is fine for QEMU.
    uart_write_reg(UART_DLL, 0x01); // Divisor Latch LSB
    uart_write_reg(UART_DLM, 0x00); // Divisor Latch MSB

    // 4. Configure the line protocol to 8 data bits, no parity, 1 stop bit (8N1).
    // Writing to LCR also clears DLAB, making THR/RBR accessible again.
    uart_write_reg(UART_LCR, LCR_8N1);

    // 5. Enable and clear the transmit/receive FIFOs.
    uart_write_reg(UART_FCR, FCR_FIFO_EN | FCR_CLEAR_RX | FCR_CLEAR_TX);
}

/**
 * @brief Transmits a single character over UART.
 *
 * This function busy-waits until the transmitter is ready.
 * @param c The character to send.
 */
void uart_putc(char c) {
    // The Line Status Register (LSR) indicates the UART's state.
    // We check the LSR_THRE bit, which is set when the Transmit Holding
    // Register (THR) is empty and ready for a new character.
    // This loop will continue until the transmitter is free.
    while ((uart_read_reg(UART_LSR) & LSR_THRE) == 0) {
        // Do nothing, just wait.
    }

    // The transmitter is ready, so write the character to the THR.
    uart_write_reg(UART_THR, (uint8_t)c);
}

/**
 * @brief Transmits a null-terminated string over UART.
 *
 * Translates newline characters '\n' into carriage return + line feed "\r\n"
 * for compatibility with most terminals.
 * @param s A pointer to the string.
 */
void uart_puts(const char *s) {
    // Loop through the string until we hit the null terminator ('\0').
    disable_global_interrupts();
    while (*s != '\0') {
        // For terminals, a newline ('\n') often needs to be sent as
        // a carriage return ('\r') followed by a line feed ('\n').
        if (*s == '\n') {
            uart_putc('\r');
        }
        uart_putc(*s);
        s++; // Move to the next character.
    }
    enable_global_interrupts();
}
