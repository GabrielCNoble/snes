#ifndef CPU_H
#define CPU_H

#include "addr.h"
#include <stdint.h>

enum OPCODE_CLASS
{
    OPCODE_CLASS_LOAD = 0,
    OPCODE_CLASS_STORE,
    OPCODE_CLASS_BRANCH,
    OPCODE_CLASS_STACK,
    OPCODE_CLASS_ALU,
    OPCODE_CLASS_TRANSFER,
    OPCODE_CLASS_STANDALONE,
};

enum ALU_OP
{
    ALU_OP_ADD = 0,
    ALU_OP_SUB,
    ALU_OP_AND,
    ALU_OP_OR,
    ALU_OP_XOR,
    ALU_OP_CMP,
    ALU_OP_INC,
    ALU_OP_DEC,
    ALU_OP_SHR,
    ALU_OP_SHL,
    ALU_OP_ROR,
    ALU_OP_ROL,
    ALU_OP_TSB,
    ALU_OP_TRB,
    ALU_OP_BIT,
    ALU_OP_BIT_IMM,
    ALU_OP_LAST
};

enum CPU_INSTS
{
    CPU_INST_BRK = 0,                        /* Force break */
    CPU_INST_BIT,                            /* Bit test */

    /* ====================================== */
    /* DO NOT CHANGE THE ORDER OF THOSE ENUMS */


    /* instead of having a separate switch case for each branch instruction a list of
    branches is used. Here the instructions are paired with their "opposite" instructions.
    For example, BCC (branch if carry is clear) comes before BCS (branch if carry is set).
    This order is intentional. A branch that is taken if the respective flag is taken has its
    LSB set to zero, while its "opposite" instruction has its LSB set to one. Shifting both
    values right by one then yields the same value, which is used to index the condition table.
    The LSB then is used to decide whether the branch should be taken or not. */

    CPU_INST_BCC,                            /* Branch on carry clear (C == 0) */
    CPU_INST_BCS,                            /* Branch on carry set (C == 1) */

    CPU_INST_BNE,                            /* Branch if not equal (Z == 0) */
    CPU_INST_BEQ,                            /* Branch if equal (Z == 1) */

    CPU_INST_BPL,                            /* Branch if result plus (N == 0) */
    CPU_INST_BMI,                            /* Branch if result minus (N == 1) */

    CPU_INST_BVC,                            /* Branch on overflow clear (V == 0) */
    CPU_INST_BVS,                            /* Branch on overflow set (V == 1) */

    CPU_INST_BRA,                            /* Branch always */
    CPU_INST_BRL,                            /* Branch always long */

    /* ====================================== */
    /* ====================================== */


    CPU_INST_CLC,                            /* Clear carry flag */
    CPU_INST_CLD,                            /* Clear decimal mode */
    CPU_INST_CLV,                            /* Clear overflow flag */
    CPU_INST_CLI,                            /* Clear interrupt disable bit */


    CPU_INST_CMP,                            /* Compare memory and accumulator */
    CPU_INST_CPX,                            /* Compare memory and index X */
    CPU_INST_CPY,                            /* Compare memory and index Y */


    /* ====================================== */
    /* DO NOT CHANGE THE ORDER OF THOSE ENUMS */

    /* bit 0 of those constants are used to index into an array containing "opposite"
    alu operations, so their order must be preserved */

    CPU_INST_ADC,                            /* Add memory to accumulator with carry */
    CPU_INST_AND,                            /* AND memory with accumulator */
    CPU_INST_SBC,                            /* Subtract memory from accumulator with borrow */
    CPU_INST_EOR,                            /* Exclusive OR memory with accumulator */
    CPU_INST_ORA,                            /* OR memory with accumulator */

    CPU_INST_ROL,                            /* Rotate left one bit (memory or accumulator) */
    CPU_INST_ROR,                            /* Rotate right one bit (memory or accumulator) */
    CPU_INST_DEC,                            /* Decrement memory or accumulator */
    CPU_INST_INC,                            /* Increment memory or accumulator */
    CPU_INST_ASL,                            /* Shift one bit left, memory or accumulator */
    CPU_INST_LSR,                            /* Shift one bit right, memory or accumulator */

    CPU_INST_DEX,                            /* Decrement index X */
    CPU_INST_INX,                            /* Increment index X */
    CPU_INST_DEY,                            /* Decrement index Y */
    CPU_INST_INY,                            /* Increment index Y */

    CPU_INST_TRB,                            /* Test and reset bit */
    CPU_INST_TSB,                            /* Test and set bit */

    /* ====================================== */
    /* ====================================== */



    CPU_INST_JMP,                            /* Jump */
    CPU_INST_JML,                            /* Jump long */
    CPU_INST_JSL,                            /* Jump subroutine long */
    CPU_INST_JSR,                            /* Jump and save return */


    /* ====================================== */
    /* DO NOT CHANGE THE ORDER OF THOSE ENUMS */
    CPU_INST_LDA,                            /* Load accumulator with memory */
    CPU_INST_LDX,                            /* Load index X with memory */
    CPU_INST_LDY,                            /* Load index Y with memory */

    CPU_INST_STA,                            /* Store accumulator in memory */
    CPU_INST_STX,                            /* Store index X in memory */
    CPU_INST_STY,                            /* Store index Y in memory */
    CPU_INST_STZ,                            /* Store zero in memory */
    /* ====================================== */
    /* ====================================== */



    CPU_INST_MVN,                            /* Block move negative */
    CPU_INST_MVP,                            /* Block move positive */
    CPU_INST_NOP,                            /* No operation */


    /* ====================================== */
    /* DO NOT CHANGE THE ORDER OF THOSE ENUMS */
    CPU_INST_PEA,                            /* Push effective absolute address on stack (or push immediate data on stack) */
    CPU_INST_PEI,                            /* Push effective absolute address on stack (or push direct data on stack) */
    CPU_INST_PER,                            /* Push effective program counter relative address on stack */
    CPU_INST_PHA,                            /* Push accumulator on stack */
    CPU_INST_PHB,                            /* Push data bank register on stack */
    CPU_INST_PHD,                            /* Push direct register on stack */
    CPU_INST_PHK,                            /* Push program bank register on stack */
    CPU_INST_PHP,                            /* Push processor status on stack */
    CPU_INST_PHX,                            /* Push index X on stack */
    CPU_INST_PHY,                            /* Push index Y on stack */

    CPU_INST_PLA,                            /* Pull accumulator from stack */
    CPU_INST_PLB,                            /* Pull data bank register from stack */
    CPU_INST_PLD,                            /* Pull direct register from stack */
    CPU_INST_PLP,                            /* Pull processor status from stack */
    CPU_INST_PLX,                            /* Pull index X from stack */
    CPU_INST_PLY,                            /* Pull index Y from stack */
    /* ====================================== */
    /* ====================================== */


