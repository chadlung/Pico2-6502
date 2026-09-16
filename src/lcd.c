#include <string.h>

#include "lcd.h"
#include "lcd_hw.h"

#define HD44780_CLEAR     0x01
#define HD44780_SET_DDRAM 0x80

static const uint8_t row_start[LCD_ROWS] = { 0x00, 0x40 };

static bool present;
static bool backlight;
static uint8_t cur_row, cur_col;
static bool cursor_moved;   // the display's cursor must be repositioned before the next character
static uint8_t mirror[LCD_ROWS][LCD_COLS];

void lcd_init(void) {
    present = lcd_hw_init();
    lcd_backlight(true);
    lcd_clear();
}

void lcd_diagnose(void) { lcd_hw_diagnose(); }

bool lcd_present(void) { return present; }

uint8_t lcd_address(void) { return lcd_hw_address(); }

void lcd_clear(void) {
    memset(mirror, ' ', sizeof mirror);
    cur_row = cur_col = 0;
    cursor_moved = false;
    if (present) lcd_hw_command(HD44780_CLEAR);   // also homes the cursor
}

void lcd_home(void) {
    cur_row = cur_col = 0;
    cursor_moved = true;
}

static void next_row(void) {
    cur_col = 0;
    cur_row = (cur_row + 1) % LCD_ROWS;
    cursor_moved = true;
}

void lcd_putc(uint8_t c) {
    if (c == '\r') {
        cur_col = 0;
        cursor_moved = true;
        return;
    }
    if (c == '\n') {
        next_row();
        return;
    }
    if (present) {
        if (cursor_moved) lcd_hw_command(HD44780_SET_DDRAM | (row_start[cur_row] + cur_col));
        lcd_hw_write(c);
    }
    cursor_moved = false;
    mirror[cur_row][cur_col] = c;
    if (++cur_col == LCD_COLS) next_row();
}

void lcd_set_row(uint8_t row) {
    cur_row = row % LCD_ROWS;
    cursor_moved = true;
}

void lcd_set_col(uint8_t col) {
    cur_col = col % LCD_COLS;
    cursor_moved = true;
}

uint8_t lcd_row(void) { return cur_row; }
uint8_t lcd_col(void) { return cur_col; }

void lcd_backlight(bool on) {
    backlight = on;
    if (present) lcd_hw_backlight(on);
}

bool lcd_backlight_on(void) { return backlight; }

uint8_t lcd_char_at(uint8_t row, uint8_t col) {
    return mirror[row % LCD_ROWS][col % LCD_COLS];
}
