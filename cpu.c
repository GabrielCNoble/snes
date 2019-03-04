#include "cpu.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


unsigned char *cpu_ram = NULL;




unsigned char reg_e;         /* emulation flag */

unsigned short reg_ir;      /* instruction register */
unsigned short reg_tcu;     /* timing control unit */


unsigned short reg_accum;   /* accumulator register */
unsigned short reg_d;       /* direct register */
unsigned short reg_s;       /* stack pointer register */
unsigned short reg_pc;       /* program counter register */

unsigned short reg_x;       /* indexing register x */
unsigned short reg_y;       /* indexing register y */

unsigned char reg_dbr;      /* data bank register */
unsigned char reg_pbr;      /* program bank register */
unsigned char reg_p;        /* status register */



unsigned char address_mode_operand_size[ADDRESS_MODE_UNKNOWN];
struct op_code_t opcode_matrix[256];



void reset_cpu()
{
    int i;

    if(!cpu_ram)
    {
        cpu_ram = calloc(1, CPU_RAM_SIZE);

        for(i = 0; i < 256; i++)
        {
            opcode_matrix[i] = OPCODE(OP_CODE_UNKNOWN, ADDRESS_MODE_UNKNOWN);
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



        opcode_matrix[0x6d] = OPCODE(OP_CODE_ADC, ADDRESS_MODE_ABSOLUTE);
        opcode_matrix[0x7d] = OPCODE(OP_CODE_ADC, ADDRESS_MODE_ABSOLUTE_INDEXED_X);
        opcode_matrix[0x79] = OPCODE(OP_CODE_ADC, ADDRESS_MODE_ABSOLUTE_INDEXED_Y);
        opcode_matrix[0x6f] = OPCODE(OP_CODE_ADC, ADDRESS_MODE_ABSOLUTE_LONG);
        opcode_matrix[0x7f] = OPCODE(OP_CODE_ADC, ADDRESS_MODE_ABSOLUTE_LONG_INDEXED_X);
        opcode_matrix[0x17] = OPCODE(OP_CODE_ADC, ADDRESS_MODE_ABSOLUTE_INDIRECT);
        opcode_matrix[0x65] = OPCODE(OP_CODE_ADC, ADDRESS_MODE_DIRECT);
        opcode_matrix[0x63] = OPCODE(OP_CODE_ADC, ADDRESS_MODE_STACK_RELATIVE);
        opcode_matrix[0x75] = OPCODE(OP_CODE_ADC, ADDRESS_MODE_DIRECT_INDEXED_X);
        opcode_matrix[0x72] = OPCODE(OP_CODE_ADC, ADDRESS_MODE_DIRECT_INDIRECT);
        opcode_matrix[0x67] = OPCODE(OP_CODE_ADC, ADDRESS_MODE_DIRECT_INDIRECT_LONG);
        opcode_matrix[0x73] = OPCODE(OP_CODE_ADC, ADDRESS_MODE_STACK_RELATIVE_INDIRECT_INDEXED);
        opcode_matrix[0x61] = OPCODE(OP_CODE_ADC, ADDRESS_MODE_DIRECT_INDEXED_INDIRECT);
        opcode_matrix[0x71] = OPCODE(OP_CODE_ADC, ADDRESS_MODE_DIRECT_INDIRECT_INDEXED);
        opcode_matrix[0x77] = OPCODE(OP_CODE_ADC, ADDRESS_MODE_DIRECT_INDIRECT_LONG_INDEXED);
        opcode_matrix[0x69] = OPCODE(OP_CODE_ADC, ADDRESS_MODE_IMMEDIATE);



        opcode_matrix[0x2d] = OPCODE(OP_CODE_AND, ADDRESS_MODE_ABSOLUTE);
        opcode_matrix[0x3e] = OPCODE(OP_CODE_AND, ADDRESS_MODE_ABSOLUTE_INDEXED_X);
        opcode_matrix[0x39] = OPCODE(OP_CODE_AND, ADDRESS_MODE_ABSOLUTE_INDEXED_Y);
        opcode_matrix[0x2f] = OPCODE(OP_CODE_AND, ADDRESS_MODE_ABSOLUTE_LONG);
        opcode_matrix[0x3f] = OPCODE(OP_CODE_AND, ADDRESS_MODE_ABSOLUTE_LONG_INDEXED_X);
        opcode_matrix[0x25] = OPCODE(OP_CODE_AND, ADDRESS_MODE_DIRECT);
        opcode_matrix[0x23] = OPCODE(OP_CODE_AND, ADDRESS_MODE_STACK_RELATIVE);
        opcode_matrix[0x36] = OPCODE(OP_CODE_AND, ADDRESS_MODE_DIRECT_INDEXED_X);
        opcode_matrix[0x32] = OPCODE(OP_CODE_AND, ADDRESS_MODE_DIRECT_INDIRECT);
        opcode_matrix[0x27] = OPCODE(OP_CODE_AND, ADDRESS_MODE_DIRECT_INDIRECT_LONG);
        opcode_matrix[0x33] = OPCODE(OP_CODE_AND, ADDRESS_MODE_STACK_RELATIVE_INDIRECT_INDEXED);
        opcode_matrix[0x91] = OPCODE(OP_CODE_AND, ADDRESS_MODE_DIRECT_INDEXED_INDIRECT);
        opcode_matrix[0x31] = OPCODE(OP_CODE_AND, ADDRESS_MODE_DIRECT_INDIRECT_INDEXED);
        opcode_matrix[0x37] = OPCODE(OP_CODE_AND, ADDRESS_MODE_DIRECT_INDIRECT_LONG_INDEXED);
        opcode_matrix[0x29] = OPCODE(OP_CODE_AND, ADDRESS_MODE_IMMEDIATE);


        opcode_matrix[0x0e] = OPCODE(OP_CODE_ASL, ADDRESS_MODE_ABSOLUTE);
        opcode_matrix[0x0a] = OPCODE(OP_CODE_ASL, ADDRESS_MODE_ACCUMULATOR);
        opcode_matrix[0x1e] = OPCODE(OP_CODE_ASL, ADDRESS_MODE_ABSOLUTE_INDEXED_X);
        opcode_matrix[0x06] = OPCODE(OP_CODE_ASL, ADDRESS_MODE_DIRECT);
        opcode_matrix[0x16] = OPCODE(OP_CODE_ASL, ADDRESS_MODE_DIRECT_INDEXED_X);


        opcode_matrix[0x90] = OPCODE(OP_CODE_BCC, ADDRESS_MODE_PROGRAM_COUNTER_RELATIVE);
        opcode_matrix[0xb0] = OPCODE(OP_CODE_BCS, ADDRESS_MODE_PROGRAM_COUNTER_RELATIVE);
        opcode_matrix[0xf0] = OPCODE(OP_CODE_BEQ, ADDRESS_MODE_PROGRAM_COUNTER_RELATIVE);


        opcode_matrix[0x2c] = OPCODE(OP_CODE_BIT, ADDRESS_MODE_ABSOLUTE);
        opcode_matrix[0x3c] = OPCODE(OP_CODE_BIT, ADDRESS_MODE_ABSOLUTE_INDEXED_X);
        opcode_matrix[0x24] = OPCODE(OP_CODE_BIT, ADDRESS_MODE_DIRECT);
        opcode_matrix[0x34] = OPCODE(OP_CODE_BIT, ADDRESS_MODE_DIRECT_INDEXED_X);
        opcode_matrix[0x89] = OPCODE(OP_CODE_BIT, ADDRESS_MODE_IMMEDIATE);


        opcode_matrix[0x30] = OPCODE(OP_CODE_BMI, ADDRESS_MODE_PROGRAM_COUNTER_RELATIVE);
        opcode_matrix[0xd0] = OPCODE(OP_CODE_BNE, ADDRESS_MODE_PROGRAM_COUNTER_RELATIVE);
        opcode_matrix[0x10] = OPCODE(OP_CODE_BPL, ADDRESS_MODE_PROGRAM_COUNTER_RELATIVE);
        opcode_matrix[0x80] = OPCODE(OP_CODE_BRA, ADDRESS_MODE_PROGRAM_COUNTER_RELATIVE);


        opcode_matrix[0x00] = OPCODE(OP_CODE_BRK, ADDRESS_MODE_STACK);


        opcode_matrix[0x82] = OPCODE(OP_CODE_BRL, ADDRESS_MODE_PROGRAM_COUNTER_RELATIVE_LONG);
        opcode_matrix[0x50] = OPCODE(OP_CODE_BVC, ADDRESS_MODE_PROGRAM_COUNTER_RELATIVE);
        opcode_matrix[0x70] = OPCODE(OP_CODE_BVS, ADDRESS_MODE_PROGRAM_COUNTER_RELATIVE);


        opcode_matrix[0x18] = OPCODE(OP_CODE_CLC, ADDRESS_MODE_IMPLIED);
        opcode_matrix[0xd8] = OPCODE(OP_CODE_CLD, ADDRESS_MODE_IMPLIED);
        opcode_matrix[0x58] = OPCODE(OP_CODE_CLI, ADDRESS_MODE_IMPLIED);
        opcode_matrix[0xb8] = OPCODE(OP_CODE_CLV, ADDRESS_MODE_IMPLIED);


        opcode_matrix[0xcd] = OPCODE(OP_CODE_CMP, ADDRESS_MODE_ABSOLUTE);
        opcode_matrix[0xdd] = OPCODE(OP_CODE_CMP, ADDRESS_MODE_ABSOLUTE_INDEXED_X);
        opcode_matrix[0xd9] = OPCODE(OP_CODE_CMP, ADDRESS_MODE_ABSOLUTE_INDEXED_Y);
        opcode_matrix[0xcf] = OPCODE(OP_CODE_CMP, ADDRESS_MODE_ABSOLUTE_LONG);
        opcode_matrix[0xdf] = OPCODE(OP_CODE_CMP, ADDRESS_MODE_ABSOLUTE_LONG_INDEXED_X);
        opcode_matrix[0xc5] = OPCODE(OP_CODE_CMP, ADDRESS_MODE_DIRECT);
        opcode_matrix[0xc3] = OPCODE(OP_CODE_CMP, ADDRESS_MODE_STACK_RELATIVE);
        opcode_matrix[0xd5] = OPCODE(OP_CODE_CMP, ADDRESS_MODE_DIRECT_INDEXED_X);
        opcode_matrix[0xd2] = OPCODE(OP_CODE_CMP, ADDRESS_MODE_DIRECT_INDIRECT);
        opcode_matrix[0xc7] = OPCODE(OP_CODE_CMP, ADDRESS_MODE_DIRECT_INDIRECT_LONG);
        opcode_matrix[0xd3] = OPCODE(OP_CODE_CMP, ADDRESS_MODE_STACK_RELATIVE_INDIRECT_INDEXED);
        opcode_matrix[0xc1] = OPCODE(OP_CODE_CMP, ADDRESS_MODE_DIRECT_INDEXED_INDIRECT);
        opcode_matrix[0xd1] = OPCODE(OP_CODE_CMP, ADDRESS_MODE_DIRECT_INDIRECT_INDEXED);
        opcode_matrix[0xd7] = OPCODE(OP_CODE_CMP, ADDRESS_MODE_DIRECT_INDIRECT_LONG_INDEXED);
        opcode_matrix[0xc9] = OPCODE(OP_CODE_CMP, ADDRESS_MODE_IMMEDIATE);


        opcode_matrix[0x02] = OPCODE(OP_CODE_COP, ADDRESS_MODE_STACK);


        opcode_matrix[0xec] = OPCODE(OP_CODE_CPX, ADDRESS_MODE_ABSOLUTE);
        opcode_matrix[0xe4] = OPCODE(OP_CODE_CPX, ADDRESS_MODE_DIRECT);
        opcode_matrix[0xe0] = OPCODE(OP_CODE_CPX, ADDRESS_MODE_IMMEDIATE);


        opcode_matrix[0xcc] = OPCODE(OP_CODE_CPY, ADDRESS_MODE_ABSOLUTE);
        opcode_matrix[0xc4] = OPCODE(OP_CODE_CPY, ADDRESS_MODE_DIRECT);
        opcode_matrix[0xc0] = OPCODE(OP_CODE_CPY, ADDRESS_MODE_IMMEDIATE);


        opcode_matrix[0xce] = OPCODE(OP_CODE_DEC, ADDRESS_MODE_ABSOLUTE);
        opcode_matrix[0x3a] = OPCODE(OP_CODE_DEC, ADDRESS_MODE_ACCUMULATOR);
        opcode_matrix[0xde] = OPCODE(OP_CODE_DEC, ADDRESS_MODE_ABSOLUTE_INDEXED_X);
        opcode_matrix[0xc6] = OPCODE(OP_CODE_DEC, ADDRESS_MODE_DIRECT);
        opcode_matrix[0xd6] = OPCODE(OP_CODE_DEC, ADDRESS_MODE_DIRECT_INDEXED_X);


        opcode_matrix[0xca] = OPCODE(OP_CODE_DEX, ADDRESS_MODE_IMPLIED);
        opcode_matrix[0x88] = OPCODE(OP_CODE_DEY, ADDRESS_MODE_IMPLIED);


        opcode_matrix[0x4d] = OPCODE(OP_CODE_EOR, ADDRESS_MODE_ABSOLUTE);
        opcode_matrix[0x5d] = OPCODE(OP_CODE_EOR, ADDRESS_MODE_ABSOLUTE_INDEXED_X);
        opcode_matrix[0x59] = OPCODE(OP_CODE_EOR, ADDRESS_MODE_ABSOLUTE_INDEXED_Y);
        opcode_matrix[0x4f] = OPCODE(OP_CODE_EOR, ADDRESS_MODE_ABSOLUTE_LONG);
        opcode_matrix[0x5f] = OPCODE(OP_CODE_EOR, ADDRESS_MODE_ABSOLUTE_LONG_INDEXED_X);
        opcode_matrix[0x5d] = OPCODE(OP_CODE_EOR, ADDRESS_MODE_ABSOLUTE_INDEXED_INDIRECT);
        opcode_matrix[0x45] = OPCODE(OP_CODE_EOR, ADDRESS_MODE_DIRECT);
        opcode_matrix[0x43] = OPCODE(OP_CODE_EOR, ADDRESS_MODE_STACK_RELATIVE);
        opcode_matrix[0x55] = OPCODE(OP_CODE_EOR, ADDRESS_MODE_DIRECT_INDEXED_X);
        opcode_matrix[0x52] = OPCODE(OP_CODE_EOR, ADDRESS_MODE_DIRECT_INDIRECT);
        opcode_matrix[0x47] = OPCODE(OP_CODE_EOR, ADDRESS_MODE_DIRECT_INDIRECT_LONG);
        opcode_matrix[0x53] = OPCODE(OP_CODE_EOR, ADDRESS_MODE_STACK_RELATIVE_INDIRECT_INDEXED);
        opcode_matrix[0x41] = OPCODE(OP_CODE_EOR, ADDRESS_MODE_DIRECT_INDEXED_INDIRECT);
        opcode_matrix[0x51] = OPCODE(OP_CODE_EOR, ADDRESS_MODE_DIRECT_INDIRECT_INDEXED);
        opcode_matrix[0x57] = OPCODE(OP_CODE_EOR, ADDRESS_MODE_DIRECT_INDIRECT_LONG_INDEXED);
        opcode_matrix[0x49] = OPCODE(OP_CODE_EOR, ADDRESS_MODE_IMMEDIATE);


        opcode_matrix[0xee] = OPCODE(OP_CODE_INC, ADDRESS_MODE_ABSOLUTE);
        opcode_matrix[0x1a] = OPCODE(OP_CODE_INC, ADDRESS_MODE_ACCUMULATOR);
        opcode_matrix[0xfe] = OPCODE(OP_CODE_INC, ADDRESS_MODE_ABSOLUTE_INDEXED_X);
        opcode_matrix[0xe6] = OPCODE(OP_CODE_INC, ADDRESS_MODE_DIRECT);
        opcode_matrix[0xf6] = OPCODE(OP_CODE_INC, ADDRESS_MODE_DIRECT_INDEXED_X);


        opcode_matrix[0xe8] = OPCODE(OP_CODE_INX, ADDRESS_MODE_IMPLIED);
        opcode_matrix[0xc8] = OPCODE(OP_CODE_INY, ADDRESS_MODE_IMPLIED);


        opcode_matrix[0xdc] = OPCODE(OP_CODE_JML, ADDRESS_MODE_ABSOLUTE_INDIRECT);


        opcode_matrix[0x4c] = OPCODE(OP_CODE_JMP, ADDRESS_MODE_ABSOLUTE);
        opcode_matrix[0x5c] = OPCODE(OP_CODE_JMP, ADDRESS_MODE_ABSOLUTE_LONG);
        opcode_matrix[0x6c] = OPCODE(OP_CODE_JMP, ADDRESS_MODE_ABSOLUTE_INDIRECT);
        opcode_matrix[0x7c] = OPCODE(OP_CODE_JMP, ADDRESS_MODE_ABSOLUTE_INDEXED_INDIRECT);


        opcode_matrix[0x22] = OPCODE(OP_CODE_JSL, ADDRESS_MODE_ABSOLUTE_LONG);


        opcode_matrix[0x20] = OPCODE(OP_CODE_JSR, ADDRESS_MODE_ABSOLUTE);
        opcode_matrix[0xfc] = OPCODE(OP_CODE_JSR, ADDRESS_MODE_ABSOLUTE_INDEXED_INDIRECT);


        opcode_matrix[0xad] = OPCODE(OP_CODE_LDA, ADDRESS_MODE_ABSOLUTE);
        opcode_matrix[0xbd] = OPCODE(OP_CODE_LDA, ADDRESS_MODE_ABSOLUTE_INDEXED_X);
        opcode_matrix[0xb9] = OPCODE(OP_CODE_LDA, ADDRESS_MODE_ABSOLUTE_INDEXED_Y);
        opcode_matrix[0xaf] = OPCODE(OP_CODE_LDA, ADDRESS_MODE_ABSOLUTE_LONG);
        opcode_matrix[0xbf] = OPCODE(OP_CODE_LDA, ADDRESS_MODE_ABSOLUTE_LONG_INDEXED_X);
        opcode_matrix[0xa5] = OPCODE(OP_CODE_LDA, ADDRESS_MODE_DIRECT);
        opcode_matrix[0xa3] = OPCODE(OP_CODE_LDA, ADDRESS_MODE_STACK_RELATIVE);
        opcode_matrix[0xb5] = OPCODE(OP_CODE_LDA, ADDRESS_MODE_DIRECT_INDEXED_X);
        opcode_matrix[0xb2] = OPCODE(OP_CODE_LDA, ADDRESS_MODE_DIRECT_INDIRECT);
        opcode_matrix[0xa7] = OPCODE(OP_CODE_LDA, ADDRESS_MODE_DIRECT_INDIRECT_LONG);
        opcode_matrix[0xb3] = OPCODE(OP_CODE_LDA, ADDRESS_MODE_STACK_RELATIVE_INDIRECT_INDEXED);
        opcode_matrix[0xa1] = OPCODE(OP_CODE_LDA, ADDRESS_MODE_DIRECT_INDEXED_INDIRECT);
        opcode_matrix[0xb1] = OPCODE(OP_CODE_LDA, ADDRESS_MODE_DIRECT_INDIRECT_INDEXED);
        opcode_matrix[0xb7] = OPCODE(OP_CODE_LDA, ADDRESS_MODE_DIRECT_INDIRECT_LONG_INDEXED);
        opcode_matrix[0xa9] = OPCODE(OP_CODE_LDA, ADDRESS_MODE_IMMEDIATE);


        opcode_matrix[0xae] = OPCODE(OP_CODE_LDX, ADDRESS_MODE_ABSOLUTE);
        opcode_matrix[0xbe] = OPCODE(OP_CODE_LDX, ADDRESS_MODE_ABSOLUTE_INDEXED_Y);
        opcode_matrix[0xa6] = OPCODE(OP_CODE_LDX, ADDRESS_MODE_DIRECT);
        opcode_matrix[0xb6] = OPCODE(OP_CODE_LDX, ADDRESS_MODE_DIRECT_INDEXED_Y);
        opcode_matrix[0xa2] = OPCODE(OP_CODE_LDX, ADDRESS_MODE_IMMEDIATE);


        opcode_matrix[0xac] = OPCODE(OP_CODE_LDY, ADDRESS_MODE_ABSOLUTE);
        opcode_matrix[0xbc] = OPCODE(OP_CODE_LDY, ADDRESS_MODE_ABSOLUTE_INDEXED_Y);
        opcode_matrix[0xa4] = OPCODE(OP_CODE_LDY, ADDRESS_MODE_DIRECT);
        opcode_matrix[0xb4] = OPCODE(OP_CODE_LDY, ADDRESS_MODE_DIRECT_INDEXED_X);
        opcode_matrix[0xa0] = OPCODE(OP_CODE_LDY, ADDRESS_MODE_IMMEDIATE);


        opcode_matrix[0x4e] = OPCODE(OP_CODE_LSR, ADDRESS_MODE_ABSOLUTE);
        opcode_matrix[0x4a] = OPCODE(OP_CODE_LSR, ADDRESS_MODE_ACCUMULATOR);
        opcode_matrix[0x5e] = OPCODE(OP_CODE_LSR, ADDRESS_MODE_ABSOLUTE_INDEXED_X);
        opcode_matrix[0x46] = OPCODE(OP_CODE_LSR, ADDRESS_MODE_DIRECT);
        opcode_matrix[0x56] = OPCODE(OP_CODE_LSR, ADDRESS_MODE_DIRECT_INDEXED_X);


        opcode_matrix[0x54] = OPCODE(OP_CODE_MVN, ADDRESS_MODE_BLOCK_MOVE);
        opcode_matrix[0x44] = OPCODE(OP_CODE_MVP, ADDRESS_MODE_BLOCK_MOVE);


        opcode_matrix[0xea] = OPCODE(OP_CODE_NOP, ADDRESS_MODE_IMPLIED);


        opcode_matrix[0x0d] = OPCODE(OP_CODE_ORA, ADDRESS_MODE_ABSOLUTE);
        opcode_matrix[0x1d] = OPCODE(OP_CODE_ORA, ADDRESS_MODE_ABSOLUTE_INDEXED_X);
        opcode_matrix[0x19] = OPCODE(OP_CODE_ORA, ADDRESS_MODE_ABSOLUTE_INDEXED_Y);
        opcode_matrix[0x0f] = OPCODE(OP_CODE_ORA, ADDRESS_MODE_ABSOLUTE_LONG);
        opcode_matrix[0x1f] = OPCODE(OP_CODE_ORA, ADDRESS_MODE_ABSOLUTE_LONG_INDEXED_X);
        opcode_matrix[0x05] = OPCODE(OP_CODE_ORA, ADDRESS_MODE_DIRECT);
        opcode_matrix[0x03] = OPCODE(OP_CODE_ORA, ADDRESS_MODE_STACK_RELATIVE);
        opcode_matrix[0x15] = OPCODE(OP_CODE_ORA, ADDRESS_MODE_DIRECT_INDEXED_X);
        opcode_matrix[0x12] = OPCODE(OP_CODE_ORA, ADDRESS_MODE_DIRECT_INDIRECT);
        opcode_matrix[0x07] = OPCODE(OP_CODE_ORA, ADDRESS_MODE_DIRECT_INDIRECT_LONG);
        opcode_matrix[0x13] = OPCODE(OP_CODE_ORA, ADDRESS_MODE_STACK_RELATIVE_INDIRECT_INDEXED);
        opcode_matrix[0x01] = OPCODE(OP_CODE_ORA, ADDRESS_MODE_DIRECT_INDEXED_INDIRECT);
        opcode_matrix[0x11] = OPCODE(OP_CODE_ORA, ADDRESS_MODE_DIRECT_INDIRECT_INDEXED);
        opcode_matrix[0x17] = OPCODE(OP_CODE_ORA, ADDRESS_MODE_DIRECT_INDIRECT_LONG_INDEXED);
        opcode_matrix[0x09] = OPCODE(OP_CODE_ORA, ADDRESS_MODE_IMMEDIATE);


        opcode_matrix[0xf4] = OPCODE(OP_CODE_PEA, ADDRESS_MODE_STACK);
        opcode_matrix[0xd4] = OPCODE(OP_CODE_PEI, ADDRESS_MODE_STACK);
        opcode_matrix[0x62] = OPCODE(OP_CODE_PER, ADDRESS_MODE_STACK);
        opcode_matrix[0x48] = OPCODE(OP_CODE_PHA, ADDRESS_MODE_STACK);
        opcode_matrix[0x8b] = OPCODE(OP_CODE_PHB, ADDRESS_MODE_STACK);
        opcode_matrix[0x0b] = OPCODE(OP_CODE_PHD, ADDRESS_MODE_STACK);
        opcode_matrix[0x4b] = OPCODE(OP_CODE_PHK, ADDRESS_MODE_STACK);
        opcode_matrix[0x08] = OPCODE(OP_CODE_PHP, ADDRESS_MODE_STACK);
        opcode_matrix[0xda] = OPCODE(OP_CODE_PHX, ADDRESS_MODE_STACK);
        opcode_matrix[0x5a] = OPCODE(OP_CODE_PHY, ADDRESS_MODE_STACK);
        opcode_matrix[0x68] = OPCODE(OP_CODE_PLA, ADDRESS_MODE_STACK);
        opcode_matrix[0xab] = OPCODE(OP_CODE_PLB, ADDRESS_MODE_STACK);
        opcode_matrix[0x2b] = OPCODE(OP_CODE_PLD, ADDRESS_MODE_STACK);
        opcode_matrix[0x28] = OPCODE(OP_CODE_PLP, ADDRESS_MODE_STACK);
        opcode_matrix[0xfa] = OPCODE(OP_CODE_PLX, ADDRESS_MODE_STACK);
        opcode_matrix[0x7a] = OPCODE(OP_CODE_PLY, ADDRESS_MODE_STACK);


        opcode_matrix[0xc2] = OPCODE(OP_CODE_REP, ADDRESS_MODE_IMMEDIATE);


        opcode_matrix[0x2e] = OPCODE(OP_CODE_ROL, ADDRESS_MODE_ABSOLUTE);
        opcode_matrix[0x2a] = OPCODE(OP_CODE_ROL, ADDRESS_MODE_ACCUMULATOR);
        opcode_matrix[0x3e] = OPCODE(OP_CODE_ROL, ADDRESS_MODE_ABSOLUTE_INDEXED_X);
        opcode_matrix[0x26] = OPCODE(OP_CODE_ROL, ADDRESS_MODE_DIRECT);
        opcode_matrix[0x36] = OPCODE(OP_CODE_ROL, ADDRESS_MODE_DIRECT_INDEXED_X);


        opcode_matrix[0x6e] = OPCODE(OP_CODE_ROR, ADDRESS_MODE_ABSOLUTE);
        opcode_matrix[0x6a] = OPCODE(OP_CODE_ROR, ADDRESS_MODE_ACCUMULATOR);
        opcode_matrix[0x7e] = OPCODE(OP_CODE_ROR, ADDRESS_MODE_ABSOLUTE_INDEXED_X);
        opcode_matrix[0x66] = OPCODE(OP_CODE_ROR, ADDRESS_MODE_DIRECT);
        opcode_matrix[0x76] = OPCODE(OP_CODE_ROR, ADDRESS_MODE_DIRECT_INDEXED_X);


        opcode_matrix[0x40] = OPCODE(OP_CODE_RTI, ADDRESS_MODE_STACK);
        opcode_matrix[0x6b] = OPCODE(OP_CODE_RTL, ADDRESS_MODE_STACK);
        opcode_matrix[0x60] = OPCODE(OP_CODE_RTS, ADDRESS_MODE_STACK);


        opcode_matrix[0xed] = OPCODE(OP_CODE_SBC, ADDRESS_MODE_ABSOLUTE);
        opcode_matrix[0xfd] = OPCODE(OP_CODE_SBC, ADDRESS_MODE_ABSOLUTE_INDEXED_X);
        opcode_matrix[0xf9] = OPCODE(OP_CODE_SBC, ADDRESS_MODE_ABSOLUTE_INDEXED_Y);
        opcode_matrix[0xff] = OPCODE(OP_CODE_SBC, ADDRESS_MODE_ABSOLUTE_LONG_INDEXED_X);
        opcode_matrix[0xe5] = OPCODE(OP_CODE_SBC, ADDRESS_MODE_DIRECT);
        opcode_matrix[0xe3] = OPCODE(OP_CODE_SBC, ADDRESS_MODE_STACK_RELATIVE);
        opcode_matrix[0xf5] = OPCODE(OP_CODE_SBC, ADDRESS_MODE_DIRECT_INDEXED_X);
        opcode_matrix[0xf2] = OPCODE(OP_CODE_SBC, ADDRESS_MODE_DIRECT_INDIRECT);
        opcode_matrix[0xe7] = OPCODE(OP_CODE_SBC, ADDRESS_MODE_DIRECT_INDIRECT_LONG);
        opcode_matrix[0xf3] = OPCODE(OP_CODE_SBC, ADDRESS_MODE_STACK_RELATIVE_INDIRECT_INDEXED);
        opcode_matrix[0xe1] = OPCODE(OP_CODE_SBC, ADDRESS_MODE_DIRECT_INDEXED_INDIRECT);
        opcode_matrix[0xf1] = OPCODE(OP_CODE_SBC, ADDRESS_MODE_DIRECT_INDIRECT_INDEXED);
        opcode_matrix[0xf7] = OPCODE(OP_CODE_SBC, ADDRESS_MODE_DIRECT_INDIRECT_LONG_INDEXED);
        opcode_matrix[0xe9] = OPCODE(OP_CODE_SBC, ADDRESS_MODE_IMMEDIATE);


        opcode_matrix[0x38] = OPCODE(OP_CODE_SEC, ADDRESS_MODE_IMPLIED);
        opcode_matrix[0xf8] = OPCODE(OP_CODE_SED, ADDRESS_MODE_IMPLIED);
        opcode_matrix[0x78] = OPCODE(OP_CODE_SEI, ADDRESS_MODE_IMPLIED);
        opcode_matrix[0xe2] = OPCODE(OP_CODE_SEP, ADDRESS_MODE_IMMEDIATE);


        opcode_matrix[0x8d] = OPCODE(OP_CODE_STA, ADDRESS_MODE_ABSOLUTE);
        opcode_matrix[0x9d] = OPCODE(OP_CODE_STA, ADDRESS_MODE_ABSOLUTE_INDEXED_X);
        opcode_matrix[0x8f] = OPCODE(OP_CODE_STA, ADDRESS_MODE_ABSOLUTE_LONG);
        opcode_matrix[0x9f] = OPCODE(OP_CODE_STA, ADDRESS_MODE_ABSOLUTE_LONG_INDEXED_X);
        opcode_matrix[0x85] = OPCODE(OP_CODE_STA, ADDRESS_MODE_DIRECT);
        opcode_matrix[0x83] = OPCODE(OP_CODE_STA, ADDRESS_MODE_STACK_RELATIVE);
        opcode_matrix[0x95] = OPCODE(OP_CODE_STA, ADDRESS_MODE_DIRECT_INDEXED_X);
        opcode_matrix[0x92] = OPCODE(OP_CODE_STA, ADDRESS_MODE_DIRECT_INDIRECT);
        opcode_matrix[0x87] = OPCODE(OP_CODE_STA, ADDRESS_MODE_DIRECT_INDIRECT_LONG);
        opcode_matrix[0x93] = OPCODE(OP_CODE_STA, ADDRESS_MODE_STACK_RELATIVE_INDIRECT_INDEXED);
        opcode_matrix[0x81] = OPCODE(OP_CODE_STA, ADDRESS_MODE_DIRECT_INDEXED_INDIRECT);
        opcode_matrix[0x91] = OPCODE(OP_CODE_STA, ADDRESS_MODE_DIRECT_INDIRECT_INDEXED);
        opcode_matrix[0x97] = OPCODE(OP_CODE_STA, ADDRESS_MODE_DIRECT_INDIRECT_LONG_INDEXED);


        opcode_matrix[0xdb] = OPCODE(OP_CODE_STP, ADDRESS_MODE_IMPLIED);


        opcode_matrix[0x8e] = OPCODE(OP_CODE_STX, ADDRESS_MODE_ABSOLUTE);
        opcode_matrix[0x86] = OPCODE(OP_CODE_STX, ADDRESS_MODE_DIRECT);
        opcode_matrix[0x96] = OPCODE(OP_CODE_STX, ADDRESS_MODE_DIRECT_INDEXED_Y);


        opcode_matrix[0x8c] = OPCODE(OP_CODE_STY, ADDRESS_MODE_ABSOLUTE);
        opcode_matrix[0x84] = OPCODE(OP_CODE_STY, ADDRESS_MODE_DIRECT);
        opcode_matrix[0x94] = OPCODE(OP_CODE_STY, ADDRESS_MODE_DIRECT_INDEXED_X);


        opcode_matrix[0x9c] = OPCODE(OP_CODE_STZ, ADDRESS_MODE_ABSOLUTE);
        opcode_matrix[0x9e] = OPCODE(OP_CODE_STZ, ADDRESS_MODE_ABSOLUTE_INDEXED_X);
        opcode_matrix[0x64] = OPCODE(OP_CODE_STZ, ADDRESS_MODE_DIRECT);
        opcode_matrix[0x74] = OPCODE(OP_CODE_STZ, ADDRESS_MODE_DIRECT_INDEXED_X);


        opcode_matrix[0xaa] = OPCODE(OP_CODE_TAX, ADDRESS_MODE_IMPLIED);
        opcode_matrix[0xab] = OPCODE(OP_CODE_TAY, ADDRESS_MODE_IMPLIED);
        opcode_matrix[0x5b] = OPCODE(OP_CODE_TCD, ADDRESS_MODE_IMPLIED);
        opcode_matrix[0x1b] = OPCODE(OP_CODE_TCS, ADDRESS_MODE_IMPLIED);
        opcode_matrix[0x7b] = OPCODE(OP_CODE_TDC, ADDRESS_MODE_IMPLIED);


        opcode_matrix[0x1c] = OPCODE(OP_CODE_TRB, ADDRESS_MODE_ABSOLUTE);
        opcode_matrix[0x14] = OPCODE(OP_CODE_TRB, ADDRESS_MODE_DIRECT);


        opcode_matrix[0x0c] = OPCODE(OP_CODE_TSB, ADDRESS_MODE_ABSOLUTE);
        opcode_matrix[0x04] = OPCODE(OP_CODE_TSB, ADDRESS_MODE_DIRECT);


        opcode_matrix[0x3b] = OPCODE(OP_CODE_TSC, ADDRESS_MODE_ACCUMULATOR);
        opcode_matrix[0xba] = OPCODE(OP_CODE_TSX, ADDRESS_MODE_ACCUMULATOR);
        opcode_matrix[0x8a] = OPCODE(OP_CODE_TXA, ADDRESS_MODE_ACCUMULATOR);
        opcode_matrix[0x9a] = OPCODE(OP_CODE_TXS, ADDRESS_MODE_ACCUMULATOR);
        opcode_matrix[0x9b] = OPCODE(OP_CODE_TXY, ADDRESS_MODE_ACCUMULATOR);
        opcode_matrix[0x98] = OPCODE(OP_CODE_TYA, ADDRESS_MODE_ACCUMULATOR);
        opcode_matrix[0xbb] = OPCODE(OP_CODE_TYX, ADDRESS_MODE_ACCUMULATOR);
        opcode_matrix[0xcb] = OPCODE(OP_CODE_WAI, ADDRESS_MODE_ACCUMULATOR);
        opcode_matrix[0x42] = OPCODE(OP_CODE_WDM, ADDRESS_MODE_ACCUMULATOR);
        opcode_matrix[0xeb] = OPCODE(OP_CODE_XBA, ADDRESS_MODE_ACCUMULATOR);
        opcode_matrix[0xfb] = OPCODE(OP_CODE_XCE, ADDRESS_MODE_ACCUMULATOR);
    }

    reg_dbr = 0;
    reg_pbr = 0;
    reg_x = 0;
    reg_d = 0;
    reg_e = 1;

    /* on reset, pc gets loaded with the contents
    of the pointer 00fffc... */
    reg_pc = *(unsigned short *)(cpu_ram + 0xfffc);
}

char *opcode_str(struct op_code_t *op_code)
{
    char *op_code_str;
    char *addr_mode_str;

    static char instruction_str[512];

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
            addr_mode_str = "absolute - a";
        break;

        case ADDRESS_MODE_ABSOLUTE_INDEXED_INDIRECT:
            addr_mode_str = "absolute indexed indirect - (a,x)";
        break;

        case ADDRESS_MODE_ABSOLUTE_INDEXED_X:
            addr_mode_str = "absolute indexed X - a,x";
        break;

        case ADDRESS_MODE_ABSOLUTE_INDEXED_Y:
            addr_mode_str = "absolute indexed Y - a,y";
        break;

        case ADDRESS_MODE_ABSOLUTE_INDIRECT:
            addr_mode_str = "absolute indirect - (a)";
        break;

        case ADDRESS_MODE_ABSOLUTE_LONG_INDEXED_X:
            addr_mode_str = "absolute long indexed x - al,x";
        break;

        case ADDRESS_MODE_ABSOLUTE_LONG:
            addr_mode_str = "absolute long - al";
        break;

        case ADDRESS_MODE_ACCUMULATOR:
            addr_mode_str = "accumulator - A";
        break;

        case ADDRESS_MODE_BLOCK_MOVE:
            addr_mode_str = "block move - xyz";
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
            addr_mode_str = "immediate - #";
        break;

        case ADDRESS_MODE_IMPLIED:
            addr_mode_str = "implied - i";
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

    strcpy(instruction_str, op_code_str);
    strcat(instruction_str, " (");
    strcat(instruction_str, addr_mode_str);
    strcat(instruction_str, ")");

    return instruction_str;
}

void disassemble(int start, int byte_count)
{
    char *op_str;
    int bytes_disassmd = 0;
    struct op_code_t opcode;
    int opcode_offset;

    int start_address;
    int address;
    int i;

    if(start >= 0)
    {
        reg_pc = start & 0xffff;
        reg_pbr = (start >> 16) & 0xff;
    }

    while(bytes_disassmd < byte_count)
    {
        address = EFFECTIVE_ADDRESS(reg_pbr, reg_pc);
        opcode = opcode_matrix[cpu_ram[address]];
        op_str = opcode_str(&opcode);

        opcode_offset = address_mode_operand_size[opcode.address_mode] + 1;

        printf("[0x%02x:0x%04x]: ", (address >> 16) & 0xff, address & 0xffff);

        for(i = 0; i < 4; i++)
        {
            if(i < opcode_offset)
            {
                printf("%02x ", cpu_ram[address + i]);
            }
            else
            {
                printf("   ");
            }
        }
        printf("; ");

        printf("%s\n", op_str);


        bytes_disassmd += opcode_offset;

        if((unsigned short)(reg_pc + (unsigned short)opcode_offset) < (unsigned short)reg_pc)
        {
            reg_pbr++;
        }

        reg_pc += opcode_offset;
    }
}

void disassemble_rom_header()
{

}

void disassemble_all()
{

}

void step_cpu()
{

}