    CPU_INST_REP,                            /* Reset status bits */


    CPU_INST_RTI,                            /* Return from interrupt */
    CPU_INST_RTL,                            /* Return from subroutine long */
    CPU_INST_RTS,                            /* Return from subroutine */


//    OPCODE_SBC,                            /* Subtract memory from accumulator with borrow */


    CPU_INST_SEP,                            /* Set processor status bit */
    CPU_INST_SEC,                            /* Set carry flag */
    CPU_INST_SED,                            /* Set decimal mode */
    CPU_INST_SEI,                            /* Set interrupt disable status */


    /* ====================================== */
    /* DO NOT CHANGE THE ORDER OF THOSE ENUMS */

    CPU_INST_TAX,                            /* Transfer accumulator to index X */
    CPU_INST_TAY,                            /* Transfer accumulator to index Y */
    CPU_INST_TCD,                            /* Transfer C accumulator to direct register */
    CPU_INST_TCS,                            /* Transfer C accumulator to stack pointer */
    CPU_INST_TDC,                            /* Transfer direct register to C accumulator */
    CPU_INST_TSC,                            /* Transfer stack pointer to C accumulator */
    CPU_INST_TSX,                            /* Transfer stack pointer to index X */
    CPU_INST_TXA,                            /* Transfer index X to accumulator */
    CPU_INST_TXS,                            /* Transfer index X to stack pointer */
    CPU_INST_TXY,                            /* Transfer index X to index Y */
    CPU_INST_TYA,                            /* Transfer index Y to accumulator */
    CPU_INST_TYX,                            /* Transfer index Y to index X */

    /* ====================================== */
    /* ====================================== */


    CPU_INST_WAI,                            /* Wait for interrupt */


    CPU_INST_WDM,                            /* Reserved */


    CPU_INST_XBA,                            /* Exchange B and A accumulator??? */



    CPU_INST_STP,                            /* Stop the clock */


    CPU_INST_COP,                            /* Coprocessor */


    CPU_INST_XCE,                            /* Exchange carry and emulation bits */

    CPU_INST_UNKNOWN,
};


enum ADDRESS_MODES
{
    ADDRESS_MODE_ABSOLUTE = 0,                      /* a */
    ADDRESS_MODE_ABSOLUTE_INDEXED_INDIRECT,         /* (a,x) */
    ADDRESS_MODE_ABSOLUTE_INDEXED_X,                /* a,x */
    ADDRESS_MODE_ABSOLUTE_INDEXED_Y,                /* a,y */
    ADDRESS_MODE_ABSOLUTE_INDIRECT,                 /* (a) */
    ADDRESS_MODE_ABSOLUTE_LONG_INDEXED_X,           /* al,x */
    ADDRESS_MODE_ABSOLUTE_LONG,                     /* al */
    ADDRESS_MODE_ACCUMULATOR,                       /* A */
    ADDRESS_MODE_BLOCK_MOVE,                        /* xyc */
    ADDRESS_MODE_DIRECT_INDEXED_INDIRECT,           /* (d,x) */
    ADDRESS_MODE_DIRECT_INDEXED_X,                  /* d,x */
    ADDRESS_MODE_DIRECT_INDEXED_Y,                  /* d,y */
    ADDRESS_MODE_DIRECT_INDIRECT_INDEXED,           /* (d),y */
    ADDRESS_MODE_DIRECT_INDIRECT_LONG_INDEXED,      /* [d],y */
    ADDRESS_MODE_DIRECT_INDIRECT_LONG,              /* [d] */
    ADDRESS_MODE_DIRECT_INDIRECT,                   /* (d) */
    ADDRESS_MODE_DIRECT,                            /* d */
    ADDRESS_MODE_IMMEDIATE,                         /* # */
    ADDRESS_MODE_IMPLIED,                           /* i */
    ADDRESS_MODE_PROGRAM_COUNTER_RELATIVE_LONG,     /* rl */
    ADDRESS_MODE_PROGRAM_COUNTER_RELATIVE,          /* r */
    ADDRESS_MODE_STACK,                             /* s */
    ADDRESS_MODE_STACK_RELATIVE,                    /* d,s */
    ADDRESS_MODE_STACK_RELATIVE_INDIRECT_INDEXED,   /* (d,s),y */
    ADDRESS_MODE_UNKNOWN

};

enum CPU_OPCODES
{
    CPU_OPCODE_ADC_ABS             = 0x6d,
    CPU_OPCODE_ADC_ABS_X           = 0x7d,
    CPU_OPCODE_ADC_ABS_Y           = 0x79,
    CPU_OPCODE_ADC_ABSL            = 0x6f,
    CPU_OPCODE_ADC_ABSL_X          = 0x7f,
    CPU_OPCODE_ADC_DIR             = 0x65,
    CPU_OPCODE_ADC_S_REL           = 0x63,
    CPU_OPCODE_ADC_DIR_X           = 0x75,
    CPU_OPCODE_ADC_DIR_IND         = 0x72,
    CPU_OPCODE_ADC_DIR_INDL        = 0x67,
    CPU_OPCODE_ADC_S_REL_IND_Y     = 0x73,
    CPU_OPCODE_ADC_DIR_X_IND       = 0x61,
    CPU_OPCODE_ADC_DIR_IND_Y       = 0x71,
    CPU_OPCODE_ADC_DIR_INDL_Y      = 0x77,
    CPU_OPCODE_ADC_IMM             = 0x69,

    CPU_OPCODE_AND_ABS             = 0x2d,
    CPU_OPCODE_AND_ABS_X           = 0x3d,
    CPU_OPCODE_AND_ABS_Y           = 0x39,
    CPU_OPCODE_AND_ABSL            = 0x2f,
    CPU_OPCODE_AND_ABSL_X          = 0x3f,
    CPU_OPCODE_AND_DIR             = 0x25,
    CPU_OPCODE_AND_S_REL           = 0x23,
    CPU_OPCODE_AND_DIR_X           = 0x35,
    CPU_OPCODE_AND_DIR_IND         = 0x32,
    CPU_OPCODE_AND_DIR_INDL        = 0x27,
    CPU_OPCODE_AND_S_REL_IND_Y     = 0x33,
    CPU_OPCODE_AND_DIR_X_IND       = 0x21,
    CPU_OPCODE_AND_DIR_IND_Y       = 0x31,
    CPU_OPCODE_AND_DIR_INDL_Y      = 0x37,
    CPU_OPCODE_AND_IMM             = 0x29,

