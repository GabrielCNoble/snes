#include "cpu.h"
#include "ppu.h"
#include "rom.h"
#include "mem.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <stdint.h>

unsigned char wram1[8192];
unsigned char wram2[20000];

union cpu_hardware_regs_t
{
    unsigned char hardware_regs[CPU_REGS_END_ADDRESS - CPU_REGS_START_ADDRESS];

    struct
    {
        /*
            NMITIMEN (0x4200) - Interrupt enable and joypad request
                7    VBlank NMI enable (0 = disable, 1 = enable) - disabled on reset
                6    Not used
                5-4  H/V IRQ (0 = disabled, 1 = at H=H + V=any, 2 = at V=V + H=0, 3 = at H=H + V=V)
                3-1  Not used
                0    Joypad enable (0 = disabled, 1 = enable automatic reading of joypad)
        */
        uint8_t nmitimen;


        uint8_t wrio;
        uint8_t wrmpya;
        uint8_t wrmpyb;
        uint8_t wrdivl;
        uint8_t wrdivh;
        uint8_t wrdivb;

        /*
            HTIMEL/HTIMEH - H-Count timer setting
                15-9  Not used
                8-0   H-Count timer value (0 ... 339) (+ 1 for long lines, -1 for short lines) (0 = leftmost)
        */
        uint8_t htimel;
        uint8_t htimeh;

        /*
            VTIMEL/VTIMEH - V-Count timer setting
                15-9  Not used
                8-0   V-Count timer value (0...261 for ntsc, 0...311 for pal) (+1 in interlace) (0 = top)


        */
        uint8_t vtimel;
        uint8_t vtimeh;


        uint8_t mdmaen;
        uint8_t unused0[2];
        uint8_t rdnmi;


        /*
            TIMEUP - H/V-Timer IRQ flag (R)(Read/Ack)
                7   H/V-Count timer IRQ flag (0 = None, 1 = Interrupt request)
                6-0 Not used

            The IRQ flag is automatically reset after reading from this register
            (except when reading at the very time when the IRQ condition is true
            (which lasts for 4-8 master cycles), then the CPU receives bit7=1, but
            register bit7 isn't cleared). The flag is also automatically cleared
            when disabling IRQs (by setting 4200h.Bit5-4 to zero). Unlike NMI handlers,
            IRQ handlers MUST acknowledge IRQs, otherwise the IRQ gets executed again
            (ie. immediately after the RTI opcode).
        */
        uint8_t timeup;

        /*
            HVBJOY - H/V-Blank flag and joypad busy flag (R)
                7   V-Blank period flag (0 = No, 1 = VBlank)
                6   H-Blank period flag (0 = No, 1 = HBlank)
                5-1 Not used
                0   Auto-Joypad-Read busy flag (1 = Busy)

            The H-Blank flag gets toggled in ALL scanlines (including during VBlank/Vsync). Both
            H-Blank and V-Blank are always toggling (even during Forced Blank, and no matter if
            IRQs or NMIs are enabled).
            
            V-blank starts at scanline $e1 or $f0, depending on bit 2 of register $2133. When this
            bit is 0, the frame will be 224 scanlines long (ignoring scanline 0), and so V-blank
            will start at scanline $e1 (225). When it's 1, the frame will be 239 scanlines long,
            (ignoring scanline 0), and so V-blank will start at scanline $f0 (240). V-blank ends
            at scanline 0, dot 0.
            
            H-blank starts at dot 274 of every scanline, and ends at dot 0.
        */
        uint8_t hvbjoy;



        uint8_t rdio;
        uint8_t rddivl;
        uint8_t rddivh;
        uint8_t rdmpyl;
        uint8_t rdmpyh;
        uint8_t stdcntrl1l;
        uint8_t stdcntrl1h;
        uint8_t stdcntrl2l;
        uint8_t stdcntrl2h;
        uint8_t stdcntrl3l;
        uint8_t stdcntrl3h;
        uint8_t stdcntrl4l;
        uint8_t stdcntrl4h;
        uint8_t unused1[0xe0];
//        unsigned char dmatparm0;
//        unsigned char dmataddr0;
//        unsigned char unused2[0x03];
//        unsigned char
//
//
//        unsigned char dmatp1;
//        unsigned char dmatc1;
//        unsigned char dmatp2;
//        unsigned char dmatc2;
//        unsigned char dmatp3;
//        unsigned char dmatc3;
//        unsigned char dmatp4;
//        unsigned char dmatc4;
//        unsigned char dmatp5;
//        unsigned char dmatc5;
//        unsigned char dmatp6;
//        unsigned char dmatc6;
//        unsigned char dmatp7;
//        unsigned char dmatc7;

    };
}hardware_regs;

unsigned long step_count = 0;


//unsigned char reg_e;         /* emulation flag */
//unsigned short reg_ir;      /* instruction register */
//unsigned short reg_tcu;     /* timing control unit */


//union
//{
//    unsigned short reg_accumC;
//
//    struct
//    {
//        unsigned char reg_accumA;
//        unsigned char reg_accumB;
//    };
//
//}reg_accum;
//
//uint32_t reg_temp0;
//uint32_t reg_temp1;
//
//
//union
//{
//    unsigned short reg_x;
//
//    struct
//    {
//        unsigned char reg_xL;
//        unsigned char reg_xH;
//    };
//
//}reg_x;
//
//
//union
//{
//    unsigned short reg_y;
//
//    struct
//    {
//        unsigned char reg_yL;
//        unsigned char reg_yH;
//    };
//
//}reg_y;
//
//uint16_t reg_d;       /* direct register */
//uint16_t reg_s;       /* stack pointer register */
//
//uint16_t reg_pc;       /* program counter register */
//struct {uint8_t reg_dbr; uint8_t unused;} reg_dbrw; /* data bank register */
//struct {uint8_t reg_pbr; uint8_t unused;} reg_pbrw; /* program bank register */     
//uint8_t reg_p;        /* status register */
//uint8_t in_irqb = 1;
//uint8_t in_rdy = 1;
//uint8_t in_resb = 1;
//uint8_t in_prev_resb = 0;
//uint8_t in_abortb = 1;
//uint8_t in_nmib = 1;
//unsigned char s_wai = 0;

struct cpu_state_t cpu_state = {.in_irqb = 1, .in_rdy = 1, .in_resb = 1, .in_abortb = 1, .in_nmib = 1};

uint32_t cpu_cycle_count = 0;

#define ALU_WIDTH_WORD 0
#define ALU_WIDTH_BYTE 1

#define HIGHSPEED_ACCESS(effective_address) (effective_address & 0x00800000)
#define ACCESS_CYCLES(effective_address) (HIGHSPEED_ACCESS(effective_address) ? 6 : 8)

#define OPCODE(op, cat, addr_mod) {.opcode = op, .address_mode = addr_mod, .opcode_class = cat}

struct opcode_t opcode_matrix[256] = 
{
    [0x61] = OPCODE(OPCODE_ADC, OPCODE_CLASS_ALU, ADDRESS_MODE_DIRECT_INDEXED_INDIRECT),
    [0x63] = OPCODE(OPCODE_ADC, OPCODE_CLASS_ALU, ADDRESS_MODE_STACK_RELATIVE),
    [0x65] = OPCODE(OPCODE_ADC, OPCODE_CLASS_ALU, ADDRESS_MODE_DIRECT),
    [0x67] = OPCODE(OPCODE_ADC, OPCODE_CLASS_ALU, ADDRESS_MODE_DIRECT_INDIRECT_LONG),
    [0x69] = OPCODE(OPCODE_ADC, OPCODE_CLASS_ALU, ADDRESS_MODE_IMMEDIATE),
    [0x6d] = OPCODE(OPCODE_ADC, OPCODE_CLASS_ALU, ADDRESS_MODE_ABSOLUTE),
    [0x6f] = OPCODE(OPCODE_ADC, OPCODE_CLASS_ALU, ADDRESS_MODE_ABSOLUTE_LONG),
    [0x71] = OPCODE(OPCODE_ADC, OPCODE_CLASS_ALU, ADDRESS_MODE_DIRECT_INDIRECT_INDEXED),
    [0x72] = OPCODE(OPCODE_ADC, OPCODE_CLASS_ALU, ADDRESS_MODE_DIRECT_INDIRECT),
    [0x73] = OPCODE(OPCODE_ADC, OPCODE_CLASS_ALU, ADDRESS_MODE_STACK_RELATIVE_INDIRECT_INDEXED),
    [0x75] = OPCODE(OPCODE_ADC, OPCODE_CLASS_ALU, ADDRESS_MODE_DIRECT_INDEXED_X),
    [0x77] = OPCODE(OPCODE_ADC, OPCODE_CLASS_ALU, ADDRESS_MODE_DIRECT_INDIRECT_LONG_INDEXED),
    [0x79] = OPCODE(OPCODE_ADC, OPCODE_CLASS_ALU, ADDRESS_MODE_ABSOLUTE_INDEXED_Y),    
    [0x7d] = OPCODE(OPCODE_ADC, OPCODE_CLASS_ALU, ADDRESS_MODE_ABSOLUTE_INDEXED_X),
    [0x7f] = OPCODE(OPCODE_ADC, OPCODE_CLASS_ALU, ADDRESS_MODE_ABSOLUTE_LONG_INDEXED_X),
        
    [0x21] = OPCODE(OPCODE_AND, OPCODE_CLASS_ALU, ADDRESS_MODE_DIRECT_INDEXED_INDIRECT),
    [0x23] = OPCODE(OPCODE_AND, OPCODE_CLASS_ALU, ADDRESS_MODE_STACK_RELATIVE),
    [0x25] = OPCODE(OPCODE_AND, OPCODE_CLASS_ALU, ADDRESS_MODE_DIRECT),
    [0x27] = OPCODE(OPCODE_AND, OPCODE_CLASS_ALU, ADDRESS_MODE_DIRECT_INDIRECT_LONG),
    [0x29] = OPCODE(OPCODE_AND, OPCODE_CLASS_ALU, ADDRESS_MODE_IMMEDIATE),
    [0x2d] = OPCODE(OPCODE_AND, OPCODE_CLASS_ALU, ADDRESS_MODE_ABSOLUTE),
    [0x2f] = OPCODE(OPCODE_AND, OPCODE_CLASS_ALU, ADDRESS_MODE_ABSOLUTE_LONG),
    [0x31] = OPCODE(OPCODE_AND, OPCODE_CLASS_ALU, ADDRESS_MODE_DIRECT_INDIRECT_INDEXED),
    [0x32] = OPCODE(OPCODE_AND, OPCODE_CLASS_ALU, ADDRESS_MODE_DIRECT_INDIRECT),
    [0x33] = OPCODE(OPCODE_AND, OPCODE_CLASS_ALU, ADDRESS_MODE_STACK_RELATIVE_INDIRECT_INDEXED),
    [0x35] = OPCODE(OPCODE_AND, OPCODE_CLASS_ALU, ADDRESS_MODE_DIRECT_INDEXED_X),
    [0x37] = OPCODE(OPCODE_AND, OPCODE_CLASS_ALU, ADDRESS_MODE_DIRECT_INDIRECT_LONG_INDEXED),
    [0x39] = OPCODE(OPCODE_AND, OPCODE_CLASS_ALU, ADDRESS_MODE_ABSOLUTE_INDEXED_Y),
    [0x3d] = OPCODE(OPCODE_AND, OPCODE_CLASS_ALU, ADDRESS_MODE_ABSOLUTE_INDEXED_X),
    [0x3f] = OPCODE(OPCODE_AND, OPCODE_CLASS_ALU, ADDRESS_MODE_ABSOLUTE_LONG_INDEXED_X),
        
    [0x06] = OPCODE(OPCODE_ASL, OPCODE_CLASS_ALU, ADDRESS_MODE_DIRECT),
    [0x0a] = OPCODE(OPCODE_ASL, OPCODE_CLASS_ALU, ADDRESS_MODE_ACCUMULATOR),
    [0x0e] = OPCODE(OPCODE_ASL, OPCODE_CLASS_ALU, ADDRESS_MODE_ABSOLUTE),
    [0x16] = OPCODE(OPCODE_ASL, OPCODE_CLASS_ALU, ADDRESS_MODE_DIRECT_INDEXED_X),
    [0x1e] = OPCODE(OPCODE_ASL, OPCODE_CLASS_ALU, ADDRESS_MODE_ABSOLUTE_INDEXED_X),
        
    [0x10] = OPCODE(OPCODE_BPL, OPCODE_CLASS_BRANCH, ADDRESS_MODE_PROGRAM_COUNTER_RELATIVE),
    [0x30] = OPCODE(OPCODE_BMI, OPCODE_CLASS_BRANCH, ADDRESS_MODE_PROGRAM_COUNTER_RELATIVE),
    [0x50] = OPCODE(OPCODE_BVC, OPCODE_CLASS_BRANCH, ADDRESS_MODE_PROGRAM_COUNTER_RELATIVE),
    [0x70] = OPCODE(OPCODE_BVS, OPCODE_CLASS_BRANCH, ADDRESS_MODE_PROGRAM_COUNTER_RELATIVE),
    [0x80] = OPCODE(OPCODE_BRA, OPCODE_CLASS_BRANCH, ADDRESS_MODE_PROGRAM_COUNTER_RELATIVE),
    [0x82] = OPCODE(OPCODE_BRL, OPCODE_CLASS_BRANCH, ADDRESS_MODE_PROGRAM_COUNTER_RELATIVE_LONG),
    [0x90] = OPCODE(OPCODE_BCC, OPCODE_CLASS_BRANCH, ADDRESS_MODE_PROGRAM_COUNTER_RELATIVE),
    [0xb0] = OPCODE(OPCODE_BCS, OPCODE_CLASS_BRANCH, ADDRESS_MODE_PROGRAM_COUNTER_RELATIVE),
    [0xd0] = OPCODE(OPCODE_BNE, OPCODE_CLASS_BRANCH, ADDRESS_MODE_PROGRAM_COUNTER_RELATIVE),
    [0xf0] = OPCODE(OPCODE_BEQ, OPCODE_CLASS_BRANCH, ADDRESS_MODE_PROGRAM_COUNTER_RELATIVE),
        
    [0x24] = OPCODE(OPCODE_BIT, OPCODE_CLASS_ALU, ADDRESS_MODE_DIRECT),
    [0x2c] = OPCODE(OPCODE_BIT, OPCODE_CLASS_ALU, ADDRESS_MODE_ABSOLUTE),
    [0x34] = OPCODE(OPCODE_BIT, OPCODE_CLASS_ALU, ADDRESS_MODE_DIRECT_INDEXED_X),
    [0x3c] = OPCODE(OPCODE_BIT, OPCODE_CLASS_ALU, ADDRESS_MODE_ABSOLUTE_INDEXED_X),
    [0x89] = OPCODE(OPCODE_BIT, OPCODE_CLASS_ALU, ADDRESS_MODE_IMMEDIATE),

    [0x00] = OPCODE(OPCODE_BRK, OPCODE_CLASS_STANDALONE, ADDRESS_MODE_STACK),
        
