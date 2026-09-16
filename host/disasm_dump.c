// Prints the disassembly of every opcode, placed at $1000 with operand bytes
// $34 $12, followed by a few fixed cases.  test_disasm.py checks the output.

#include <stdio.h>

#include "disasm.h"

static void dump(uint16_t pc, uint8_t b0, uint8_t b1, uint8_t b2) {
    char text[DISASM_TEXT_SIZE];
    uint8_t bytes[3] = { b0, b1, b2 };
    int len = disasm(pc, bytes, text, sizeof text);
    printf("%04X %d %s\n", pc, len, text);
}

int main(void) {
    for (int op = 0; op < 256; op++) dump(0x1000, op, 0x34, 0x12);
    dump(0x0207, 0x8D, 0x00, 0xF0);   // STA $F000  ; LCD_CONTROL
    dump(0x050B, 0xD0, 0xF5, 0x00);   // BNE backwards
    dump(0x0300, 0xBD, 0x10, 0xF0);   // LDA $F010,X  ; SERIAL_DATA
    dump(0xFFFE, 0x10, 0x01, 0x00);   // BPL wrapping past $FFFF
    return 0;
}