    CPU_OPCODE_ASL_ABS             = 0x0e,
    CPU_OPCODE_ASL_ACC             = 0x0a,
    CPU_OPCODE_ASL_ABS_X           = 0x1e,
    CPU_OPCODE_ASL_DIR             = 0x06,
    CPU_OPCODE_ASL_DIR_X           = 0X16,

    CPU_OPCODE_BCC_PC_REL          = 0x90,

    CPU_OPCODE_BCS_PC_REL          = 0xb0,

    CPU_OPCODE_BEQ_PC_REL          = 0xf0,

    CPU_OPCODE_BIT_ABS             = 0x2c,
    CPU_OPCODE_BIT_ABS_X           = 0x3c,
    CPU_OPCODE_BIT_DIR             = 0x24,
    CPU_OPCODE_BIT_DIR_X           = 0x34,
    CPU_OPCODE_BIT_IMM             = 0x89,

    CPU_OPCODE_BMI_PC_REL          = 0x30,

    CPU_OPCODE_BNE_PC_REL          = 0xd0,

    CPU_OPCODE_BPL_PC_REL          = 0x10,

    CPU_OPCODE_BRA_PC_REL          = 0x80,

    CPU_OPCODE_BRK_S               = 0x00,

    CPU_OPCODE_BRL_PC_RELL         = 0x82,

    CPU_OPCODE_BVC_PC_REL          = 0x50,

    CPU_OPCODE_BVS_PC_REL          = 0x70,

    CPU_OPCODE_CLC_IMP             = 0x18,

    CPU_OPCODE_CLD_IMP             = 0xd8,

    CPU_OPCODE_CLI_IMP             = 0x58,

    CPU_OPCODE_CLV_IMP             = 0xb8,

    CPU_OPCODE_CMP_ABS             = 0xcd,
    CPU_OPCODE_CMP_ABS_X           = 0xdd,
    CPU_OPCODE_CMP_ABS_Y           = 0xd9,
    CPU_OPCODE_CMP_ABSL            = 0xcf,
    CPU_OPCODE_CMP_ABSL_X          = 0xdf,
    CPU_OPCODE_CMP_DIR             = 0xc5,
    CPU_OPCODE_CMP_S_REL           = 0xc3,
    CPU_OPCODE_CMP_DIR_X           = 0xd5,
    CPU_OPCODE_CMP_DIR_IND         = 0xd2,
    CPU_OPCODE_CMP_DIR_INDL        = 0xc7,
    CPU_OPCODE_CMP_S_REL_IND_Y     = 0xd3,
    CPU_OPCODE_CMP_DIR_X_IND       = 0xc1,
    CPU_OPCODE_CMP_DIR_IND_Y       = 0xd1,
    CPU_OPCODE_CMP_DIR_INDL_Y      = 0xd7,
    CPU_OPCODE_CMP_IMM             = 0xc9,

    CPU_OPCODE_COP_S               = 0x02,

    CPU_OPCODE_CPX_ABS             = 0xec,
    CPU_OPCODE_CPX_DIR             = 0xe4,
    CPU_OPCODE_CPX_IMM             = 0xe0,

    CPU_OPCODE_CPY_ABS             = 0xcc,
    CPU_OPCODE_CPY_DIR             = 0xc4,
    CPU_OPCODE_CPY_IMM             = 0xc0,

    CPU_OPCODE_DEC_ABS             = 0xce,
    CPU_OPCODE_DEC_ACC             = 0x3a,
    CPU_OPCODE_DEC_ABS_X           = 0xde,
    CPU_OPCODE_DEC_DIR             = 0xc6,
    CPU_OPCODE_DEC_DIR_X           = 0xd6,

    CPU_OPCODE_DEX_IMP             = 0xca,

    CPU_OPCODE_DEY_IMP             = 0x88,

    CPU_OPCODE_EOR_ABS             = 0x4d,
    CPU_OPCODE_EOR_ABS_X           = 0x5d,
    CPU_OPCODE_EOR_ABS_Y           = 0x59,
    CPU_OPCODE_EOR_ABSL            = 0x4f,
    CPU_OPCODE_EOR_ABSL_X          = 0x5f,
    CPU_OPCODE_EOR_DIR             = 0x45,
    CPU_OPCODE_EOR_S_REL           = 0x43,
    CPU_OPCODE_EOR_DIR_X           = 0x55,
    CPU_OPCODE_EOR_DIR_IND         = 0x52,
    CPU_OPCODE_EOR_DIR_INDL        = 0x47,
    CPU_OPCODE_EOR_S_REL_IND_Y     = 0x53,
    CPU_OPCODE_EOR_DIR_X_IND       = 0x41,
    CPU_OPCODE_EOR_DIR_IND_Y       = 0x51,
    CPU_OPCODE_EOR_DIR_INDL_Y      = 0x57,
    CPU_OPCODE_EOR_IMM             = 0x49,

    CPU_OPCODE_INC_ABS             = 0xee,
    CPU_OPCODE_INC_ACC             = 0x1a,
    CPU_OPCODE_INC_ABS_X           = 0xfe,
    CPU_OPCODE_INC_DIR             = 0xe6,
    CPU_OPCODE_INC_DIR_X           = 0xf6,

    CPU_OPCODE_INX_IMP             = 0xe8,

    CPU_OPCODE_INY_IMP             = 0xc8,

    CPU_OPCODE_JML_ABS_IND         = 0xdc,

    CPU_OPCODE_JMP_ABS             = 0x4c,
    CPU_OPCODE_JMP_ABSL            = 0x5c,
    CPU_OPCODE_JMP_ABS_IND         = 0x6c,
    CPU_OPCODE_JMP_ABS_X_IND       = 0x7c,

    CPU_OPCODE_JSL_ABSL            = 0x22,

    CPU_OPCODE_JSR_ABS             = 0x20,
    CPU_OPCODE_JSR_ABS_X_IND       = 0xfc,

    CPU_OPCODE_LDA_ABS             = 0xad,
    CPU_OPCODE_LDA_ABS_X           = 0xbd,
    CPU_OPCODE_LDA_ABS_Y           = 0xb9,
    CPU_OPCODE_LDA_ABSL            = 0xaf,
    CPU_OPCODE_LDA_ABSL_X          = 0xbf,
    CPU_OPCODE_LDA_DIR             = 0xa5,
    CPU_OPCODE_LDA_S_REL           = 0xa3,
    CPU_OPCODE_LDA_DIR_X           = 0xb5,
    CPU_OPCODE_LDA_DIR_IND         = 0xb2,
    CPU_OPCODE_LDA_DIR_INDL        = 0xa7,
    CPU_OPCODE_LDA_S_REL_IND_Y     = 0xb3,
    CPU_OPCODE_LDA_DIR_X_IND       = 0xa1,
    CPU_OPCODE_LDA_DIR_IND_Y       = 0xb1,
    CPU_OPCODE_LDA_DIR_INDL_Y      = 0xb7,
    CPU_OPCODE_LDA_IMM             = 0xa9,

