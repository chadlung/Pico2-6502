#include <ctype.h>
#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

#include "bus.h"
#include "disasm.h"
#include "fake6502.h"
#include "lcd.h"
#include "monitor.h"
#include "platform.h"

#define LINE_LEN          80
#define CTRL_C            0x03
#define LOAD_TIMEOUT_US   2000000   // give up on an upload after this long without a byte
#define IDLE_POLL_US      10000
#define BATCH_US          10000     // emulated time per batch when speed-limited
#define UNLIMITED_STEPS   20000     // instructions per batch when not speed-limited

static char line[LINE_LEN + 1];
static unsigned line_len;
static bool last_was_cr;

static bool running;
static unsigned speed_khz = 1000;   // a stock 6502 runs at 1 MHz; 0 = as fast as possible
static long breakpoint = -1;
static bool skip_breakpoint;        // lets 'c' continue from the breakpoint it stopped at
static uint64_t throttle_start_us, throttle_cycles;
static uint64_t run_start_us, run_cycles;
static long unassemble_next = -1;   // where a bare 'u' continues; -1 = at PC

static void prompt(void) {
    printf("> ");
    fflush(stdout);
}

// ------------------ CPU state -----------------------------------------------

static uint16_t vector(uint16_t at) {
    return bus_peek(at) | bus_peek((uint16_t)(at + 1)) << 8;
}

// Disassembles the instruction at addr into text; returns its length.
static int disassemble(uint16_t addr, char *text, size_t size) {
    uint8_t bytes[3] = { bus_peek(addr), bus_peek((uint16_t)(addr + 1)), bus_peek((uint16_t)(addr + 2)) };
    return disasm(addr, bytes, text, size);
}

static void show_registers(void) {
    static const char names[] = "NV-BDIZC";
    char text[DISASM_TEXT_SIZE];
    uint8_t p = getP();
    printf("PC=%04X A=%02X X=%02X Y=%02X SP=%02X P=", PC, A, X, Y, SP);
    for (int i = 0; i < 8; i++) putchar(p & (0x80 >> i) ? names[i] : '.');
    disassemble(PC, text, sizeof text);
    printf("  %s\n", text);
    unassemble_next = -1;
}

static void begin_run(void) {
    running = true;
    throttle_start_us = run_start_us = plat_time_us();
    throttle_cycles = run_cycles = 0;
    plat_led(true);
}

static void stop(const char *why) {
    running = false;
    plat_led(false);
    uint64_t us = plat_time_us() - run_start_us;
    printf("\n%s at $%04X after %llu cycles", why, PC, (unsigned long long)run_cycles);
    if (us >= 100000) printf(" (%llu kHz)", (unsigned long long)(run_cycles * 1000 / us));
    printf("\n");
    show_registers();
    prompt();
}

static void run_from_reset(void) {
    reset6502();
    serial_rx_clear();
    skip_breakpoint = false;
    printf("OK running from $%04X (Ctrl-C stops)\n", PC);
    begin_run();
}

void monitor_go(uint16_t addr) {
    memory[VEC_RESET] = addr & 0xFF;
    memory[VEC_RESET + 1] = addr >> 8;
    run_from_reset();
}

// Runs one slice of the program, then sleeps if it is ahead of the speed limit.
static void run_batch(void) {
    uint64_t budget = speed_khz ? (uint64_t)speed_khz * BATCH_US / 1000 : UINT64_MAX;
    unsigned steps = speed_khz ? UINT_MAX : UNLIMITED_STEPS;
    const char *why = NULL;
    uint64_t spent = 0;

    for (unsigned n = 0; n < steps && spent < budget; n++) {
        uint16_t pc = PC;
        if (pc == breakpoint && !skip_breakpoint) {
            why = "Breakpoint";
            break;
        }
        skip_breakpoint = false;
        if (bus_peek(pc) == 0x00 && vector(VEC_IRQ) == 0x0000) {
            why = "BRK (no IRQ/BRK vector set)";
            break;
        }
        spent += step6502();
        if (PC == pc) {   // e.g. "done: jmp done" -- nothing can ever change
            why = "Halted in endless loop";
            break;
        }
    }
    run_cycles += spent;
    fflush(stdout);
    if (why) {
        stop(why);
        return;
    }

    if (speed_khz) {
        throttle_cycles += spent;
        uint64_t due = throttle_start_us + throttle_cycles * 1000 / speed_khz;
        uint64_t now = plat_time_us();
        if (due > now) {
            plat_sleep_us(due - now);
        } else if (now - due > 100000) {
            // Fell behind (slow LCD writes); start over rather than race to catch up.
            throttle_start_us = now;
            throttle_cycles = 0;
        }
    }
}

// ------------------ Commands ------------------------------------------------

// Reads the next number from *s; an optional leading '$' is allowed.
static bool parse_num(char **s, unsigned *out, int base) {
    while (**s == ' ') (*s)++;
    if (**s == '$') (*s)++;
    char *end;
    unsigned long v = strtoul(*s, &end, base);
    if (end == *s || !isxdigit((unsigned char)**s)) return false;
    *s = end;
    *out = v > UINT_MAX ? UINT_MAX : (unsigned)v;
    return true;
}

