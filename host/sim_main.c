// sim6502: the Pico 6502 computer running on a PC.
//
// The monitor, bus and CPU are the same code the Pico runs.  This terminal
// (or, with -p, a virtual serial port for tools/upload.py) stands in for USB
// serial, and the LCD exists only as the text shown by the monitor's 'd'
// command.

#define _XOPEN_SOURCE 700
#define _DEFAULT_SOURCE

#include <fcntl.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <time.h>
#include <unistd.h>

#include "bus.h"
#include "demo_program.h"
#include "lcd.h"
#include "lcd_hw.h"
#include "monitor.h"
#include "platform.h"

#define CTRL_RIGHT_BRACKET 0x1D

static struct termios saved_termios;
static bool terminal_raw;

// ------------------ platform.h ----------------------------------------------

int plat_getc(uint32_t timeout_us) {
    struct pollfd pfd = { .fd = STDIN_FILENO, .events = POLLIN };
    if (poll(&pfd, 1, (int)((timeout_us + 999) / 1000)) <= 0) return -1;
    unsigned char c;
    ssize_t n = read(STDIN_FILENO, &c, 1);
    if (n == 0) exit(0);            // piped input finished
    if (n < 0) return -1;
    if (c == CTRL_RIGHT_BRACKET && terminal_raw) exit(0);
    return c;
}

uint64_t plat_time_us(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec * 1000000ull + ts.tv_nsec / 1000;
}

void plat_sleep_us(uint64_t us) {
    struct timespec ts = { .tv_sec = us / 1000000, .tv_nsec = us % 1000000 * 1000 };
    nanosleep(&ts, NULL);
}

void plat_led(bool on) { (void)on; }

// ------------------ lcd_hw.h: no display, only lcd.c's mirror ---------------

bool lcd_hw_init(void) { return false; }
uint8_t lcd_hw_address(void) { return 0; }
void lcd_hw_diagnose(void) { printf("The simulator has no I2C bus.\n"); }
void lcd_hw_command(uint8_t cmd) { (void)cmd; }
void lcd_hw_write(uint8_t data) { (void)data; }
void lcd_hw_backlight(bool on) { (void)on; }

// ----------------------------------------------------------------------------

static void restore_terminal(void) {
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &saved_termios);
}

// Deliver keystrokes (including Ctrl-C) straight to the monitor.
static void make_terminal_raw(void) {
    if (!isatty(STDIN_FILENO) || tcgetattr(STDIN_FILENO, &saved_termios) < 0) return;
    terminal_raw = true;
    atexit(restore_terminal);
    struct termios t = saved_termios;
    t.c_lflag &= ~(ICANON | ECHO | ISIG);
    t.c_iflag &= ~(IXON | ICRNL);
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &t);
    fprintf(stderr, "sim6502: Ctrl-] quits\n");
}

// Serve the monitor on a new pseudo-terminal instead of this terminal.
static void open_virtual_port(void) {
    int master = posix_openpt(O_RDWR | O_NOCTTY);
    if (master < 0 || grantpt(master) < 0 || unlockpt(master) < 0) {
        perror("sim6502: pseudo-terminal");
        exit(1);
    }
    const char *name = ptsname(master);

    // Holding the port's far end open keeps it usable between client
    // connections; raw mode passes binary uploads through untouched.
    int port = open(name, O_RDWR | O_NOCTTY);
    struct termios t;
    if (port < 0 || tcgetattr(port, &t) < 0) {
        perror(name);
        exit(1);
    }
    cfmakeraw(&t);
    tcsetattr(port, TCSANOW, &t);

    fprintf(stderr, "sim6502: serial port is %s (Ctrl-C quits)\n", name);
    dup2(master, STDIN_FILENO);
    dup2(master, STDOUT_FILENO);
}

static void usage(void) {
    fprintf(stderr,
            "usage: sim6502 [-p] [FILE.bin [LOAD_ADDR]]\n"
            "  Loads FILE (default: the built-in demo) at LOAD_ADDR (hex, default 0200)\n"
            "  and runs it.\n"
            "  -p  serve the monitor on a virtual serial port instead of this terminal\n");
    exit(2);
}

int main(int argc, char **argv) {
    bool virtual_port = false;
    int opt;
    while ((opt = getopt(argc, argv, "ph")) != -1) {
        if (opt == 'p') virtual_port = true;
        else usage();
    }
    if (argc - optind > 2) usage();

    bus_reset();
    lcd_init();

    unsigned start = DEMO_LOAD_ADDR;
    if (optind < argc) {
        start = argc - optind == 2 ? strtoul(argv[optind + 1], NULL, 16) : 0x0200;
        if (start > 0xFFFF) usage();
        FILE *f = fopen(argv[optind], "rb");
        if (!f) {
            perror(argv[optind]);
            return 1;
        }
        size_t n = fread(memory + start, 1, sizeof memory - start, f);
        fclose(f);
        fprintf(stderr, "sim6502: loaded %zu bytes at $%04X\n", n, start);
    } else {
        memcpy(memory + DEMO_LOAD_ADDR, demo_program, sizeof demo_program);
    }

    if (virtual_port) open_virtual_port();
    else make_terminal_raw();

    monitor_init();
    monitor_go(start);
    for (;;) monitor_poll();
}