    [0x18] = OPCODE(OPCODE_CLC, OPCODE_CLASS_STANDALONE, ADDRESS_MODE_IMPLIED),
    [0x38] = OPCODE(OPCODE_SEC, OPCODE_CLASS_STANDALONE, ADDRESS_MODE_IMPLIED),
    [0x58] = OPCODE(OPCODE_CLI, OPCODE_CLASS_STANDALONE, ADDRESS_MODE_IMPLIED),
    [0x78] = OPCODE(OPCODE_SEI, OPCODE_CLASS_STANDALONE, ADDRESS_MODE_IMPLIED),
    [0xb8] = OPCODE(OPCODE_CLV, OPCODE_CLASS_STANDALONE, ADDRESS_MODE_IMPLIED),
    [0xd8] = OPCODE(OPCODE_CLD, OPCODE_CLASS_STANDALONE, ADDRESS_MODE_IMPLIED),
    [0xf8] = OPCODE(OPCODE_SED, OPCODE_CLASS_STANDALONE, ADDRESS_MODE_IMPLIED),
        
    [0xc2] = OPCODE(OPCODE_REP, OPCODE_CLASS_STANDALONE, ADDRESS_MODE_IMMEDIATE),
    [0xe2] = OPCODE(OPCODE_SEP, OPCODE_CLASS_STANDALONE, ADDRESS_MODE_IMMEDIATE),

    [0xc1] = OPCODE(OPCODE_CMP, OPCODE_CLASS_ALU, ADDRESS_MODE_DIRECT_INDEXED_INDIRECT),
    [0xc3] = OPCODE(OPCODE_CMP, OPCODE_CLASS_ALU, ADDRESS_MODE_STACK_RELATIVE),
    [0xc5] = OPCODE(OPCODE_CMP, OPCODE_CLASS_ALU, ADDRESS_MODE_DIRECT),
    [0xc7] = OPCODE(OPCODE_CMP, OPCODE_CLASS_ALU, ADDRESS_MODE_DIRECT_INDIRECT_LONG),
    [0xc9] = OPCODE(OPCODE_CMP, OPCODE_CLASS_ALU, ADDRESS_MODE_IMMEDIATE),
    [0xcd] = OPCODE(OPCODE_CMP, OPCODE_CLASS_ALU, ADDRESS_MODE_ABSOLUTE),
    [0xcf] = OPCODE(OPCODE_CMP, OPCODE_CLASS_ALU, ADDRESS_MODE_ABSOLUTE_LONG),
    [0xd1] = OPCODE(OPCODE_CMP, OPCODE_CLASS_ALU, ADDRESS_MODE_DIRECT_INDIRECT_INDEXED),
    [0xd2] = OPCODE(OPCODE_CMP, OPCODE_CLASS_ALU, ADDRESS_MODE_DIRECT_INDIRECT),
    [0xd3] = OPCODE(OPCODE_CMP, OPCODE_CLASS_ALU, ADDRESS_MODE_STACK_RELATIVE_INDIRECT_INDEXED),
    [0xd5] = OPCODE(OPCODE_CMP, OPCODE_CLASS_ALU, ADDRESS_MODE_DIRECT_INDEXED_X),
    [0xd7] = OPCODE(OPCODE_CMP, OPCODE_CLASS_ALU, ADDRESS_MODE_DIRECT_INDIRECT_LONG_INDEXED),
    [0xd9] = OPCODE(OPCODE_CMP, OPCODE_CLASS_ALU, ADDRESS_MODE_ABSOLUTE_INDEXED_Y),
    [0xdd] = OPCODE(OPCODE_CMP, OPCODE_CLASS_ALU, ADDRESS_MODE_ABSOLUTE_INDEXED_X),
    [0xdf] = OPCODE(OPCODE_CMP, OPCODE_CLASS_ALU, ADDRESS_MODE_ABSOLUTE_LONG_INDEXED_X),

    [0x02] = OPCODE(OPCODE_COP, OPCODE_CLASS_STANDALONE, ADDRESS_MODE_STACK),

    [0xe0] = OPCODE(OPCODE_CPX, OPCODE_CLASS_ALU, ADDRESS_MODE_IMMEDIATE),
    [0xe4] = OPCODE(OPCODE_CPX, OPCODE_CLASS_ALU, ADDRESS_MODE_DIRECT),
    [0xec] = OPCODE(OPCODE_CPX, OPCODE_CLASS_ALU, ADDRESS_MODE_ABSOLUTE),
        
    [0xc0] = OPCODE(OPCODE_CPY, OPCODE_CLASS_ALU, ADDRESS_MODE_IMMEDIATE),
    [0xc4] = OPCODE(OPCODE_CPY, OPCODE_CLASS_ALU, ADDRESS_MODE_DIRECT),
    [0xcc] = OPCODE(OPCODE_CPY, OPCODE_CLASS_ALU, ADDRESS_MODE_ABSOLUTE),

    [0x3a] = OPCODE(OPCODE_DEC, OPCODE_CLASS_ALU, ADDRESS_MODE_ACCUMULATOR),
    [0xc6] = OPCODE(OPCODE_DEC, OPCODE_CLASS_ALU, ADDRESS_MODE_DIRECT),
    [0xce] = OPCODE(OPCODE_DEC, OPCODE_CLASS_ALU, ADDRESS_MODE_ABSOLUTE),
    [0xd6] = OPCODE(OPCODE_DEC, OPCODE_CLASS_ALU, ADDRESS_MODE_DIRECT_INDEXED_X),
    [0xde] = OPCODE(OPCODE_DEC, OPCODE_CLASS_ALU, ADDRESS_MODE_ABSOLUTE_INDEXED_X),
        
    [0xca] = OPCODE(OPCODE_DEX, OPCODE_CLASS_ALU, ADDRESS_MODE_IMPLIED),
    [0x88] = OPCODE(OPCODE_DEY, OPCODE_CLASS_ALU, ADDRESS_MODE_IMPLIED),

    [0x41] = OPCODE(OPCODE_EOR, OPCODE_CLASS_ALU, ADDRESS_MODE_DIRECT_INDEXED_INDIRECT),
    [0x43] = OPCODE(OPCODE_EOR, OPCODE_CLASS_ALU, ADDRESS_MODE_STACK_RELATIVE),
    [0x45] = OPCODE(OPCODE_EOR, OPCODE_CLASS_ALU, ADDRESS_MODE_DIRECT),
    [0x47] = OPCODE(OPCODE_EOR, OPCODE_CLASS_ALU, ADDRESS_MODE_DIRECT_INDIRECT_LONG),
    [0x49] = OPCODE(OPCODE_EOR, OPCODE_CLASS_ALU, ADDRESS_MODE_IMMEDIATE),
    [0x4d] = OPCODE(OPCODE_EOR, OPCODE_CLASS_ALU, ADDRESS_MODE_ABSOLUTE),
    [0x4f] = OPCODE(OPCODE_EOR, OPCODE_CLASS_ALU, ADDRESS_MODE_ABSOLUTE_LONG),
    [0x51] = OPCODE(OPCODE_EOR, OPCODE_CLASS_ALU, ADDRESS_MODE_DIRECT_INDIRECT_INDEXED),
    [0x52] = OPCODE(OPCODE_EOR, OPCODE_CLASS_ALU, ADDRESS_MODE_DIRECT_INDIRECT),
    [0x53] = OPCODE(OPCODE_EOR, OPCODE_CLASS_ALU, ADDRESS_MODE_STACK_RELATIVE_INDIRECT_INDEXED),
    [0x55] = OPCODE(OPCODE_EOR, OPCODE_CLASS_ALU, ADDRESS_MODE_DIRECT_INDEXED_X),
    [0x57] = OPCODE(OPCODE_EOR, OPCODE_CLASS_ALU, ADDRESS_MODE_DIRECT_INDIRECT_LONG_INDEXED),
    [0x59] = OPCODE(OPCODE_EOR, OPCODE_CLASS_ALU, ADDRESS_MODE_ABSOLUTE_INDEXED_Y),
    [0x5d] = OPCODE(OPCODE_EOR, OPCODE_CLASS_ALU, ADDRESS_MODE_ABSOLUTE_INDEXED_X),
    [0x5f] = OPCODE(OPCODE_EOR, OPCODE_CLASS_ALU, ADDRESS_MODE_ABSOLUTE_LONG_INDEXED_X),
        
    [0x1a] = OPCODE(OPCODE_INC, OPCODE_CLASS_ALU, ADDRESS_MODE_ACCUMULATOR),
    [0xe6] = OPCODE(OPCODE_INC, OPCODE_CLASS_ALU, ADDRESS_MODE_DIRECT),
    [0xee] = OPCODE(OPCODE_INC, OPCODE_CLASS_ALU, ADDRESS_MODE_ABSOLUTE),
    [0xf6] = OPCODE(OPCODE_INC, OPCODE_CLASS_ALU, ADDRESS_MODE_DIRECT_INDEXED_X),
    [0xfe] = OPCODE(OPCODE_INC, OPCODE_CLASS_ALU, ADDRESS_MODE_ABSOLUTE_INDEXED_X),
        
    [0xe8] = OPCODE(OPCODE_INX, OPCODE_CLASS_ALU, ADDRESS_MODE_IMPLIED),
    [0xc8] = OPCODE(OPCODE_INY, OPCODE_CLASS_ALU, ADDRESS_MODE_IMPLIED),

    [0x4c] = OPCODE(OPCODE_JMP, OPCODE_CLASS_BRANCH, ADDRESS_MODE_ABSOLUTE),
    [0x5c] = OPCODE(OPCODE_JMP, OPCODE_CLASS_BRANCH, ADDRESS_MODE_ABSOLUTE_LONG),
    [0x6c] = OPCODE(OPCODE_JMP, OPCODE_CLASS_BRANCH, ADDRESS_MODE_ABSOLUTE_INDIRECT),
    [0x7c] = OPCODE(OPCODE_JMP, OPCODE_CLASS_BRANCH, ADDRESS_MODE_ABSOLUTE_INDEXED_INDIRECT),
    [0xdc] = OPCODE(OPCODE_JML, OPCODE_CLASS_BRANCH, ADDRESS_MODE_ABSOLUTE_INDIRECT),
        
    [0x22] = OPCODE(OPCODE_JSL, OPCODE_CLASS_BRANCH, ADDRESS_MODE_ABSOLUTE_LONG),

    [0x20] = OPCODE(OPCODE_JSR, OPCODE_CLASS_BRANCH, ADDRESS_MODE_ABSOLUTE),
    [0xfc] = OPCODE(OPCODE_JSR, OPCODE_CLASS_BRANCH, ADDRESS_MODE_ABSOLUTE_INDEXED_INDIRECT),
        
    [0xa1] = OPCODE(OPCODE_LDA, OPCODE_CLASS_LOAD, ADDRESS_MODE_DIRECT_INDEXED_INDIRECT),
    [0xa3] = OPCODE(OPCODE_LDA, OPCODE_CLASS_LOAD, ADDRESS_MODE_STACK_RELATIVE),
    [0xa5] = OPCODE(OPCODE_LDA, OPCODE_CLASS_LOAD, ADDRESS_MODE_DIRECT),
    [0xa7] = OPCODE(OPCODE_LDA, OPCODE_CLASS_LOAD, ADDRESS_MODE_DIRECT_INDIRECT_LONG),
    [0xa9] = OPCODE(OPCODE_LDA, OPCODE_CLASS_LOAD, ADDRESS_MODE_IMMEDIATE),
    [0xad] = OPCODE(OPCODE_LDA, OPCODE_CLASS_LOAD, ADDRESS_MODE_ABSOLUTE),
    [0xaf] = OPCODE(OPCODE_LDA, OPCODE_CLASS_LOAD, ADDRESS_MODE_ABSOLUTE_LONG),
    [0xb1] = OPCODE(OPCODE_LDA, OPCODE_CLASS_LOAD, ADDRESS_MODE_DIRECT_INDIRECT_INDEXED),
    [0xb2] = OPCODE(OPCODE_LDA, OPCODE_CLASS_LOAD, ADDRESS_MODE_DIRECT_INDIRECT),
    [0xb3] = OPCODE(OPCODE_LDA, OPCODE_CLASS_LOAD, ADDRESS_MODE_STACK_RELATIVE_INDIRECT_INDEXED),
    [0xb5] = OPCODE(OPCODE_LDA, OPCODE_CLASS_LOAD, ADDRESS_MODE_DIRECT_INDEXED_X),
    [0xb7] = OPCODE(OPCODE_LDA, OPCODE_CLASS_LOAD, ADDRESS_MODE_DIRECT_INDIRECT_LONG_INDEXED),
    [0xb9] = OPCODE(OPCODE_LDA, OPCODE_CLASS_LOAD, ADDRESS_MODE_ABSOLUTE_INDEXED_Y),
    [0xbd] = OPCODE(OPCODE_LDA, OPCODE_CLASS_LOAD, ADDRESS_MODE_ABSOLUTE_INDEXED_X),
    [0xbf] = OPCODE(OPCODE_LDA, OPCODE_CLASS_LOAD, ADDRESS_MODE_ABSOLUTE_LONG_INDEXED_X),
        
    [0xa2] = OPCODE(OPCODE_LDX, OPCODE_CLASS_LOAD, ADDRESS_MODE_IMMEDIATE),
    [0xa6] = OPCODE(OPCODE_LDX, OPCODE_CLASS_LOAD, ADDRESS_MODE_DIRECT),
    [0xae] = OPCODE(OPCODE_LDX, OPCODE_CLASS_LOAD, ADDRESS_MODE_ABSOLUTE),
    [0xb6] = OPCODE(OPCODE_LDX, OPCODE_CLASS_LOAD, ADDRESS_MODE_DIRECT_INDEXED_Y),
    [0xbe] = OPCODE(OPCODE_LDX, OPCODE_CLASS_LOAD, ADDRESS_MODE_ABSOLUTE_INDEXED_Y),
        
    [0xa0] = OPCODE(OPCODE_LDY, OPCODE_CLASS_LOAD, ADDRESS_MODE_IMMEDIATE),
    [0xa4] = OPCODE(OPCODE_LDY, OPCODE_CLASS_LOAD, ADDRESS_MODE_DIRECT),
    [0xac] = OPCODE(OPCODE_LDY, OPCODE_CLASS_LOAD, ADDRESS_MODE_ABSOLUTE),
    [0xb4] = OPCODE(OPCODE_LDY, OPCODE_CLASS_LOAD, ADDRESS_MODE_DIRECT_INDEXED_X),
    [0xbc] = OPCODE(OPCODE_LDY, OPCODE_CLASS_LOAD, ADDRESS_MODE_ABSOLUTE_INDEXED_Y),
        
    [0x46] = OPCODE(OPCODE_LSR, OPCODE_CLASS_ALU, ADDRESS_MODE_DIRECT),
    [0x4a] = OPCODE(OPCODE_LSR, OPCODE_CLASS_ALU, ADDRESS_MODE_ACCUMULATOR),
    [0x4e] = OPCODE(OPCODE_LSR, OPCODE_CLASS_ALU, ADDRESS_MODE_ABSOLUTE),
    [0x56] = OPCODE(OPCODE_LSR, OPCODE_CLASS_ALU, ADDRESS_MODE_DIRECT_INDEXED_X),
    [0x5e] = OPCODE(OPCODE_LSR, OPCODE_CLASS_ALU, ADDRESS_MODE_ABSOLUTE_INDEXED_X),
        
    [0x54] = OPCODE(OPCODE_MVN, OPCODE_CLASS_STANDALONE, ADDRESS_MODE_BLOCK_MOVE),
    [0x44] = OPCODE(OPCODE_MVP, OPCODE_CLASS_STANDALONE, ADDRESS_MODE_BLOCK_MOVE),

    [0xea] = OPCODE(OPCODE_NOP, OPCODE_CLASS_STANDALONE, ADDRESS_MODE_IMPLIED),
        