    CPU_OPCODE_LDX_ABS             = 0xae,
    CPU_OPCODE_LDX_ABS_Y           = 0xbe,
    CPU_OPCODE_LDX_DIR             = 0xa6,
    CPU_OPCODE_LDX_DIR_Y           = 0xb6,
    CPU_OPCODE_LDX_IMM             = 0xa2,

    CPU_OPCODE_LDY_ABS             = 0xac,
    CPU_OPCODE_LDY_ABS_X           = 0xbc,
    CPU_OPCODE_LDY_DIR             = 0xa4,
    CPU_OPCODE_LDY_DIR_X           = 0xb4,
    CPU_OPCODE_LDY_IMM             = 0xa0,

    CPU_OPCODE_LSR_ABS             = 0x4e,
    CPU_OPCODE_LSR_ACC             = 0x4a,
    CPU_OPCODE_LSR_ABS_X           = 0x5e,
    CPU_OPCODE_LSR_DIR             = 0x46,
    CPU_OPCODE_LSR_DIR_X           = 0x56,

    CPU_OPCODE_MVN_BLK             = 0x54,

    CPU_OPCODE_MVP_BLK             = 0x44,

    CPU_OPCODE_NOP_IMP             = 0xea,

    CPU_OPCODE_ORA_ABS             = 0x0d,
    CPU_OPCODE_ORA_ABS_X           = 0x1d,
    CPU_OPCODE_ORA_ABS_Y           = 0x19,
    CPU_OPCODE_ORA_ABSL            = 0x0f,
    CPU_OPCODE_ORA_ABSL_X          = 0x1f,
    CPU_OPCODE_ORA_DIR             = 0x05,
    CPU_OPCODE_ORA_S_REL           = 0x03,
    CPU_OPCODE_ORA_DIR_X           = 0x15,
    CPU_OPCODE_ORA_DIR_IND         = 0x12,
    CPU_OPCODE_ORA_DIR_INDL        = 0x07,
    CPU_OPCODE_ORA_S_REL_IND_Y     = 0x13,
    CPU_OPCODE_ORA_DIR_X_IND       = 0x01,
    CPU_OPCODE_ORA_DIR_IND_Y       = 0x11,
    CPU_OPCODE_ORA_DIR_INDL_Y      = 0x17,
    CPU_OPCODE_ORA_IMM             = 0x09,

    CPU_OPCODE_PEA_S               = 0xf4,

    CPU_OPCODE_PEI_S               = 0xd4,

    CPU_OPCODE_PER_S               = 0x62,

    CPU_OPCODE_PHA_S               = 0x48,

    CPU_OPCODE_PHB_S               = 0x8b,

    CPU_OPCODE_PHD_S               = 0x0b,

    CPU_OPCODE_PHK_S               = 0x4b,

    CPU_OPCODE_PHP_S               = 0x08,

    CPU_OPCODE_PHX_S               = 0xda,

    CPU_OPCODE_PHY_S               = 0x5a,

    CPU_OPCODE_PLA_S               = 0x68,

    CPU_OPCODE_PLB_S               = 0xab,

    CPU_OPCODE_PLD_S               = 0x2b,

    CPU_OPCODE_PLP_S               = 0x28,

    CPU_OPCODE_PLX_S               = 0xfa,

    CPU_OPCODE_PLY_S               = 0x7a,

    CPU_OPCODE_REP_IMM             = 0xc2,

    CPU_OPCODE_ROL_ABS             = 0x2e,
    CPU_OPCODE_ROL_ACC             = 0x2a,
    CPU_OPCODE_ROL_ABS_X           = 0x3e,
    CPU_OPCODE_ROL_DIR             = 0x26,
    CPU_OPCODE_ROL_DIR_X           = 0x36,

    CPU_OPCODE_ROR_ABS             = 0x6e,
    CPU_OPCODE_ROR_ACC             = 0x6a,
    CPU_OPCODE_ROR_ABS_X           = 0x7e,
    CPU_OPCODE_ROR_DIR             = 0x66,
    CPU_OPCODE_ROR_DIR_X           = 0x76,

    CPU_OPCODE_RTI_S               = 0x40,

    CPU_OPCODE_RTL_S               = 0x6b,

    CPU_OPCODE_RTS_S               = 0x60,

    CPU_OPCODE_SBC_ABS             = 0xed,
    CPU_OPCODE_SBC_ABS_X           = 0xfd,
    CPU_OPCODE_SBC_ABS_Y           = 0xf9,
    CPU_OPCODE_SBC_ABSL            = 0xef,
    CPU_OPCODE_SBC_ABSL_X          = 0xff,
    CPU_OPCODE_SBC_DIR             = 0xe5,
    CPU_OPCODE_SBC_S_REL           = 0xe3,
    CPU_OPCODE_SBC_DIR_X           = 0xf5,
    CPU_OPCODE_SBC_DIR_IND         = 0xf2,
    CPU_OPCODE_SBC_DIR_INDL        = 0xe7,
    CPU_OPCODE_SBC_S_REL_IND_Y     = 0xf3,
    CPU_OPCODE_SBC_DIR_X_IND       = 0xe1,
    CPU_OPCODE_SBC_DIR_IND_Y       = 0xf1,
    CPU_OPCODE_SBC_DIR_INDL_Y      = 0xf7,
    CPU_OPCODE_SBC_IMM             = 0xe9,

    CPU_OPCODE_SEC_IMP             = 0x38,

    CPU_OPCODE_SED_IMP             = 0xf8,

    CPU_OPCODE_SEI_IMP             = 0x78,

    CPU_OPCODE_SEP_IMM             = 0xe2,

    CPU_OPCODE_STA_ABS             = 0x8d,
    CPU_OPCODE_STA_ABS_X           = 0x9d,
    CPU_OPCODE_STA_ABS_Y           = 0x99,
    CPU_OPCODE_STA_ABSL            = 0x8f,
    CPU_OPCODE_STA_ABSL_X          = 0x9f,
    CPU_OPCODE_STA_DIR             = 0x85,
    CPU_OPCODE_STA_S_REL           = 0x83,
    CPU_OPCODE_STA_DIR_X           = 0x95,
    CPU_OPCODE_STA_DIR_IND         = 0x92,
    CPU_OPCODE_STA_DIR_INDL        = 0x87,
    CPU_OPCODE_STA_S_REL_IND_Y     = 0x93,
    CPU_OPCODE_STA_DIR_X_IND       = 0x81,
    CPU_OPCODE_STA_DIR_IND_Y       = 0x91,
    CPU_OPCODE_STA_DIR_INDL_Y      = 0x97,

