cmake_minimum_required(VERSION 3.13)

set(PICO_BOARD pico2 CACHE STRING "Board type")

# Pull in the Pico SDK (set PICO_SDK_PATH in the environment or on the command line).
include(pico_sdk_import.cmake)

project(pico6502 C CXX ASM)
set(CMAKE_C_STANDARD 11)

pico_sdk_init()

add_executable(pico6502
    src/pico/main.c
    src/pico/lcd_pcf8574.c
    src/bus.c
    src/disasm.c
    src/lcd.c
    src/monitor.c
    src/fake6502/fake6502.c
)

target_include_directories(pico6502 PRIVATE src src/fake6502)
target_compile_options(pico6502 PRIVATE -Wall)
target_link_libraries(pico6502 pico_stdlib hardware_i2c)

# The monitor talks over USB serial.
pico_enable_stdio_usb(pico6502 1)
pico_enable_stdio_uart(pico6502 0)

# Run from SRAM rather than flash for a faster emulator.
pico_set_binary_type(pico6502 copy_to_ram)

pico_add_extra_outputs(pico6502)
