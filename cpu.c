#include "cpu.h"
#include "ppu.h"
#include "rom.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


//unsigned char *cpu_ram = NULL;


unsigned char wram1[8192];
unsigned char wram2[20000];

union
{
    unsigned char hardware_regs[CPU_REGS_END_ADDRESS - CPU_REGS_START_ADDRESS];

    struct
    {
//        struct dma_transfer
//        {
//            unsigned char dmatparam;
//            unsigned char dmataddr;
//            unsigned char unused0;
//            unsigned char unused1;
//            unsigned char unused2;
//            unsigned char dmataddrstorel;
//            unsigned char dmataddrstoreh;
//            unsigned char
//        };


        unsigned char nmitimen;
        unsigned char wrio;
        unsigned char wrmpya;
        unsigned char wrmpyb;
        unsigned char wrdivl;
        unsigned char wrdivh;
        unsigned char wrdivb;
        unsigned char htimel;
        unsigned char htimeh;
        unsigned char vtimel;
        unsigned char vtimeh;
        unsigned char mdmaen;
        unsigned char unused0[2];
        unsigned char rdnmi;
        unsigned char timeup;
        unsigned char hvbjoy;
        unsigned char rdio;
        unsigned char rddivl;
        unsigned char rddivh;
        unsigned char rdmpyl;
        unsigned char rdmpyh;
        unsigned char stdcntrl1l;
        unsigned char stdcntrl1h;
        unsigned char stdcntrl2l;
        unsigned char stdcntrl2h;
        unsigned char stdcntrl3l;
        unsigned char stdcntrl3h;
        unsigned char stdcntrl4l;
        unsigned char stdcntrl4h;
        unsigned char unused1[0xe0];
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


unsigned char reg_e;         /* emulation flag */
unsigned short reg_ir;      /* instruction register */
unsigned short reg_tcu;     /* timing control unit */





union
{
    unsigned short reg_accumC;

    struct
    {
        unsigned char reg_accumA;
        unsigned char reg_accumB;
    };

}reg_accum;


union
{
    unsigned short reg_x;

    struct
    {
        unsigned char reg_xL;
        unsigned char reg_xH;
    };

}reg_x;


union
{
    unsigned short reg_y;