    CPU_OPCODE_STP_IMP             = 0xdb,

    CPU_OPCODE_STX_ABS             = 0x8e,
    CPU_OPCODE_STX_DIR             = 0x86,
    CPU_OPCODE_STX_DIR_Y           = 0x96,

    CPU_OPCODE_STY_ABS             = 0x8c,
    CPU_OPCODE_STY_DIR             = 0x84,
    CPU_OPCODE_STY_DIR_X           = 0x94,

    CPU_OPCODE_STZ_ABS             = 0x9c,
    CPU_OPCODE_STZ_ABS_X           = 0x9e,
    CPU_OPCODE_STZ_DIR             = 0x64,
    CPU_OPCODE_STZ_DIR_X           = 0x74,

    CPU_OPCODE_TAX_IMP             = 0xaa,

    CPU_OPCODE_TAY_IMP             = 0xa8,

    CPU_OPCODE_TCD_IMP             = 0x5b,

    CPU_OPCODE_TCS_IMP             = 0x1b,

    CPU_OPCODE_TDC_IMP             = 0x7b,

    CPU_OPCODE_TRB_ABS             = 0x1c,
    CPU_OPCODE_TRB_DIR             = 0x14,

    CPU_OPCODE_TSB_ABS             = 0x0c,
    CPU_OPCODE_TSB_DIR             = 0x04,

    CPU_OPCODE_TSC_ACC             = 0x3b,

    CPU_OPCODE_TSX_ACC             = 0xba,

    CPU_OPCODE_TXA_ACC             = 0x8a,

    CPU_OPCODE_TXS_ACC             = 0x9a,

    CPU_OPCODE_TXY_ACC             = 0x9b,

    CPU_OPCODE_TYA_ACC             = 0x98,

    CPU_OPCODE_TYX_ACC             = 0xbb,

    CPU_OPCODE_WAI_ACC             = 0xcb,

    CPU_OPCODE_WDM_ACC             = 0x42,

    CPU_OPCODE_XBA_ACC             = 0xeb,

    CPU_OPCODE_XCE_ACC             = 0xfb,

    CPU_OPCODE_FETCH               = 0x100,

    CPU_OPCODE_INT_HW              = 0x101,
};

enum CPU_MEM_REGS
{
    CPU_MEM_REG_JOYWR                           = 0x4016,
    CPU_MEM_REG_JOYA                            = 0x4016,
    CPU_MEM_REG_JOYB                            = 0x4017,
    CPU_MEM_REG_NMITIMEN                        = 0x4200,
    CPU_MEM_REG_WRIO                            = 0x4201,
    CPU_MEM_REG_WRMPYA                          = 0x4202,
    CPU_MEM_REG_WRMPYB                          = 0x4203,
    CPU_MEM_REG_WRDIVL                          = 0x4204,
    CPU_MEM_REG_WRDIVH                          = 0x4205,
    CPU_MEM_REG_WRDIVB                          = 0x4206,
    CPU_MEM_REG_HTIMEL                          = 0x4207,
    CPU_MEM_REG_HTIMEH                          = 0x4208,
    CPU_MEM_REG_VTIMEL                          = 0x4209,
    CPU_MEM_REG_VTIMEH                          = 0x420a,
    CPU_MEM_REG_MDMAEN                          = 0x420b,

    /* channels designated for HDMA. All enabled HDMA channels are initialized
    after the end of v-blank, on line 0, starting around dot 8. Before channels
    are initialized, there's a 18 cycles overhead. After that, channels are
    initialized, with a 8 byte overhead for direct table channels, and 24 cycles
    overhead for indirect table channels.

    I'm not really sure what should happen if the cpu ends up writing to this
    register during this initialization period. Didn't really find whether the
    value gets latched when initialization begins. The behavior implemented here
    is: the channels will be initialized one after another, taking into consideration
    all the appropriate cycle overheads. When the initialization for a channel begins,
    the channel bit gets "latched". That is, if it gets cleared after it's been seen
    as set, the channel will be initialized regardless, and will only be deactivated
    when v-blank happens. If it gets seen as clear when initialization for that channel
    should happen, writing to it afterwards won't cause it to be initialized. To get
    the channel to behave properly, then, it'll be necessary to initialze all the appropriate
    registers manually, similar to how it's done when enabling hdma mid-frame. */

    CPU_MEM_REG_HDMAEN                          = 0x420c,
    CPU_MEM_REG_MEMSEL                          = 0x420d,
    CPU_MEM_REG_RDNMI                           = 0x4210,
    CPU_MEM_REG_TIMEUP                          = 0x4211,
    CPU_MEM_REG_HVBJOY                          = 0x4212,
    CPU_MEM_REG_RDIO                            = 0x4213,
    CPU_MEM_REG_RDDIVL                          = 0x4214,
    CPU_MEM_REG_RDDIVH                          = 0x4215,
    CPU_MEM_REG_RDMPYL                          = 0x4216,
    CPU_MEM_REG_RDMPYH                          = 0x4217,
    CPU_MEM_REG_STDCTRL1L                       = 0x4218,
    CPU_MEM_REG_STDCTRL1H                       = 0x4219,
    CPU_MEM_REG_STDCTRL2L                       = 0x421a,
    CPU_MEM_REG_STDCTRL2H                       = 0x421b,
    CPU_MEM_REG_STDCTRL3L                       = 0x421c,
    CPU_MEM_REG_STDCTRL3H                       = 0x421d,
    CPU_MEM_REG_STDCTRL4L                       = 0x421e,
    CPU_MEM_REG_STDCTRL4H                       = 0x421f,

    CPU_MEM_REG_DMA0_PARAM                      = 0x4300,
    CPU_MEM_REG_DMA0_BBUS_ADDR                  = 0x4301,

    CPU_MEM_REG_HDMA0_ATAB_DMA0_ADDRL           = 0x4302,
    CPU_MEM_REG_HDMA0_ATAB_DMA0_ADDRH           = 0x4303,
    CPU_MEM_REG_HDMA0_ATAB_DMA0_BANK            = 0x4304,