    [0x01] = OPCODE(OPCODE_ORA, OPCODE_CLASS_ALU, ADDRESS_MODE_DIRECT_INDEXED_INDIRECT),
    [0x03] = OPCODE(OPCODE_ORA, OPCODE_CLASS_ALU, ADDRESS_MODE_STACK_RELATIVE),
    [0x05] = OPCODE(OPCODE_ORA, OPCODE_CLASS_ALU, ADDRESS_MODE_DIRECT),
    [0x07] = OPCODE(OPCODE_ORA, OPCODE_CLASS_ALU, ADDRESS_MODE_DIRECT_INDIRECT_LONG),
    [0x09] = OPCODE(OPCODE_ORA, OPCODE_CLASS_ALU, ADDRESS_MODE_IMMEDIATE),
    [0x0d] = OPCODE(OPCODE_ORA, OPCODE_CLASS_ALU, ADDRESS_MODE_ABSOLUTE),
    [0x0f] = OPCODE(OPCODE_ORA, OPCODE_CLASS_ALU, ADDRESS_MODE_ABSOLUTE_LONG),
    [0x11] = OPCODE(OPCODE_ORA, OPCODE_CLASS_ALU, ADDRESS_MODE_DIRECT_INDIRECT_INDEXED),
    [0x12] = OPCODE(OPCODE_ORA, OPCODE_CLASS_ALU, ADDRESS_MODE_DIRECT_INDIRECT),
    [0x13] = OPCODE(OPCODE_ORA, OPCODE_CLASS_ALU, ADDRESS_MODE_STACK_RELATIVE_INDIRECT_INDEXED),
    [0x15] = OPCODE(OPCODE_ORA, OPCODE_CLASS_ALU, ADDRESS_MODE_DIRECT_INDEXED_X),
    [0x17] = OPCODE(OPCODE_ORA, OPCODE_CLASS_ALU, ADDRESS_MODE_DIRECT_INDIRECT_LONG_INDEXED),
    [0x19] = OPCODE(OPCODE_ORA, OPCODE_CLASS_ALU, ADDRESS_MODE_ABSOLUTE_INDEXED_Y),
    [0x1d] = OPCODE(OPCODE_ORA, OPCODE_CLASS_ALU, ADDRESS_MODE_ABSOLUTE_INDEXED_X),
    [0x1f] = OPCODE(OPCODE_ORA, OPCODE_CLASS_ALU, ADDRESS_MODE_ABSOLUTE_LONG_INDEXED_X),
        
        
    [0xf4] = OPCODE(OPCODE_PEA, OPCODE_CLASS_STACK, ADDRESS_MODE_STACK),
    [0xd4] = OPCODE(OPCODE_PEI, OPCODE_CLASS_STACK, ADDRESS_MODE_STACK),
    [0x62] = OPCODE(OPCODE_PER, OPCODE_CLASS_STACK, ADDRESS_MODE_STACK),
    [0x48] = OPCODE(OPCODE_PHA, OPCODE_CLASS_STACK, ADDRESS_MODE_STACK),
    [0x8b] = OPCODE(OPCODE_PHB, OPCODE_CLASS_STACK, ADDRESS_MODE_STACK),
    [0x0b] = OPCODE(OPCODE_PHD, OPCODE_CLASS_STACK, ADDRESS_MODE_STACK),
    [0x4b] = OPCODE(OPCODE_PHK, OPCODE_CLASS_STACK, ADDRESS_MODE_STACK),
    [0x08] = OPCODE(OPCODE_PHP, OPCODE_CLASS_STACK, ADDRESS_MODE_STACK),
    [0xda] = OPCODE(OPCODE_PHX, OPCODE_CLASS_STACK, ADDRESS_MODE_STACK),
    [0x5a] = OPCODE(OPCODE_PHY, OPCODE_CLASS_STACK, ADDRESS_MODE_STACK),
    [0x68] = OPCODE(OPCODE_PLA, OPCODE_CLASS_STACK, ADDRESS_MODE_STACK),
    [0xab] = OPCODE(OPCODE_PLB, OPCODE_CLASS_STACK, ADDRESS_MODE_STACK),
    [0x2b] = OPCODE(OPCODE_PLD, OPCODE_CLASS_STACK, ADDRESS_MODE_STACK),
    [0x28] = OPCODE(OPCODE_PLP, OPCODE_CLASS_STACK, ADDRESS_MODE_STACK),
    [0xfa] = OPCODE(OPCODE_PLX, OPCODE_CLASS_STACK, ADDRESS_MODE_STACK),
    [0x7a] = OPCODE(OPCODE_PLY, OPCODE_CLASS_STACK, ADDRESS_MODE_STACK),

    [0x26] = OPCODE(OPCODE_ROL, OPCODE_CLASS_ALU, ADDRESS_MODE_DIRECT),
    [0x2a] = OPCODE(OPCODE_ROL, OPCODE_CLASS_ALU, ADDRESS_MODE_ACCUMULATOR),
    [0x2e] = OPCODE(OPCODE_ROL, OPCODE_CLASS_ALU, ADDRESS_MODE_ABSOLUTE),
    [0x36] = OPCODE(OPCODE_ROL, OPCODE_CLASS_ALU, ADDRESS_MODE_DIRECT_INDEXED_X),
    [0x3e] = OPCODE(OPCODE_ROL, OPCODE_CLASS_ALU, ADDRESS_MODE_ABSOLUTE_INDEXED_X),
        
    [0x66] = OPCODE(OPCODE_ROR, OPCODE_CLASS_ALU, ADDRESS_MODE_DIRECT),
    [0x6a] = OPCODE(OPCODE_ROR, OPCODE_CLASS_ALU, ADDRESS_MODE_ACCUMULATOR),
    [0x6e] = OPCODE(OPCODE_ROR, OPCODE_CLASS_ALU, ADDRESS_MODE_ABSOLUTE),
    [0x76] = OPCODE(OPCODE_ROR, OPCODE_CLASS_ALU, ADDRESS_MODE_DIRECT_INDEXED_X),
    [0x7e] = OPCODE(OPCODE_ROR, OPCODE_CLASS_ALU, ADDRESS_MODE_ABSOLUTE_INDEXED_X),

    [0x40] = OPCODE(OPCODE_RTI, OPCODE_CLASS_BRANCH, ADDRESS_MODE_STACK),
    [0x6b] = OPCODE(OPCODE_RTL, OPCODE_CLASS_BRANCH, ADDRESS_MODE_STACK),
    [0x60] = OPCODE(OPCODE_RTS, OPCODE_CLASS_BRANCH, ADDRESS_MODE_STACK),

    [0xe1] = OPCODE(OPCODE_SBC, OPCODE_CLASS_ALU, ADDRESS_MODE_DIRECT_INDEXED_INDIRECT),
    [0xe3] = OPCODE(OPCODE_SBC, OPCODE_CLASS_ALU, ADDRESS_MODE_STACK_RELATIVE),
    [0xe5] = OPCODE(OPCODE_SBC, OPCODE_CLASS_ALU, ADDRESS_MODE_DIRECT),
    [0xe7] = OPCODE(OPCODE_SBC, OPCODE_CLASS_ALU, ADDRESS_MODE_DIRECT_INDIRECT_LONG),
    [0xe9] = OPCODE(OPCODE_SBC, OPCODE_CLASS_ALU, ADDRESS_MODE_IMMEDIATE),
    [0xed] = OPCODE(OPCODE_SBC, OPCODE_CLASS_ALU, ADDRESS_MODE_ABSOLUTE),
    [0xef] = OPCODE(OPCODE_SBC, OPCODE_CLASS_ALU, ADDRESS_MODE_ABSOLUTE_LONG),
    [0xf1] = OPCODE(OPCODE_SBC, OPCODE_CLASS_ALU, ADDRESS_MODE_DIRECT_INDIRECT_INDEXED),
    [0xf2] = OPCODE(OPCODE_SBC, OPCODE_CLASS_ALU, ADDRESS_MODE_DIRECT_INDIRECT),
    [0xf3] = OPCODE(OPCODE_SBC, OPCODE_CLASS_ALU, ADDRESS_MODE_STACK_RELATIVE_INDIRECT_INDEXED),
    [0xf5] = OPCODE(OPCODE_SBC, OPCODE_CLASS_ALU, ADDRESS_MODE_DIRECT_INDEXED_X),
    [0xf7] = OPCODE(OPCODE_SBC, OPCODE_CLASS_ALU, ADDRESS_MODE_DIRECT_INDIRECT_LONG_INDEXED),
    [0xf9] = OPCODE(OPCODE_SBC, OPCODE_CLASS_ALU, ADDRESS_MODE_ABSOLUTE_INDEXED_Y),
    [0xfd] = OPCODE(OPCODE_SBC, OPCODE_CLASS_ALU, ADDRESS_MODE_ABSOLUTE_INDEXED_X),
    [0xff] = OPCODE(OPCODE_SBC, OPCODE_CLASS_ALU, ADDRESS_MODE_ABSOLUTE_LONG_INDEXED_X),
        
    [0xdb] = OPCODE(OPCODE_STP, OPCODE_CLASS_STANDALONE, ADDRESS_MODE_IMPLIED),

    [0x81] = OPCODE(OPCODE_STA, OPCODE_CLASS_STORE, ADDRESS_MODE_DIRECT_INDEXED_INDIRECT),
    [0x83] = OPCODE(OPCODE_STA, OPCODE_CLASS_STORE, ADDRESS_MODE_STACK_RELATIVE),
    [0x85] = OPCODE(OPCODE_STA, OPCODE_CLASS_STORE, ADDRESS_MODE_DIRECT),
    [0x87] = OPCODE(OPCODE_STA, OPCODE_CLASS_STORE, ADDRESS_MODE_DIRECT_INDIRECT_LONG),
    [0x8d] = OPCODE(OPCODE_STA, OPCODE_CLASS_STORE, ADDRESS_MODE_ABSOLUTE),
    [0x8f] = OPCODE(OPCODE_STA, OPCODE_CLASS_STORE, ADDRESS_MODE_ABSOLUTE_LONG),
    [0x91] = OPCODE(OPCODE_STA, OPCODE_CLASS_STORE, ADDRESS_MODE_DIRECT_INDIRECT_INDEXED),
    [0x92] = OPCODE(OPCODE_STA, OPCODE_CLASS_STORE, ADDRESS_MODE_DIRECT_INDIRECT),
    [0x93] = OPCODE(OPCODE_STA, OPCODE_CLASS_STORE, ADDRESS_MODE_STACK_RELATIVE_INDIRECT_INDEXED),
    [0x95] = OPCODE(OPCODE_STA, OPCODE_CLASS_STORE, ADDRESS_MODE_DIRECT_INDEXED_X),
    [0x97] = OPCODE(OPCODE_STA, OPCODE_CLASS_STORE, ADDRESS_MODE_DIRECT_INDIRECT_LONG_INDEXED),
    [0x99] = OPCODE(OPCODE_STA, OPCODE_CLASS_STORE, ADDRESS_MODE_ABSOLUTE_INDEXED_Y),
    [0x9d] = OPCODE(OPCODE_STA, OPCODE_CLASS_STORE, ADDRESS_MODE_ABSOLUTE_INDEXED_X),
    [0x9f] = OPCODE(OPCODE_STA, OPCODE_CLASS_STORE, ADDRESS_MODE_ABSOLUTE_LONG_INDEXED_X),
        
    [0x86] = OPCODE(OPCODE_STX, OPCODE_CLASS_STORE, ADDRESS_MODE_DIRECT),
    [0x8e] = OPCODE(OPCODE_STX, OPCODE_CLASS_STORE, ADDRESS_MODE_ABSOLUTE),
    [0x96] = OPCODE(OPCODE_STX, OPCODE_CLASS_STORE, ADDRESS_MODE_DIRECT_INDEXED_Y),

    [0x84] = OPCODE(OPCODE_STY, OPCODE_CLASS_STORE, ADDRESS_MODE_DIRECT),
    [0x8c] = OPCODE(OPCODE_STY, OPCODE_CLASS_STORE, ADDRESS_MODE_ABSOLUTE),
    [0x94] = OPCODE(OPCODE_STY, OPCODE_CLASS_STORE, ADDRESS_MODE_DIRECT_INDEXED_X),

    [0x64] = OPCODE(OPCODE_STZ, OPCODE_CLASS_STORE, ADDRESS_MODE_DIRECT),
    [0x74] = OPCODE(OPCODE_STZ, OPCODE_CLASS_STORE, ADDRESS_MODE_DIRECT_INDEXED_X),
    [0x9c] = OPCODE(OPCODE_STZ, OPCODE_CLASS_STORE, ADDRESS_MODE_ABSOLUTE),
    [0x9e] = OPCODE(OPCODE_STZ, OPCODE_CLASS_STORE, ADDRESS_MODE_ABSOLUTE_INDEXED_X),
        
    [0xaa] = OPCODE(OPCODE_TAX, OPCODE_CLASS_TRANSFER, ADDRESS_MODE_IMPLIED),
    [0xa8] = OPCODE(OPCODE_TAY, OPCODE_CLASS_TRANSFER, ADDRESS_MODE_IMPLIED),
    [0x5b] = OPCODE(OPCODE_TCD, OPCODE_CLASS_TRANSFER, ADDRESS_MODE_IMPLIED),
    [0x1b] = OPCODE(OPCODE_TCS, OPCODE_CLASS_TRANSFER, ADDRESS_MODE_IMPLIED),
    [0x7b] = OPCODE(OPCODE_TDC, OPCODE_CLASS_TRANSFER, ADDRESS_MODE_IMPLIED),

    [0x14] = OPCODE(OPCODE_TRB, OPCODE_CLASS_ALU, ADDRESS_MODE_DIRECT),
    [0x1c] = OPCODE(OPCODE_TRB, OPCODE_CLASS_ALU, ADDRESS_MODE_ABSOLUTE),
        
    [0x04] = OPCODE(OPCODE_TSB, OPCODE_CLASS_ALU, ADDRESS_MODE_DIRECT),
    [0x0c] = OPCODE(OPCODE_TSB, OPCODE_CLASS_ALU, ADDRESS_MODE_ABSOLUTE),

    [0x3b] = OPCODE(OPCODE_TSC, OPCODE_CLASS_TRANSFER, ADDRESS_MODE_ACCUMULATOR),
    [0xba] = OPCODE(OPCODE_TSX, OPCODE_CLASS_TRANSFER, ADDRESS_MODE_ACCUMULATOR),
    [0x8a] = OPCODE(OPCODE_TXA, OPCODE_CLASS_TRANSFER, ADDRESS_MODE_ACCUMULATOR),
    [0x9a] = OPCODE(OPCODE_TXS, OPCODE_CLASS_TRANSFER, ADDRESS_MODE_ACCUMULATOR),
    [0x9b] = OPCODE(OPCODE_TXY, OPCODE_CLASS_TRANSFER, ADDRESS_MODE_ACCUMULATOR),
    [0x98] = OPCODE(OPCODE_TYA, OPCODE_CLASS_TRANSFER, ADDRESS_MODE_ACCUMULATOR),
    [0xbb] = OPCODE(OPCODE_TYX, OPCODE_CLASS_TRANSFER, ADDRESS_MODE_ACCUMULATOR),
    [0xcb] = OPCODE(OPCODE_WAI, OPCODE_CLASS_STANDALONE, ADDRESS_MODE_ACCUMULATOR),
    [0x42] = OPCODE(OPCODE_WDM, OPCODE_CLASS_STANDALONE, ADDRESS_MODE_ACCUMULATOR),
    [0xeb] = OPCODE(OPCODE_XBA, OPCODE_CLASS_STANDALONE, ADDRESS_MODE_ACCUMULATOR),
    [0xfb] = OPCODE(OPCODE_XCE, OPCODE_CLASS_STANDALONE, ADDRESS_MODE_ACCUMULATOR)
};

