// Services each build target provides to the portable code: the Pico
// firmware (src/pico/main.c) and the PC simulator (host/sim_main.c).
// Text output goes through ordinary stdio (printf/putchar).
#pragma once
#include <stdbool.h>
#include <stdint.h>

int      plat_getc(uint32_t timeout_us);   // next input byte 0-255, or -1 on timeout
uint64_t plat_time_us(void);
void     plat_sleep_us(uint64_t us);
void     plat_led(bool on);                // lit while a 6502 program runs