    /* not really mentioned anywhere, but I guess those should be incremented (except the bank) after
    every byte written, just like happens with <43X8> and <43X9>...? */
    CPU_MEM_REG_HDMA0_IND_ADDR_DMA0_COUNTL      = 0x4305,
    CPU_MEM_REG_HDMA0_IND_ADDR_DMA0_COUNTH      = 0x4306,
    CPU_MEM_REG_HDMA0_IND_ADDR_BANK             = 0x4307,

    /*
        Those get loaded from <0x43X2> and <0x43X3>, and are incremented as the hdma goes forward.

        If the transfer is using a direct table, this is the address of the data to be transferred,
        and is incremented every time a byte is transferred. How many times it's incremented depends
        on the value specified for the write mode in <0x43X1>. If the write mode specifies a single
        write to a single register, it'll be incremented once. If it specifies two writes to the same
        register, or one write to two consecutive registers, then it gets incremented twice. And so on.
        Once the amount of writes specified is reached, the address will be pointing at the next direct
        table entry. The line count byte will be loaded into <0x43Xa> and those registers will be updated
        to point at the data right after it, and then the whole thing repeats.

        If the transfer is using an indirect table, this is the address of an indirect table entry,
        and it's incremented every time the entry is used. Each entry is composed by 3 bytes. The
        first is the line count, and the next two are the pointer to the actual data to be transferred.
        Once the data referenced by the entry is transferred, this register gets incremente by 3,
        which makes it point at the next indirect table entry. The line count byte gets loaded into
        <0x43Xa>, the register gets updated to point at the indirect address portion of the indirect
        table entry, and the indirect address gets loaded into <0x43X5> and <0x43X6>.
    */

    CPU_MEM_REG_HDMA0_DIR_ADDRL                 = 0x4308,
    CPU_MEM_REG_HDMA0_DIR_ADDRH                 = 0x4309,


    CPU_MEM_REG_HDMA0_CUR_LINE_COUNT            = 0x430a,
};

enum CPU_NMITIMEN_FLAGS
{
    CPU_NMITIMEN_FLAG_STD_CTRL_EN   = 1,
    CPU_NMITIMEN_FLAG_HTIMER_EN     = 1 << 4,
    CPU_NMITIMEN_FLAG_VTIMER_EN     = 1 << 5,
    CPU_NMITIMEN_FLAG_NMI_ENABLE    = 1 << 7,
};

enum CPU_RDNMI_FLAGS
{
    CPU_RDNMI_BLANK_NMI = 1 << 7
};

enum CPU_HVBJOY_FLAGS
{
    CPU_HVBJOY_FLAG_VBLANK = 1 << 7,
    CPU_HVBJOY_FLAG_HBLANK = 1 << 6,
};

// enum CPU_STATUS_FLAGS
// {
//     CPU_STATUS_FLAG_CARRY           = 1,           /* (C) */
//     CPU_STATUS_FLAG_IRQ_DISABLE     = 1 << 1,      /* (I) */
//     CPU_STATUS_FLAG_ZERO            = 1 << 2,      /* (Z) */
//     CPU_STATUS_FLAG_DECIMAL         = 1 << 3,      /* (D) */
//     CPU_STATUS_FLAG_INDEX           = 1 << 4,      /* (X) */
//     CPU_STATUS_FLAG_BREAK           = 1 << 5,      /* (B) */
//     CPU_STATUS_FLAG_MEMORY          = 1 << 5,      /* (M) */
//     CPU_STATUS_FLAG_OVERFLOW        = 1 << 6,      /* (V) */
//     CPU_STATUS_FLAG_NEGATIVE        = 1 << 7,      /* (N) */
// };

// enum CPU_STORE_RESULT
// {
//     CPU_STORE_RESULT_NO_STORE = 0,          /* result gets used to set some status flags, and then is thrown away */
//     CPU_STORE_RESULT_REGISTER,              /* result is kept in the register */
//     CPU_STORE_RESULT_MEMORY                 /* result gets written back to memory */
// };

// enum CPU_INTERRUPT
// {
//     CPU_INTERRUPT_RESET = 0,
//     CPU_INTERRUPT_IRQB,
//     CPU_INTERRUPT_NMIB,
//     CPU_INTERRUPT_ABORTB
// };

struct opcode_t
{
    uint8_t opcode;
    uint8_t address_mode : 5;
    uint8_t opcode_class : 3;
};

struct opcode_info_t
{
    uint8_t width[2];
    uint8_t width_flag;
    uint8_t opcode;
    uint8_t addr_mode;
};

struct disasm_inst_t
{
    uint8_t         bytes[7];
    uint8_t         width;
    const char *    opcode_str;
    const char *    addr_mode_str;
};
// struct branch_cond_t
// {
//     uint8_t condition;
// };

// #define TPARM_INDEX(opcode) (opcode - OPCODE_TAX)
// struct transfer_params_t
// {
//     void *      src_reg;
//     void *      dst_reg;
//     uint8_t     flag;
// };

// struct transfer_t
// {
//     void *      src_reg;
//     void *      dst_reg;
//     uint8_t     flag;
// };

// struct load_t
// {
//     void *      dst_reg;
//     uint8_t     flag;
// };

// struct store_t
// {
//     void *      src_reg;
//     uint8_t     flag;
// };

// struct push_t
// {
//     void *      src_reg;
//     uint16_t    flag;
// };

// struct pop_t
// {
//     void *      dst_reg;
//     uint16_t     flag;
// };

enum CPU_REGS
{
    CPU_REG_ACCUM,
    CPU_REG_X,
    CPU_REG_Y,
    CPU_REG_D,
    CPU_REG_S,
    CPU_REG_PBR,
    CPU_REG_DBR,
    CPU_REG_INST,
    CPU_REG_PC,
    CPU_REG_P,
    CPU_REG_TEMP,
    CPU_REG_ADDR,
    CPU_REG_BANK,
    CPU_REG_ZERO,
    CPU_REG_LAST,
};

struct reg_t
{
    union
    {
        uint16_t        word;
        uint8_t         byte[2];
    };

    uint16_t flag;
};

#define MEM_SPEED_FAST_CYCLES   6
#define MEM_SPEED_MED_CYCLES    8
#define MEM_SPEED_SLOW_CYCLES   12
#define CPU_MUL_MACHINE_CYCLES  8
#define CPU_DIV_MACHINE_CYCLES  16