struct branch_cond_t branch_conds[5] = 
{
    [0] = {CPU_STATUS_FLAG_CARRY},
    [1] = {CPU_STATUS_FLAG_ZERO},
    [2] = {CPU_STATUS_FLAG_NEGATIVE},
    [3] = {CPU_STATUS_FLAG_OVERFLOW},
    [4] = {0xff}
};

struct transfer_params_t transfer_params[] = 
{
    [TPARM_INDEX(OPCODE_TAX)] = {.src_reg = &cpu_state.reg_accum.reg_accumC, .dst_reg = &cpu_state.reg_x.reg_x,          .flag = CPU_STATUS_FLAG_INDEX},
    [TPARM_INDEX(OPCODE_TAY)] = {.src_reg = &cpu_state.reg_accum.reg_accumC, .dst_reg = &cpu_state.reg_y.reg_y,          .flag = CPU_STATUS_FLAG_INDEX},
    [TPARM_INDEX(OPCODE_TCD)] = {.src_reg = &cpu_state.reg_accum.reg_accumC, .dst_reg = &cpu_state.reg_d},
    [TPARM_INDEX(OPCODE_TCS)] = {.src_reg = &cpu_state.reg_accum.reg_accumC, .dst_reg = &cpu_state.reg_s},
    [TPARM_INDEX(OPCODE_TDC)] = {.src_reg = &cpu_state.reg_d,                .dst_reg = &cpu_state.reg_accum.reg_accumC, .flag = CPU_STATUS_FLAG_MEMORY},
    [TPARM_INDEX(OPCODE_TSC)] = {.src_reg = &cpu_state.reg_s,                .dst_reg = &cpu_state.reg_accum.reg_accumC, .flag = CPU_STATUS_FLAG_MEMORY},
    [TPARM_INDEX(OPCODE_TSX)] = {.src_reg = &cpu_state.reg_s,                .dst_reg = &cpu_state.reg_x.reg_x,          .flag = CPU_STATUS_FLAG_INDEX},
    [TPARM_INDEX(OPCODE_TXA)] = {.src_reg = &cpu_state.reg_x.reg_x,          .dst_reg = &cpu_state.reg_accum.reg_accumC, .flag = CPU_STATUS_FLAG_MEMORY},
    [TPARM_INDEX(OPCODE_TXS)] = {.src_reg = &cpu_state.reg_x.reg_x,          .dst_reg = &cpu_state.reg_s},
    [TPARM_INDEX(OPCODE_TXY)] = {.src_reg = &cpu_state.reg_x.reg_x,          .dst_reg = &cpu_state.reg_y.reg_y,          .flag = CPU_STATUS_FLAG_INDEX},
    [TPARM_INDEX(OPCODE_TYA)] = {.src_reg = &cpu_state.reg_y.reg_y,          .dst_reg = &cpu_state.reg_accum.reg_accumC, .flag = CPU_STATUS_FLAG_MEMORY},
    [TPARM_INDEX(OPCODE_TYX)] = {.src_reg = &cpu_state.reg_y.reg_y,          .dst_reg = &cpu_state.reg_x.reg_x,          .flag = CPU_STATUS_FLAG_INDEX},
};

uint8_t carry_flag_alu_op[ALU_OP_LAST] = 
{
    [ALU_OP_ADD] = 1,
    [ALU_OP_SUB] = 1,
    [ALU_OP_SHL] = 1,
    [ALU_OP_SHR] = 1,
    [ALU_OP_ROR] = 1,
    [ALU_OP_ROL] = 1,
    [ALU_OP_CMP] = 1
};




char *memory_str(unsigned int effective_address)
{
    switch(effective_address)
    {
        case 0x2100:
            return "INIDISP";
        break;

        case 0x2101:
            return "OBJSEL";
        break;

        case 0x2102:
            return "OAMADD";
        break;

        case 0x2104:
            return "OAM DATA";
        break;

        case 0x2105:
            return "BG MODE";
        break;

        case 0x2106:
            return "MOSAIC";
        break;

        case 0x2107:
            return "BG1SC";
        break;

        case 0x2108:
            return "BG2SC";
        break;

        case 0x2109:
            return "BG3SC";
        break;

        case 0x210a:
            return "BG4SC";
        break;

        case 0x210b:
            return "BG12NBA";
        break;

        case 0x210c:
            return "BG34NBA";
        break;



        case 0x210d:
            return "BG1H0FS";
        break;

        case 0x210e:
            return "BG1V0FS";
        break;

        case 0x210f:
            return "BG2H0FS";
        break;

        case 0x2110:
            return "BG2V0FS";
        break;

        case 0x2111:
            return "BG3H0FS";
        break;

        case 0x2112:
            return "BG3V0FS";
        break;

        case 0x2113:
            return "BG4H0FS";
        break;

        case 0x2114:
            return "BG4V0FS";
        break;



        case 0x2115:
            return "VMAINC";
        break;

        case 0x2116:
            return "VMADDL";
        break;

        case 0x2117:
            return "VMADDH";
        break;

        case 0x2118:
            return "VMDATAL";
        break;

        case 0x2119:
            return "VMDATAH";
        break;

        case 0x211a:
            return "M7SEL";
        break;



        case 0x211b:
            return "M7A";
        break;

        case 0x211c:
            return "M7B";
        break;

        case 0x211d:
            return "M7C";
        break;

        case 0x211e:
            return "M7D";
        break;

        case 0x211f:
            return "M7X";
        break;

        case 0x2120:
            return "M7Y";
        break;


        case 0x2121:
            return "CGADD";
        break;

        case 0x2122:
            return "CGDATA";
        break;

        case 0x2123:
            return "W12SEL";
        break;

        case 0x2124:
            return "W34SEL";
        break;

        case 0x2125:
            return "WOBJSEL";
        break;


        case 0x2126:
            return "WH0";
        break;

        case 0x2127:
            return "WH1";
        break;

        case 0x2128:
            return "WH2";
        break;

        case 0x2129:
            return "WH3";
        break;


        case 0x4200:
            return "NMITIMEN";
        break;

        case 0x4201:
            return "WRIO";
        break;

        case 0x4202:
            return "WRMPYA";
        break;

        case 0x4203:
            return "WRMPYB";
        break;

        case 0x420d:
            return "MEMSEL";
        break;

        case 0x4212:
            return "HVBJOY";
        break;

        default:
            return NULL;
        break;
    }
}


uint32_t opcode_width(struct opcode_t *opcode)
{
    uint32_t width = 0;
    switch(opcode->address_mode)
    {
        case ADDRESS_MODE_ABSOLUTE:
        case ADDRESS_MODE_ABSOLUTE_INDEXED_INDIRECT:
        case ADDRESS_MODE_ABSOLUTE_INDEXED_X:
        case ADDRESS_MODE_ABSOLUTE_INDEXED_Y:
        case ADDRESS_MODE_BLOCK_MOVE:
        case ADDRESS_MODE_PROGRAM_COUNTER_RELATIVE_LONG:
            width = 3;
        break;
        
        case ADDRESS_MODE_ABSOLUTE_INDIRECT:
            if(opcode->opcode == OPCODE_JML)
            {
                width = 4;
            }
            else
            {
                width = 3;
            }
        break;
        
        case ADDRESS_MODE_ABSOLUTE_LONG:
            width = 4;
        break;
        
        case ADDRESS_MODE_ACCUMULATOR:
        case ADDRESS_MODE_IMPLIED:
        case ADDRESS_MODE_STACK:
            width = 1;
        break;
        
        case ADDRESS_MODE_DIRECT_INDEXED_INDIRECT:
        case ADDRESS_MODE_DIRECT_INDEXED_X:
        case ADDRESS_MODE_DIRECT_INDEXED_Y:
        case ADDRESS_MODE_DIRECT_INDIRECT_INDEXED:
        case ADDRESS_MODE_DIRECT_INDIRECT_LONG_INDEXED:
        case ADDRESS_MODE_DIRECT_INDIRECT_LONG:
        case ADDRESS_MODE_DIRECT_INDIRECT:
        case ADDRESS_MODE_DIRECT:
        case ADDRESS_MODE_PROGRAM_COUNTER_RELATIVE:
        case ADDRESS_MODE_STACK_RELATIVE:
        case ADDRESS_MODE_STACK_RELATIVE_INDIRECT_INDEXED:
            width = 2;
        break;
        
        case ADDRESS_MODE_IMMEDIATE:
            switch(opcode->opcode)
            {
                case OPCODE_LDA:
                case OPCODE_CMP:
                case OPCODE_AND:
                case OPCODE_EOR:
                case OPCODE_ORA:
                    if(cpu_state.reg_p & CPU_STATUS_FLAG_MEMORY)
                    {
                        width = 2;
                    }
                    else
                    {
                        width = 3;
                    }
                break;
                    
                case OPCODE_LDX:
                case OPCODE_LDY:
                case OPCODE_CPX:
                case OPCODE_CPY:
                    if(cpu_state.reg_p & CPU_STATUS_FLAG_INDEX)
                    {
                        width = 2;
                    }
                    else
                    {
                        width = 3;
                    }
                break;
                
                default:
                    width = 2;
                break;
            }
        break;
    }
    
    return width;
}

char instruction_str_buffer[512];

