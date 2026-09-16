// Low-level HD44780 access.  The Pico build implements this for the Freenove
// I2C LCD1602's PCF8574 backpack (src/pico/lcd_pcf8574.c); the PC simulator
// stubs it out.
#pragma once
#include <stdbool.h>
#include <stdint.h>

bool    lcd_hw_init(void);             // false if no display answers; may be called again
void    lcd_hw_diagnose(void);         // print the I2C line levels and the devices that answer
uint8_t lcd_hw_address(void);          // I2C address found by lcd_hw_init, 0 if none
void    lcd_hw_command(uint8_t cmd);   // instruction byte (RS low)
void    lcd_hw_write(uint8_t data);    // character byte (RS high)
void    lcd_hw_backlight(bool on);
