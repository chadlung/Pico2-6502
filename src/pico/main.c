// Pico 2 firmware entry point: USB serial monitor, 6502 CPU and 16x2 LCD.

#include <string.h>

#include "pico/stdlib.h"

#include "bus.h"
#include "demo_program.h"
#include "lcd.h"
#include "monitor.h"
#include "platform.h"

int plat_getc(uint32_t timeout_us) {
    int c = getchar_timeout_us(timeout_us);
    return c < 0 ? -1 : c;
}

uint64_t plat_time_us(void) { return time_us_64(); }

void plat_sleep_us(uint64_t us) { sleep_us(us); }

void plat_led(bool on) {
#ifdef PICO_DEFAULT_LED_PIN
    gpio_put(PICO_DEFAULT_LED_PIN, on);
#else
    (void)on;
#endif
}

int main(void) {
    stdio_init_all();
#ifdef PICO_DEFAULT_LED_PIN
    gpio_init(PICO_DEFAULT_LED_PIN);
    gpio_set_dir(PICO_DEFAULT_LED_PIN, GPIO_OUT);
#endif

    bus_reset();
    lcd_init();

    // Start the built-in demo so the display shows signs of life right away.
    memcpy(memory + DEMO_LOAD_ADDR, demo_program, sizeof demo_program);
    monitor_init();
    monitor_go(DEMO_LOAD_ADDR);

    for (;;) monitor_poll();
}