enum CPU_STATUS_FLAGS
{
    CPU_STATUS_FLAG_C,
    CPU_STATUS_FLAG_Z,
    CPU_STATUS_FLAG_I,
    CPU_STATUS_FLAG_D,
    CPU_STATUS_FLAG_X,
    CPU_STATUS_FLAG_M,
    CPU_STATUS_FLAG_V,
    CPU_STATUS_FLAG_N,
    CPU_STATUS_FLAG_E,
    CPU_STATUS_FLAG_B,
    /* extra flag set when address computations cross a bank */
    CPU_STATUS_FLAG_BANK,
    /* extra flag set when address computations cross a page */
    CPU_STATUS_FLAG_PAGE,
    /* extra flag set when reg D LSB is not zero */
    CPU_STATUS_FLAG_DL,
    /* extra flag set when the accumulator is 0xffff (used for MVN and MVP instructions) */
    CPU_STATUS_FLAG_AM,
    CPU_STATUS_FLAG_LAST,
};

enum CPU_INTS
{
    CPU_INT_BRK = 0,
    CPU_INT_IRQ,
    CPU_INT_NMI,
    CPU_INT_COP,
    CPU_INT_RES,
    CPU_INT_LAST
};

// enum CPU_PINS
// {
//     CPU_PIN_RDY,
//     CPU_PIN_IRQ,
//     CPU_PIN_NMI,
//     CPU_PIN_LAST,
// };

struct cpu_state_t
{
    // uint8_t pins[CPU_PIN_LAST];

    uint8_t nmi0;
    uint8_t nmi1;
    uint8_t irq;
    uint8_t wai;
    uint8_t stp;
    uint8_t res;
    uint8_t rdy;
    uint8_t cur_interrupt;
    /* interrupts waiting to be serviced */
    uint8_t interrupts[CPU_INT_LAST];

    union
    {
        struct
        {
            /* carry */
            uint8_t c;
            /* zero */
            uint8_t z;
            /* irq disable */
            uint8_t i;
            /* decimal */
            uint8_t d;
            /* index */
            uint8_t x;
            /* memory */
            uint8_t m;
            /* overflow */
            uint8_t v;
            /* negative */
            uint8_t n;
            /* emulation */
            uint8_t e;
            /* break */
            uint8_t b;
            /* set when address computations cross a bank */
            uint8_t bank;
            /* set when address computations cross a page */
            uint8_t page;
            /* set when the LSB of D register is not zero */
            uint8_t dl;
            /* set when the accumulator is 0xffff */
            uint8_t am;
        };

        uint8_t     flags[CPU_STATUS_FLAG_LAST];
    }reg_p;

    struct uop_t *      uop;
    uint32_t            last_uop;
    uint32_t            uop_index;
    int32_t             uop_cycles;
    struct inst_t *     instruction;
    int32_t             instruction_cycles;
    uint32_t            instruction_address;

    uint32_t            shifter;
    uint32_t            run_mul;
    int32_t             mul_cycles;

    uint32_t            run_div;
    int32_t             div_cycles;
    uint16_t            latched_dividend;

    struct reg_t        regs[CPU_REG_LAST];
};

#define CPU_MASTER_CYCLES 6

struct disasm_state_t
{
    uint16_t    reg_p;
    uint16_t    reg_pc;
    uint8_t     reg_pbr;
};

// enum CPU_SIGNALS
// {
//     CPU_SIGNAL_RDY = 0,
//     CPU_SIGNAL_IRQ,
//     CPU_SIGNAL_NMI,
// };


// char *instruction_str(uint32_t effective_address);

// char *instruction_str2(uint32_t effective_address);

int dump_cpu(int show_registers);

int view_hardware_registers();

uint32_t cpu_mem_cycles(uint32_t effective_address);

void cpu_write_byte(uint32_t effective_address, uint8_t data);

void cpu_write_word(uint32_t effective_address, uint16_t data);

uint8_t cpu_read_byte(uint32_t effective_address);

uint16_t cpu_read_word(uint32_t effective_address);

uint32_t cpu_read_wordbyte(uint32_t effective_address);

uint16_t alu(uint32_t operand0, uint32_t operand1, uint32_t op, uint32_t width);

uint32_t check_int();

void reset_cpu();

void reset_core();

void assert_nmi(uint8_t bit);

void deassert_nmi(uint8_t bit);

void assert_irq(uint8_t bit);

void deassert_irq(uint8_t bit);

void assert_rdy(uint8_t bit);

void deassert_rdy(uint8_t bit);

// void assert_pin(uint32_t pin);

// void deassert_pin(uint32_t pin);

// void set_signal(uint32_t signal, uint8_t value);

// uint8_t get_signal(uint32_t signal);

void load_instruction();

void load_uop();

void next_uop();

uint32_t step_cpu(int32_t *cycle_count);

void init_disasm(struct disasm_state_t *disasm_state, struct cpu_state_t *cpu_state);

uint32_t disasm(struct disasm_state_t *disasm_state, struct disasm_inst_t *instruction);

uint8_t io_read(uint32_t effective_address);

void io_write(uint32_t effective_address, uint8_t value);

void nmitimen_write(uint32_t effective_address, uint8_t value);

uint8_t timeup_read(uint32_t effective_address);

uint8_t rdnmi_read(uint32_t effective_address);

uint8_t hvbjoy_read(uint32_t effective_address);

void wrmpyb_write(uint32_t effective_address, uint8_t value);

void wrdivb_write(uint32_t effective_address, uint8_t value);

// void step_cpu(int32_t cycle_count);



/*********************** cpu uops ***********************/

// #define UOP(fn, arg) ((struct uop_t){(fn), (arg)})

// uint32_t inc_pc(uint32_t arg);
// /* increment PC register by [inc] */
// #define INC_PC(inc) UOP(inc_pc, inc)

// uint32_t decode(uint32_t arg);
// /* decode instruction at PBR:PC */
// #define DECODE UOP(decode, 0)

// enum MOV_BYTES
// {
//     MOV_LSB = 0,
//     MOV_MSB
// };

// uint32_t mov_lpc(uint32_t arg);
// /* load [dst_reg] low/high byte ([byte_index]) with the value at PBR:PC, and then increment PC */
// #define MOV_LPC(byte_index, dst_reg) UOP(mov_lpc, ((uint32_t)dst_reg << 24) | ((uint32_t)byte_index))

// uint32_t mov_l(uint32_t arg);
// /* load [dst_reg] low/high byte ([byte_index]) from [bank_reg]:[addr_reg] */
// #define MOV_L(byte_index, addr_reg, bank_reg, dst_reg) UOP(mov_l, (((uint32_t)dst_reg << 24)) | ((uint32_t)bank_reg << 16) | ((uint32_t)addr_reg << 8) | ((uint32_t)byte_index))

// uint32_t mov_s(uint32_t arg);
// /* store [dst_reg] low/high byte ([byte_index]) at [bank_reg]:[addr_reg] */
// #define MOV_S(byte_index, addr_reg, bank_reg, src_reg) UOP(mov_s, ((uint32_t)src_reg << 24) | ((uint32_t)bank_reg << 16) | ((uint32_t)addr_reg << 8) | ((uint32_t)byte_index))

