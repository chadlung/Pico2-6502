// HD44780 16x2 LCD on a PCF8574 I2C backpack, as on the Freenove I2C LCD1602
// module.  PCF8574T boards answer at 0x27 and PCF8574AT boards at 0x3F;
// soldering the A0-A2 pads moves them within 0x20-0x27 or 0x38-0x3F, so all
// of those addresses are tried.  Bit assignments follow Freenove's I2C_LCD.py
// and the LiquidCrystal_I2C library.

#include <stdio.h>

#include "hardware/i2c.h"
#include "pico/stdlib.h"

#include "lcd_hw.h"

#define LCD_I2C          i2c0
#define LCD_SDA_PIN      4          // GP4, physical pin 6
#define LCD_SCL_PIN      5          // GP5, physical pin 7
#define LCD_I2C_BAUD     100000     // the PCF8574 is a 100 kHz part
#define I2C_TIMEOUT_US   20000

// PCF8574 outputs P0-P7 on the backpack.
#define BIT_RS           0x01
#define BIT_RW           0x02       // kept low: the display is only written
#define BIT_E            0x04
#define BIT_BACKLIGHT    0x08
#define DATA_SHIFT       4          // P4-P7 = D4-D7

static uint8_t address;             // 0 until a backpack answers
static uint8_t outputs;             // the PCF8574 has one register: its output pins

static bool pcf_write(uint8_t addr, uint8_t value) {
    return i2c_write_timeout_us(LCD_I2C, addr, &value, 1, false, I2C_TIMEOUT_US) == 1;
}

static void set_outputs(uint8_t value) {
    outputs = value;
    pcf_write(address, value);
}

// Put a nibble on D4-D7 and pulse E; the display latches on the falling edge.
// Each I2C transaction takes far longer than the HD44780's minimum timings.
static void write_nibble(uint8_t nibble) {
    uint8_t out = (outputs & (BIT_RS | BIT_BACKLIGHT)) | ((nibble & 0x0F) << DATA_SHIFT);
    set_outputs(out);
    set_outputs(out | BIT_E);
    set_outputs(out);
}

static void write_byte(uint8_t value, bool rs) {
    outputs = rs ? outputs | BIT_RS : outputs & ~BIT_RS;
    write_nibble(value >> 4);
    write_nibble(value);
}

static uint8_t find_backpack(void) {
    static const uint8_t candidates[] = {
        0x27, 0x3F,                                 // factory addresses
        0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26,   // PCF8574T with address pads soldered
        0x38, 0x39, 0x3A, 0x3B, 0x3C, 0x3D, 0x3E,   // PCF8574AT with address pads soldered
    };
    for (unsigned i = 0; i < sizeof candidates; i++) {
        if (pcf_write(candidates[i], 0x00)) return candidates[i];
    }
    return 0;
}

bool lcd_hw_init(void) {
    // The module has its own pull-up resistors, so the Pico's are left off.
    i2c_init(LCD_I2C, LCD_I2C_BAUD);
    gpio_set_function(LCD_SDA_PIN, GPIO_FUNC_I2C);
    gpio_set_function(LCD_SCL_PIN, GPIO_FUNC_I2C);

    sleep_ms(50);                   // HD44780 needs >40 ms after power-up

    address = find_backpack();
    if (!address) return false;
    set_outputs(0);

    // 4-bit initialisation sequence (HD44780 datasheet, figure 24).
    write_nibble(0x03);
    sleep_us(4500);
    write_nibble(0x03);
    sleep_us(4500);
    write_nibble(0x03);
    sleep_us(150);
    write_nibble(0x02);

    lcd_hw_command(0x28);           // function set: 4-bit, 2 lines, 5x8 font
    lcd_hw_command(0x0C);           // display on, cursor off, blink off
    lcd_hw_command(0x06);           // entry mode: cursor moves right, no shift
    lcd_hw_command(0x01);           // clear
    return true;
}

// An idle I2C line is held high by the module's pull-up resistors.  For a line
// that reads low, switching on the Pico's weak pull-up tells two faults apart.
static void report_line(const char *name, unsigned pin) {
    if (gpio_get(pin)) {
        printf("%s (GP%u) is high, as it should be\n", name, pin);
        return;
    }
    gpio_pull_up(pin);
    sleep_ms(5);
    bool rises = gpio_get(pin);
    gpio_pull_down(pin);            // back to the power-on default
    printf("%s (GP%u) is LOW. With the Pico's pull-up it reads %s\n", name, pin,
           rises ? "high: the pin is not connected to the module,\n"
                   "  or the module has no pull-up resistors"
                 : "LOW: something holds it down, usually a module\n"
                   "  without power (check VCC and GND) or a short to ground");
}

void lcd_hw_diagnose(void) {
    report_line("SDA", LCD_SDA_PIN);
    report_line("SCL", LCD_SCL_PIN);
    printf("I2C devices answering:");
    int found = 0;
    for (uint8_t addr = 0x08; addr < 0x78; addr++) {
        uint8_t byte;
        if (i2c_read_timeout_us(LCD_I2C, addr, &byte, 1, false, I2C_TIMEOUT_US) == 1) {
            printf(" %02X", addr);
            found++;
        }
    }
    printf(found ? "\n" : " none\n");
}

uint8_t lcd_hw_address(void) {
    return address;
}

void lcd_hw_command(uint8_t cmd) {
    write_byte(cmd, false);
    if (cmd <= 0x03) sleep_ms(2);   // clear and home take up to 1.52 ms
}

void lcd_hw_write(uint8_t data) {
    write_byte(data, true);
}

void lcd_hw_backlight(bool on) {
    set_outputs(on ? outputs | BIT_BACKLIGHT : outputs & ~BIT_BACKLIGHT);
}
