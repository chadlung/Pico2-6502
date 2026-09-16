// Serial monitor: loads programs, runs the CPU in batches between checks for
// input, and provides debugging commands.  Type 'h' in a terminal for help.
#pragma once
#include <stdint.h>

void monitor_init(void);           // print the banner
void monitor_go(uint16_t addr);    // set the reset vector to addr, reset the CPU and run
void monitor_poll(void);           // call repeatedly from the main loop
