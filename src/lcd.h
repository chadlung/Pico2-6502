// 16x2 character display as seen by 6502 programs.
//
// The firmware tracks the cursor itself so text wraps from the end of one row
// to the start of the next (the HD44780 does not do this on its own).  Every
// character also lands in a text mirror, which lets the monitor show the
// screen and lets the simulator work with no display at all.
#pragma once
#include <stdbool.h>
#include <stdint.h>

#define LCD_COLS 16
#define LCD_ROWS 2

void    lcd_init(void);                 // (re)detect the display and clear it
void    lcd_diagnose(void);             // print I2C details for troubleshooting
bool    lcd_present(void);
uint8_t lcd_address(void);              // I2C address of the display, 0 if none
void    lcd_clear(void);                // blank the screen, cursor to row 0 col 0
void    lcd_home(void);                 // cursor to row 0 col 0
void    lcd_putc(uint8_t c);            // print and advance; CR and LF move the cursor
void    lcd_set_row(uint8_t row);       // taken modulo LCD_ROWS
void    lcd_set_col(uint8_t col);       // taken modulo LCD_COLS
uint8_t lcd_row(void);
uint8_t lcd_col(void);
void    lcd_backlight(bool on);
bool    lcd_backlight_on(void);
uint8_t lcd_char_at(uint8_t row, uint8_t col);
