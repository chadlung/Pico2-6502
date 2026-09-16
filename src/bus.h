// The emulated machine's address space: 64K of RAM with a page of
// memory-mapped peripherals at $F000-$F0FF.
#pragma once
#include <stdbool.h>
#include <stdint.h>

#define IO_PAGE          0xF000
#define IO_LCD_CONTROL   0xF000   // W: $01 clear display + home cursor, $02 home cursor
#define IO_LCD_DATA      0xF001   // W: print character and advance the cursor
#define IO_LCD_ROW       0xF002   // RW: cursor row 0-1
#define IO_LCD_COL       0xF003   // RW: cursor column 0-15
#define IO_LCD_BACKLIGHT 0xF004   // RW: 0 = off, anything else = on
#define IO_SERIAL_DATA   0xF010   // W: send byte over USB serial; R: next received byte (0 if none)
#define IO_SERIAL_STATUS 0xF011   // R: bit 7 set when a received byte is waiting

#define VEC_NMI   0xFFFA
#define VEC_RESET 0xFFFC
#define VEC_IRQ   0xFFFE

extern uint8_t memory[65536];

void    bus_reset(void);                // zero all RAM
uint8_t bus_peek(uint16_t addr);        // read without side effects, for the monitor
void    serial_rx_push(uint8_t c);      // queue a byte for the 6502 (dropped if full)
void    serial_rx_clear(void);

static inline bool bus_is_io(uint16_t addr) { return (addr & 0xFF00) == IO_PAGE; }