// enum MOV_RRW_WIDTH
// {
//     MOV_RRW_BYTE = 1,
//     MOV_RRW_WORD = 0,
// };

// uint32_t mov_rrw(uint32_t arg);
// /* mov [src] to [dst], explicit [width] */
// #define MOV_RRW(src, dst, width) UOP(mov_rrw, ((uint32_t)(width) << 16) | ((uint32_t)(src) << 8) | ((uint32_t)(dst)))

// uint32_t mov_rr(uint32_t arg);
// /* mov [src] to [dst], implicit width based on MX flags */
// #define MOV_RR(src, dst) UOP(mov_rr, ((uint32_t)(src) << 8) | ((uint32_t)(dst)))

// uint32_t mov_p(uint32_t arg);
// /* load/store the P register */
// #define MOV_P(src, dst) UOP(mov_p, ((uint32_t)(src) << 8) | ((uint32_t)(dst)))

// uint32_t decs(uint32_t arg);
// /* decrement the S register */
// #define DECS UOP(decs, 0)

// uint32_t dec_rw(uint32_t arg);
// /* decrement word register [reg] */
// #define DEC_RW(reg) UOP(dec_rw, reg)

// uint32_t dec_rb(uint32_t arg);
// /* decrement byte register [reg] */
// #define DEC_RB(reg) UOP(dec_rb, reg)

// uint32_t incs(uint32_t arg);
// /* increment the S register */
// #define INCS UOP(incs, 0)

// uint32_t inc_rw(uint32_t arg);
// /* increment word register [reg] */
// #define INC_RW(reg) UOP(inc_rw, reg)

// uint32_t inc_rb(uint32_t arg);
// /* increment byte register [reg] */
// #define INC_RB(reg) UOP(inc_rb, reg)

// uint32_t zext(uint32_t arg);
// /* zero extend byte register [reg] */
// #define ZEXT(reg) UOP(zext, reg)

// uint32_t sext(uint32_t arg);
// /* sign extend byte register [reg] */
// #define SEXT(reg) UOP(sext, reg)

// uint32_t set_p(uint32_t arg);
// /* set flags [flag] */
// #define SET_P(flag) UOP(set_p, flag)

// uint32_t clr_p(uint32_t arg);
// /* clear flags [flag] */
// #define CLR_P(flag) UOP(clr_p, flag)

// uint32_t xce(uint32_t arg);
// /* XCE */
// #define XCE UOP(xce, 0)

// uint32_t xba(uint32_t arg);
// /* XBA */
// #define XBA UOP(xba, 0)

// uint32_t wai(uint32_t);
// /* WAI */
// #define WAI UOP(wai, 0)

// uint32_t io(uint32_t arg);
// /* arbitrary internal operation (idles the cpu) */
// #define IO UOP(io, 0)


// enum ADDR_OFF_BANK
// {
//     ADDR_OFF_BANK_NEXT = 1,
//     ADDR_OFF_BANK_WRAP = 0,
// };

// enum ADDR_OFF_SIGN
// {
//     ADDR_OFF_SIGNED = 1,
//     ADDR_OFF_UNSIGNED = 0
// };

// uint32_t addr_offr(uint32_t arg);
// /*  offset ADDR register by word register [addr_reg], with explicit
// offset sign ([offset_sign]) with or without bank wrapping ([bank_wrap]) */
// #define ADDR_OFFRS(addr_reg, bank_wrap, offset_sign) UOP(addr_offr, ((uint32_t)offset_sign << 16) | ((uint32_t)bank_wrap << 8) | ((uint32_t)addr_reg))
// /* unsigned offset ADDR register by word register [addr_reg], with or without bank wrapping ([bank_wrap])*/
// #define ADDR_OFFR(addr_reg, bank_wrap) ADDR_OFFRS(addr_reg, bank_wrap, ADDR_OFF_UNSIGNED)

// uint32_t addr_offi(uint32_t arg);
// /*  offset ADDR register by word constant [offset], with explicit
// offset sign ([offset_sign]) with or without bank wrapping ([bank_wrap]) */
// #define ADDR_OFFIS(offset, bank_wrap, offset_sign) UOP(addr_offi, ((uint32_t)offset_sign << 24) | ((uint32_t)bank_wrap << 16) | ((uint32_t)offset))
// /* unsigned offset ADDR register by word constant [offset], with or without bank wrapping ([bank_wrap])*/
// #define ADDR_OFFI(offset, bank_wrap) ADDR_OFFIS(offset, bank_wrap, ADDR_OFF_UNSIGNED)

// uint32_t skips(uint32_t arg);
// /* skip [count] uops if [flag] is set */
// #define SKIPS(count, flag) UOP(skips, ((uint32_t)flag << 8) | ((uint32_t)count))

// uint32_t skipc(uint32_t arg);
// /* skip [count] uops if [flag] is clear */
// #define SKIPC(count, flag) UOP(skipc, ((uint32_t)flag << 8) | ((uint32_t)count))

// enum CHK_ZW_WIDTH
// {
//     CHK_ZW_BYTE = 1,
//     CHK_ZW_WORD = 0
// };

// uint32_t chk_znw(uint32_t arg);
// /* test register [reg] and set Z and N flags, with explicit width [width]  */
// #define CHK_ZNW(reg, width) UOP(chk_znw, ((uint32_t)width << 8) | ((uint32_t)reg))

// uint32_t chk_zn(uint32_t arg);
// /* test register [reg] and set Z and N flags, with implicit width based on MX flags  */
// #define CHK_ZN(reg) UOP(chk_zn, reg)

// uint32_t alu_op(uint32_t arg);
// /* perform alu op [op], with width [width], between register operands [operand_a] and [operand_b]*/
// #define ALU_OP(op, width_flag, operand_a, operand_b) UOP(alu_op, ((uint32_t)operand_b << 24) | ((uint32_t)operand_a << 16) | ((uint32_t)op << 8) | ((uint32_t)width_flag))

// uint32_t brk(uint32_t arg);
// /* BRK */
// #define BRK UOP(brk, 0)

// uint32_t cop(uint32_t arg);
// /* COP */
// #define COP UOP(cop, 0)

// uint32_t stp(uint32_t arg);
// /* STP */
// #define STP UOP(stp, 0)

// /* hangs the cpu when encountering an unknown instruction */
// uint32_t unimplemented(uint32_t arg);
// #define UNIMPLEMENTED UOP(unimplemented, 0);



#endif // CPU_H