static void help(void) {
    printf("Numbers are hex unless noted.\n"
           "  r                 show CPU registers\n"
           "  m ADDR [LEN]      dump memory\n"
           "  w ADDR BB [BB..]  write bytes (I/O registers work too: w F001 41)\n"
           "  g [ADDR]          reset CPU and run from ADDR (becomes the reset vector),\n"
           "                    or from the current reset vector\n"
           "  c                 continue from the current PC\n"
           "  s [N]             single-step N instructions\n"
           "  u [ADDR [N]]      disassemble N instructions (default 10); u alone continues\n"
           "  x                 reset CPU without running\n"
           "  b [ADDR | -]      show, set or clear the breakpoint\n"
           "  t [KHZ]           show or set the speed limit, decimal kHz (0 = unlimited)\n"
           "  d                 show what is on the LCD\n"
           "  i                 check the display's I2C bus and look for it again\n"
           "  l ADDR LEN CRC    receive LEN raw bytes (used by tools/upload.py)\n"
           "  Ctrl-C            stop the running program\n");
}

static void cmd_memory(char *s) {
    unsigned addr, len = 64;
    if (!parse_num(&s, &addr, 16) || addr > 0xFFFF) {
        printf("ERR usage: m ADDR [LEN]\n");
        return;
    }
    parse_num(&s, &len, 16);
    for (unsigned i = 0; i < len && addr + i <= 0xFFFF; i += 16) {
        unsigned n = 16;
        if (len - i < n) n = len - i;
        if (0x10000 - (addr + i) < n) n = 0x10000 - (addr + i);
        printf("%04X:", addr + i);
        for (unsigned j = 0; j < 16; j++) {
            if (j < n) printf(" %02X", bus_peek(addr + i + j));
            else printf("   ");
        }
        printf("  ");
        for (unsigned j = 0; j < n; j++) {
            uint8_t b = bus_peek(addr + i + j);
            putchar(b >= 0x20 && b < 0x7F ? b : '.');
        }
        putchar('\n');
    }
}

static void cmd_write(char *s) {
    unsigned addr, value, count = 0;
    if (!parse_num(&s, &addr, 16) || addr > 0xFFFF) {
        printf("ERR usage: w ADDR BB [BB..]\n");
        return;
    }
    while (parse_num(&s, &value, 16)) {
        if (value > 0xFF) {
            printf("ERR byte value %X too large\n", value);
            return;
        }
        write6502((uint16_t)(addr + count++), value);
    }
    printf("OK wrote %u byte%s\n", count, count == 1 ? "" : "s");
}

static void cmd_go(char *s) {
    unsigned addr;
    if (!parse_num(&s, &addr, 16)) run_from_reset();
    else if (addr > 0xFFFF) printf("ERR address out of range\n");
    else monitor_go(addr);
}

static void cmd_continue(void) {
    printf("OK continuing at $%04X (Ctrl-C stops)\n", PC);
    skip_breakpoint = true;
    begin_run();
}

static void cmd_step(char *s) {
    unsigned n = 1;
    parse_num(&s, &n, 16);
    for (unsigned i = 0; i < n; i++) {
        step6502();
        show_registers();
        if (plat_getc(0) == CTRL_C) break;
    }
}

static void cmd_unassemble(char *s) {
    unsigned addr, count = 0x10;
    if (parse_num(&s, &addr, 16)) {
        if (addr > 0xFFFF) {
            printf("ERR usage: u [ADDR [N]]\n");
            return;
        }
        parse_num(&s, &count, 16);
    } else {
        addr = unassemble_next < 0 ? PC : (unsigned)unassemble_next;
    }
    char text[DISASM_TEXT_SIZE];
    for (unsigned i = 0; i < count; i++) {
        int len = disassemble(addr, text, sizeof text);
        printf("%04X  %s\n", addr, text);
        addr = (addr + len) & 0xFFFF;
    }
    unassemble_next = addr;
}

static void cmd_breakpoint(char *s) {
    unsigned addr;
    while (*s == ' ') s++;
    if (*s == '-') {
        breakpoint = -1;
        printf("OK breakpoint cleared\n");
    } else if (parse_num(&s, &addr, 16) && addr <= 0xFFFF) {
        breakpoint = addr;
        printf("OK breakpoint at $%04X\n", addr);
    } else if (breakpoint < 0) {
        printf("No breakpoint set\n");
    } else {
        printf("Breakpoint at $%04X\n", (unsigned)breakpoint);
    }
}

static void cmd_speed(char *s) {
    unsigned khz;
    if (parse_num(&s, &khz, 10)) speed_khz = khz;
    if (speed_khz) printf("Speed limit %u kHz\n", speed_khz);
    else printf("No speed limit\n");
}

