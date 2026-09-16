#include <stdio.h>
#include <string.h>

#include "bus.h"
#include "fake6502.h"
#include "lcd.h"

uint8_t memory[65536];

#define RX_SIZE 64

static uint8_t rx_buf[RX_SIZE];
static uint8_t rx_head, rx_tail;   // equal when empty

void bus_reset(void) {
    memset(memory, 0, sizeof memory);
    serial_rx_clear();
}

void serial_rx_push(uint8_t c) {
    uint8_t next = (rx_head + 1) % RX_SIZE;
    if (next == rx_tail) return;   // overrun, like a real UART
    rx_buf[rx_head] = c;
    rx_head = next;
}

void serial_rx_clear(void) { rx_head = rx_tail = 0; }

static uint8_t io_read(uint16_t addr, bool consume) {
    switch (addr) {
    case IO_LCD_ROW:       return lcd_row();
    case IO_LCD_COL:       return lcd_col();
    case IO_LCD_BACKLIGHT: return lcd_backlight_on();
    case IO_SERIAL_STATUS: return rx_head != rx_tail ? 0x80 : 0x00;
    case IO_SERIAL_DATA: {
        if (rx_head == rx_tail) return 0;
        uint8_t c = rx_buf[rx_tail];
        if (consume) rx_tail = (rx_tail + 1) % RX_SIZE;
        return c;
    }
    default:
        return 0;
    }
}

static void io_write(uint16_t addr, uint8_t value) {
    switch (addr) {
    case IO_LCD_CONTROL:
        if (value & 0x01) lcd_clear();
        else if (value & 0x02) lcd_home();
        break;
    case IO_LCD_DATA:      lcd_putc(value); break;
    case IO_LCD_ROW:       lcd_set_row(value); break;
    case IO_LCD_COL:       lcd_set_col(value); break;
    case IO_LCD_BACKLIGHT: lcd_backlight(value != 0); break;
    case IO_SERIAL_DATA:   putchar(value); break;
    }
}

// Fake6502 calls these for every memory access.

uint8_t read6502(uint16_t addr) {
    return bus_is_io(addr) ? io_read(addr, true) : memory[addr];
}

void write6502(uint16_t addr, uint8_t value) {
    if (bus_is_io(addr)) io_write(addr, value);
    else memory[addr] = value;
}

uint8_t bus_peek(uint16_t addr) {
    return bus_is_io(addr) ? io_read(addr, false) : memory[addr];
}
