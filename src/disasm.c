#include <stdio.h>

#include "bus.h"
#include "disasm.h"

enum mode { IMP, ACC, IMM, ZP, ZPX, ZPY, ABS, ABX, ABY, IND, IZX, IZY, REL };

static const uint8_t mode_length[] = {
    [IMP] = 1, [ACC] = 1, [IMM] = 2, [ZP] = 2, [ZPX] = 2, [ZPY] = 2, [ABS] = 3,
    [ABX] = 3, [ABY] = 3, [IND] = 3, [IZX] = 2, [IZY] = 2, [REL] = 2,
};

struct opcode {
    char name[4];
    uint8_t mode;
};

#define OP(name, mode) { #name, mode }
#define ___            { "???", IMP }   // undocumented opcode

static const struct opcode opcodes[256] = {
    /* 00 */ OP(BRK, IMP), OP(ORA, IZX), ___,          ___,          ___,          OP(ORA, ZP),  OP(ASL, ZP),  ___,
    /* 08 */ OP(PHP, IMP), OP(ORA, IMM), OP(ASL, ACC), ___,          ___,          OP(ORA, ABS), OP(ASL, ABS), ___,
    /* 10 */ OP(BPL, REL), OP(ORA, IZY), ___,          ___,          ___,          OP(ORA, ZPX), OP(ASL, ZPX), ___,
    /* 18 */ OP(CLC, IMP), OP(ORA, ABY), ___,          ___,          ___,          OP(ORA, ABX), OP(ASL, ABX), ___,
    /* 20 */ OP(JSR, ABS), OP(AND, IZX), ___,          ___,          OP(BIT, ZP),  OP(AND, ZP),  OP(ROL, ZP),  ___,
    /* 28 */ OP(PLP, IMP), OP(AND, IMM), OP(ROL, ACC), ___,          OP(BIT, ABS), OP(AND, ABS), OP(ROL, ABS), ___,
    /* 30 */ OP(BMI, REL), OP(AND, IZY), ___,          ___,          ___,          OP(AND, ZPX), OP(ROL, ZPX), ___,
    /* 38 */ OP(SEC, IMP), OP(AND, ABY), ___,          ___,          ___,          OP(AND, ABX), OP(ROL, ABX), ___,
    /* 40 */ OP(RTI, IMP), OP(EOR, IZX), ___,          ___,          ___,          OP(EOR, ZP),  OP(LSR, ZP),  ___,
    /* 48 */ OP(PHA, IMP), OP(EOR, IMM), OP(LSR, ACC), ___,          OP(JMP, ABS), OP(EOR, ABS), OP(LSR, ABS), ___,
    /* 50 */ OP(BVC, REL), OP(EOR, IZY), ___,          ___,          ___,          OP(EOR, ZPX), OP(LSR, ZPX), ___,
    /* 58 */ OP(CLI, IMP), OP(EOR, ABY), ___,          ___,          ___,          OP(EOR, ABX), OP(LSR, ABX), ___,
    /* 60 */ OP(RTS, IMP), OP(ADC, IZX), ___,          ___,          ___,          OP(ADC, ZP),  OP(ROR, ZP),  ___,
    /* 68 */ OP(PLA, IMP), OP(ADC, IMM), OP(ROR, ACC), ___,          OP(JMP, IND), OP(ADC, ABS), OP(ROR, ABS), ___,
    /* 70 */ OP(BVS, REL), OP(ADC, IZY), ___,          ___,          ___,          OP(ADC, ZPX), OP(ROR, ZPX), ___,
    /* 78 */ OP(SEI, IMP), OP(ADC, ABY), ___,          ___,          ___,          OP(ADC, ABX), OP(ROR, ABX), ___,
    /* 80 */ ___,          OP(STA, IZX), ___,          ___,          OP(STY, ZP),  OP(STA, ZP),  OP(STX, ZP),  ___,
    /* 88 */ OP(DEY, IMP), ___,          OP(TXA, IMP), ___,          OP(STY, ABS), OP(STA, ABS), OP(STX, ABS), ___,
    /* 90 */ OP(BCC, REL), OP(STA, IZY), ___,          ___,          OP(STY, ZPX), OP(STA, ZPX), OP(STX, ZPY), ___,
    /* 98 */ OP(TYA, IMP), OP(STA, ABY), OP(TXS, IMP), ___,          ___,          OP(STA, ABX), ___,          ___,
    /* A0 */ OP(LDY, IMM), OP(LDA, IZX), OP(LDX, IMM), ___,          OP(LDY, ZP),  OP(LDA, ZP),  OP(LDX, ZP),  ___,
    /* A8 */ OP(TAY, IMP), OP(LDA, IMM), OP(TAX, IMP), ___,          OP(LDY, ABS), OP(LDA, ABS), OP(LDX, ABS), ___,
    /* B0 */ OP(BCS, REL), OP(LDA, IZY), ___,          ___,          OP(LDY, ZPX), OP(LDA, ZPX), OP(LDX, ZPY), ___,
    /* B8 */ OP(CLV, IMP), OP(LDA, ABY), OP(TSX, IMP), ___,          OP(LDY, ABX), OP(LDA, ABX), OP(LDX, ABY), ___,
    /* C0 */ OP(CPY, IMM), OP(CMP, IZX), ___,          ___,          OP(CPY, ZP),  OP(CMP, ZP),  OP(DEC, ZP),  ___,
    /* C8 */ OP(INY, IMP), OP(CMP, IMM), OP(DEX, IMP), ___,          OP(CPY, ABS), OP(CMP, ABS), OP(DEC, ABS), ___,
    /* D0 */ OP(BNE, REL), OP(CMP, IZY), ___,          ___,          ___,          OP(CMP, ZPX), OP(DEC, ZPX), ___,
    /* D8 */ OP(CLD, IMP), OP(CMP, ABY), ___,          ___,          ___,          OP(CMP, ABX), OP(DEC, ABX), ___,
    /* E0 */ OP(CPX, IMM), OP(SBC, IZX), ___,          ___,          OP(CPX, ZP),  OP(SBC, ZP),  OP(INC, ZP),  ___,
    /* E8 */ OP(INX, IMP), OP(SBC, IMM), OP(NOP, IMP), ___,          OP(CPX, ABS), OP(SBC, ABS), OP(INC, ABS), ___,
    /* F0 */ OP(BEQ, REL), OP(SBC, IZY), ___,          ___,          ___,          OP(SBC, ZPX), OP(INC, ZPX), ___,
    /* F8 */ OP(SED, IMP), OP(SBC, ABY), ___,          ___,          ___,          OP(SBC, ABX), OP(INC, ABX), ___,
};