static void cmd_i2c(void) {
    lcd_diagnose();
    lcd_init();
    if (lcd_present()) printf("OK display found at I2C address %02X\n", lcd_address());
    else printf("ERR display not found\n");
}

static void cmd_display(void) {
    printf("+----------------+\n");
    for (uint8_t r = 0; r < LCD_ROWS; r++) {
        putchar('|');
        for (uint8_t c = 0; c < LCD_COLS; c++) {
            uint8_t ch = lcd_char_at(r, c);
            putchar(ch >= 0x20 && ch < 0x7F ? ch : '.');
        }
        printf("|\n");
    }
    printf("+----------------+\n");
    printf("Cursor row %u col %u, backlight %s, ", lcd_row(), lcd_col(),
           lcd_backlight_on() ? "on" : "off");
    if (lcd_present()) printf("display at I2C address %02X\n", lcd_address());
    else printf("display NOT DETECTED\n");
}

// CRC-16/CCITT-FALSE, the same as Python's binascii.crc_hqx(data, 0xFFFF).
static uint16_t crc16_update(uint16_t crc, uint8_t b) {
    crc ^= (uint16_t)b << 8;
    for (int i = 0; i < 8; i++) crc = crc & 0x8000 ? (crc << 1) ^ 0x1021 : crc << 1;
    return crc;
}

// l ADDR LEN CRC: reply READY, receive LEN raw bytes into memory, then reply
// OK or ERR.  A failed load can leave partial data behind.
static void cmd_load(char *s) {
    unsigned addr, len, crc;
    if (!parse_num(&s, &addr, 16) || !parse_num(&s, &len, 16) || !parse_num(&s, &crc, 16)) {
        printf("ERR usage: l ADDR LEN CRC\n");
        return;
    }
    if (addr > 0xFFFF || len == 0 || len > 0x10000 - addr) {
        printf("ERR load must lie within $0000-$FFFF\n");
        return;
    }
    if (addr < IO_PAGE + 0x100 && addr + len > IO_PAGE) {
        printf("ERR load overlaps the I/O page $F000-$F0FF\n");
        return;
    }
    printf("READY\n");
    fflush(stdout);

    uint16_t sum = 0xFFFF;
    for (unsigned i = 0; i < len; i++) {
        int c = plat_getc(LOAD_TIMEOUT_US);
        if (c < 0) {
            printf("ERR timeout after %u of %u bytes\n", i, len);
            return;
        }
        memory[addr + i] = c;
        sum = crc16_update(sum, c);
    }
    if (sum != crc) printf("ERR checksum %04X, expected %04X\n", sum, crc);
    else printf("OK loaded %u bytes at $%04X-$%04X\n", len, addr, addr + len - 1);
}

static void execute(char *s) {
    while (*s == ' ') s++;
    char cmd = tolower((unsigned char)*s);
    if (!cmd) return;
    s++;
    switch (cmd) {
    case 'h': case '?': help(); break;
    case 'r': show_registers(); break;
    case 'm': cmd_memory(s); break;
    case 'w': cmd_write(s); break;
    case 'g': cmd_go(s); break;
    case 'c': cmd_continue(); break;
    case 's': cmd_step(s); break;
    case 'u': cmd_unassemble(s); break;
    case 'x': reset6502(); show_registers(); break;
    case 'b': cmd_breakpoint(s); break;
    case 't': cmd_speed(s); break;
    case 'd': cmd_display(); break;
    case 'i': cmd_i2c(); break;
    case 'l': cmd_load(s); break;
    default: printf("ERR unknown command, h for help\n"); break;
    }
}

// ------------------ Input ---------------------------------------------------

static void handle_key(int c) {
    if (c == '\r' || c == '\n') {
        putchar('\n');
        line[line_len] = '\0';
        line_len = 0;
        execute(line);
        if (!running) prompt();
    } else if (c == CTRL_C) {
        line_len = 0;
        printf("^C\n");
        prompt();
    } else if (c == 0x08 || c == 0x7F) {
        if (line_len) {
            line_len--;
            printf("\b \b");
            fflush(stdout);
        }
    } else if (c >= 0x20 && c < 0x7F && line_len < LINE_LEN) {
        line[line_len++] = c;
        putchar(c);
        fflush(stdout);
    }
}

void monitor_init(void) {
    printf("\nPico 6502: Fake6502 CPU, 64K RAM, 16x2 LCD%s\nType h for help.\n",
           lcd_present() ? "" : " (display not detected)");
}

void monitor_poll(void) {
    int c = plat_getc(running ? 0 : IDLE_POLL_US);
    while (c >= 0) {
        // Swallow the LF of a CR LF pair so it neither repeats a command nor
        // reaches a program that was just started.
        bool lf_after_cr = c == '\n' && last_was_cr;
        last_was_cr = c == '\r';
        if (lf_after_cr) {
            // nothing
        } else if (!running) {
            handle_key(c);
        } else if (c == CTRL_C) {
            stop("Stopped");
        } else {
            serial_rx_push(c);
        }
        c = plat_getc(0);
    }
    if (running) run_batch();
}