    struct
    {
        unsigned char reg_yL;
        unsigned char reg_yH;
    };

}reg_y;

unsigned short reg_d;       /* direct register */
unsigned short reg_s;       /* stack pointer register */
unsigned short reg_pc;       /* program counter register */
unsigned char reg_dbr;      /* data bank register */
unsigned char reg_pbr;      /* program bank register */
unsigned char reg_p;        /* status register */





unsigned char address_mode_operand_size[ADDRESS_MODE_UNKNOWN];
struct op_code_t opcode_matrix[256];


void reset_cpu()
{
    int i;
    unsigned short *reset_vector;

    //if(!cpu_ram)
    {
        //cpu_ram = calloc(1, CPU_RAM_SIZE);

        for(i = 0; i < 256; i++)
        {
            opcode_matrix[i] = OPCODE(OP_CODE_UNKNOWN, OP_CODE_CATEGORY_LOAD, ADDRESS_MODE_UNKNOWN);
        }

        address_mode_operand_size[ADDRESS_MODE_ABSOLUTE] = 2;
        address_mode_operand_size[ADDRESS_MODE_ABSOLUTE_INDEXED_INDIRECT] = 2;
        address_mode_operand_size[ADDRESS_MODE_ABSOLUTE_INDEXED_X] = 2;
        address_mode_operand_size[ADDRESS_MODE_ABSOLUTE_INDEXED_Y] = 2;
        address_mode_operand_size[ADDRESS_MODE_ABSOLUTE_INDIRECT] = 2;
        address_mode_operand_size[ADDRESS_MODE_ABSOLUTE_LONG_INDEXED_X] = 3;
        address_mode_operand_size[ADDRESS_MODE_ABSOLUTE_LONG] = 3;
        address_mode_operand_size[ADDRESS_MODE_ACCUMULATOR] = 0;
        address_mode_operand_size[ADDRESS_MODE_BLOCK_MOVE] = 1;
        address_mode_operand_size[ADDRESS_MODE_DIRECT_INDEXED_INDIRECT] = 1;
        address_mode_operand_size[ADDRESS_MODE_DIRECT_INDEXED_X] = 1;
        address_mode_operand_size[ADDRESS_MODE_DIRECT_INDEXED_Y] = 1;
        address_mode_operand_size[ADDRESS_MODE_DIRECT_INDIRECT_INDEXED] = 1;
        address_mode_operand_size[ADDRESS_MODE_DIRECT_INDIRECT_LONG_INDEXED] = 1;
        address_mode_operand_size[ADDRESS_MODE_DIRECT_INDIRECT_LONG] = 1;
        address_mode_operand_size[ADDRESS_MODE_DIRECT_INDIRECT] = 1;
        address_mode_operand_size[ADDRESS_MODE_DIRECT] = 1;
        address_mode_operand_size[ADDRESS_MODE_IMMEDIATE] = 2;
        address_mode_operand_size[ADDRESS_MODE_IMPLIED] = 0;
        address_mode_operand_size[ADDRESS_MODE_PROGRAM_COUNTER_RELATIVE_LONG] = 2;
        address_mode_operand_size[ADDRESS_MODE_PROGRAM_COUNTER_RELATIVE] = 1;
        address_mode_operand_size[ADDRESS_MODE_STACK] = 0;
        address_mode_operand_size[ADDRESS_MODE_STACK_RELATIVE] = 1;
        address_mode_operand_size[ADDRESS_MODE_STACK_RELATIVE_INDIRECT_INDEXED] = 1;



        opcode_matrix[0x6d] = OPCODE(OP_CODE_ADC, OP_CODE_CATEGORY_ARITHMETIC, ADDRESS_MODE_ABSOLUTE);
        opcode_matrix[0x7d] = OPCODE(OP_CODE_ADC, OP_CODE_CATEGORY_ARITHMETIC, ADDRESS_MODE_ABSOLUTE_INDEXED_X);
        opcode_matrix[0x79] = OPCODE(OP_CODE_ADC, OP_CODE_CATEGORY_ARITHMETIC, ADDRESS_MODE_ABSOLUTE_INDEXED_Y);
        opcode_matrix[0x6f] = OPCODE(OP_CODE_ADC, OP_CODE_CATEGORY_ARITHMETIC, ADDRESS_MODE_ABSOLUTE_LONG);
        opcode_matrix[0x7f] = OPCODE(OP_CODE_ADC, OP_CODE_CATEGORY_ARITHMETIC, ADDRESS_MODE_ABSOLUTE_LONG_INDEXED_X);
        opcode_matrix[0x17] = OPCODE(OP_CODE_ADC, OP_CODE_CATEGORY_ARITHMETIC, ADDRESS_MODE_ABSOLUTE_INDIRECT);
        opcode_matrix[0x65] = OPCODE(OP_CODE_ADC, OP_CODE_CATEGORY_ARITHMETIC, ADDRESS_MODE_DIRECT);
        opcode_matrix[0x63] = OPCODE(OP_CODE_ADC, OP_CODE_CATEGORY_ARITHMETIC, ADDRESS_MODE_STACK_RELATIVE);
        opcode_matrix[0x75] = OPCODE(OP_CODE_ADC, OP_CODE_CATEGORY_ARITHMETIC, ADDRESS_MODE_DIRECT_INDEXED_X);
        opcode_matrix[0x72] = OPCODE(OP_CODE_ADC, OP_CODE_CATEGORY_ARITHMETIC, ADDRESS_MODE_DIRECT_INDIRECT);
        opcode_matrix[0x67] = OPCODE(OP_CODE_ADC, OP_CODE_CATEGORY_ARITHMETIC, ADDRESS_MODE_DIRECT_INDIRECT_LONG);
        opcode_matrix[0x73] = OPCODE(OP_CODE_ADC, OP_CODE_CATEGORY_ARITHMETIC, ADDRESS_MODE_STACK_RELATIVE_INDIRECT_INDEXED);
        opcode_matrix[0x61] = OPCODE(OP_CODE_ADC, OP_CODE_CATEGORY_ARITHMETIC, ADDRESS_MODE_DIRECT_INDEXED_INDIRECT);
        opcode_matrix[0x71] = OPCODE(OP_CODE_ADC, OP_CODE_CATEGORY_ARITHMETIC, ADDRESS_MODE_DIRECT_INDIRECT_INDEXED);
        opcode_matrix[0x77] = OPCODE(OP_CODE_ADC, OP_CODE_CATEGORY_ARITHMETIC, ADDRESS_MODE_DIRECT_INDIRECT_LONG_INDEXED);
        opcode_matrix[0x69] = OPCODE(OP_CODE_ADC, OP_CODE_CATEGORY_ARITHMETIC, ADDRESS_MODE_IMMEDIATE);



        opcode_matrix[0x2d] = OPCODE(OP_CODE_AND, OP_CODE_CATEGORY_ARITHMETIC, ADDRESS_MODE_ABSOLUTE);
        opcode_matrix[0x3e] = OPCODE(OP_CODE_AND, OP_CODE_CATEGORY_ARITHMETIC, ADDRESS_MODE_ABSOLUTE_INDEXED_X);
        opcode_matrix[0x39] = OPCODE(OP_CODE_AND, OP_CODE_CATEGORY_ARITHMETIC, ADDRESS_MODE_ABSOLUTE_INDEXED_Y);
        opcode_matrix[0x2f] = OPCODE(OP_CODE_AND, OP_CODE_CATEGORY_ARITHMETIC, ADDRESS_MODE_ABSOLUTE_LONG);
        opcode_matrix[0x3f] = OPCODE(OP_CODE_AND, OP_CODE_CATEGORY_ARITHMETIC, ADDRESS_MODE_ABSOLUTE_LONG_INDEXED_X);
        opcode_matrix[0x25] = OPCODE(OP_CODE_AND, OP_CODE_CATEGORY_ARITHMETIC, ADDRESS_MODE_DIRECT);
        opcode_matrix[0x23] = OPCODE(OP_CODE_AND, OP_CODE_CATEGORY_ARITHMETIC, ADDRESS_MODE_STACK_RELATIVE);
        opcode_matrix[0x36] = OPCODE(OP_CODE_AND, OP_CODE_CATEGORY_ARITHMETIC, ADDRESS_MODE_DIRECT_INDEXED_X);
        opcode_matrix[0x32] = OPCODE(OP_CODE_AND, OP_CODE_CATEGORY_ARITHMETIC, ADDRESS_MODE_DIRECT_INDIRECT);
        opcode_matrix[0x27] = OPCODE(OP_CODE_AND, OP_CODE_CATEGORY_ARITHMETIC, ADDRESS_MODE_DIRECT_INDIRECT_LONG);
        opcode_matrix[0x33] = OPCODE(OP_CODE_AND, OP_CODE_CATEGORY_ARITHMETIC, ADDRESS_MODE_STACK_RELATIVE_INDIRECT_INDEXED);
        opcode_matrix[0x91] = OPCODE(OP_CODE_AND, OP_CODE_CATEGORY_ARITHMETIC, ADDRESS_MODE_DIRECT_INDEXED_INDIRECT);
        opcode_matrix[0x31] = OPCODE(OP_CODE_AND, OP_CODE_CATEGORY_ARITHMETIC, ADDRESS_MODE_DIRECT_INDIRECT_INDEXED);
        opcode_matrix[0x37] = OPCODE(OP_CODE_AND, OP_CODE_CATEGORY_ARITHMETIC, ADDRESS_MODE_DIRECT_INDIRECT_LONG_INDEXED);
        opcode_matrix[0x29] = OPCODE(OP_CODE_AND, OP_CODE_CATEGORY_ARITHMETIC, ADDRESS_MODE_IMMEDIATE);


        opcode_matrix[0x0e] = OPCODE(OP_CODE_ASL, OP_CODE_CATEGORY_ARITHMETIC, ADDRESS_MODE_ABSOLUTE);
        opcode_matrix[0x0a] = OPCODE(OP_CODE_ASL, OP_CODE_CATEGORY_ARITHMETIC, ADDRESS_MODE_ACCUMULATOR);
        opcode_matrix[0x1e] = OPCODE(OP_CODE_ASL, OP_CODE_CATEGORY_ARITHMETIC, ADDRESS_MODE_ABSOLUTE_INDEXED_X);
        opcode_matrix[0x06] = OPCODE(OP_CODE_ASL, OP_CODE_CATEGORY_ARITHMETIC, ADDRESS_MODE_DIRECT);
        opcode_matrix[0x16] = OPCODE(OP_CODE_ASL, OP_CODE_CATEGORY_ARITHMETIC, ADDRESS_MODE_DIRECT_INDEXED_X);


        opcode_matrix[0x90] = OPCODE(OP_CODE_BCC, OP_CODE_CATEGORY_BRANCH, ADDRESS_MODE_PROGRAM_COUNTER_RELATIVE);
        opcode_matrix[0xb0] = OPCODE(OP_CODE_BCS, OP_CODE_CATEGORY_BRANCH, ADDRESS_MODE_PROGRAM_COUNTER_RELATIVE);
        opcode_matrix[0xf0] = OPCODE(OP_CODE_BEQ, OP_CODE_CATEGORY_BRANCH, ADDRESS_MODE_PROGRAM_COUNTER_RELATIVE);


        opcode_matrix[0x2c] = OPCODE(OP_CODE_BIT, OP_CODE_CATEGORY_COMPARISON, ADDRESS_MODE_ABSOLUTE);
        opcode_matrix[0x3c] = OPCODE(OP_CODE_BIT, OP_CODE_CATEGORY_COMPARISON, ADDRESS_MODE_ABSOLUTE_INDEXED_X);
        opcode_matrix[0x24] = OPCODE(OP_CODE_BIT, OP_CODE_CATEGORY_COMPARISON, ADDRESS_MODE_DIRECT);
        opcode_matrix[0x34] = OPCODE(OP_CODE_BIT, OP_CODE_CATEGORY_COMPARISON, ADDRESS_MODE_DIRECT_INDEXED_X);
        opcode_matrix[0x89] = OPCODE(OP_CODE_BIT, OP_CODE_CATEGORY_COMPARISON, ADDRESS_MODE_IMMEDIATE);


        opcode_matrix[0x30] = OPCODE(OP_CODE_BMI, OP_CODE_CATEGORY_BRANCH, ADDRESS_MODE_PROGRAM_COUNTER_RELATIVE);
        opcode_matrix[0xd0] = OPCODE(OP_CODE_BNE, OP_CODE_CATEGORY_BRANCH, ADDRESS_MODE_PROGRAM_COUNTER_RELATIVE);
        opcode_matrix[0x10] = OPCODE(OP_CODE_BPL, OP_CODE_CATEGORY_BRANCH, ADDRESS_MODE_PROGRAM_COUNTER_RELATIVE);
        opcode_matrix[0x80] = OPCODE(OP_CODE_BRA, OP_CODE_CATEGORY_BRANCH, ADDRESS_MODE_PROGRAM_COUNTER_RELATIVE);


        opcode_matrix[0x00] = OPCODE(OP_CODE_BRK, OP_CODE_CATEGORY_STANDALONE, ADDRESS_MODE_STACK);


        opcode_matrix[0x82] = OPCODE(OP_CODE_BRL, OP_CODE_CATEGORY_BRANCH, ADDRESS_MODE_PROGRAM_COUNTER_RELATIVE_LONG);
        opcode_matrix[0x50] = OPCODE(OP_CODE_BVC, OP_CODE_CATEGORY_BRANCH, ADDRESS_MODE_PROGRAM_COUNTER_RELATIVE);
        opcode_matrix[0x70] = OPCODE(OP_CODE_BVS, OP_CODE_CATEGORY_BRANCH, ADDRESS_MODE_PROGRAM_COUNTER_RELATIVE);


        opcode_matrix[0x18] = OPCODE(OP_CODE_CLC, OP_CODE_CATEGORY_STANDALONE, ADDRESS_MODE_IMPLIED);
        opcode_matrix[0xd8] = OPCODE(OP_CODE_CLD, OP_CODE_CATEGORY_STANDALONE, ADDRESS_MODE_IMPLIED);
        opcode_matrix[0x58] = OPCODE(OP_CODE_CLI, OP_CODE_CATEGORY_STANDALONE, ADDRESS_MODE_IMPLIED);
        opcode_matrix[0xb8] = OPCODE(OP_CODE_CLV, OP_CODE_CATEGORY_STANDALONE, ADDRESS_MODE_IMPLIED);
        opcode_matrix[0xc2] = OPCODE(OP_CODE_REP, OP_CODE_CATEGORY_STANDALONE, ADDRESS_MODE_IMMEDIATE);


        opcode_matrix[0xcd] = OPCODE(OP_CODE_CMP, OP_CODE_CATEGORY_COMPARISON, ADDRESS_MODE_ABSOLUTE);
        opcode_matrix[0xdd] = OPCODE(OP_CODE_CMP, OP_CODE_CATEGORY_COMPARISON, ADDRESS_MODE_ABSOLUTE_INDEXED_X);
        opcode_matrix[0xd9] = OPCODE(OP_CODE_CMP, OP_CODE_CATEGORY_COMPARISON, ADDRESS_MODE_ABSOLUTE_INDEXED_Y);
        opcode_matrix[0xcf] = OPCODE(OP_CODE_CMP, OP_CODE_CATEGORY_COMPARISON, ADDRESS_MODE_ABSOLUTE_LONG);
        opcode_matrix[0xdf] = OPCODE(OP_CODE_CMP, OP_CODE_CATEGORY_COMPARISON, ADDRESS_MODE_ABSOLUTE_LONG_INDEXED_X);
        opcode_matrix[0xc5] = OPCODE(OP_CODE_CMP, OP_CODE_CATEGORY_COMPARISON, ADDRESS_MODE_DIRECT);
        opcode_matrix[0xc3] = OPCODE(OP_CODE_CMP, OP_CODE_CATEGORY_COMPARISON, ADDRESS_MODE_STACK_RELATIVE);
        opcode_matrix[0xd5] = OPCODE(OP_CODE_CMP, OP_CODE_CATEGORY_COMPARISON, ADDRESS_MODE_DIRECT_INDEXED_X);
        opcode_matrix[0xd2] = OPCODE(OP_CODE_CMP, OP_CODE_CATEGORY_COMPARISON, ADDRESS_MODE_DIRECT_INDIRECT);
        opcode_matrix[0xc7] = OPCODE(OP_CODE_CMP, OP_CODE_CATEGORY_COMPARISON, ADDRESS_MODE_DIRECT_INDIRECT_LONG);
        opcode_matrix[0xd3] = OPCODE(OP_CODE_CMP, OP_CODE_CATEGORY_COMPARISON, ADDRESS_MODE_STACK_RELATIVE_INDIRECT_INDEXED);
        opcode_matrix[0xc1] = OPCODE(OP_CODE_CMP, OP_CODE_CATEGORY_COMPARISON, ADDRESS_MODE_DIRECT_INDEXED_INDIRECT);
        opcode_matrix[0xd1] = OPCODE(OP_CODE_CMP, OP_CODE_CATEGORY_COMPARISON, ADDRESS_MODE_DIRECT_INDIRECT_INDEXED);
        opcode_matrix[0xd7] = OPCODE(OP_CODE_CMP, OP_CODE_CATEGORY_COMPARISON, ADDRESS_MODE_DIRECT_INDIRECT_LONG_INDEXED);
        opcode_matrix[0xc9] = OPCODE(OP_CODE_CMP, OP_CODE_CATEGORY_COMPARISON, ADDRESS_MODE_IMMEDIATE);


        opcode_matrix[0x02] = OPCODE(OP_CODE_COP, OP_CODE_CATEGORY_STANDALONE, ADDRESS_MODE_STACK);


        opcode_matrix[0xec] = OPCODE(OP_CODE_CPX, OP_CODE_CATEGORY_COMPARISON, ADDRESS_MODE_ABSOLUTE);
        opcode_matrix[0xe4] = OPCODE(OP_CODE_CPX, OP_CODE_CATEGORY_COMPARISON, ADDRESS_MODE_DIRECT);
        opcode_matrix[0xe0] = OPCODE(OP_CODE_CPX, OP_CODE_CATEGORY_COMPARISON, ADDRESS_MODE_IMMEDIATE);


        opcode_matrix[0xcc] = OPCODE(OP_CODE_CPY, OP_CODE_CATEGORY_COMPARISON, ADDRESS_MODE_ABSOLUTE);
        opcode_matrix[0xc4] = OPCODE(OP_CODE_CPY, OP_CODE_CATEGORY_COMPARISON, ADDRESS_MODE_DIRECT);
        opcode_matrix[0xc0] = OPCODE(OP_CODE_CPY, OP_CODE_CATEGORY_COMPARISON, ADDRESS_MODE_IMMEDIATE);


        opcode_matrix[0xce] = OPCODE(OP_CODE_DEC, OP_CODE_CATEGORY_ARITHMETIC, ADDRESS_MODE_ABSOLUTE);
        opcode_matrix[0x3a] = OPCODE(OP_CODE_DEC, OP_CODE_CATEGORY_ARITHMETIC, ADDRESS_MODE_ACCUMULATOR);
        opcode_matrix[0xde] = OPCODE(OP_CODE_DEC, OP_CODE_CATEGORY_ARITHMETIC, ADDRESS_MODE_ABSOLUTE_INDEXED_X);
        opcode_matrix[0xc6] = OPCODE(OP_CODE_DEC, OP_CODE_CATEGORY_ARITHMETIC, ADDRESS_MODE_DIRECT);
        opcode_matrix[0xd6] = OPCODE(OP_CODE_DEC, OP_CODE_CATEGORY_ARITHMETIC, ADDRESS_MODE_DIRECT_INDEXED_X);


        opcode_matrix[0xca] = OPCODE(OP_CODE_DEX, OP_CODE_CATEGORY_ARITHMETIC, ADDRESS_MODE_IMPLIED);
        opcode_matrix[0x88] = OPCODE(OP_CODE_DEY, OP_CODE_CATEGORY_ARITHMETIC, ADDRESS_MODE_IMPLIED);


        opcode_matrix[0x4d] = OPCODE(OP_CODE_EOR, OP_CODE_CATEGORY_ARITHMETIC, ADDRESS_MODE_ABSOLUTE);
        opcode_matrix[0x5d] = OPCODE(OP_CODE_EOR, OP_CODE_CATEGORY_ARITHMETIC, ADDRESS_MODE_ABSOLUTE_INDEXED_X);
        opcode_matrix[0x59] = OPCODE(OP_CODE_EOR, OP_CODE_CATEGORY_ARITHMETIC, ADDRESS_MODE_ABSOLUTE_INDEXED_Y);
        opcode_matrix[0x4f] = OPCODE(OP_CODE_EOR, OP_CODE_CATEGORY_ARITHMETIC, ADDRESS_MODE_ABSOLUTE_LONG);
        opcode_matrix[0x5f] = OPCODE(OP_CODE_EOR, OP_CODE_CATEGORY_ARITHMETIC, ADDRESS_MODE_ABSOLUTE_LONG_INDEXED_X);
        opcode_matrix[0x5d] = OPCODE(OP_CODE_EOR, OP_CODE_CATEGORY_ARITHMETIC, ADDRESS_MODE_ABSOLUTE_INDEXED_INDIRECT);
        opcode_matrix[0x45] = OPCODE(OP_CODE_EOR, OP_CODE_CATEGORY_ARITHMETIC, ADDRESS_MODE_DIRECT);
        opcode_matrix[0x43] = OPCODE(OP_CODE_EOR, OP_CODE_CATEGORY_ARITHMETIC, ADDRESS_MODE_STACK_RELATIVE);
        opcode_matrix[0x55] = OPCODE(OP_CODE_EOR, OP_CODE_CATEGORY_ARITHMETIC, ADDRESS_MODE_DIRECT_INDEXED_X);
        opcode_matrix[0x52] = OPCODE(OP_CODE_EOR, OP_CODE_CATEGORY_ARITHMETIC, ADDRESS_MODE_DIRECT_INDIRECT);
        opcode_matrix[0x47] = OPCODE(OP_CODE_EOR, OP_CODE_CATEGORY_ARITHMETIC, ADDRESS_MODE_DIRECT_INDIRECT_LONG);
        opcode_matrix[0x53] = OPCODE(OP_CODE_EOR, OP_CODE_CATEGORY_ARITHMETIC, ADDRESS_MODE_STACK_RELATIVE_INDIRECT_INDEXED);
        opcode_matrix[0x41] = OPCODE(OP_CODE_EOR, OP_CODE_CATEGORY_ARITHMETIC, ADDRESS_MODE_DIRECT_INDEXED_INDIRECT);
        opcode_matrix[0x51] = OPCODE(OP_CODE_EOR, OP_CODE_CATEGORY_ARITHMETIC, ADDRESS_MODE_DIRECT_INDIRECT_INDEXED);
        opcode_matrix[0x57] = OPCODE(OP_CODE_EOR, OP_CODE_CATEGORY_ARITHMETIC, ADDRESS_MODE_DIRECT_INDIRECT_LONG_INDEXED);
        opcode_matrix[0x49] = OPCODE(OP_CODE_EOR, OP_CODE_CATEGORY_ARITHMETIC, ADDRESS_MODE_IMMEDIATE);


        opcode_matrix[0xee] = OPCODE(OP_CODE_INC, OP_CODE_CATEGORY_ARITHMETIC, ADDRESS_MODE_ABSOLUTE);
        opcode_matrix[0x1a] = OPCODE(OP_CODE_INC, OP_CODE_CATEGORY_ARITHMETIC, ADDRESS_MODE_ACCUMULATOR);
        opcode_matrix[0xfe] = OPCODE(OP_CODE_INC, OP_CODE_CATEGORY_ARITHMETIC, ADDRESS_MODE_ABSOLUTE_INDEXED_X);
        opcode_matrix[0xe6] = OPCODE(OP_CODE_INC, OP_CODE_CATEGORY_ARITHMETIC, ADDRESS_MODE_DIRECT);
        opcode_matrix[0xf6] = OPCODE(OP_CODE_INC, OP_CODE_CATEGORY_ARITHMETIC, ADDRESS_MODE_DIRECT_INDEXED_X);


        opcode_matrix[0xe8] = OPCODE(OP_CODE_INX, OP_CODE_CATEGORY_ARITHMETIC, ADDRESS_MODE_IMPLIED);
        opcode_matrix[0xc8] = OPCODE(OP_CODE_INY, OP_CODE_CATEGORY_ARITHMETIC, ADDRESS_MODE_IMPLIED);


        opcode_matrix[0xdc] = OPCODE(OP_CODE_JML, OP_CODE_CATEGORY_BRANCH, ADDRESS_MODE_ABSOLUTE_INDIRECT);


        opcode_matrix[0x4c] = OPCODE(OP_CODE_JMP, OP_CODE_CATEGORY_BRANCH, ADDRESS_MODE_ABSOLUTE);
        opcode_matrix[0x5c] = OPCODE(OP_CODE_JMP, OP_CODE_CATEGORY_BRANCH, ADDRESS_MODE_ABSOLUTE_LONG);
        opcode_matrix[0x6c] = OPCODE(OP_CODE_JMP, OP_CODE_CATEGORY_BRANCH, ADDRESS_MODE_ABSOLUTE_INDIRECT);
        opcode_matrix[0x7c] = OPCODE(OP_CODE_JMP, OP_CODE_CATEGORY_BRANCH, ADDRESS_MODE_ABSOLUTE_INDEXED_INDIRECT);


        opcode_matrix[0x22] = OPCODE(OP_CODE_JSL, OP_CODE_CATEGORY_BRANCH, ADDRESS_MODE_ABSOLUTE_LONG);


        opcode_matrix[0x20] = OPCODE(OP_CODE_JSR, OP_CODE_CATEGORY_BRANCH, ADDRESS_MODE_ABSOLUTE);
        opcode_matrix[0xfc] = OPCODE(OP_CODE_JSR, OP_CODE_CATEGORY_BRANCH, ADDRESS_MODE_ABSOLUTE_INDEXED_INDIRECT);


        opcode_matrix[0xad] = OPCODE(OP_CODE_LDA, OP_CODE_CATEGORY_LOAD, ADDRESS_MODE_ABSOLUTE);
        opcode_matrix[0xbd] = OPCODE(OP_CODE_LDA, OP_CODE_CATEGORY_LOAD, ADDRESS_MODE_ABSOLUTE_INDEXED_X);
        opcode_matrix[0xb9] = OPCODE(OP_CODE_LDA, OP_CODE_CATEGORY_LOAD, ADDRESS_MODE_ABSOLUTE_INDEXED_Y);
        opcode_matrix[0xaf] = OPCODE(OP_CODE_LDA, OP_CODE_CATEGORY_LOAD, ADDRESS_MODE_ABSOLUTE_LONG);
        opcode_matrix[0xbf] = OPCODE(OP_CODE_LDA, OP_CODE_CATEGORY_LOAD, ADDRESS_MODE_ABSOLUTE_LONG_INDEXED_X);
        opcode_matrix[0xa5] = OPCODE(OP_CODE_LDA, OP_CODE_CATEGORY_LOAD, ADDRESS_MODE_DIRECT);
        opcode_matrix[0xa3] = OPCODE(OP_CODE_LDA, OP_CODE_CATEGORY_LOAD, ADDRESS_MODE_STACK_RELATIVE);
        opcode_matrix[0xb5] = OPCODE(OP_CODE_LDA, OP_CODE_CATEGORY_LOAD, ADDRESS_MODE_DIRECT_INDEXED_X);
        opcode_matrix[0xb2] = OPCODE(OP_CODE_LDA, OP_CODE_CATEGORY_LOAD, ADDRESS_MODE_DIRECT_INDIRECT);
        opcode_matrix[0xa7] = OPCODE(OP_CODE_LDA, OP_CODE_CATEGORY_LOAD, ADDRESS_MODE_DIRECT_INDIRECT_LONG);
        opcode_matrix[0xb3] = OPCODE(OP_CODE_LDA, OP_CODE_CATEGORY_LOAD, ADDRESS_MODE_STACK_RELATIVE_INDIRECT_INDEXED);
        opcode_matrix[0xa1] = OPCODE(OP_CODE_LDA, OP_CODE_CATEGORY_LOAD, ADDRESS_MODE_DIRECT_INDEXED_INDIRECT);
        opcode_matrix[0xb1] = OPCODE(OP_CODE_LDA, OP_CODE_CATEGORY_LOAD, ADDRESS_MODE_DIRECT_INDIRECT_INDEXED);
        opcode_matrix[0xb7] = OPCODE(OP_CODE_LDA, OP_CODE_CATEGORY_LOAD, ADDRESS_MODE_DIRECT_INDIRECT_LONG_INDEXED);
        opcode_matrix[0xa9] = OPCODE(OP_CODE_LDA, OP_CODE_CATEGORY_LOAD, ADDRESS_MODE_IMMEDIATE);


        opcode_matrix[0xae] = OPCODE(OP_CODE_LDX, OP_CODE_CATEGORY_LOAD, ADDRESS_MODE_ABSOLUTE);
        opcode_matrix[0xbe] = OPCODE(OP_CODE_LDX, OP_CODE_CATEGORY_LOAD, ADDRESS_MODE_ABSOLUTE_INDEXED_Y);
        opcode_matrix[0xa6] = OPCODE(OP_CODE_LDX, OP_CODE_CATEGORY_LOAD, ADDRESS_MODE_DIRECT);
        opcode_matrix[0xb6] = OPCODE(OP_CODE_LDX, OP_CODE_CATEGORY_LOAD, ADDRESS_MODE_DIRECT_INDEXED_Y);
        opcode_matrix[0xa2] = OPCODE(OP_CODE_LDX, OP_CODE_CATEGORY_LOAD, ADDRESS_MODE_IMMEDIATE);


        opcode_matrix[0xac] = OPCODE(OP_CODE_LDY, OP_CODE_CATEGORY_LOAD, ADDRESS_MODE_ABSOLUTE);
        opcode_matrix[0xbc] = OPCODE(OP_CODE_LDY, OP_CODE_CATEGORY_LOAD, ADDRESS_MODE_ABSOLUTE_INDEXED_Y);
        opcode_matrix[0xa4] = OPCODE(OP_CODE_LDY, OP_CODE_CATEGORY_LOAD, ADDRESS_MODE_DIRECT);
        opcode_matrix[0xb4] = OPCODE(OP_CODE_LDY, OP_CODE_CATEGORY_LOAD, ADDRESS_MODE_DIRECT_INDEXED_X);
        opcode_matrix[0xa0] = OPCODE(OP_CODE_LDY, OP_CODE_CATEGORY_LOAD, ADDRESS_MODE_IMMEDIATE);


        opcode_matrix[0x4e] = OPCODE(OP_CODE_LSR, OP_CODE_CATEGORY_ARITHMETIC, ADDRESS_MODE_ABSOLUTE);
        opcode_matrix[0x4a] = OPCODE(OP_CODE_LSR, OP_CODE_CATEGORY_ARITHMETIC, ADDRESS_MODE_ACCUMULATOR);
        opcode_matrix[0x5e] = OPCODE(OP_CODE_LSR, OP_CODE_CATEGORY_ARITHMETIC, ADDRESS_MODE_ABSOLUTE_INDEXED_X);
        opcode_matrix[0x46] = OPCODE(OP_CODE_LSR, OP_CODE_CATEGORY_ARITHMETIC, ADDRESS_MODE_DIRECT);
        opcode_matrix[0x56] = OPCODE(OP_CODE_LSR, OP_CODE_CATEGORY_ARITHMETIC, ADDRESS_MODE_DIRECT_INDEXED_X);


        opcode_matrix[0x54] = OPCODE(OP_CODE_MVN, OP_CODE_CATEGORY_STANDALONE, ADDRESS_MODE_BLOCK_MOVE);
        opcode_matrix[0x44] = OPCODE(OP_CODE_MVP, OP_CODE_CATEGORY_STANDALONE, ADDRESS_MODE_BLOCK_MOVE);


        opcode_matrix[0xea] = OPCODE(OP_CODE_NOP, OP_CODE_CATEGORY_STANDALONE, ADDRESS_MODE_IMPLIED);


        opcode_matrix[0x0d] = OPCODE(OP_CODE_ORA, OP_CODE_CATEGORY_ARITHMETIC, ADDRESS_MODE_ABSOLUTE);
        opcode_matrix[0x1d] = OPCODE(OP_CODE_ORA, OP_CODE_CATEGORY_ARITHMETIC, ADDRESS_MODE_ABSOLUTE_INDEXED_X);
        opcode_matrix[0x19] = OPCODE(OP_CODE_ORA, OP_CODE_CATEGORY_ARITHMETIC, ADDRESS_MODE_ABSOLUTE_INDEXED_Y);
        opcode_matrix[0x0f] = OPCODE(OP_CODE_ORA, OP_CODE_CATEGORY_ARITHMETIC, ADDRESS_MODE_ABSOLUTE_LONG);
        opcode_matrix[0x1f] = OPCODE(OP_CODE_ORA, OP_CODE_CATEGORY_ARITHMETIC, ADDRESS_MODE_ABSOLUTE_LONG_INDEXED_X);
        opcode_matrix[0x05] = OPCODE(OP_CODE_ORA, OP_CODE_CATEGORY_ARITHMETIC, ADDRESS_MODE_DIRECT);
        opcode_matrix[0x03] = OPCODE(OP_CODE_ORA, OP_CODE_CATEGORY_ARITHMETIC, ADDRESS_MODE_STACK_RELATIVE);
        opcode_matrix[0x15] = OPCODE(OP_CODE_ORA, OP_CODE_CATEGORY_ARITHMETIC, ADDRESS_MODE_DIRECT_INDEXED_X);
        opcode_matrix[0x12] = OPCODE(OP_CODE_ORA, OP_CODE_CATEGORY_ARITHMETIC, ADDRESS_MODE_DIRECT_INDIRECT);
        opcode_matrix[0x07] = OPCODE(OP_CODE_ORA, OP_CODE_CATEGORY_ARITHMETIC, ADDRESS_MODE_DIRECT_INDIRECT_LONG);
        opcode_matrix[0x13] = OPCODE(OP_CODE_ORA, OP_CODE_CATEGORY_ARITHMETIC, ADDRESS_MODE_STACK_RELATIVE_INDIRECT_INDEXED);
        opcode_matrix[0x01] = OPCODE(OP_CODE_ORA, OP_CODE_CATEGORY_ARITHMETIC, ADDRESS_MODE_DIRECT_INDEXED_INDIRECT);
        opcode_matrix[0x11] = OPCODE(OP_CODE_ORA, OP_CODE_CATEGORY_ARITHMETIC, ADDRESS_MODE_DIRECT_INDIRECT_INDEXED);
        opcode_matrix[0x17] = OPCODE(OP_CODE_ORA, OP_CODE_CATEGORY_ARITHMETIC, ADDRESS_MODE_DIRECT_INDIRECT_LONG_INDEXED);
        opcode_matrix[0x09] = OPCODE(OP_CODE_ORA, OP_CODE_CATEGORY_ARITHMETIC, ADDRESS_MODE_IMMEDIATE);


        opcode_matrix[0xf4] = OPCODE(OP_CODE_PEA, OP_CODE_CATEGORY_STACK, ADDRESS_MODE_STACK);
        opcode_matrix[0xd4] = OPCODE(OP_CODE_PEI, OP_CODE_CATEGORY_STACK, ADDRESS_MODE_STACK);
        opcode_matrix[0x62] = OPCODE(OP_CODE_PER, OP_CODE_CATEGORY_STACK, ADDRESS_MODE_STACK);
        opcode_matrix[0x48] = OPCODE(OP_CODE_PHA, OP_CODE_CATEGORY_STACK, ADDRESS_MODE_STACK);
        opcode_matrix[0x8b] = OPCODE(OP_CODE_PHB, OP_CODE_CATEGORY_STACK, ADDRESS_MODE_STACK);
        opcode_matrix[0x0b] = OPCODE(OP_CODE_PHD, OP_CODE_CATEGORY_STACK, ADDRESS_MODE_STACK);
        opcode_matrix[0x4b] = OPCODE(OP_CODE_PHK, OP_CODE_CATEGORY_STACK, ADDRESS_MODE_STACK);
        opcode_matrix[0x08] = OPCODE(OP_CODE_PHP, OP_CODE_CATEGORY_STACK, ADDRESS_MODE_STACK);
        opcode_matrix[0xda] = OPCODE(OP_CODE_PHX, OP_CODE_CATEGORY_STACK, ADDRESS_MODE_STACK);
        opcode_matrix[0x5a] = OPCODE(OP_CODE_PHY, OP_CODE_CATEGORY_STACK, ADDRESS_MODE_STACK);
        opcode_matrix[0x68] = OPCODE(OP_CODE_PLA, OP_CODE_CATEGORY_STACK, ADDRESS_MODE_STACK);
        opcode_matrix[0xab] = OPCODE(OP_CODE_PLB, OP_CODE_CATEGORY_STACK, ADDRESS_MODE_STACK);
        opcode_matrix[0x2b] = OPCODE(OP_CODE_PLD, OP_CODE_CATEGORY_STACK, ADDRESS_MODE_STACK);
        opcode_matrix[0x28] = OPCODE(OP_CODE_PLP, OP_CODE_CATEGORY_STACK, ADDRESS_MODE_STACK);
        opcode_matrix[0xfa] = OPCODE(OP_CODE_PLX, OP_CODE_CATEGORY_STACK, ADDRESS_MODE_STACK);
        opcode_matrix[0x7a] = OPCODE(OP_CODE_PLY, OP_CODE_CATEGORY_STACK, ADDRESS_MODE_STACK);



        opcode_matrix[0x2e] = OPCODE(OP_CODE_ROL, OP_CODE_CATEGORY_ARITHMETIC, ADDRESS_MODE_ABSOLUTE);
        opcode_matrix[0x2a] = OPCODE(OP_CODE_ROL, OP_CODE_CATEGORY_ARITHMETIC, ADDRESS_MODE_ACCUMULATOR);
        opcode_matrix[0x3e] = OPCODE(OP_CODE_ROL, OP_CODE_CATEGORY_ARITHMETIC, ADDRESS_MODE_ABSOLUTE_INDEXED_X);
        opcode_matrix[0x26] = OPCODE(OP_CODE_ROL, OP_CODE_CATEGORY_ARITHMETIC, ADDRESS_MODE_DIRECT);
        opcode_matrix[0x36] = OPCODE(OP_CODE_ROL, OP_CODE_CATEGORY_ARITHMETIC, ADDRESS_MODE_DIRECT_INDEXED_X);


        opcode_matrix[0x6e] = OPCODE(OP_CODE_ROR, OP_CODE_CATEGORY_ARITHMETIC, ADDRESS_MODE_ABSOLUTE);
        opcode_matrix[0x6a] = OPCODE(OP_CODE_ROR, OP_CODE_CATEGORY_ARITHMETIC, ADDRESS_MODE_ACCUMULATOR);
        opcode_matrix[0x7e] = OPCODE(OP_CODE_ROR, OP_CODE_CATEGORY_ARITHMETIC, ADDRESS_MODE_ABSOLUTE_INDEXED_X);
        opcode_matrix[0x66] = OPCODE(OP_CODE_ROR, OP_CODE_CATEGORY_ARITHMETIC, ADDRESS_MODE_DIRECT);
        opcode_matrix[0x76] = OPCODE(OP_CODE_ROR, OP_CODE_CATEGORY_ARITHMETIC, ADDRESS_MODE_DIRECT_INDEXED_X);


        opcode_matrix[0x40] = OPCODE(OP_CODE_RTI, OP_CODE_CATEGORY_BRANCH, ADDRESS_MODE_STACK);
        opcode_matrix[0x6b] = OPCODE(OP_CODE_RTL, OP_CODE_CATEGORY_BRANCH, ADDRESS_MODE_STACK);
        opcode_matrix[0x60] = OPCODE(OP_CODE_RTS, OP_CODE_CATEGORY_BRANCH, ADDRESS_MODE_STACK);


        opcode_matrix[0xed] = OPCODE(OP_CODE_SBC, OP_CODE_CATEGORY_ARITHMETIC, ADDRESS_MODE_ABSOLUTE);
        opcode_matrix[0xfd] = OPCODE(OP_CODE_SBC, OP_CODE_CATEGORY_ARITHMETIC, ADDRESS_MODE_ABSOLUTE_INDEXED_X);
        opcode_matrix[0xf9] = OPCODE(OP_CODE_SBC, OP_CODE_CATEGORY_ARITHMETIC, ADDRESS_MODE_ABSOLUTE_INDEXED_Y);
        opcode_matrix[0xff] = OPCODE(OP_CODE_SBC, OP_CODE_CATEGORY_ARITHMETIC, ADDRESS_MODE_ABSOLUTE_LONG_INDEXED_X);
        opcode_matrix[0xe5] = OPCODE(OP_CODE_SBC, OP_CODE_CATEGORY_ARITHMETIC, ADDRESS_MODE_DIRECT);
        opcode_matrix[0xe3] = OPCODE(OP_CODE_SBC, OP_CODE_CATEGORY_ARITHMETIC, ADDRESS_MODE_STACK_RELATIVE);
        opcode_matrix[0xf5] = OPCODE(OP_CODE_SBC, OP_CODE_CATEGORY_ARITHMETIC, ADDRESS_MODE_DIRECT_INDEXED_X);
        opcode_matrix[0xf2] = OPCODE(OP_CODE_SBC, OP_CODE_CATEGORY_ARITHMETIC, ADDRESS_MODE_DIRECT_INDIRECT);
        opcode_matrix[0xe7] = OPCODE(OP_CODE_SBC, OP_CODE_CATEGORY_ARITHMETIC, ADDRESS_MODE_DIRECT_INDIRECT_LONG);
        opcode_matrix[0xf3] = OPCODE(OP_CODE_SBC, OP_CODE_CATEGORY_ARITHMETIC, ADDRESS_MODE_STACK_RELATIVE_INDIRECT_INDEXED);
        opcode_matrix[0xe1] = OPCODE(OP_CODE_SBC, OP_CODE_CATEGORY_ARITHMETIC, ADDRESS_MODE_DIRECT_INDEXED_INDIRECT);
        opcode_matrix[0xf1] = OPCODE(OP_CODE_SBC, OP_CODE_CATEGORY_ARITHMETIC, ADDRESS_MODE_DIRECT_INDIRECT_INDEXED);
        opcode_matrix[0xf7] = OPCODE(OP_CODE_SBC, OP_CODE_CATEGORY_ARITHMETIC, ADDRESS_MODE_DIRECT_INDIRECT_LONG_INDEXED);
        opcode_matrix[0xe9] = OPCODE(OP_CODE_SBC, OP_CODE_CATEGORY_ARITHMETIC, ADDRESS_MODE_IMMEDIATE);


        opcode_matrix[0x38] = OPCODE(OP_CODE_SEC, OP_CODE_CATEGORY_STANDALONE, ADDRESS_MODE_IMPLIED);
        opcode_matrix[0xf8] = OPCODE(OP_CODE_SED, OP_CODE_CATEGORY_STANDALONE, ADDRESS_MODE_IMPLIED);
        opcode_matrix[0x78] = OPCODE(OP_CODE_SEI, OP_CODE_CATEGORY_STANDALONE, ADDRESS_MODE_IMPLIED);
        opcode_matrix[0xe2] = OPCODE(OP_CODE_SEP, OP_CODE_CATEGORY_STANDALONE, ADDRESS_MODE_IMMEDIATE);


        opcode_matrix[0x8d] = OPCODE(OP_CODE_STA, OP_CODE_CATEGORY_STORE, ADDRESS_MODE_ABSOLUTE);
        opcode_matrix[0x9d] = OPCODE(OP_CODE_STA, OP_CODE_CATEGORY_STORE, ADDRESS_MODE_ABSOLUTE_INDEXED_X);
        opcode_matrix[0x8f] = OPCODE(OP_CODE_STA, OP_CODE_CATEGORY_STORE, ADDRESS_MODE_ABSOLUTE_LONG);
        opcode_matrix[0x9f] = OPCODE(OP_CODE_STA, OP_CODE_CATEGORY_STORE, ADDRESS_MODE_ABSOLUTE_LONG_INDEXED_X);
        opcode_matrix[0x85] = OPCODE(OP_CODE_STA, OP_CODE_CATEGORY_STORE, ADDRESS_MODE_DIRECT);
        opcode_matrix[0x83] = OPCODE(OP_CODE_STA, OP_CODE_CATEGORY_STORE, ADDRESS_MODE_STACK_RELATIVE);
        opcode_matrix[0x95] = OPCODE(OP_CODE_STA, OP_CODE_CATEGORY_STORE, ADDRESS_MODE_DIRECT_INDEXED_X);
        opcode_matrix[0x92] = OPCODE(OP_CODE_STA, OP_CODE_CATEGORY_STORE, ADDRESS_MODE_DIRECT_INDIRECT);
        opcode_matrix[0x87] = OPCODE(OP_CODE_STA, OP_CODE_CATEGORY_STORE, ADDRESS_MODE_DIRECT_INDIRECT_LONG);
        opcode_matrix[0x93] = OPCODE(OP_CODE_STA, OP_CODE_CATEGORY_STORE, ADDRESS_MODE_STACK_RELATIVE_INDIRECT_INDEXED);
        opcode_matrix[0x81] = OPCODE(OP_CODE_STA, OP_CODE_CATEGORY_STORE, ADDRESS_MODE_DIRECT_INDEXED_INDIRECT);
        opcode_matrix[0x91] = OPCODE(OP_CODE_STA, OP_CODE_CATEGORY_STORE, ADDRESS_MODE_DIRECT_INDIRECT_INDEXED);
        opcode_matrix[0x97] = OPCODE(OP_CODE_STA, OP_CODE_CATEGORY_STORE, ADDRESS_MODE_DIRECT_INDIRECT_LONG_INDEXED);


        opcode_matrix[0xdb] = OPCODE(OP_CODE_STP, OP_CODE_CATEGORY_STANDALONE, ADDRESS_MODE_IMPLIED);


        opcode_matrix[0x8e] = OPCODE(OP_CODE_STX, OP_CODE_CATEGORY_STORE, ADDRESS_MODE_ABSOLUTE);
        opcode_matrix[0x86] = OPCODE(OP_CODE_STX, OP_CODE_CATEGORY_STORE, ADDRESS_MODE_DIRECT);
        opcode_matrix[0x96] = OPCODE(OP_CODE_STX, OP_CODE_CATEGORY_STORE, ADDRESS_MODE_DIRECT_INDEXED_Y);


        opcode_matrix[0x8c] = OPCODE(OP_CODE_STY, OP_CODE_CATEGORY_STORE, ADDRESS_MODE_ABSOLUTE);
        opcode_matrix[0x84] = OPCODE(OP_CODE_STY, OP_CODE_CATEGORY_STORE, ADDRESS_MODE_DIRECT);
        opcode_matrix[0x94] = OPCODE(OP_CODE_STY, OP_CODE_CATEGORY_STORE, ADDRESS_MODE_DIRECT_INDEXED_X);


        opcode_matrix[0x9c] = OPCODE(OP_CODE_STZ, OP_CODE_CATEGORY_STORE, ADDRESS_MODE_ABSOLUTE);
        opcode_matrix[0x9e] = OPCODE(OP_CODE_STZ, OP_CODE_CATEGORY_STORE, ADDRESS_MODE_ABSOLUTE_INDEXED_X);
        opcode_matrix[0x64] = OPCODE(OP_CODE_STZ, OP_CODE_CATEGORY_STORE, ADDRESS_MODE_DIRECT);
        opcode_matrix[0x74] = OPCODE(OP_CODE_STZ, OP_CODE_CATEGORY_STORE, ADDRESS_MODE_DIRECT_INDEXED_X);


        opcode_matrix[0xaa] = OPCODE(OP_CODE_TAX, OP_CODE_CATEGORY_TRANSFER, ADDRESS_MODE_IMPLIED);
        opcode_matrix[0xab] = OPCODE(OP_CODE_TAY, OP_CODE_CATEGORY_TRANSFER, ADDRESS_MODE_IMPLIED);
        opcode_matrix[0x5b] = OPCODE(OP_CODE_TCD, OP_CODE_CATEGORY_TRANSFER, ADDRESS_MODE_IMPLIED);
        opcode_matrix[0x1b] = OPCODE(OP_CODE_TCS, OP_CODE_CATEGORY_TRANSFER, ADDRESS_MODE_IMPLIED);
        opcode_matrix[0x7b] = OPCODE(OP_CODE_TDC, OP_CODE_CATEGORY_TRANSFER, ADDRESS_MODE_IMPLIED);


        opcode_matrix[0x1c] = OPCODE(OP_CODE_TRB, OP_CODE_CATEGORY_STANDALONE, ADDRESS_MODE_ABSOLUTE);
        opcode_matrix[0x14] = OPCODE(OP_CODE_TRB, OP_CODE_CATEGORY_STANDALONE, ADDRESS_MODE_DIRECT);


        opcode_matrix[0x0c] = OPCODE(OP_CODE_TSB, OP_CODE_CATEGORY_STANDALONE, ADDRESS_MODE_ABSOLUTE);
        opcode_matrix[0x04] = OPCODE(OP_CODE_TSB, OP_CODE_CATEGORY_STANDALONE, ADDRESS_MODE_DIRECT);


        opcode_matrix[0x3b] = OPCODE(OP_CODE_TSC, OP_CODE_CATEGORY_TRANSFER, ADDRESS_MODE_ACCUMULATOR);
        opcode_matrix[0xba] = OPCODE(OP_CODE_TSX, OP_CODE_CATEGORY_TRANSFER, ADDRESS_MODE_ACCUMULATOR);
        opcode_matrix[0x8a] = OPCODE(OP_CODE_TXA, OP_CODE_CATEGORY_TRANSFER, ADDRESS_MODE_ACCUMULATOR);
        opcode_matrix[0x9a] = OPCODE(OP_CODE_TXS, OP_CODE_CATEGORY_TRANSFER, ADDRESS_MODE_ACCUMULATOR);
        opcode_matrix[0x9b] = OPCODE(OP_CODE_TXY, OP_CODE_CATEGORY_TRANSFER, ADDRESS_MODE_ACCUMULATOR);
        opcode_matrix[0x98] = OPCODE(OP_CODE_TYA, OP_CODE_CATEGORY_TRANSFER, ADDRESS_MODE_ACCUMULATOR);
        opcode_matrix[0xbb] = OPCODE(OP_CODE_TYX, OP_CODE_CATEGORY_TRANSFER, ADDRESS_MODE_ACCUMULATOR);
        opcode_matrix[0xcb] = OPCODE(OP_CODE_WAI, OP_CODE_CATEGORY_STANDALONE, ADDRESS_MODE_ACCUMULATOR);
        opcode_matrix[0x42] = OPCODE(OP_CODE_WDM, OP_CODE_CATEGORY_STANDALONE, ADDRESS_MODE_ACCUMULATOR);
        opcode_matrix[0xeb] = OPCODE(OP_CODE_XBA, OP_CODE_CATEGORY_STANDALONE, ADDRESS_MODE_ACCUMULATOR);
        opcode_matrix[0xfb] = OPCODE(OP_CODE_XCE, OP_CODE_CATEGORY_STANDALONE, ADDRESS_MODE_ACCUMULATOR);
    }

    reg_dbr = 0;
    reg_pbr = 0;
    reg_x.reg_xL = 0;
    reg_y.reg_yL = 0;
    reg_d = 0;
    reg_e = 1;
    reg_s = (reg_s & 0xff) | 0x0100;

    reg_p = CPU_STATUS_FLAG_MEMORY | CPU_STATUS_FLAG_INDEX | CPU_STATUS_FLAG_IRQ_DISABLE | CPU_STATUS_FLAG_CARRY;

    /* on reset, pc gets loaded with the contents
    of the pointer 00fffc... */
    //reg_pc = *(unsigned short *)(cpu_ram + 0xfffc);
    //access_memory(EFFECTIVE_ADDRESS(0x00, 0xfffc), 0, &reg_pc, 2);
    reset_vector = memory_pointer(EFFECTIVE_ADDRESS(0x00, 0xfffc));

    if(reset_vector)
    {
        reg_pc = *reset_vector;
    }
}


char *opcode_str(struct op_code_t *op_code, int effective_address)
{
    char *op_code_str;
    char *addr_mode_str;
//    char *addr_mode_end_str;

    unsigned char *opcode_address;

    int opcode_offset;

//    unsigned char value;

    static char instruction_str[512];
    static char byte_buffer[128];
    static char bytes_buffer[256];

    bytes_buffer[0] = 0;
    instruction_str[0] = 0;

    opcode_offset = address_mode_operand_size[op_code->address_mode] + 1;
    opcode_address = memory_pointer(effective_address);

    switch(op_code->opcode)
    {
        case OP_CODE_ADC:
            op_code_str = "ADC";
        break;

        case OP_CODE_AND:
            op_code_str = "AND";
        break;

        case OP_CODE_ASL:
            op_code_str = "ASL";
        break;

        case OP_CODE_BCC:
            op_code_str = "BCC";
        break;

        case OP_CODE_BCS:
            op_code_str = "BCS";
        break;

        case OP_CODE_BEQ:
            op_code_str = "BEQ";
        break;

        case OP_CODE_BMI:
            op_code_str = "BMI";
        break;

        case OP_CODE_BNE:
            op_code_str = "BNE";
        break;

        case OP_CODE_BPL:
            op_code_str = "BPL";
        break;

        case OP_CODE_BRA:
            op_code_str = "BRA";
        break;

        case OP_CODE_BRK:
            op_code_str = "BRK";
        break;

        case OP_CODE_BRL:
            op_code_str = "BRL";
        break;

        case OP_CODE_BVC:
            op_code_str = "BVC";
        break;

        case OP_CODE_BVS:
            op_code_str = "BVS";
        break;

        case OP_CODE_BIT:
            op_code_str = "BIT";
        break;

        case OP_CODE_CLC:
            op_code_str = "CLC";
        break;

        case OP_CODE_CLD:
            op_code_str = "CLD";
        break;

        case OP_CODE_CLV:
            op_code_str = "CLV";
        break;

        case OP_CODE_CLI:
            op_code_str = "CLI";
        break;

        case OP_CODE_CMP:
            op_code_str = "CMP";
        break;

        case OP_CODE_CPX:
            op_code_str = "CPX";
        break;

        case OP_CODE_CPY:
            op_code_str = "CPY";
        break;

        case OP_CODE_DEC:
            op_code_str = "DEC";
        break;

        case OP_CODE_INC:
            op_code_str = "INC";
        break;

        case OP_CODE_DEX:
            op_code_str = "DEX";
        break;

        case OP_CODE_INX:
            op_code_str = "INX";
        break;

        case OP_CODE_DEY:
            op_code_str = "DEY";
        break;

        case OP_CODE_INY:
            op_code_str = "INY";
        break;

        case OP_CODE_JMP:
            op_code_str = "JMP";
        break;

        case OP_CODE_JML:
            op_code_str = "JML";
        break;

        case OP_CODE_JSL:
            op_code_str = "JSL";
        break;

        case OP_CODE_JSR:
            op_code_str = "JSR";
        break;

        case OP_CODE_LDA:
            op_code_str = "LDA";
        break;

        case OP_CODE_STA:
            op_code_str = "STA";
        break;

        case OP_CODE_LDX:
            op_code_str = "LDX";
        break;

        case OP_CODE_STX:
            op_code_str = "STX";
        break;

        case OP_CODE_LDY:
            op_code_str = "LDY";
        break;

        case OP_CODE_STY:
            op_code_str = "STY";
        break;

        case OP_CODE_STZ:
            op_code_str = "STZ";
        break;

        case OP_CODE_LSR:
            op_code_str = "LSR";
        break;

        case OP_CODE_MVN:
            op_code_str = "MVN";
        break;

        case OP_CODE_MVP:
            op_code_str = "MVP";
        break;

        case OP_CODE_NOP:
            op_code_str = "NOP";
        break;

        case OP_CODE_PEA:
            op_code_str = "PEA";
        break;

        case OP_CODE_PEI:
            op_code_str = "PEI";
        break;

        case OP_CODE_PER:
            op_code_str = "PER";
        break;

        case OP_CODE_PHA:
            op_code_str = "PHA";
        break;

        case OP_CODE_PHB:
            op_code_str = "PHB";
        break;

        case OP_CODE_PHD:
            op_code_str = "PHD";
        break;

        case OP_CODE_PHK:
            op_code_str = "PHK";
        break;

        case OP_CODE_PHP:
            op_code_str = "PHP";
        break;

        case OP_CODE_PHX:
            op_code_str = "PHX";
        break;

        case OP_CODE_PHY:
            op_code_str = "PHY";
        break;

        case OP_CODE_PLA:
            op_code_str = "PLA";
        break;

        case OP_CODE_PLB:
            op_code_str = "PLB";
        break;

        case OP_CODE_PLD:
            op_code_str = "PLD";
        break;

        case OP_CODE_PLP:
            op_code_str = "PLP";
        break;

        case OP_CODE_PLX:
            op_code_str = "PLX";
        break;

        case OP_CODE_PLY:
            op_code_str = "PLY";
        break;

        case OP_CODE_REP:
            op_code_str = "REP";
        break;

        case OP_CODE_ROL:
            op_code_str = "ROL";
        break;

        case OP_CODE_ROR:
            op_code_str = "ROR";
        break;

        case OP_CODE_RTI:
            op_code_str = "RTI";
        break;

        case OP_CODE_RTL:
            op_code_str = "RTL";
        break;

        case OP_CODE_RTS:
            op_code_str = "RTS";
        break;

        case OP_CODE_SBC:
            op_code_str = "SBC";
        break;

        case OP_CODE_SEP:
            op_code_str = "SEP";
        break;

        case OP_CODE_SEC:
            op_code_str = "SEC";
        break;

        case OP_CODE_SED:
            op_code_str = "SED";
        break;

        case OP_CODE_SEI:
            op_code_str = "SEI";
        break;

        case OP_CODE_TAX:
            op_code_str = "TAX";
        break;

        case OP_CODE_TAY:
            op_code_str = "TAY";
        break;

        case OP_CODE_TCD:
            op_code_str = "TCD";
        break;

        case OP_CODE_TCS:
            op_code_str = "TCS";
        break;

        case OP_CODE_TDC:
            op_code_str = "TDC";
        break;

        case OP_CODE_TSC:
            op_code_str = "TSC";
        break;

        case OP_CODE_TSX:
            op_code_str = "TSX";
        break;

        case OP_CODE_TXA:
            op_code_str = "TXA";
        break;

        case OP_CODE_TXS:
            op_code_str = "TXS";
        break;

        case OP_CODE_TXY:
            op_code_str = "TXY";
        break;

        case OP_CODE_TYA:
            op_code_str = "TYA";
        break;

        case OP_CODE_TYX:
            op_code_str = "TYX";
        break;

        case OP_CODE_WAI:
            op_code_str = "WAI";
        break;

        case OP_CODE_WDM:
            op_code_str = "WDM";
        break;

        case OP_CODE_XBA:
            op_code_str = "XBA";
        break;

        case OP_CODE_TRB:
            op_code_str = "TRB";
        break;

        case OP_CODE_TSB:
            op_code_str = "TSB";
        break;

        case OP_CODE_STP:
            op_code_str = "STP";
        break;

        case OP_CODE_COP:
            op_code_str = "COP";
        break;

        case OP_CODE_EOR:
            op_code_str = "EOR";
        break;

        case OP_CODE_ORA:
            op_code_str = "ORA";
        break;

        case OP_CODE_XCE:
            op_code_str = "XCE";
        break;

        default:
        case OP_CODE_UNKNOWN:
            op_code_str = "UNKNOWN";
        break;
    }


    switch(op_code->address_mode)
    {
        case ADDRESS_MODE_ABSOLUTE:
            addr_mode_str = "(addr) ";
 //           addr_mode_end_str = "";
        break;

        case ADDRESS_MODE_ABSOLUTE_INDEXED_INDIRECT:
            addr_mode_str = "(pointer + X) ";
        break;

        case ADDRESS_MODE_ABSOLUTE_INDEXED_X:
            addr_mode_str = "(addr + X) ";
        break;

        case ADDRESS_MODE_ABSOLUTE_INDEXED_Y:
            addr_mode_str = "(addr + Y) ";
        break;

        case ADDRESS_MODE_ABSOLUTE_INDIRECT:
            addr_mode_str = "(pointer) ";
        break;

        case ADDRESS_MODE_ABSOLUTE_LONG_INDEXED_X:
            addr_mode_str = "(long addr + X) ";
        break;

        case ADDRESS_MODE_ABSOLUTE_LONG:
            addr_mode_str = "(long addr) ";
        break;

        case ADDRESS_MODE_ACCUMULATOR:
            addr_mode_str = "(accumulator) ";
        break;

        case ADDRESS_MODE_BLOCK_MOVE:
            addr_mode_str = "(dst_bank src_bank) ";
        break;

        case ADDRESS_MODE_DIRECT_INDEXED_INDIRECT:
            addr_mode_str = "direct indexed indirect - (d,x)";
        break;

        case ADDRESS_MODE_DIRECT_INDEXED_X:
            addr_mode_str = "direct indexed X - d,x";
        break;

        case ADDRESS_MODE_DIRECT_INDEXED_Y:
            addr_mode_str = "direct indexed Y - d,y";
        break;

        case ADDRESS_MODE_DIRECT_INDIRECT_INDEXED:
            addr_mode_str = "direct indirect indexed - (d),y";
        break;

        case ADDRESS_MODE_DIRECT_INDIRECT_LONG_INDEXED:
            addr_mode_str = "direct indirect long indexed - [d],y";
        break;

        case ADDRESS_MODE_DIRECT_INDIRECT_LONG:
            addr_mode_str = "direct indirect long - [d]";
        break;

        case ADDRESS_MODE_DIRECT_INDIRECT:
            addr_mode_str = "direct indirect - (d)";
        break;

        case ADDRESS_MODE_DIRECT:
            addr_mode_str = "direct - d";
        break;

        case ADDRESS_MODE_IMMEDIATE:
            if(reg_p & CPU_STATUS_FLAG_MEMORY)
            {
                opcode_offset--;
            }
            addr_mode_str = "(value)";
        break;

        case ADDRESS_MODE_IMPLIED:
            addr_mode_str = "";
        break;

        case ADDRESS_MODE_PROGRAM_COUNTER_RELATIVE_LONG:
            addr_mode_str = "program counter relative long - rl";
        break;

        case ADDRESS_MODE_PROGRAM_COUNTER_RELATIVE:
            addr_mode_str = "program counter relative - r";
        break;

        case ADDRESS_MODE_STACK:
            addr_mode_str = "stack - s";
        break;

        case ADDRESS_MODE_STACK_RELATIVE:
            addr_mode_str = "stack relative - d,s";
        break;

        case ADDRESS_MODE_STACK_RELATIVE_INDIRECT_INDEXED:
            addr_mode_str = "stack relative indirect indexed - (d,s),y";
        break;

        default:
        case ADDRESS_MODE_UNKNOWN:
            addr_mode_str = "unknown";
        break;
    }

    for(; opcode_offset > 1; opcode_offset--)
    {
        //access_memory(effective_address + opcode_offset - 1, 0, &value, 1);
        sprintf(byte_buffer, "%02x", opcode_address[opcode_offset - 1]);
        strcat(bytes_buffer, byte_buffer);
    }

    strcpy(instruction_str, op_code_str);
    strcat(instruction_str, " ");
    strcat(instruction_str, addr_mode_str);
    strcat(instruction_str, bytes_buffer);

//    strcat(instruction_str, addr_mode_str);
//    strcat(instruction_str, ")");

    return instruction_str;
}

void disassemble(int start, int byte_count)
{
    int bytes_disassmd = 0;
    int opcode_offset;

    if(start >= 0)
    {
        reg_pc = start & 0xffff;
        reg_pbr = (start >> 16) & 0xff;
    }

    while(bytes_disassmd < byte_count)
    {
        opcode_offset = disassemble_current(0);

        bytes_disassmd += opcode_offset;

        if((unsigned short)(reg_pc + (unsigned short)opcode_offset) < (unsigned short)reg_pc)
        {
            reg_pbr++;
        }

        reg_pc += opcode_offset;
    }
}

int disassemble_current(int show_registers)
{
    int address;
    int opcode_offset;
    struct op_code_t opcode;
//    unsigned char opcode_byte;
    int i;
    char *op_str;
    unsigned char *opcode_address;


    //opcode = opcode_matrix[cpu_ram[address]];
    //opcode_offset = address_mode_operand_size[opcode.address_mode] + 1;

//    address = EFFECTIVE_ADDRESS(reg_pbr, reg_pc);
//    access_memory(address, 0, &opcode_byte, 1);
//    opcode = opcode_matrix[opcode_byte];
//    op_str = opcode_str(&opcode, address);
//    opcode_offset = address_mode_operand_size[opcode.address_mode];

    address = EFFECTIVE_ADDRESS(reg_pbr, reg_pc);
    opcode_address = memory_pointer(address);
    opcode = opcode_matrix[opcode_address[0]];
    op_str = opcode_str(&opcode, address);
    opcode_offset = address_mode_operand_size[opcode.address_mode];



    if(show_registers)
    {
        printf("\n\n");
        printf("=========== REGISTERS ===========\n");
        printf("[A: %04x] | ", reg_accum.reg_accumC);
        printf("[X: %04x] | ", reg_x.reg_x);
        printf("[Y: %04x] | ", reg_y.reg_y);
        printf("[S: %04x] | ", reg_s);
        printf("[D: %04x]\n", reg_d);
        printf("[DBR: %02x] | ", reg_dbr);
        printf("[PBR: %02x] | ", reg_pbr);
        printf("[P: N:%d V:%d M:%d X:%d D:%d Z:%d I:%d C:%d]\n", (reg_p & CPU_STATUS_FLAG_NEGATIVE) && 1,
                                                                (reg_p & CPU_STATUS_FLAG_OVERFLOW) && 1,
                                                                (reg_p & CPU_STATUS_FLAG_MEMORY) && 1,
                                                                (reg_p & CPU_STATUS_FLAG_INDEX) && 1,
                                                                (reg_p & CPU_STATUS_FLAG_DECIMAL) && 1,
                                                                (reg_p & CPU_STATUS_FLAG_ZERO) && 1,
                                                                (reg_p & CPU_STATUS_FLAG_IRQ_DISABLE) && 1,
                                                                (reg_p & CPU_STATUS_FLAG_CARRY) && 1);
        printf("[E: %02x]\n", reg_e);
        printf("=========== REGISTERS ===========\n");
    }

    printf("[0x%02x:0x%04x]: ", (address >> 16) & 0xff, address & 0xffff);

    if(opcode.address_mode == ADDRESS_MODE_IMMEDIATE)
    {
        if(reg_p & CPU_STATUS_FLAG_MEMORY)
        {
            opcode_offset--;
        }
    }

    for(i = 0; i < 4; i++)
    {
        if(i < opcode_offset)
        {
            //access_memory(address + opcode_offset + 1, 0, &opcode_byte, 1);
            //printf("%02x ", opcode_byte);
            printf("%02x ", opcode_address[i + 1]);
        }
        else
        {
            printf("   ");
        }
    }
    printf("; ");

    printf("%s\n", op_str);

    return opcode_offset;
}

int view_hardware_registers()
{
    int i;

    for(i = 0; i < sizeof(hardware_regs.hardware_regs); i++)
    {
        printf("<0x%04x>: 0x%02x\n", CPU_REGS_START_ADDRESS + i, hardware_regs.hardware_regs[i]);
    }
}

__fastcall int cpu_access_location(unsigned int effective_address)
{
    unsigned int temp_address;
    unsigned int temp_bank;

    temp_address = effective_address & 0xffff;

    if(temp_address >= PPU_START_ADDRESS && temp_address <= PPU_END_ADDRESS)
    {
        /* ppu registers... */
        return CPU_ACCESS_PPU;
    }
    else if(temp_address >= CPU_REGS_START_ADDRESS && temp_address <= CPU_REGS_END_ADDRESS)
    {
        return CPU_ACCESS_REGS;
    }
    else
    {
        if(effective_address >= RAM2_ADDRESS_START && effective_address <= RAM2_ADDRESS_END)
        {
            /* WRAM2... */
            return CPU_ACCESS_WRAM2;
        }
        else
        {
            temp_address = effective_address & 0xffff;
            temp_bank = (effective_address & 0x007f0000) >> 16;
            /* either WRAM1 or ROM access... */
            if((temp_address >= RAM1_START_ADDRESS && temp_address <= RAM1_END_ADDRESS) &&
               (temp_bank >= RAM1_START_BANK && temp_bank <= RAM1_END_BANK))
            {
                /* WRAM1... */
                return CPU_ACCESS_WRAM1;
            }
            else
            {
                /* ROM... */
                return CPU_ACCESS_ROM;
            }
        }
    }
}

__fastcall void *memory_pointer(unsigned int effective_address)
{
    int access_location;
    void *pointer = NULL;

    access_location = cpu_access_location(effective_address);

    switch(access_location)
    {
        case CPU_ACCESS_ROM:
            pointer = rom_pointer(effective_address);
        break;

        case CPU_ACCESS_REGS:
            pointer = (void *)(hardware_regs.hardware_regs + (effective_address & 0xffff) - CPU_REGS_START_ADDRESS);
        break;

        case CPU_ACCESS_WRAM1:
            pointer = (void *)(wram1 + (effective_address & 0xffff) - RAM1_START_ADDRESS);
        break;

        case CPU_ACCESS_PPU:
            pointer = ppu_pointer(effective_address);
        break;
    }

    return pointer;
}

void step_cpu()
{
    unsigned int src_value;
    unsigned int dst_value;
    struct op_code_t opcode;
    unsigned int effective_address;
//    unsigned int instruction_offset;
    int temp;
    int temp2;
    int temp3;
    int byte_write;
//    unsigned char opcode_byte;
    unsigned char *opcode_address;
    unsigned char *src_address;
    unsigned char *dst_address;


    effective_address = EFFECTIVE_ADDRESS(reg_pbr, reg_pc);
    opcode_address = memory_pointer(effective_address);
    opcode = opcode_matrix[opcode_address[0]];
    reg_pc += address_mode_operand_size[opcode.address_mode] + 1;

    printf("cpu step: %lu\n", step_count);
    step_count++;

    switch(opcode.address_mode)
    {
        case ADDRESS_MODE_ABSOLUTE:
            src_value = *(unsigned short *)(opcode_address + 1);
            src_value |= reg_dbr << 16;
        break;

        case ADDRESS_MODE_ABSOLUTE_INDEXED_INDIRECT:
            src_value = *(unsigned short *)(opcode_address + 1);
            src_value &= 0x0000ffff;
        break;

        case ADDRESS_MODE_ABSOLUTE_INDEXED_X:
            src_value = *(unsigned short *)(opcode_address + 1) + reg_x.reg_x;
            src_value |= reg_dbr << 16;
        break;

        case ADDRESS_MODE_ABSOLUTE_INDEXED_Y:
            src_value = *(unsigned short *)(opcode_address + 1) + reg_y.reg_y;
            src_value |= reg_dbr << 16;
        break;

        case ADDRESS_MODE_ABSOLUTE_INDIRECT:
            src_value = *(unsigned short *)(opcode_address + 1);
        break;

        case ADDRESS_MODE_ABSOLUTE_LONG_INDEXED_X:
            src_value = *(unsigned int *)(opcode_address + 1) & 0x00ffffff;
            src_value += reg_x.reg_x;
        break;

        case ADDRESS_MODE_ABSOLUTE_LONG:
            src_value = *(unsigned int *)(opcode_address + 1) & 0x00ffffff;
            src_value &= 0x00ffffff;
        break;

        case ADDRESS_MODE_ACCUMULATOR:
            src_value = reg_accum.reg_accumC;
        break;

        case ADDRESS_MODE_BLOCK_MOVE:
            reg_dbr = opcode_address[1];
            dst_value = (((unsigned int)reg_dbr) << 16) & 0x00ffffff;
            dst_value |= reg_y.reg_y;

            src_value = (((unsigned int)opcode_address[2]) << 16) & 0x00ffffff;
            src_value |= reg_x.reg_x;
        break;

        case ADDRESS_MODE_DIRECT_INDEXED_INDIRECT:
            src_value = opcode_address[1];
            src_value += reg_d + reg_x.reg_x;
            opcode_address = memory_pointer(src_value);
            reg_x.reg_x = *(unsigned short *)(opcode_address);
            src_value = ((unsigned int)reg_x.reg_x) | (((unsigned int)reg_dbr) << 16);
        break;

        case ADDRESS_MODE_DIRECT_INDEXED_X:
            src_value = opcode_address[1];
            src_value += reg_d + reg_x.reg_x;
        break;

        case ADDRESS_MODE_DIRECT_INDEXED_Y:
            src_value = opcode_address[1];
            src_value += reg_d + reg_y.reg_y;
        break;

        case ADDRESS_MODE_DIRECT_INDIRECT_INDEXED:
            src_value = opcode_address[1] + reg_d;
            src_value |= (((unsigned int)reg_dbr) << 16) & 0x00ffffff;
            src_value += reg_y.reg_y;
        break;

        case ADDRESS_MODE_DIRECT_INDIRECT_LONG_INDEXED:
            src_value = opcode_address[1] + reg_d;
            opcode_address = memory_pointer(src_value);
            src_value = *(unsigned int *)(opcode_address) & 0x00ffffff;
            src_value += reg_y.reg_y;
        break;

        case ADDRESS_MODE_DIRECT_INDIRECT_LONG:
            src_value = opcode_address[1] + reg_d;
        break;

        case ADDRESS_MODE_DIRECT_INDIRECT:
            src_value = opcode_address[1] + reg_d;
            src_value |= (((unsigned int)reg_dbr) << 16) & 0x00ffffff;
        break;

        case ADDRESS_MODE_DIRECT:
            src_value = opcode_address[1] + reg_d;
        break;

        case ADDRESS_MODE_IMMEDIATE:
            src_value = *(unsigned short *)(opcode_address + 1);

            if((opcode.opcode == OP_CODE_LDA && (reg_p & CPU_STATUS_FLAG_MEMORY)) ||
               ((opcode.opcode == OP_CODE_LDX || opcode.opcode == OP_CODE_LDY) && (reg_p & CPU_STATUS_FLAG_INDEX)) ||
               (opcode.opcode == OP_CODE_REP || opcode.opcode == OP_CODE_SEP))
            {
                /* this is done here because reg_pc got advanced by two bytes
                after the instruction byte, but when the memory flag is one,
                immediate addressing takes a single byte from memory... */
                reg_pc--;
            }
        break;

        case ADDRESS_MODE_IMPLIED:

        break;

        case ADDRESS_MODE_PROGRAM_COUNTER_RELATIVE_LONG:
            src_value = *(unsigned short *)(opcode_address + 1);
        break;

        case ADDRESS_MODE_PROGRAM_COUNTER_RELATIVE:
            src_value = opcode_address[1];
        break;

        case ADDRESS_MODE_STACK:

        break;

        case ADDRESS_MODE_STACK_RELATIVE:
            src_value = reg_s + opcode_address[1];
        break;

        case ADDRESS_MODE_STACK_RELATIVE_INDIRECT_INDEXED:
            src_value = reg_s + opcode_address[1];
            opcode_address = memory_pointer(src_value);
            src_value = *(unsigned int *)(opcode_address);
            src_value |= (((unsigned int)reg_dbr) << 16) & 0x00ffffff;
            src_value += reg_y.reg_y;
        break;

        default:
        case ADDRESS_MODE_UNKNOWN:
//            addr_mode_str = "unknown";
        break;
    }









    switch(opcode.opcode_category)
    {

        case OP_CODE_CATEGORY_ARITHMETIC:
//
//            byte_write = 0;
//
//                 case OP_CODE_ROL:
////            op_code_str = "ROL";
//        break;
//
//        case OP_CODE_ROR:
////            op_code_str = "ROR";
//        break;

            switch(opcode.opcode)
            {
                case OP_CODE_ADC:
                case OP_CODE_ASL:
                case OP_CODE_SBC:
                case OP_CODE_ROR:
                case OP_CODE_ROL:

                    switch(opcode.opcode)
                    {
                        case OP_CODE_ADC:
                            byte_write = reg_p & CPU_STATUS_FLAG_MEMORY;

                            if(byte_write)
                            {
                                temp2 = (reg_accum.reg_accumA & 0x80) >> 7;
                                temp = (signed char)reg_accum.reg_accumA + (signed char)src_value + (reg_p & CPU_STATUS_FLAG_CARRY);
                                reg_accum.reg_accumA = temp;
                                temp2 = temp2 != ((temp & 0x80) >> 7);
                            }
                            else
                            {
                                temp2 = (reg_accum.reg_accumC & 0x8000) >> 15;
                                temp = (signed short)reg_accum.reg_accumC + (signed short)src_value + (reg_p & CPU_STATUS_FLAG_CARRY);
                                reg_accum.reg_accumA = temp;
                                temp2 = temp2 != (temp & 0x8000) >> 15;
                            }
                        break;


                        case OP_CODE_SBC:

                        break;


                        case OP_CODE_ROR:
                        case OP_CODE_ROL:

                        break;


                        case OP_CODE_ASL:
                        case OP_CODE_LSR:
                            if(opcode.address_mode == ADDRESS_MODE_ACCUMULATOR)
                            {
                                byte_write = reg_p & CPU_STATUS_FLAG_MEMORY;

                                if(byte_write)
                                {
                                    temp = opcode.opcode == OP_CODE_ASL ? reg_accum.reg_accumA << 1 : reg_accum.reg_accumA >> 1;
                                    reg_accum.reg_accumA = temp;
                                }
                                else
                                {
                                    temp = opcode.opcode == OP_CODE_ASL ? reg_accum.reg_accumC << 1 : reg_accum.reg_accumC >> 1;
                                    reg_accum.reg_accumC = temp;
                                }
                            }
                            else
                            {
                                opcode_address = memory_pointer(src_value);
                                temp = (*(unsigned short *)(opcode_address));
                                temp = opcode.opcode == OP_CODE_ASL ? temp << 1 : temp >> 1;
                                (*(unsigned short *)(opcode_address)) = temp;
                            }
                        break;
                    }

                    if(byte_write)
                    {
                        temp3 = temp & 0x00ffffff00;
                    }
                    else
                    {
                        temp3 = temp & 0xffff0000;
                    }

                    reg_p = temp2 ? reg_p | CPU_STATUS_FLAG_OVERFLOW : reg_p & (~CPU_STATUS_FLAG_OVERFLOW);
                    reg_p = temp3 ? reg_p | CPU_STATUS_FLAG_CARRY : reg_p & (~CPU_STATUS_FLAG_CARRY);
                break;

                case OP_CODE_AND:
                    if(reg_p & CPU_STATUS_FLAG_MEMORY)
                    {
                        byte_write = 1;
                        reg_accum.reg_accumA &= src_value;
                    }
                    else
                    {
                        reg_accum.reg_accumC &= src_value;
                    }

                    temp = reg_accum.reg_accumC;
                break;

                case OP_CODE_EOR:
        //            op_code_str = "EOR";
                break;

                case OP_CODE_ORA:
        //            op_code_str = "ORA";
                break;

                case OP_CODE_DEC:
        //            op_code_str = "DEC";
                break;

                case OP_CODE_INC:
        //            op_code_str = "INC";
                break;

                case OP_CODE_DEX:
                    if(reg_p & CPU_STATUS_FLAG_INDEX)
                    {
                        reg_x.reg_xL--;
                        byte_write = 1;
                    }
                    else
                    {
                        reg_x.reg_x--;
                    }

                    temp = reg_x.reg_x;
                break;

                case OP_CODE_INX:
                    if(reg_p & CPU_STATUS_FLAG_INDEX)
                    {
                        reg_x.reg_xL++;
                        byte_write = 1;
                    }
                    else
                    {
                        reg_x.reg_x++;
                    }
                    temp = reg_x.reg_x;
                break;

                case OP_CODE_DEY:
                    if(reg_p & CPU_STATUS_FLAG_INDEX)
                    {
                        reg_y.reg_yL--;
                        byte_write = 1;
                    }
                    else
                    {
                        reg_y.reg_y--;
                    }

                    temp = reg_y.reg_y;
                break;

                case OP_CODE_INY:
                    if(reg_p & CPU_STATUS_FLAG_INDEX)
                    {
                        reg_y.reg_yL++;
                        byte_write = 1;
                    }
                    else
                    {
                        reg_y.reg_y++;
                    }
                    temp = reg_y.reg_y;
                break;
            }

            if(byte_write)
            {
                temp2 = (unsigned char)temp == 0;
                temp = (signed char)temp < 0;
            }
            else
            {
                temp2 = (unsigned short)temp == 0;
                temp = (signed short)temp < 0;
            }

            reg_p = temp2 ? reg_p | CPU_STATUS_FLAG_ZERO : reg_p & (~CPU_STATUS_FLAG_ZERO);
            reg_p = temp ? reg_p | CPU_STATUS_FLAG_NEGATIVE : reg_p & (~CPU_STATUS_FLAG_NEGATIVE);
        break;






        case OP_CODE_CATEGORY_COMPARISON:
            switch(opcode.opcode)
            {
                 case OP_CODE_CMP:
                    byte_write = reg_pc & CPU_STATUS_FLAG_MEMORY;

                    if(byte_write)
                    {
                        temp = (signed char)src_value - (signed char)reg_accum.reg_accumA;
                    }
                    else
                    {
                        temp = (signed short)src_value - (signed short)reg_accum.reg_accumC;
                    }
                break;

                case OP_CODE_CPX:
                    byte_write = reg_pc & CPU_STATUS_FLAG_INDEX;

                    if(byte_write)
                    {
                        temp = (signed char)src_value - (signed char)reg_x.reg_xL;
                    }
                    else
                    {
                        temp = (signed short)src_value - (signed short)reg_x.reg_x;
                    }
                break;

                case OP_CODE_CPY:
                    byte_write = reg_pc & CPU_STATUS_FLAG_INDEX;

                    if(byte_write)
                    {
                        temp = (signed char)src_value - (signed char)reg_y.reg_yL;
                    }
                    else
                    {
                        temp = (signed short)src_value - (signed short)reg_y.reg_y;
                    }
                break;

                case OP_CODE_BIT:
        //            op_code_str = "BIT";
                break;
            }

            if(byte_write)
            {
                temp3 = temp & 0xffffff00;
                temp2 = (signed char)temp == 0;
                temp = (signed char)temp < 0;
            }
            else
            {
                temp3 = temp & 0xffff0000;
                temp2 = (signed short)temp == 0;
                temp = (signed short)temp < 0;
            }

            reg_p = temp3 ? reg_p | CPU_STATUS_FLAG_CARRY : reg_p & (~CPU_STATUS_FLAG_CARRY);
            reg_p = temp2 ? reg_p | CPU_STATUS_FLAG_ZERO : reg_p & (~CPU_STATUS_FLAG_ZERO);
            reg_p = temp ? reg_p | CPU_STATUS_FLAG_NEGATIVE : reg_p & (~CPU_STATUS_FLAG_NEGATIVE);
        break;






        case OP_CODE_CATEGORY_BRANCH:

            byte_write = 1;

            switch(opcode.opcode)
            {
                case OP_CODE_BCC:
                    src_value = reg_p & CPU_STATUS_FLAG_CARRY ? 0 : src_value;
                break;

                case OP_CODE_BCS:
                    src_value = reg_p & CPU_STATUS_FLAG_CARRY ? src_value : 0;
                break;

                case OP_CODE_BEQ:
                    src_value = reg_p & CPU_STATUS_FLAG_ZERO ? src_value : 0;
                break;

                case OP_CODE_BMI:
                    src_value = reg_p & CPU_STATUS_FLAG_NEGATIVE ? src_value : 0;
                break;

                case OP_CODE_BNE:
                    src_value = reg_p & CPU_STATUS_FLAG_ZERO ? 0 : src_value;
                break;

                case OP_CODE_BPL:
                    src_value = reg_p & CPU_STATUS_FLAG_NEGATIVE ? 0 : src_value;
                break;

                case OP_CODE_BRA:

                break;

                case OP_CODE_BRL:
                    byte_write = 0;
                break;

                case OP_CODE_BVC:
                    src_value = reg_p & CPU_STATUS_FLAG_OVERFLOW ? 0 : src_value;
                break;

                case OP_CODE_BVS:
                    src_value = reg_p & CPU_STATUS_FLAG_OVERFLOW ? src_value : 0;
                break;


                case OP_CODE_JMP:
                    reg_pc = src_value;

                    if(opcode.address_mode == ADDRESS_MODE_ABSOLUTE_LONG)
                    {
                        reg_pbr = (src_value >> 16) & 0xff;
                    }

                    src_value = 0;
                break;

                case OP_CODE_JML:
        //            op_code_str = "JML";
                break;

                case OP_CODE_JSL:
        //            op_code_str = "JSL";
                break;

                case OP_CODE_JSR:
        //            op_code_str = "JSR";
                break;

                case OP_CODE_RTI:
        //            op_code_str = "RTI";
                break;

                case OP_CODE_RTL:
        //            op_code_str = "RTL";
                break;

                case OP_CODE_RTS:
        //            op_code_str = "RTS";
                break;
            }

            if(byte_write)
            {
                reg_pc += (signed char)src_value;
            }
            else
            {
                reg_pc += (signed short)src_value;
            }
        break;






        case OP_CODE_CATEGORY_LOAD:
        //case OP_CODE_CATEGORY_STORE:

            byte_write = 0;

            if(opcode.address_mode == ADDRESS_MODE_ABSOLUTE)
            {
                opcode_address = memory_pointer(src_value);
                src_value = *(unsigned short *)opcode_address;
            }

            switch(opcode.opcode)
            {
                case OP_CODE_LDA:
                    opcode_address = (unsigned char *)&reg_accum.reg_accumC;
                    byte_write = reg_p & CPU_STATUS_FLAG_MEMORY;
                break;

                case OP_CODE_LDX:
                    opcode_address = (unsigned char *)&reg_x.reg_x;
                    byte_write = reg_p & CPU_STATUS_FLAG_INDEX;
                break;

                case OP_CODE_LDY:
                    opcode_address = (unsigned char *)&reg_y.reg_y;
                    byte_write = reg_p & CPU_STATUS_FLAG_INDEX;
                break;
            }

            if(byte_write)
            {
                src_value &= 0xff;
                opcode_address[0] = src_value;
                temp2 = (signed char)src_value < 0;
                temp = (signed char)src_value == 0;
            }
            else
            {
                *((unsigned short *)opcode_address) = src_value;
                temp2 = (signed short)src_value < 0;
                temp = (signed short)src_value == 0;
            }

            if(opcode.opcode_category == OP_CODE_CATEGORY_LOAD)
            {
                reg_p = temp ? reg_p | CPU_STATUS_FLAG_ZERO : reg_p & (~CPU_STATUS_FLAG_ZERO);
                reg_p = temp2 ? reg_p | CPU_STATUS_FLAG_NEGATIVE : reg_p & (~CPU_STATUS_FLAG_NEGATIVE);
            }
        break;






        case OP_CODE_CATEGORY_STORE:
            opcode_address = memory_pointer(src_value);
            byte_write = 0;

            switch(opcode.opcode)
            {
                case OP_CODE_STA:
                    src_value = reg_accum.reg_accumC;
                    byte_write = reg_p & CPU_STATUS_FLAG_MEMORY;
                break;

                case OP_CODE_STX:
                    src_value = reg_x.reg_x;
                    byte_write = reg_p & CPU_STATUS_FLAG_INDEX;
                break;

                case OP_CODE_STY:
                    src_value = reg_y.reg_y;
                    byte_write = reg_p & CPU_STATUS_FLAG_MEMORY;
                break;

                case OP_CODE_STZ:
                    src_value = 0;
                break;
            }

            if(byte_write)
            {
                opcode_address[0] = src_value;
            }
            else
            {
                *((unsigned short *)opcode_address) = src_value;
            }
        break;







        case OP_CODE_CATEGORY_STACK:

            src_address = NULL;
            dst_address = NULL;
            byte_write = 0;

            switch(opcode.opcode)
            {
                case OP_CODE_PEA:
                    src_address = (unsigned char *)&src_value;
                break;

                case OP_CODE_PEI:
                    src_address = (unsigned char *)&src_value;
                break;

                case OP_CODE_PER:
        //            op_code_str = "PER";
                break;


                case OP_CODE_PHA:
                    src_address = (unsigned char *)&reg_accum.reg_accumC;
                break;

                case OP_CODE_PLA:
                    dst_address = (unsigned char *)&reg_accum.reg_accumC;
                break;


                case OP_CODE_PHB:
                    byte_write = 1;
                    src_address = &reg_dbr;
                break;

                case OP_CODE_PLB:
                    byte_write = 1;
                    dst_address = &reg_dbr;
                break;


                case OP_CODE_PHD:
                    src_address = (unsigned char *)&reg_d;
                break;

                case OP_CODE_PLD:
                    dst_address = (unsigned char *)&reg_d;
                break;


                case OP_CODE_PHK:
                    src_address = &reg_pbr;
                    byte_write = 1;
                break;


                case OP_CODE_PHP:
                    src_address = &reg_p;
                break;

                case OP_CODE_PLP:
                    dst_address = &reg_p;
                break;


                case OP_CODE_PHX:
                    src_address = (unsigned char *)&reg_x.reg_x;
                break;

                case OP_CODE_PLX:
                    dst_address = (unsigned char *)&reg_x.reg_x;
                break;


                case OP_CODE_PHY:
                    src_address = (unsigned char *)&reg_y.reg_y;
                break;

                case OP_CODE_PLY:
                    dst_address = (unsigned char *)&reg_y.reg_y;
                break;
            }

            if(src_address)
            {
                /* we're pushing... */
                reg_s -= 2;
                dst_address = memory_pointer(reg_s);
            }
            else
            {
                /* poping... */
                src_address = memory_pointer(reg_s);
                reg_s += 2;
            }

            if(byte_write)
            {
                dst_address[0] = src_address[0];
            }
            else
            {
                *(unsigned short *)dst_address = *(unsigned short *)src_address;
            }
        break;






        case OP_CODE_CATEGORY_STANDALONE:
            switch(opcode.opcode)
            {
                case OP_CODE_REP:
                    reg_p &= ~((unsigned char)src_value);
                break;

                case OP_CODE_CLC:
                    reg_p &= ~CPU_STATUS_FLAG_CARRY;
                break;

                case OP_CODE_CLD:
                    reg_p &= ~CPU_STATUS_FLAG_DECIMAL;
                break;

                case OP_CODE_CLV:
                    reg_p &= ~CPU_STATUS_FLAG_OVERFLOW;
                break;

                case OP_CODE_CLI:
                    reg_p &= ~CPU_STATUS_FLAG_IRQ_DISABLE;
                break;




                case OP_CODE_SEP:
                    reg_p |= src_value;
                break;

                case OP_CODE_SEC:
                    reg_p |= CPU_STATUS_FLAG_CARRY;
                break;

                case OP_CODE_SED:
                    reg_p |= CPU_STATUS_FLAG_DECIMAL;
                break;

                case OP_CODE_SEI:
                    reg_p |= CPU_STATUS_FLAG_IRQ_DISABLE;
                break;

                case OP_CODE_WAI:
        //            op_code_str = "WAI";
                break;

                case OP_CODE_WDM:
        //            op_code_str = "WDM";
                break;

                case OP_CODE_XBA:
                    reg_accum.reg_accumC = ((reg_accum.reg_accumC >> 8) & 0xff) | ((reg_accum.reg_accumC << 8) & 0xff00);
                break;

                case OP_CODE_NOP:
            //            op_code_str = "NOP";
                break;

                case OP_CODE_XCE:
                    temp = reg_p & CPU_STATUS_FLAG_CARRY;
                    reg_p = (reg_p & (~CPU_STATUS_FLAG_CARRY)) | reg_e;
                    reg_e = temp;
                break;

                case OP_CODE_BRK:

                break;

                case OP_CODE_MVN:
        //            op_code_str = "MVN";
                break;

                case OP_CODE_MVP:
        //            op_code_str = "MVP";
                break;

                case OP_CODE_TRB:
        //            op_code_str = "TRB";
                break;

                case OP_CODE_TSB:
        //            op_code_str = "TSB";
                break;

                case OP_CODE_STP:
        //            op_code_str = "STP";
                break;

                case OP_CODE_COP:
        //            op_code_str = "COP";
                break;

                case OP_CODE_UNKNOWN:

                break;
            }
        break;






        case OP_CODE_CATEGORY_TRANSFER:
            byte_write = 0;

            switch(opcode.opcode)
            {
                case OP_CODE_TAX:
                    src_address = (unsigned char *)&reg_accum.reg_accumC;
                    dst_address = (unsigned char *)&reg_x.reg_x;
                    byte_write = reg_p & CPU_STATUS_FLAG_INDEX;
                break;

                case OP_CODE_TAY:
                    src_address = (unsigned char *)&reg_accum.reg_accumC;
                    dst_address = (unsigned char *)&reg_y.reg_y;
                    byte_write = reg_p & CPU_STATUS_FLAG_INDEX;
                break;

                case OP_CODE_TCD:
                    src_address = (unsigned char *)&reg_accum.reg_accumC;
                    dst_address = (unsigned char *)&reg_d;
                break;

                case OP_CODE_TCS:
                    src_address = (unsigned char *)&reg_accum.reg_accumC;
                    dst_address = (unsigned char *)&reg_s;
                break;

                case OP_CODE_TDC:
                    dst_address = (unsigned char *)&reg_accum.reg_accumC;
                    src_address = (unsigned char *)&reg_d;
                    byte_write = reg_p & CPU_STATUS_FLAG_MEMORY;
                break;

                case OP_CODE_TSC:
                    dst_address = (unsigned char *)&reg_accum.reg_accumC;
                    src_address = (unsigned char *)&reg_s;
                    byte_write = reg_p & CPU_STATUS_FLAG_MEMORY;
                break;

                case OP_CODE_TSX:
                    src_address = (unsigned char *)&reg_s;
                    dst_address = (unsigned char *)&reg_x.reg_x;
                break;

                case OP_CODE_TXA:
                    src_address = (unsigned char *)&reg_x.reg_x;
                    dst_address = (unsigned char *)&reg_accum.reg_accumC;
                    byte_write = reg_p & CPU_STATUS_FLAG_MEMORY;
                break;

                case OP_CODE_TXS:
                    src_address = (unsigned char *)&reg_x.reg_x;
                    dst_address = (unsigned char *)&reg_s;
                break;

                case OP_CODE_TXY:
                    src_address = (unsigned char *)&reg_x.reg_x;
                    dst_address = (unsigned char *)&reg_y.reg_y;
                    byte_write = reg_p & CPU_STATUS_FLAG_INDEX;
                break;

                case OP_CODE_TYA:
                    src_address = (unsigned char *)&reg_y.reg_y;
                    dst_address = (unsigned char *)&reg_accum.reg_accumC;
                    byte_write = reg_p & CPU_STATUS_FLAG_MEMORY;
                break;

                case OP_CODE_TYX:
                    src_address = (unsigned char *)&reg_y.reg_y;
                    dst_address = (unsigned char *)&reg_x.reg_x;
                    byte_write = reg_p & CPU_STATUS_FLAG_INDEX;
                break;
            }

            if(byte_write)
            {
                *dst_address = *src_address;
            }
            else
            {
                *(unsigned short *)dst_address = *(unsigned short *)src_address;
            }

        break;
    }

}