static const char *register_name(uint16_t addr) {
    switch (addr) {
    case IO_LCD_CONTROL:   return "LCD_CONTROL";
    case IO_LCD_DATA:      return "LCD_DATA";
    case IO_LCD_ROW:       return "LCD_ROW";
    case IO_LCD_COL:       return "LCD_COL";
    case IO_LCD_BACKLIGHT: return "LCD_BACKLIGHT";
    case IO_SERIAL_DATA:   return "SERIAL_DATA";
    case IO_SERIAL_STATUS: return "SERIAL_STATUS";
    default:               return NULL;
    }
}

int disasm(uint16_t pc, const uint8_t bytes[3], char *text, size_t size) {
    const struct opcode *op = &opcodes[bytes[0]];
    int len = mode_length[op->mode];
    uint8_t byte = bytes[1];
    uint16_t word = bytes[1] | bytes[2] << 8;
    char hex[9], operand[12];

    if (len == 1) snprintf(hex, sizeof hex, "%02X", bytes[0]);
    else if (len == 2) snprintf(hex, sizeof hex, "%02X %02X", bytes[0], bytes[1]);
    else snprintf(hex, sizeof hex, "%02X %02X %02X", bytes[0], bytes[1], bytes[2]);

    switch (op->mode) {
    case IMP: operand[0] = '\0'; break;
    case ACC: snprintf(operand, sizeof operand, " A"); break;
    case IMM: snprintf(operand, sizeof operand, " #$%02X", byte); break;
    case ZP:  snprintf(operand, sizeof operand, " $%02X", byte); break;
    case ZPX: snprintf(operand, sizeof operand, " $%02X,X", byte); break;
    case ZPY: snprintf(operand, sizeof operand, " $%02X,Y", byte); break;
    case ABS: snprintf(operand, sizeof operand, " $%04X", word); break;
    case ABX: snprintf(operand, sizeof operand, " $%04X,X", word); break;
    case ABY: snprintf(operand, sizeof operand, " $%04X,Y", word); break;
    case IND: snprintf(operand, sizeof operand, " ($%04X)", word); break;
    case IZX: snprintf(operand, sizeof operand, " ($%02X,X)", byte); break;
    case IZY: snprintf(operand, sizeof operand, " ($%02X),Y", byte); break;
    case REL: snprintf(operand, sizeof operand, " $%04X", (uint16_t)(pc + 2 + (int8_t)byte)); break;
    }

    const char *name = NULL;
    if (op->mode == ABS || op->mode == ABX || op->mode == ABY || op->mode == IND) name = register_name(word);

    snprintf(text, size, "%-8s  %s%s%s%s", hex, op->name, operand, name ? "  ; " : "", name ? name : "");
    return len;
}