char *instruction_str(unsigned int effective_address)
{
//    char *opcode_str;
    char *mem_str;
    unsigned char *opcode_address;
    char *opcode_str;
    char *mem_address;
    char addr_mode_str[128];
    char temp_str[64];
    int32_t width = 0;
    uint32_t address = 0;
    struct opcode_t op_code;

    opcode_address = memory_pointer(effective_address);
    op_code = opcode_matrix[opcode_address[0]];
    width = opcode_width(&op_code);

//    for(int32_t i = width - 1; i > 0; i--)
//    {
//        address <<= 8;
//        address |= opcode_address[i];
//    }

    switch(op_code.address_mode)
    {
        case ADDRESS_MODE_ABSOLUTE:
            strcpy(addr_mode_str, "absolute addr(");
            sprintf(temp_str, "%02x", cpu_state.reg_dbrw.reg_dbr);
            strcat(addr_mode_str, temp_str);
            for(int32_t i = width - 1; i > 0; i--)
            {
                sprintf(temp_str, "%02x", opcode_address[i]);
                strcat(addr_mode_str, temp_str);
            }
            strcat(addr_mode_str, ")");
        break;

        case ADDRESS_MODE_ABSOLUTE_INDEXED_INDIRECT:
            strcpy(addr_mode_str, "absolute addr( pointer (");
            for(int32_t i = width - 1; i > 0; i--)
            {
                sprintf(temp_str, "%02x", opcode_address[i]);
                strcat(addr_mode_str, temp_str);
            }
            
            strcat(addr_mode_str, ") + ");
            sprintf(temp_str, "X(%04x) )", cpu_state.reg_x.reg_x);
            strcat(addr_mode_str, temp_str);
        break;

        case ADDRESS_MODE_ABSOLUTE_INDEXED_X:
            strcpy(addr_mode_str, "absolute addr(");
            for(int32_t i = width - 1; i > 0; i--)
            {
                sprintf(temp_str, "%02x", opcode_address[i]);
                strcat(addr_mode_str, temp_str);
            }

            strcat(addr_mode_str, ") + ");
            sprintf(temp_str, "X(%04x)", cpu_state.reg_x.reg_x);
            strcat(addr_mode_str, temp_str);
        break;

        case ADDRESS_MODE_ABSOLUTE_INDEXED_Y:
            strcpy(addr_mode_str, "absolute addr(");
            for(int32_t i = width - 1; i > 0; i--)
            {
                sprintf(temp_str, "%02x", opcode_address[i]);
                strcat(addr_mode_str, temp_str);
            }

            strcat(addr_mode_str, ") + ");
            sprintf(temp_str, "Y(%04x)", cpu_state.reg_y.reg_y);
            strcat(addr_mode_str, temp_str);
        break;

        case ADDRESS_MODE_ABSOLUTE_INDIRECT:
            strcpy(addr_mode_str, "absolute addr( pointer (");
            for(int32_t i = width - 1; i > 0; i--)
            {
                sprintf(temp_str, "%02x", opcode_address[i]);
                strcat(addr_mode_str, temp_str);
            }
            strcat(addr_mode_str, ") )");
        break;

        case ADDRESS_MODE_ABSOLUTE_LONG_INDEXED_X:
            strcpy(addr_mode_str, "absolute long addr(");
            for(int32_t i = width - 1; i > 0; i--)
            {
                sprintf(temp_str, "%02x", opcode_address[i]);
                strcat(addr_mode_str, temp_str);
            }

            strcat(addr_mode_str, ") + ");
            sprintf(temp_str, "X(%04x)", cpu_state.reg_x.reg_x);
            strcat(addr_mode_str, temp_str);
        break;

        case ADDRESS_MODE_ABSOLUTE_LONG:
            strcpy(addr_mode_str, "absolute long addr(");
            for(int32_t i = width - 1; i > 0; i--)
            {
                sprintf(temp_str, "%02x", opcode_address[i]);
                strcat(addr_mode_str, temp_str);
            }

            strcat(addr_mode_str, ")");
        break;

        case ADDRESS_MODE_ACCUMULATOR:
            sprintf(addr_mode_str, "accumulator(%04x)", cpu_state.reg_accum.reg_accumC);
        break;

        case ADDRESS_MODE_BLOCK_MOVE:
            sprintf(addr_mode_str, "dst addr(%02x:%04x), src addr(%02x:%04x)", opcode_address[1], cpu_state.reg_y.reg_y,
                                                                               opcode_address[2], cpu_state.reg_x.reg_x);
        break;

        case ADDRESS_MODE_DIRECT_INDEXED_INDIRECT:
            strcpy(addr_mode_str, "direct indexed indirect - (d,x)");
        break;

        case ADDRESS_MODE_DIRECT_INDEXED_X:
            sprintf(addr_mode_str, "D(%04x) + X(%04x)", cpu_state.reg_d, cpu_state.reg_x.reg_x);
        break;

        case ADDRESS_MODE_DIRECT_INDEXED_Y:
            sprintf(addr_mode_str, "D(%04x) + Y(%04x)", cpu_state.reg_d, cpu_state.reg_y.reg_y);
        break;

        case ADDRESS_MODE_DIRECT_INDIRECT_INDEXED:
            mem_address = memory_pointer(cpu_state.reg_d + opcode_address[1]);
            sprintf(addr_mode_str, "[pointer(D(%04x) + offset(%02x))](%04x) + Y(%04x)", cpu_state.reg_d, opcode_address[1],
                                                                                            *(unsigned short *)mem_address, cpu_state.reg_y.reg_y);
        break;

        case ADDRESS_MODE_DIRECT_INDIRECT_LONG_INDEXED:
            strcpy(addr_mode_str, "direct indirect long indexed - [d],y");
        break;

        case ADDRESS_MODE_DIRECT_INDIRECT_LONG:
            strcpy(addr_mode_str, "direct indirect long - [d]");
        break;

        case ADDRESS_MODE_DIRECT_INDIRECT:
            sprintf(addr_mode_str, "pointer(D(%04x) + offset(%02x))", cpu_state.reg_d, opcode_address[1]);
        break;

        case ADDRESS_MODE_DIRECT:
            sprintf(addr_mode_str, "D(%04x) + offset(%02x)", cpu_state.reg_d, opcode_address[1]);
        break;

        case ADDRESS_MODE_IMMEDIATE:
            strcpy(addr_mode_str, "immediate (");

            for(int32_t i = width - 1; i > 0; i--)
            {
                sprintf(temp_str, "%02x", opcode_address[i]);
                strcat(addr_mode_str, temp_str);
            }
            strcat(addr_mode_str, ")");
        break;

        case ADDRESS_MODE_IMPLIED:
            strcpy(addr_mode_str, "");
        break;

        case ADDRESS_MODE_PROGRAM_COUNTER_RELATIVE_LONG:
            strcpy(addr_mode_str, "program counter relative long - rl");
        break;

        case ADDRESS_MODE_PROGRAM_COUNTER_RELATIVE:
            sprintf(addr_mode_str, "pc(%04x) + offset(%02x)", cpu_state.reg_pc, opcode_address[1]);
        break;

        case ADDRESS_MODE_STACK:
            addr_mode_str[0] = '\0';
            //strcpy(addr_mode_str, "stack - s");
        break;

        case ADDRESS_MODE_STACK_RELATIVE:
            sprintf(addr_mode_str, "S(%04x) + offset(%02x)", cpu_state.reg_s, opcode_address[1]);
        break;

        case ADDRESS_MODE_STACK_RELATIVE_INDIRECT_INDEXED:
            strcpy(addr_mode_str, "stack relative indirect indexed - (d,s),y");
        break;

        default:
        case ADDRESS_MODE_UNKNOWN:
            strcpy(addr_mode_str, "unknown");
        break;
    }
    
    
    switch(op_code.opcode)
    {
        case OPCODE_ADC:
            opcode_str = "ADC";
        break;

        case OPCODE_AND:
            opcode_str = "AND";
        break;

        case OPCODE_ASL:
            opcode_str = "ASL";
        break;

        case OPCODE_BCC:
            opcode_str = "BCC";
        break;

        case OPCODE_BCS:
            opcode_str = "BCS";
        break;

        case OPCODE_BEQ:
            opcode_str = "BEQ";
        break;

        case OPCODE_BMI:
            opcode_str = "BMI";
        break;

        case OPCODE_BNE:
            opcode_str = "BNE";
        break;

        case OPCODE_BPL:
            opcode_str = "BPL";
        break;

        case OPCODE_BRA:
            opcode_str = "BRA";
        break;

        case OPCODE_BRK:
            opcode_str = "BRK";
        break;

        case OPCODE_BRL:
            opcode_str = "BRL";
        break;

        case OPCODE_BVC:
            opcode_str = "BVC";
        break;

        case OPCODE_BVS:
            opcode_str = "BVS";
        break;

        case OPCODE_BIT:
            opcode_str = "BIT";
        break;

        case OPCODE_CLC:
            opcode_str = "CLC";
        break;

        case OPCODE_CLD:
            opcode_str = "CLD";
        break;

        case OPCODE_CLV:
            opcode_str = "CLV";
        break;

        case OPCODE_CLI:
            opcode_str = "CLI";
        break;

        case OPCODE_CMP:
            opcode_str = "CMP";
        break;

        case OPCODE_CPX:
            opcode_str = "CPX";
        break;

        case OPCODE_CPY:
            opcode_str = "CPY";
        break;

        case OPCODE_DEC:
            opcode_str = "DEC";
        break;

        case OPCODE_INC:
            opcode_str = "INC";
        break;

        case OPCODE_DEX:
            opcode_str = "DEX";
        break;

        case OPCODE_INX:
            opcode_str = "INX";
        break;

        case OPCODE_DEY:
            opcode_str = "DEY";
        break;

        case OPCODE_INY:
            opcode_str = "INY";
        break;

        case OPCODE_JMP:
            opcode_str = "JMP";
        break;

        case OPCODE_JML:
            opcode_str = "JML";
        break;

        case OPCODE_JSL:
            opcode_str = "JSL";
        break;

        case OPCODE_JSR:
            opcode_str = "JSR";
        break;

        case OPCODE_LDA:
            opcode_str = "LDA";
        break;

        case OPCODE_STA:
            opcode_str = "STA";
        break;

        case OPCODE_LDX:
            opcode_str = "LDX";
        break;

        case OPCODE_STX:
            opcode_str = "STX";
        break;

        case OPCODE_LDY:
            opcode_str = "LDY";
        break;

        case OPCODE_STY:
            opcode_str = "STY";
        break;

        case OPCODE_STZ:
            opcode_str = "STZ";
        break;

        case OPCODE_LSR:
            opcode_str = "LSR";
        break;

        case OPCODE_MVN:
            opcode_str = "MVN";
        break;

        case OPCODE_MVP:
            opcode_str = "MVP";
        break;

        case OPCODE_NOP:
            opcode_str = "NOP";
        break;

        case OPCODE_PEA:
            opcode_str = "PEA";
        break;

        case OPCODE_PEI:
            opcode_str = "PEI";
        break;

        case OPCODE_PER:
            opcode_str = "PER";
        break;

        case OPCODE_PHA:
            opcode_str = "PHA";
        break;

        case OPCODE_PHB:
            opcode_str = "PHB";
        break;

        case OPCODE_PHD:
            opcode_str = "PHD";
        break;

        case OPCODE_PHK:
            opcode_str = "PHK";
        break;

        case OPCODE_PHP:
            opcode_str = "PHP";
        break;

        case OPCODE_PHX:
            opcode_str = "PHX";
        break;

        case OPCODE_PHY:
            opcode_str = "PHY";
        break;

        case OPCODE_PLA:
            opcode_str = "PLA";
        break;

        case OPCODE_PLB:
            opcode_str = "PLB";
        break;

        case OPCODE_PLD:
            opcode_str = "PLD";
        break;

        case OPCODE_PLP:
            opcode_str = "PLP";
        break;

        case OPCODE_PLX:
            opcode_str = "PLX";
        break;

        case OPCODE_PLY:
            opcode_str = "PLY";
        break;

        case OPCODE_REP:
            opcode_str = "REP";
        break;

        case OPCODE_ROL:
            opcode_str = "ROL";
        break;

        case OPCODE_ROR:
            opcode_str = "ROR";
        break;

        case OPCODE_RTI:
            opcode_str = "RTI";
        break;

        case OPCODE_RTL:
            opcode_str = "RTL";
        break;

        case OPCODE_RTS:
            opcode_str = "RTS";
        break;

        case OPCODE_SBC:
            opcode_str = "SBC";
        break;

        case OPCODE_SEP:
            opcode_str = "SEP";
        break;

        case OPCODE_SEC:
            opcode_str = "SEC";
        break;

        case OPCODE_SED:
            opcode_str = "SED";
        break;

        case OPCODE_SEI:
            opcode_str = "SEI";
        break;

        case OPCODE_TAX:
            opcode_str = "TAX";
        break;

        case OPCODE_TAY:
            opcode_str = "TAY";
        break;

        case OPCODE_TCD:
            opcode_str = "TCD";
        break;

        case OPCODE_TCS:
            opcode_str = "TCS";
        break;

        case OPCODE_TDC:
            opcode_str = "TDC";
        break;

        case OPCODE_TSC:
            opcode_str = "TSC";
        break;

        case OPCODE_TSX:
            opcode_str = "TSX";
        break;

        case OPCODE_TXA:
            opcode_str = "TXA";
        break;

        case OPCODE_TXS:
            opcode_str = "TXS";
        break;

        case OPCODE_TXY:
            opcode_str = "TXY";
        break;

        case OPCODE_TYA:
            opcode_str = "TYA";
        break;

        case OPCODE_TYX:
            opcode_str = "TYX";
        break;

        case OPCODE_WAI:
            opcode_str = "WAI";
        break;

        case OPCODE_WDM:
            opcode_str = "WDM";
        break;

        case OPCODE_XBA:
            opcode_str = "XBA";
        break;

        case OPCODE_TRB:
            opcode_str = "TRB";
        break;

        case OPCODE_TSB:
            opcode_str = "TSB";
        break;

        case OPCODE_STP:
            opcode_str = "STP";
        break;

        case OPCODE_COP:
            opcode_str = "COP";
        break;

        case OPCODE_EOR:
            opcode_str = "EOR";
        break;

        case OPCODE_ORA:
            opcode_str = "ORA";
        break;

        case OPCODE_XCE:
            opcode_str = "XCE";
        break;

        default:
        case OPCODE_UNKNOWN:
            opcode_str = "UNKNOWN";
        break;
    }

    sprintf(instruction_str_buffer, "[%02x:%04x]: ", (effective_address >> 16) & 0xff, effective_address & 0xffff);

    for(int32_t i = 0; i < width; i++)
    {
        sprintf(temp_str, "%02x ", opcode_address[i]);
        strcat(instruction_str_buffer, temp_str);
    }

    strcat(instruction_str_buffer, "; ");
    strcat(instruction_str_buffer, opcode_str);

    if(addr_mode_str[0])
    {
        strcat(instruction_str_buffer, " ");
        strcat(instruction_str_buffer, addr_mode_str);
    }

    return instruction_str_buffer;
}

void disassemble(int start, int byte_count)
{
//    int bytes_disassmd = 0;
//    int opcode_offset;
//
//    if(start >= 0)
//    {
//        cpu_state.reg_pc = start & 0xffff;
//        cpu_state.reg_pbrw.reg_pbr = (start >> 16) & 0xff;
//    }
//
//    while(bytes_disassmd < byte_count)
//    {
//        opcode_offset = disassemble_current(0);
//
//        bytes_disassmd += opcode_offset;
//
//        if((unsigned short)(cpu_state.reg_pc + (unsigned short)opcode_offset) < (unsigned short)cpu_state.reg_pc)
//        {
//            cpu_state.reg_pbrw.reg_pbr++;
//        }
//
//        reg_pc += opcode_offset;
//    }
}

int dump_cpu(int show_registers)
{
    int address;
    int opcode_offset;
    struct opcode_t opcode;
//    unsigned char opcode_byte;
//    int i;
    char *op_str;
    unsigned char *opcode_address;

    address = EFFECTIVE_ADDRESS(cpu_state.reg_pbrw.reg_pbr, cpu_state.reg_pc);
//    opcode_address = memory_pointer(address);
//    opcode = opcode_matrix[opcode_address[0]];
    op_str = instruction_str(address);

    printf("cycles elapsed: %d\n\n", cpu_cycle_count);


//    printf("cpu step: %lu\n", step_count);
    if(show_registers)
    {
        printf("=========== REGISTERS ===========\n");
        printf("[A: %04x] | ", cpu_state.reg_accum.reg_accumC);
        printf("[X: %04x] | ", cpu_state.reg_x.reg_x);
        printf("[Y: %04x] | ", cpu_state.reg_y.reg_y);
        printf("[S: %04x] | ", cpu_state.reg_s);
        printf("[D: %04x]\n", cpu_state.reg_d);
        printf("[DBR: %02x] | ", cpu_state.reg_dbrw.reg_dbr);
        printf("[PBR: %02x] | ", cpu_state.reg_pbrw.reg_pbr);
//        printf("[MASK: %02x] | ", current_rom.header->data.rom_makeup);
        printf("[P: N:%d V:%d M:%d X:%d D:%d Z:%d I:%d C:%d]\n", (cpu_state.reg_p & CPU_STATUS_FLAG_NEGATIVE) && 1,
                                                                 (cpu_state.reg_p & CPU_STATUS_FLAG_OVERFLOW) && 1,
                                                                 (cpu_state.reg_p & CPU_STATUS_FLAG_MEMORY) && 1,
                                                                 (cpu_state.reg_p & CPU_STATUS_FLAG_INDEX) && 1,
                                                                 (cpu_state.reg_p & CPU_STATUS_FLAG_DECIMAL) && 1,
                                                                 (cpu_state.reg_p & CPU_STATUS_FLAG_ZERO) && 1,
                                                                 (cpu_state.reg_p & CPU_STATUS_FLAG_IRQ_DISABLE) && 1,
                                                                 (cpu_state.reg_p & CPU_STATUS_FLAG_CARRY) && 1);
        printf("[E: %02x] | [PC: %04x]\n", cpu_state.reg_e, cpu_state.reg_pc);
        printf("=========== REGISTERS ===========\n");
    }

    printf("%s\n", op_str);

    return 0;
}

void poke(uint32_t effective_address, uint32_t *value)
{
    unsigned char *memory;

    memory = memory_pointer(effective_address);

    if(memory)
    {
        printf("[%02x:%04x] %02x %02x %02x %02x ", (effective_address >> 16) & 0xff, effective_address & 0xffff,
                                                        memory[0], memory[1], memory[2], memory[3]);

        if(value)
        {
            if(*value & 0x00ff0000)
            {
                memcpy(memory, value, 3);
            }
            else if(*value & 0x0000ff00)
            {
                memcpy(memory, value, 2);
            }
            else
            {
                memcpy(memory, value, 1);
            }

            printf("-> %02x %02x %02x %02x ", memory[0], memory[1], memory[2], memory[3]);
        }

        printf("\n");
    }
}

int view_hardware_registers()
{
    int i;

    for(i = 0; i < sizeof(hardware_regs.hardware_regs); i++)
    {
        printf("<0x%04x>: 0x%02x\n", CPU_REGS_START_ADDRESS + i, hardware_regs.hardware_regs[i]);
    }
}

void *cpu_pointer(uint32_t effective_address, uint32_t access_location)
{
    void *pointer = NULL;

    switch(access_location)
    {
        case ACCESS_LOCATION_REGS:
            pointer = (void *)(hardware_regs.hardware_regs + (effective_address & 0xffff) - CPU_REGS_START_ADDRESS);
        break;

        case ACCESS_LOCATION_WRAM1:
            pointer = (void *)(wram1 + (effective_address & 0xffff) - RAM1_START_ADDRESS);
        break;
    }
    
    return pointer;
}


void cpu_write_byte(uint32_t effective_address, uint8_t data)
{
    cpu_cycle_count += ACCESS_CYCLES(effective_address);
    write_data(effective_address, data, 1);
}

void cpu_write_word(uint32_t effective_address, uint16_t data)
{
    cpu_write_byte(effective_address, data);
    cpu_write_byte(effective_address + 1, (data >> 8) & 0xff);
}


void cpu_regs_write(uint32_t effective_address, uint32_t data, uint32_t byte_write)
{
    void *pointer = (void *)(hardware_regs.hardware_regs + (effective_address & 0xffff) - CPU_REGS_START_ADDRESS);
    
    if(byte_write)
    {
        *(uint8_t *)pointer = data;
    }
    else
    {
        *(uint16_t *)pointer = data;
    }
}

void cpu_wram1_write(uint32_t effective_address, uint32_t data, uint32_t byte_write)
{
    void *pointer = (void *)(wram1 + (effective_address & 0xffff) - RAM1_START_ADDRESS);
    
    if(byte_write)
    {
        *(uint8_t *)pointer = data;
    }
    else
    {
        *(uint16_t *)pointer = data;
    }
}

void cpu_wram2_write(uint32_t effective_address, uint32_t data, uint32_t byte_write)
{
    
}

uint8_t cpu_read_byte(uint32_t effective_address)
{
    cpu_cycle_count += ACCESS_CYCLES(effective_address);
    return read_data(effective_address);
}

