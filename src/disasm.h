// 6502 disassembler for the monitor's r, s and u commands.  Only the
// documented NMOS opcodes are decoded; the others show as ???.
#pragma once
#include <stddef.h>
#include <stdint.h>

#define DISASM_TEXT_SIZE 48

// Formats the instruction at address pc, whose first bytes are bytes[0..2],
// e.g. "8D 10 F0  STA $F010  ; SERIAL_DATA".  Returns the instruction length.
int disasm(uint16_t pc, const uint8_t bytes[3], char *text, size_t size);