uint16_t cpu_read_word(uint32_t effective_address)
{
    return (uint16_t)cpu_read_byte(effective_address) | ((uint16_t)cpu_read_byte(effective_address + 1) << 8);
}

uint32_t cpu_read_wordbyte(uint32_t effective_address)
{
    return (uint32_t)cpu_read_word(effective_address) | ((uint32_t)cpu_read_byte(effective_address + 2) << 16);
}

uint32_t cpu_regs_read(uint32_t effective_address)
{
    uint32_t *pointer = (void *)(hardware_regs.hardware_regs + (effective_address & 0xffff) - CPU_REGS_START_ADDRESS);
    return *pointer;
}

uint32_t cpu_wram1_read(uint32_t effective_address)
{
    uint32_t *pointer = (void *)(wram1 + (effective_address & 0xffff) - RAM1_START_ADDRESS);
    return *pointer;
}

uint32_t cpu_wram2_read(uint32_t effective_address)
{
    return 0;
}

uint16_t alu(uint32_t operand0, uint32_t operand1, uint32_t op, uint32_t width)
{
    uint32_t sign_mask = width ? 0x00000080 : 0x00008000;
    uint32_t carry_mask = width ? 0x00000100 : 0x00010000;
    uint32_t zero_mask = width ? 0x000000ff : 0x0000ffff;
    uint32_t carry = cpu_state.reg_p & CPU_STATUS_FLAG_CARRY;
    uint32_t result;
//    uint32_t flags = 0;

    operand0 = operand0 & zero_mask;
    /* if it's a subtraction or a comparison, invert the second operand for the subtraction */
    operand1 = ((op == ALU_OP_SUB || op == ALU_OP_CMP) ? -operand1 : operand1) & zero_mask;
    carry = (op == ALU_OP_SUB ? (carry ^ 1) : carry) & zero_mask;

    switch(op)
    {
        case ALU_OP_CMP:
            /* clear carry for comparision subtraction */
            carry = 0;
        case ALU_OP_SUB:
        case ALU_OP_ADD:
            result = operand0 + operand1 + carry;
        break;

        case ALU_OP_AND:
            result = operand0 & operand1;
        break;

        case ALU_OP_OR:
            result = operand0 | operand1;
        break;

        case ALU_OP_XOR:
            result = operand0 ^ operand1;
        break;

        case ALU_OP_INC:
            result = operand0 + 1;
        break;

        case ALU_OP_DEC:
            result = operand0 - 1;
        break;

        case ALU_OP_ROL:
        case ALU_OP_SHL:
            /* shift left shifts msb into the carry */
            (operand0 & sign_mask) ? (cpu_state.reg_p |= CPU_STATUS_FLAG_CARRY) : (cpu_state.reg_p &= ~CPU_STATUS_FLAG_CARRY);
            /* rotate left shifts the carry into the lsb */
            result = (operand0 << 1) | (op == ALU_OP_ROL && carry);
        break;


        case ALU_OP_ROR:
            /* rotate right shifts the carry into the msb */
            operand0 = carry ? (operand0 | carry_mask) : operand0;
        case ALU_OP_SHR:
            /* shift right shifts lsb into the carry */
            (operand0 & 1) ? (cpu_state.reg_p |= CPU_STATUS_FLAG_CARRY) : (cpu_state.reg_p &= ~CPU_STATUS_FLAG_CARRY);
            result = operand0 >> 1;
        break;

        case ALU_OP_TSB:

        break;

        case ALU_OP_TRB:

        break;
    }

    /* every alu op affects negative and zero */
    (result & sign_mask) ? (cpu_state.reg_p |= CPU_STATUS_FLAG_NEGATIVE) : (cpu_state.reg_p &= ~CPU_STATUS_FLAG_NEGATIVE);
    (result & zero_mask) ? (cpu_state.reg_p &= ~CPU_STATUS_FLAG_ZERO) : (cpu_state.reg_p |= CPU_STATUS_FLAG_ZERO);
    
    if(carry_flag_alu_op[op])
    {
        (result & carry_mask) ? (cpu_state.reg_p |= CPU_STATUS_FLAG_CARRY) : (cpu_state.reg_p &= ~CPU_STATUS_FLAG_CARRY); 
    }
    
    if(op == ALU_OP_ADD)
    {
        /* if operand0 and operand1 have the same sign, and the result have a different 
        sign than the operands, then overflow ocurred */
        (((~(operand0 ^ operand1)) & (operand1 ^ result)) & sign_mask) ? 
            (cpu_state.reg_p |= CPU_STATUS_FLAG_OVERFLOW) : (cpu_state.reg_p &= ~CPU_STATUS_FLAG_OVERFLOW);
    }
    
    return result & zero_mask;
}

void reset_cpu()
{
    int i;
    unsigned short *reset_vector;

    cpu_state.reg_dbrw.reg_dbr = 0;
    cpu_state.reg_pbrw.reg_pbr = 0;
    cpu_state.reg_x.reg_xL = 0;
    cpu_state.reg_y.reg_yL = 0;
    cpu_state.reg_d = 0;
    cpu_state.reg_e = 1;
    cpu_state.reg_s = (cpu_state.reg_s & 0xff) | 0x0100;

    cpu_state.reg_p = CPU_STATUS_FLAG_MEMORY | CPU_STATUS_FLAG_INDEX | CPU_STATUS_FLAG_IRQ_DISABLE | CPU_STATUS_FLAG_CARRY;
    cpu_state.reg_pc = (uint16_t)read_data(0xfffc);
}

uint32_t step_cpu()
{
    struct opcode_t opcode;
    uint32_t effective_address;
    uint32_t old_effective_address;
    uint16_t handler = 0;
    int temp;
    int temp2;
    int temp3;
    int byte_write;
    
    cpu_cycle_count = 0;
    
    step_count++;
    
    if(cpu_state.s_wai)
    {
        cpu_state.in_rdy = 0;
    }

    if(hardware_regs.nmitimen)
    {
        
    }
    
    if(!cpu_state.in_abortb)
    {
        /* abort */
        handler = 0xfff8;
    }
    else if(!cpu_state.in_nmib)
    {
        /* non-mask-able interrupt */
        handler = 0xfffa;
    }
    else if(!cpu_state.in_irqb && (!(cpu_state.reg_p & CPU_STATUS_FLAG_IRQ_DISABLE)))
    {
        /* mask-able interrupt, will only be noticed if the I flag is
        clear */
        handler = 0xfffe;
    }
    
    if((!handler && cpu_state.s_wai) || !cpu_state.in_rdy)
    {
        /* WAI condition is active, and no interrupt happened, or rdy pin is being held
        low, so we do nothing */
        return 1;
    }

    /* if we got here, either we wasn't waiting for an interrupt, or we were waiting
    and an interrupt has happened. Either way, we clear the WAI condition */
    cpu_state.s_wai = 0;
    
    _brk_interrupt:
    
    if(handler)
    {
        /* handle an interrupt. Push the program bank register (if in native mode),
        the program counter register, and the cpu status register, and then load 
        the interrupt handler address */
        if(!cpu_state.reg_e)
        {
            /* when in emulation mode, the program bank register doesn't get saved
            onto the stack automatically. This means interrupts should always return
            in the same mode they were entered, otherwise junk may be popped out of
            the stack when it shouldn't, or important data might be left in the stack,
            when it shouldn't. */
            cpu_write_byte(cpu_state.reg_s, cpu_state.reg_pbrw.reg_pbr);
            cpu_state.reg_s--;
        }
        
        cpu_state.reg_pbrw.reg_pbr = 0;
        
        cpu_write_byte(cpu_state.reg_s, cpu_state.reg_pc >> 8);
        cpu_state.reg_s--;
        cpu_write_byte(cpu_state.reg_s, cpu_state.reg_pc);
        cpu_state.reg_s--;
        cpu_write_byte(cpu_state.reg_s, cpu_state.reg_p);
        cpu_state.reg_s--;
        
        cpu_state.reg_pc = cpu_read_word(handler);
        
        return cpu_cycle_count;
    }
    
    if(cpu_state.reg_e)
    {
        /* some conditions that have to be enforced when in emulation mode */
        
        /* it's possible to push a value onto the stack, and pop it into
        the status register. When in emulation mode, M and X flags are 
        forced to 1, so here we enforce this behavior */
        cpu_state.reg_p |= CPU_STATUS_FLAG_MEMORY | CPU_STATUS_FLAG_INDEX;
        
        /* in emulation mode, the upper byte of the stack pointer is forced
        to 1, so here we enforce it to. */
        cpu_state.reg_s = (cpu_state.reg_s & 0xff) | 0x0100;
        cpu_state.reg_x.reg_xH = 0;
        cpu_state.reg_y.reg_yH = 0;
    }
    
    effective_address = EFFECTIVE_ADDRESS(cpu_state.reg_pbrw.reg_pbr, cpu_state.reg_pc);
    opcode = opcode_matrix[cpu_read_byte(effective_address)];
    cpu_state.reg_pc++;
    effective_address = EFFECTIVE_ADDRESS(cpu_state.reg_pbrw.reg_pbr, cpu_state.reg_pc);
    
    /* instruction processing happens in two phases here: 
    
        First, the operand phase: in this phase the operand is handled (if there's any). This phase will output the 
        effective address to the. If an instruction uses absolute indexed indirect addressing, for example, the 
        effective address will point to the next byte after the current instruction.
        
        Then, the opcode phase: in this phase the effective address returned by phase one will be used. If the 
        instruction is a load instruction, then a real pointer is acquired from the effective address. 
        If it's a store instruction, a real pointer will also be acquired from the effective address, and will be 
        used to write the value back to ram. For branch instructions, the effective address will be used directly.
        For jumps using absolute long addressing, the effective address contains the value of PC on the lower
        16 bits, and the value of PBR on the upper 8 bits. In the case of conditional jumps, where the only
        addressing mode is an offset relative to the current PC, the effective address will reference the
        byte right after the current instruction. A real pointer to the value will be acquired if the branch
        is taken, and the value will be added to PC.
    */

    switch(opcode.address_mode)
    {
        case ADDRESS_MODE_ABSOLUTE:
            /* the second and third bytes of the instruction form the low-order 16 bits of the
            effective address. The Data Bank Register contains the high-order 8 bits of the
            operand address... */
            effective_address = EFFECTIVE_ADDRESS(cpu_state.reg_dbrw.reg_dbr, cpu_read_word(effective_address));
            cpu_state.reg_pc += 2;
        break;

        case ADDRESS_MODE_ABSOLUTE_INDEXED_INDIRECT:
            /* the second and third bytes of the instruction are added to the X Index Register
            to form a 16-bit pointer in Bank 0. The contents of this pointer is loaded in the
            Program Counter for the JMP instruction... */

            /* only used by JMP, JSR and EOR... */
            effective_address = EFFECTIVE_ADDRESS(0, cpu_read_word(effective_address) + cpu_state.reg_x.reg_x);
            effective_address = (uint16_t)cpu_read_word(effective_address);
            cpu_state.reg_pc += 2;
        break;

        case ADDRESS_MODE_ABSOLUTE_INDEXED_X:
            /* the second and third bytes of the instruction are added to the X Index Register to
            form the low-order 16-bits of the effective address.  The Data Bank Register contains
            the high-order 8 bits of the effective address... */
            effective_address = EFFECTIVE_ADDRESS(cpu_state.reg_dbrw.reg_dbr, cpu_read_word(effective_address) + cpu_state.reg_x.reg_x);
            cpu_state.reg_pc += 2;
        break;

        case ADDRESS_MODE_ABSOLUTE_INDEXED_Y:
            /* the second and third bytes of the instruction are added to the Y Index Register to
            form the low-order 16-bits of the effective address.  The Data Bank Register contains
            the high-order 8 bits of the effective address... */
            effective_address = EFFECTIVE_ADDRESS(cpu_state.reg_dbrw.reg_dbr, cpu_read_word(effective_address) + cpu_state.reg_y.reg_y);
            cpu_state.reg_pc += 2;
        break;

        case ADDRESS_MODE_ABSOLUTE_INDIRECT:
            /* the second and third bytes of the instruction form an address to a pointer in Bank 0.
            The Program Counter is loaded with the first and second bytes at this pointer.
            With the Jump Long (JML) instruction, the Program Bank Register is loaded with the
            third byte at the pointer...  */
            
            effective_address = cpu_read_word(effective_address & 0x0000ffff);
            
            if(opcode.opcode == OPCODE_JML)
            {
                effective_address = cpu_read_wordbyte(effective_address);
                cpu_state.reg_pc += 3;
            }
            else
            {
                effective_address = cpu_read_word(effective_address);
                cpu_state.reg_pc += 2;
            }
        break;

        case ADDRESS_MODE_ABSOLUTE_LONG_INDEXED_X:
            /* the second, third and fourth bytes of the instruction form a 24-bit base address.
            The effective address is the sum of this 24-bit address and the X Index Register... */
            effective_address = (cpu_read_wordbyte(effective_address) + cpu_state.reg_x.reg_x) & 0x00ffffff;
            cpu_state.reg_pc += 3;
        break;

        case ADDRESS_MODE_ABSOLUTE_LONG:
            /* the second, third and fourth byte of the instruction form the 24-bit effective address... */
            effective_address = cpu_read_wordbyte(effective_address);
            cpu_state.reg_pc += 3;
        break;

        case ADDRESS_MODE_ACCUMULATOR:
            /* nothing to really do here. */
        break;

        case ADDRESS_MODE_BLOCK_MOVE:
            /* the second byte of the instruction contains the high-order 8 bits of the
            destination address and the Y Index Register contains the low-order 16 bits of
            the destination address.  The third byte of the instruction contains the
            high-order 8 bits of the source address and the X Index Register contains
            the low-order bits of the source address.  The C Accumulator contains one less
            than the number of bytes to move.  The second byte of the block move instructions
            is also loaded into the Data Bank Register... */
            effective_address = cpu_read_word(effective_address);
            cpu_state.reg_pc += 2;
            cpu_state.reg_dbrw.reg_dbr = effective_address & 0x000000ff;
        break;

        case ADDRESS_MODE_DIRECT_INDEXED_INDIRECT:
            /* the second byte of the instruction is added to the sum of the
            Direct Register and the X Index Register. The result points to the
            low-order 16 bits of the effective address. The Data Bank Register
            contains the high-order 8 bits of the effective address...  */
            effective_address = (uint16_t)(cpu_read_byte(effective_address) + cpu_state.reg_d + cpu_state.reg_x.reg_x);
            effective_address = EFFECTIVE_ADDRESS(cpu_state.reg_pbrw.reg_pbr, cpu_read_word(effective_address));
            cpu_state.reg_pc++;
        break;

        case ADDRESS_MODE_DIRECT_INDEXED_X:
            /* the second byte of the instruction is added to the sum of the
            Direct Register and the X Index Register to form the 16-bit effective address.
            The operand is always in Bank 0...  */
            effective_address = (uint16_t)(cpu_read_byte(effective_address) + cpu_state.reg_d + cpu_state.reg_x.reg_x);
            cpu_state.reg_pc++;
        break;

        case ADDRESS_MODE_DIRECT_INDEXED_Y:
            /* the second byte of the instruction is added to the sum of the
            Direct Register and the Y Index Register to form the 16-bit effective address.
            The operand is always in Bank 0...  */
            effective_address = (uint16_t)(cpu_read_byte(effective_address) + cpu_state.reg_d + cpu_state.reg_y.reg_y);
            cpu_state.reg_pc++;
        break;

        case ADDRESS_MODE_DIRECT_INDIRECT_INDEXED:
            /* the second byte of the instruction is added to the Direct Register (D).
            The 16-bit content of this memory location is then combined with the
            Data Bank register to form a 24-bit base address.  The Y Index Register
            is added to the base address to form the effective address... */
            effective_address = (uint16_t)(cpu_read_byte(effective_address) + cpu_state.reg_d);
            effective_address = (EFFECTIVE_ADDRESS(cpu_state.reg_dbrw.reg_dbr, cpu_read_word(effective_address)) + cpu_state.reg_y.reg_y) & 0x00ffffff;
            cpu_state.reg_pc++;
        break;

        case ADDRESS_MODE_DIRECT_INDIRECT_LONG_INDEXED:
            /* the 24-bit base address is pointed to by the sum of the second byte of
            the instruction and the Direct Register.  The effective address is this
            24-bit base address plus the Y Index Register... */
            effective_address = (uint16_t)(cpu_read_byte(effective_address) + cpu_state.reg_d);
            effective_address = (cpu_read_wordbyte(effective_address) + cpu_state.reg_y.reg_y) & 0x00ffffff;
            cpu_state.reg_pc++;
        break;

        case ADDRESS_MODE_DIRECT_INDIRECT_LONG:
            /* the second byte of the instruction is added to the Direct Register
            to form a pointer to the 24-bit effective address... */
            effective_address = (uint16_t)(cpu_read_byte(effective_address) + cpu_state.reg_d);
            effective_address = cpu_read_wordbyte(effective_address) & 0x00ffffff;
            cpu_state.reg_pc++;
        break;

        case ADDRESS_MODE_DIRECT_INDIRECT:
            /* the second byte of the instruction is added to the Direct Register to
            form a pointer to the low-order 16 bits of the effective address.
            The Data Bank Register contains the high-order 8 bits of the effective address... */
            effective_address = (uint16_t)(cpu_read_byte(effective_address) + cpu_state.reg_d);
            effective_address = EFFECTIVE_ADDRESS(cpu_state.reg_dbrw.reg_dbr, cpu_read_word(effective_address));
            cpu_state.reg_pc++;
        break;

        case ADDRESS_MODE_DIRECT:
            /* the second byte of the instruction is added to the Direct Register (D)
            to form the effective address. */
            effective_address = (uint16_t)(cpu_read_byte(effective_address) + cpu_state.reg_d);
            cpu_state.reg_pc++;
        break;

        case ADDRESS_MODE_IMMEDIATE:
        {
            /* the operand is the second byte (second and third bytes when in the 16-bit mode)
            of the instruction... */
            uint32_t take_third = 0;
            
            switch(opcode.opcode)
            {
                case OPCODE_LDA: case OPCODE_CMP: case OPCODE_AND: case OPCODE_BIT:
                case OPCODE_EOR: case OPCODE_ORA: case OPCODE_ADC: case OPCODE_SBC:
                    /* if the M flag is 0, the accumulator is in 16 bit mode,
                    which means we'll be taking the next two bytes after the opcode */
                    cpu_state.reg_pc += (cpu_state.reg_p & CPU_STATUS_FLAG_MEMORY) == 0;
                break;
                    
                case OPCODE_LDX: case OPCODE_LDY:
                case OPCODE_CPX: case OPCODE_CPY:
                    /* if the X flag is 0, the index registers are be in 16 bit mode,
                    which means we'll be taking the next two bytes after the opcode */
                    cpu_state.reg_pc += (cpu_state.reg_p & CPU_STATUS_FLAG_INDEX) == 0;
                break;
            }
            
            cpu_state.reg_pc++;
        }
        break;

        case ADDRESS_MODE_IMPLIED:
            /* :) */
        break;

        case ADDRESS_MODE_PROGRAM_COUNTER_RELATIVE_LONG:
            /* the second and third bytes of the instruction are added to the Program Counter,
            which has been updated to point to the OpCode of the next instruction.
            With the branch instruction, the Program Counter is loaded with the result.
            With the Push Effective Relative instruction, the result is stored on the stack.
            The offset is a signed 16-bit quantity in the range from -32768 to 32767.
            The Program Bank Register is not affected...  */
            cpu_state.reg_pc += 2;
            /* the cast for int32_t is necessary to not have reg_pc be seen as a negative number in
            case it's value is above 0x7fff */
            effective_address = EFFECTIVE_ADDRESS(cpu_state.reg_pbrw.reg_pbr, ((int32_t)cpu_state.reg_pc + (int16_t)cpu_read_word(effective_address)));
        break;

        case ADDRESS_MODE_PROGRAM_COUNTER_RELATIVE:
            /* used only with the branch instructions. If the condition being tested is met,
            the second byte of the instruction is added to the Program Counter, which
            has been updated to point to the OpCode of the next instruction. The offset
            is a signed 8-bit quantity in the range from -128 to 127.  The Program Bank
            Register is not affected... */
            cpu_state.reg_pc++;
            /* the cast for int32_t is necessary to not have reg_pc be seen as a negative number in
            case it's value is above 0x7fff */
            effective_address = EFFECTIVE_ADDRESS(cpu_state.reg_pbrw.reg_pbr, ((int32_t)cpu_state.reg_pc + (int8_t)cpu_read_byte(effective_address)));
        break;

        case ADDRESS_MODE_STACK:
            if(opcode.opcode == OPCODE_PEA)
            {
                cpu_state.reg_pc += 2;
            }
            else if(opcode.opcode == OPCODE_PEI)
            {
                cpu_state.reg_pc++;
            }
            /* :) */
        break;

        case ADDRESS_MODE_STACK_RELATIVE:
            /* the low-order 16 bits of the effective address is formed from the
            sum of the second byte of the instruction and the stack pointer.
            The high-order 8 bits of the effective address are always zero.
            The relative offset is an unsigned 8-bit quantity in the range of 0 to 255. */
            effective_address = (uint16_t)((uint8_t)cpu_read_byte(effective_address) + cpu_state.reg_s);
            cpu_state.reg_pc++;
        break;

        case ADDRESS_MODE_STACK_RELATIVE_INDIRECT_INDEXED:
            /* the second byte of the instruction is added to the Stack Pointer to
            form a pointer to the low-order 16-bit base address in Bank 0.
            The Data Bank Register contains the high-order 8 bits of the base address.
            The effective address is the sum of the 24-bit base address and the Y
            Index Register... */
            effective_address = (uint16_t)((uint8_t)cpu_read_byte(effective_address) + cpu_state.reg_s);
            effective_address = (EFFECTIVE_ADDRESS(cpu_state.reg_dbrw.reg_dbr, cpu_read_word(effective_address)) + cpu_state.reg_y.reg_y) & 0x00ffffff;
            cpu_state.reg_pc++;
        break;

        default:
        case ADDRESS_MODE_UNKNOWN:
           /* :( */ 
        break;
    }
    
    switch(opcode.opcode_class)
    {
        case OPCODE_CLASS_ALU:
        {
            uint32_t alu_op = 0;
            uint32_t alu_width = 0;
            uint32_t operand0 = 0;
            uint32_t operand1;
            void *store_address;
            uint8_t prev_p;
            uint32_t store_result = CPU_STORE_RESULT_REGISTER;
            
            switch(opcode.opcode) 
            {
                case OPCODE_ADC: case OPCODE_AND: case OPCODE_SBC: case OPCODE_EOR: case OPCODE_ORA:
                    /* those 5 instructions will load something from memory, and the result will always 
                    be stored in the accumulator */
                    alu_op = (uint32_t []){ALU_OP_ADD, ALU_OP_AND, ALU_OP_SUB, ALU_OP_XOR, ALU_OP_OR}[opcode.opcode - OPCODE_ADC];
                    alu_width = cpu_state.reg_p & CPU_STATUS_FLAG_MEMORY;
                    operand0 = cpu_state.reg_accum.reg_accumC;
                    store_address = &cpu_state.reg_accum.reg_accumC;
                break;

                case OPCODE_CMP:
                    alu_op = ALU_OP_CMP;
                    alu_width = cpu_state.reg_p & CPU_STATUS_FLAG_MEMORY;
                    operand0 = cpu_state.reg_accum.reg_accumC;
                    store_result = CPU_STORE_RESULT_NO_STORE;
                break;

                case OPCODE_CPX:
                    alu_op = ALU_OP_CMP;
                    alu_width = cpu_state.reg_p & CPU_STATUS_FLAG_INDEX;
                    operand0 = cpu_state.reg_x.reg_x;
                    store_result = CPU_STORE_RESULT_NO_STORE;
                break;

                case OPCODE_CPY:
                    alu_op = ALU_OP_CMP;
                    alu_width = cpu_state.reg_p & CPU_STATUS_FLAG_INDEX;
                    operand0 = cpu_state.reg_y.reg_y;
                    store_result = CPU_STORE_RESULT_NO_STORE;
                break;

                case OPCODE_ROL: case OPCODE_ROR: case OPCODE_DEC: case OPCODE_INC: case OPCODE_ASL: case OPCODE_LSR:
                    alu_op = (uint32_t []){ALU_OP_ROL, ALU_OP_ROR, ALU_OP_DEC, ALU_OP_INC, ALU_OP_SHL, ALU_OP_SHR}[opcode.opcode - OPCODE_ROL];
                    alu_width = cpu_state.reg_p & CPU_STATUS_FLAG_MEMORY;
                    if(opcode.address_mode != ADDRESS_MODE_ACCUMULATOR)
                    {
                        store_result = CPU_STORE_RESULT_MEMORY;
                        store_address = (void *)effective_address;
                        operand0 = 0xffffffff;
                    }
                    else
                    {
                        store_address = &cpu_state.reg_accum.reg_accumC;
                        operand0 = cpu_state.reg_accum.reg_accumC;
                    }
                break;

                case OPCODE_DEX: case OPCODE_INX:
                    /* OPCODE_INX lsb = 1, OPCODE_DEX lsb = 0 */
                    alu_op = (uint32_t []){ALU_OP_DEC, ALU_OP_INC}[opcode.opcode & 1];
                    alu_width = cpu_state.reg_p & CPU_STATUS_FLAG_INDEX;
                    operand0 = cpu_state.reg_x.reg_x;
                    store_address = &cpu_state.reg_x.reg_x;
                break;

                case OPCODE_DEY: case OPCODE_INY:
                    /* OPCODE_INY lsb = 1, OPCODE_DEY = 0 */
                    alu_op = (uint32_t []){ALU_OP_DEC, ALU_OP_INC}[opcode.opcode & 1];
                    alu_width = cpu_state.reg_p & CPU_STATUS_FLAG_INDEX;
                    operand0 = cpu_state.reg_y.reg_y;
                    store_address = &cpu_state.reg_y.reg_y;
                break;  

                case OPCODE_TRB: case OPCODE_TSB:
                    /* OPCODE_TSB lsb = 1, OPCODE_TRB lsb = 0 */
                    alu_op = (uint32_t []){ALU_OP_TRB, ALU_OP_TSB}[opcode.opcode & 1];
                    alu_width = 1;
                    store_result = CPU_STORE_RESULT_NO_STORE;
                break;
                
                case OPCODE_BIT:
                    alu_op = ALU_OP_AND;
                    alu_width = cpu_state.reg_p & CPU_STATUS_FLAG_MEMORY;
                    store_result = CPU_STORE_RESULT_NO_STORE;
                    operand0 = cpu_state.reg_accum.reg_accumC;
                    prev_p = cpu_state.reg_p;
                break;
            }
            
            if(opcode.address_mode != ADDRESS_MODE_ACCUMULATOR && opcode.address_mode != ADDRESS_MODE_IMPLIED)
            {
                if(alu_width)
                {
                    operand1 = cpu_read_byte(effective_address);
                }
                else
                {
                    operand1 = cpu_read_word(effective_address);
                }
                
                if(operand0 == 0xffffffff)
                {
                    /* it's not possible to have an operand with the value 0xffffffff, so it
                    works nicely to signal that we'll be treating a value from memory as the
                    operand0 */
                    operand0 = operand1;
                }
            }
            
            uint16_t result = alu(operand0, operand1, alu_op, alu_width);
            
            if(opcode.opcode == OPCODE_BIT)
            {
                if(opcode.address_mode != ADDRESS_MODE_IMMEDIATE)
                {
                    /* if the addressing mode is not immediate, BIT will copy the msb and the next 
                    bit to it to the negative and overflow bits, respectively, in the status register. 
                    The alu took care of doing the AND and setting the Z flag, so we just need to assign 
                    the bits to the status register here */
                    if(!alu_width)
                    {
                        operand1 >>= 8;
                    }                
                }
                else
                {
                    /* when using immediate addressing mode, the N and V flags are not affected,
                    so restore N and V to the values they had before the alu op took place. */
                    operand1 = prev_p;
                }
                
                /* corresponding bits with different values will produce a 1, and corresponding bits
                with the same value will produce a 0. Bits 1 are the bits that have to be changed, 
                and whenever there's a xor between 1 and something else, the value is flipped. So
                we just take the result of the first xor, mask out all but bit 7 and 6, and then 
                xor this with the status register. This will flip only the bits that are different. */
                operand1 = (cpu_state.reg_p ^ operand1) & 0xc0;
                cpu_state.reg_p ^= operand1;
            }
            
            switch(store_result)
            {
                case CPU_STORE_RESULT_REGISTER:
                    if(alu_width)
                    {
                        *(uint8_t* )store_address = result;
                    }
                    else
                    {
                        *(uint16_t* )store_address = result;
                    }
                break;
                
                case CPU_STORE_RESULT_MEMORY:
                    if(alu_width)
                    {
                        cpu_write_byte(effective_address, result);
                    }
                    else
                    {
                        cpu_write_word(effective_address, result);
                    }
                break;
            }
        }
        break;

        case OPCODE_CLASS_BRANCH:
        {
            /*
                https://wiki.superfamicom.org/uploads/assembly-programming-manual-for-w65c816.pdf, pg.361, pg.362
                
                the return address of jump to subroutines instructions is the address of the last byte of the instrunction,
                not the address of the first byte of the next. That's why pc gets decremented before being pushed onto the
                stack when we're executing a JSR or JSL. This means that the popped value gets incremented by one before 
                being loaded into pc.
            */
            
            /*
                https://wiki.superfamicom.org/uploads/assembly-programming-manual-for-w65c816.pdf, pg.391
                
                differently from JSR and JSL, the return address pushed onto the stack when an interrupt
                happens is the address of the next instruction, instead of being the address of the last
                byte of the instruction.
            */
            uint8_t *stack_address;
            switch(opcode.opcode)
            {
                case OPCODE_JML:
                    cpu_state.reg_pc = (uint16_t )effective_address;
                    cpu_state.reg_pbrw.reg_pbr = (uint8_t )(effective_address >> 16);
                break;
                
                case OPCODE_JMP:
                    cpu_state.reg_pc = (uint16_t )effective_address;

                    if(opcode.address_mode == ADDRESS_MODE_ABSOLUTE_LONG)
                    {
                        cpu_state.reg_pbrw.reg_pbr = (uint8_t )(effective_address >> 16);
                    }
                break;

                case OPCODE_JSL:
                    cpu_state.reg_pc--;
                    
                    cpu_write_byte(cpu_state.reg_s, cpu_state.reg_pbrw.reg_pbr);
                    cpu_state.reg_s--;
                    cpu_write_byte(cpu_state.reg_s, cpu_state.reg_pc >> 8);
                    cpu_state.reg_s--;
                    cpu_write_byte(cpu_state.reg_s, cpu_state.reg_pc);
                    cpu_state.reg_s--;
                    
                    cpu_state.reg_pc = (uint16_t )effective_address;
                    cpu_state.reg_pbrw.reg_pbr = (uint8_t )(effective_address >> 16);
                break;

                case OPCODE_JSR:                     
                    cpu_state.reg_pc--;
                                  
                    cpu_write_byte(cpu_state.reg_s, cpu_state.reg_pc >> 8);
                    cpu_state.reg_s--;
                    cpu_write_byte(cpu_state.reg_s, cpu_state.reg_pc);
                    cpu_state.reg_s--;
                    
                    cpu_state.reg_pc = (uint16_t )effective_address;
                break;

                case OPCODE_RTI:
                    cpu_state.reg_s++;
                    cpu_state.reg_p = cpu_read_byte(cpu_state.reg_s);
                    cpu_state.reg_s++;
                    cpu_state.reg_pc = cpu_read_byte(cpu_state.reg_s);
                    cpu_state.reg_s++;
                    cpu_state.reg_pc |= (uint16_t)cpu_read_byte(cpu_state.reg_s) << 8;
                    
                    if(!cpu_state.reg_e)
                    {
                        cpu_state.reg_s++;
                        cpu_state.reg_pbrw.reg_pbr = (uint8_t)cpu_read_byte(cpu_state.reg_s);
                    }
                break;

                case OPCODE_RTL:
                    cpu_state.reg_s++;
                    cpu_state.reg_pc = cpu_read_byte(cpu_state.reg_s);
                    cpu_state.reg_s++;
                    cpu_state.reg_pc |= (uint16_t)cpu_read_byte(cpu_state.reg_s) << 8;
                    cpu_state.reg_pc++;
                    cpu_state.reg_s++;
                    cpu_state.reg_pbrw.reg_pbr = cpu_read_byte(cpu_state.reg_s);
                break;

                case OPCODE_RTS:
                    cpu_state.reg_s++;
                    cpu_state.reg_pc = cpu_read_byte(cpu_state.reg_s);
                    cpu_state.reg_s++;
                    cpu_state.reg_pc |= (uint16_t)cpu_read_byte(cpu_state.reg_s) << 8;
                    cpu_state.reg_pc++;
                break;
                
                case OPCODE_BRL:
                    cpu_state.reg_pbrw.reg_pbr = (uint8_t)(effective_address >> 16);
                case OPCODE_BRA:
                    cpu_state.reg_pc = (uint16_t)effective_address;
                break;
                
                default:
                {
                    /* All conditional branch instructions use the program counter relative addressing
                    mode. */
                    
                    uint32_t execute_branch;
                    uint32_t cond_index = (opcode.opcode - OPCODE_BCC);
                    
                    /* "opposite" branch instructions - instructions that use the same flag
                    to decide whether the branch is taken or not - have numeric values that
                    come one after another, and the instruction that takes the branch if
                    the flag is cleared has its LSB equal to 0, while the one that takes
                    if the flag is set has its LSB equal to 1. The branch condition table
                    is indexed by right shifting the branch instruction index by one, thus
                    getting rid of the different LSB. The condition value then is bitwise 
                    AND'ed with the status register, and then logically AND'ed with one. 
                    This returns 1 if the flag is set, 0 otherwise. After that, this value
                    is compared to the condition index's lsb. If the value is 1, and the
                    condition index lsb is one, the branch is taken. If the value is 0,
                    and the condition index lsb is 0, the branch is taken. The branch is
                    not taken otherwise. */
                    execute_branch = (cpu_state.reg_p & branch_conds[cond_index >> 1].condition) && 1;
                    execute_branch = execute_branch == (cond_index & 1);
                    
                    if(execute_branch)
                    {
                        cpu_state.reg_pc = (uint16_t)effective_address;
                    }
                }
                break;
            }
        }
        break;

        case OPCODE_CLASS_LOAD:
        {
            void *dst_addr;
            uint32_t src_value;
            uint32_t flag = 0;
            switch(opcode.opcode)
            {
                case OPCODE_LDA:
                    dst_addr = &cpu_state.reg_accum.reg_accumC;
                    flag = CPU_STATUS_FLAG_MEMORY;
                break;

                case OPCODE_LDX:
                    dst_addr = &cpu_state.reg_x.reg_x;
                    flag = CPU_STATUS_FLAG_INDEX;
                break;

                case OPCODE_LDY:
                    dst_addr = &cpu_state.reg_y.reg_y;
                    flag = CPU_STATUS_FLAG_INDEX;
                break;
            }

            if(cpu_state.reg_p & flag)
            {
                src_value = cpu_read_byte(effective_address);
                *(uint8_t *)dst_addr = src_value;
                temp2 = (int8_t)src_value < 0;
                temp = (int8_t)src_value == 0;
            }
            else
            {
                src_value = cpu_read_word(effective_address);
                *(uint16_t *)dst_addr = src_value;
                temp2 = (int16_t)src_value < 0;
                temp = (int16_t)src_value == 0;
            }

            cpu_state.reg_p = temp ? (cpu_state.reg_p | CPU_STATUS_FLAG_ZERO) : (cpu_state.reg_p & (~CPU_STATUS_FLAG_ZERO));
            cpu_state.reg_p = temp2 ? (cpu_state.reg_p | CPU_STATUS_FLAG_NEGATIVE) : (cpu_state.reg_p & (~CPU_STATUS_FLAG_NEGATIVE));
        }
        break;

        case OPCODE_CLASS_STORE:
        {
            uint32_t src_value;
            uint8_t flag = 0;
            switch(opcode.opcode)
            {
                case OPCODE_STA:
                    src_value = cpu_state.reg_accum.reg_accumC;
                    flag = CPU_STATUS_FLAG_MEMORY;
                break;

                case OPCODE_STX:
                    src_value = cpu_state.reg_x.reg_x;
                    flag = CPU_STATUS_FLAG_INDEX;
                break;

                case OPCODE_STY:
                    src_value = cpu_state.reg_y.reg_y;
                    flag = CPU_STATUS_FLAG_INDEX;
                break;

                case OPCODE_STZ:
                    src_value = 0;
                    flag = CPU_STATUS_FLAG_MEMORY;
                break;
            }
            
            if(cpu_state.reg_p & flag)
            {
                cpu_write_byte(effective_address, src_value);
            }
            else
            {
                cpu_write_word(effective_address, src_value);
            }
        }
        break;

        case OPCODE_CLASS_STACK:
        {
            uint16_t src_value;
            void *dst_addr = NULL;
            /* so, I'm not sure why the datasheet is so vague about the stack behavior. On some 
            tables it seems to indicate that 16 bit is always pushed onto the stack, but then it 
            kind of suggests that flags M and X affects what's pushed onto the stack. Emulators
            around seems to deal with both 8 bit and 16 bit push/pulls, and that's also mentioned
            in https://wiki.superfamicom.org/uploads/assembly-programming-manual-for-w65c816.pdf.
            Another fun bit is that the datasheet doesn't make any effort in explaining how the 
            stack pointer is incremented/decremented. In x86, the stack pointer points at the 
            LAST PUSHED thing, and pushing DECREMENTS FIRST, THEN STORES THE VALUE. In w65c816, 
            it's the opposite. It STORES FIRST, THEN DECREMENTS THE VALUE, which makes the stack
            pointer point at the NEXT AVAILABLE POSITION. That's a pretty important difference.
            */
            byte_write = 1;
            
            switch(opcode.opcode)
            {
                case OPCODE_PEA:
                    src_value = cpu_read_word(effective_address);
                break;

                case OPCODE_PEI:
                    effective_address = (cpu_state.reg_d + cpu_read_byte(effective_address)) & 0x0000ffff;
                    src_value = cpu_read_word(effective_address);
                break;

                case OPCODE_PER:
        
                break;

                case OPCODE_PHA:
                    byte_write = cpu_state.reg_p & CPU_STATUS_FLAG_MEMORY;
                    src_value = cpu_state.reg_accum.reg_accumC;
                break;

                case OPCODE_PLA:
                    byte_write = cpu_state.reg_p & CPU_STATUS_FLAG_MEMORY;
                    dst_addr = &cpu_state.reg_accum.reg_accumC;
                break;

                case OPCODE_PHB:
                    src_value = cpu_state.reg_dbrw.reg_dbr;
                break;

                case OPCODE_PLB:
                    dst_addr = &cpu_state.reg_dbrw.reg_dbr;
                break;
                
                /* although not mentioned, the behavior for pushing the D register is
                actually clear. It's a 16 bit register, and there's no flag to control
                its width, so two bytes get pushed. The datasheet says PHD and PLD work
                in emulation mode, so it's going to be 2 bytes being pushed too. However,
                that doesn't break backwards compatibility with 6502 programs because the
                6502 doesn't have those two instructions, so stack sanity will be preserved */
                case OPCODE_PHD:
                    byte_write = 0;
                    src_value = cpu_state.reg_d;
                break;

                case OPCODE_PLD:
                    byte_write = 0;
                    dst_addr = &cpu_state.reg_d;
                break;

                case OPCODE_PHK:
                    src_value = cpu_state.reg_pbrw.reg_pbr;
                break;

                case OPCODE_PHP:
                    src_value = cpu_state.reg_p;
                break;

                case OPCODE_PLP:
                    dst_addr = &cpu_state.reg_p;
                break;

                case OPCODE_PHX:
                    byte_write = cpu_state.reg_p & CPU_STATUS_FLAG_INDEX;
                    src_value = cpu_state.reg_x.reg_x;
                break;

                case OPCODE_PLX:
                    byte_write = cpu_state.reg_p & CPU_STATUS_FLAG_INDEX;
                    dst_addr = &cpu_state.reg_x.reg_x;
                break;
                 
                case OPCODE_PHY:
                    byte_write = cpu_state.reg_p & CPU_STATUS_FLAG_INDEX;
                    src_value = cpu_state.reg_y.reg_y;
                break;

                case OPCODE_PLY:
                    byte_write = cpu_state.reg_p & CPU_STATUS_FLAG_INDEX;
                    dst_addr = &cpu_state.reg_y.reg_y;
                break;
            }

            if(!dst_addr)
            {
                if(!byte_write)
                {
                    cpu_write_byte(cpu_state.reg_s, src_value >> 8);
                    cpu_state.reg_s--;
                }
                
                /* pushing */
                cpu_write_byte(cpu_state.reg_s, src_value);
                cpu_state.reg_s--;
            }
            else
            {
                /* popping */
                cpu_state.reg_s++;
                *(uint8_t *)dst_addr = cpu_read_byte(cpu_state.reg_s);
                
                if(!byte_write)
                {
                    cpu_state.reg_s++;
                    *((uint8_t *)dst_addr + 1) = cpu_read_byte(cpu_state.reg_s);
                }
            }
        }
        break;

        case OPCODE_CLASS_STANDALONE:
            switch(opcode.opcode)
            {
                case OPCODE_REP:
                    cpu_state.reg_p &= ~cpu_read_byte(effective_address);
                break;

                case OPCODE_CLC:
                    cpu_state.reg_p &= ~CPU_STATUS_FLAG_CARRY;
                break;

                case OPCODE_CLD:
                    cpu_state.reg_p &= ~CPU_STATUS_FLAG_DECIMAL;
                break;

                case OPCODE_CLV:
                    cpu_state.reg_p &= ~CPU_STATUS_FLAG_OVERFLOW;
                break;

                case OPCODE_CLI:
                    cpu_state.reg_p &= ~CPU_STATUS_FLAG_IRQ_DISABLE;
                break;

                case OPCODE_SEP:
                    cpu_state.reg_p |= cpu_read_byte(effective_address);
                break;

                case OPCODE_SEC:
                    cpu_state.reg_p |= CPU_STATUS_FLAG_CARRY;
                break;

                case OPCODE_SED:
                    cpu_state.reg_p |= CPU_STATUS_FLAG_DECIMAL;
                break;

                case OPCODE_SEI:
                    cpu_state.reg_p |= CPU_STATUS_FLAG_IRQ_DISABLE;
                break;

                case OPCODE_WAI:
                    cpu_state.s_wai = 1;
                break;

                case OPCODE_WDM:
        //            op_code_str = "WDM";
                break;

                case OPCODE_XBA:
                    cpu_state.reg_accum.reg_accumC = ((cpu_state.reg_accum.reg_accumC >> 8) & 0xff) | ((cpu_state.reg_accum.reg_accumC << 8) & 0xff00);
                break;

                case OPCODE_NOP:
            //            op_code_str = "NOP";
                break;

                case OPCODE_XCE:
                    temp = cpu_state.reg_p & CPU_STATUS_FLAG_CARRY;
                    cpu_state.reg_p = (cpu_state.reg_p & (~CPU_STATUS_FLAG_CARRY)) | cpu_state.reg_e;
                    cpu_state.reg_e = temp;

//                    if(cpu_state.reg_e)
//                    {
//                        cpu_state.reg_x.reg_xH = 0;
//                        cpu_state.reg_y.reg_yH = 0;
//                    }
                break;

                case OPCODE_BRK:
                    if(cpu_state.reg_e)
                    {
                        handler = 0xfffe;
                    }
                    else
                    {
                        handler = 0xfff6;
                    }
                    goto _brk_interrupt;
                break;

                case OPCODE_MVN:
                    while(cpu_state.reg_accum.reg_accumC)
                    {
                        uint32_t dst_effective_address = EFFECTIVE_ADDRESS(cpu_state.reg_y.reg_yL, (uint8_t)effective_address);
                        uint32_t src_effective_address = EFFECTIVE_ADDRESS(cpu_state.reg_x.reg_xL, (uint8_t)(effective_address >> 8));
                        cpu_state.reg_y.reg_yL--;
                        cpu_state.reg_x.reg_xL--;
                        cpu_state.reg_accum.reg_accumC--;
                        cpu_write_byte(dst_effective_address, cpu_read_byte(src_effective_address));
                    }
                break;

                case OPCODE_MVP:
                    while(cpu_state.reg_accum.reg_accumC)
                    {
                        uint32_t dst_effective_address = EFFECTIVE_ADDRESS(cpu_state.reg_y.reg_yL, (uint8_t)effective_address);
                        uint32_t src_effective_address = EFFECTIVE_ADDRESS(cpu_state.reg_x.reg_xL, (uint8_t)(effective_address >> 8));
                        cpu_state.reg_y.reg_yL++;
                        cpu_state.reg_x.reg_xL++;
                        cpu_state.reg_accum.reg_accumC--;
                        cpu_write_byte(dst_effective_address, cpu_read_byte(src_effective_address));
                    }
                break;

                case OPCODE_STP:
        
                break;

                case OPCODE_COP:
                    
                break;

                case OPCODE_UNKNOWN:

                break;
            }
        break;

        case OPCODE_CLASS_TRANSFER:
        {
            struct transfer_params_t *transfer = transfer_params + TPARM_INDEX(opcode.opcode);

            if(cpu_state.reg_p & transfer->flag)
            {
                *(uint8_t *)transfer->dst_reg = *(uint8_t *)transfer->src_reg;
            }
            else
            {
                *(uint16_t *)transfer->dst_reg = *(uint16_t *)transfer->src_reg;
            }
        }
        break;
    }
    
    if(cpu_state.reg_p & CPU_STATUS_FLAG_INDEX)
    {
        cpu_state.reg_x.reg_xH = 0;
        cpu_state.reg_y.reg_yH = 0;
    }
    
    return cpu_cycle_count;
}















