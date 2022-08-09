#ifndef CPU_H
#define CPU_H

#include "../addr.h"
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
    ALU_OP_LAST
};

enum OPCODES
{
    OPCODE_BRK = 0,                        /* Force break */
    OPCODE_BIT,                            /* Bit test */

    /* ====================================== */
    /* DO NOT CHANGE THE ORDER OF THOSE ENUMS */


    /* instead of having a separate switch case for each branch instruction a list of
    branches is used. Here the instructions are paired with their "opposite" instructions.
    For example, BCC (branch if carry is clear) comes before BCS (branch if carry is set).
    This order is intentional. A branch that is taken if the respective flag is taken has its
    LSB set to zero, while its "opposite" instruction has its LSB set to one. Shifting both
    values right by one then yields the same value, which is used to index the condition table.
    The LSB then is used to decide whether the branch should be taken or not. */

    OPCODE_BCC,                            /* Branch on carry clear (C == 0) */
    OPCODE_BCS,                            /* Branch on carry set (C == 1) */

    OPCODE_BNE,                            /* Branch if not equal (Z == 0) */
    OPCODE_BEQ,                            /* Branch if equal (Z == 1) */

    OPCODE_BPL,                            /* Branch if result plus (N == 0) */
    OPCODE_BMI,                            /* Branch if result minus (N == 1) */

    OPCODE_BVC,                            /* Branch on overflow clear (V == 0) */
    OPCODE_BVS,                            /* Branch on overflow set (V == 1) */

    OPCODE_BRA,                            /* Branch always */
    OPCODE_BRL,                            /* Branch always long */

    /* ====================================== */
    /* ====================================== */


    OPCODE_CLC,                            /* Clear carry flag */
    OPCODE_CLD,                            /* Clear decimal mode */
    OPCODE_CLV,                            /* Clear overflow flag */
    OPCODE_CLI,                            /* Clear interrupt disable bit */


    OPCODE_CMP,                            /* Compare memory and accumulator */
    OPCODE_CPX,                            /* Compare memory and index X */
    OPCODE_CPY,                            /* Compare memory and index Y */


    /* ====================================== */
    /* DO NOT CHANGE THE ORDER OF THOSE ENUMS */

    /* bit 0 of those constants are used to index into an array containing "opposite"
    alu operations, so their order must be preserved */

    OPCODE_ADC,                            /* Add memory to accumulator with carry */
    OPCODE_AND,                            /* AND memory with accumulator */
    OPCODE_SBC,                            /* Subtract memory from accumulator with borrow */
    OPCODE_EOR,                            /* Exclusive OR memory with accumulator */
    OPCODE_ORA,                            /* OR memory with accumulator */

    OPCODE_ROL,                            /* Rotate left one bit (memory or accumulator) */
    OPCODE_ROR,                            /* Rotate right one bit (memory or accumulator) */
    OPCODE_DEC,                            /* Decrement memory or accumulator */
    OPCODE_INC,                            /* Increment memory or accumulator */
    OPCODE_ASL,                            /* Shift one bit left, memory or accumulator */
    OPCODE_LSR,                            /* Shift one bit right, memory or accumulator */

    OPCODE_DEX,                            /* Decrement index X */
    OPCODE_INX,                            /* Increment index X */
    OPCODE_DEY,                            /* Decrement index Y */
    OPCODE_INY,                            /* Increment index Y */

    OPCODE_TRB,                            /* Test and reset bit */
    OPCODE_TSB,                            /* Test and set bit */

    /* ====================================== */
    /* ====================================== */



    OPCODE_JMP,                            /* Jump */
    OPCODE_JML,                            /* Jump long */
    OPCODE_JSL,                            /* Jump subroutine long */
    OPCODE_JSR,                            /* Jump and save return */


    /* ====================================== */
    /* DO NOT CHANGE THE ORDER OF THOSE ENUMS */
    OPCODE_LDA,                            /* Load accumulator with memory */
    OPCODE_LDX,                            /* Load index X with memory */
    OPCODE_LDY,                            /* Load index Y with memory */

    OPCODE_STA,                            /* Store accumulator in memory */
    OPCODE_STX,                            /* Store index X in memory */
    OPCODE_STY,                            /* Store index Y in memory */
    OPCODE_STZ,                            /* Store zero in memory */
    /* ====================================== */
    /* ====================================== */



    OPCODE_MVN,                            /* Block move negative */
    OPCODE_MVP,                            /* Block move positive */
    OPCODE_NOP,                            /* No operation */


    /* ====================================== */
    /* DO NOT CHANGE THE ORDER OF THOSE ENUMS */
    OPCODE_PEA,                            /* Push effective absolute address on stack (or push immediate data on stack) */
    OPCODE_PEI,                            /* Push effective absolute address on stack (or push direct data on stack) */
    OPCODE_PER,                            /* Push effective program counter relative address on stack */
    OPCODE_PHA,                            /* Push accumulator on stack */
    OPCODE_PHB,                            /* Push data bank register on stack */
    OPCODE_PHD,                            /* Push direct register on stack */
    OPCODE_PHK,                            /* Push program bank register on stack */
    OPCODE_PHP,                            /* Push processor status on stack */
    OPCODE_PHX,                            /* Push index X on stack */
    OPCODE_PHY,                            /* Push index Y on stack */

    OPCODE_PLA,                            /* Pull accumulator from stack */
    OPCODE_PLB,                            /* Pull data bank register from stack */
    OPCODE_PLD,                            /* Pull direct register from stack */
    OPCODE_PLP,                            /* Pull processor status from stack */
    OPCODE_PLX,                            /* Pull index X from stack */
    OPCODE_PLY,                            /* Pull index Y from stack */
    /* ====================================== */
    /* ====================================== */


    OPCODE_REP,                            /* Reset status bits */


    OPCODE_RTI,                            /* Return from interrupt */
    OPCODE_RTL,                            /* Return from subroutine long */
    OPCODE_RTS,                            /* Return from subroutine */


//    OPCODE_SBC,                            /* Subtract memory from accumulator with borrow */


    OPCODE_SEP,                            /* Set processor status bit */
    OPCODE_SEC,                            /* Set carry flag */
    OPCODE_SED,                            /* Set decimal mode */
    OPCODE_SEI,                            /* Set interrupt disable status */


    /* ====================================== */
    /* DO NOT CHANGE THE ORDER OF THOSE ENUMS */

    OPCODE_TAX,                            /* Transfer accumulator to index X */
    OPCODE_TAY,                            /* Transfer accumulator to index Y */
    OPCODE_TCD,                            /* Transfer C accumulator to direct register */
    OPCODE_TCS,                            /* Transfer C accumulator to stack pointer */
    OPCODE_TDC,                            /* Transfer direct register to C accumulator */
    OPCODE_TSC,                            /* Transfer stack pointer to C accumulator */
    OPCODE_TSX,                            /* Transfer stack pointer to index X */
    OPCODE_TXA,                            /* Transfer index X to accumulator */
    OPCODE_TXS,                            /* Transfer index X to stack pointer */
    OPCODE_TXY,                            /* Transfer index X to index Y */
    OPCODE_TYA,                            /* Transfer index Y to accumulator */
    OPCODE_TYX,                            /* Transfer index Y to index X */

    /* ====================================== */
    /* ====================================== */


    OPCODE_WAI,                            /* Wait for interrupt */


    OPCODE_WDM,                            /* Reserved */


    OPCODE_XBA,                            /* Exchange B and A accumulator??? */



    OPCODE_STP,                            /* Stop the clock */


    OPCODE_COP,                            /* Coprocessor */


    OPCODE_XCE,                            /* Exchange carry and emulation bits */

    OPCODE_UNKNOWN,
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

enum INSTRUCTIONS
{
    ADC_ABS             = 0x6d,
    ADC_ABS_X           = 0x7d,
    ADC_ABS_Y           = 0x79,
    ADC_ABSL            = 0x6f,
    ADC_ABSL_X          = 0x7f,
    ADC_DIR             = 0x65,
    ADC_S_REL           = 0x63,
    ADC_DIR_X           = 0x75,
    ADC_DIR_IND         = 0x72,
    ADC_DIR_INDL        = 0x67,
    ADC_S_REL_IND_Y     = 0x73,
    ADC_DIR_X_IND       = 0x61,
    ADC_DIR_IND_Y       = 0x71,
    ADC_DIR_INDL_Y      = 0x77,
    ADC_IMM             = 0x69,

    AND_ABS             = 0x2d,
    AND_ABS_X           = 0x3d,
    AND_ABS_Y           = 0x39,
    AND_ABSL            = 0x2f,
    AND_ABSL_X          = 0x3f,
    AND_DIR             = 0x25,
    AND_S_REL           = 0x23,
    AND_DIR_X           = 0x35,
    AND_DIR_IND         = 0x32,
    AND_DIR_INDL        = 0x27,
    AND_S_REL_IND_Y     = 0x33,
    AND_DIR_X_IND       = 0x21,
    AND_DIR_IND_Y       = 0x31,
    AND_DIR_INDL_Y      = 0x37,
    AND_IMM             = 0x29,

    ASL_ABS             = 0x0e,
    ASL_ACC             = 0x0a,
    ASL_ABS_X           = 0x1e,
    ASL_DIR             = 0x06,
    ASL_DIR_X           = 0X16,

    BCC_PC_REL          = 0x90,

    BCS_PC_REL          = 0xb0,

    BEQ_PC_REL          = 0xf0,

    BIT_ABS             = 0x2c,
    BIT_ABS_X           = 0x3c,
    BIT_DIR             = 0x24,
    BIT_DIR_X           = 0x34,
    BIT_IMM             = 0x89,

    BMI_PC_REL          = 0x30,

    BNE_PC_REL          = 0xd0,

    BPL_PC_REL          = 0x10,

    BRA_PC_REL          = 0x80,

    BRK_S               = 0x00,

    BRL_PC_RELL         = 0x82,

    BVC_PC_REL          = 0x50,

    BVS_PC_REL          = 0x70,

    CLC_IMP             = 0x18,

    CLD_IMP             = 0xd8,

    CLI_IMP             = 0x58,

    CLV_IMP             = 0xb8,

    CMP_ABS             = 0xcd,
    CMP_ABS_X           = 0xdd,
    CMP_ABS_Y           = 0xd9,
    CMP_ABSL            = 0xcf,
    CMP_ABSL_X          = 0xdf,
    CMP_DIR             = 0xc5,
    CMP_S_REL           = 0xc3,
    CMP_DIR_X           = 0xd5,
    CMP_DIR_IND         = 0xd2,
    CMP_DIR_INDL        = 0xc7,
    CMP_S_REL_IND_Y     = 0xd3,
    CMP_DIR_X_IND       = 0xc1,
    CMP_DIR_IND_Y       = 0xd1,
    CMP_DIR_INDL_Y      = 0xd7,
    CMP_IMM             = 0xc9,

    CPX_ABS             = 0xec,
    CPX_DIR             = 0xe4,
    CPX_IMM             = 0xe0,

    CPY_ABS             = 0xcc,
    CPY_DIR             = 0xc4,
    CPY_IMM             = 0xc0,

    DEC_ABS             = 0xce,
    DEC_ACC             = 0x3a,
    DEC_ABS_X           = 0xde,
    DEC_DIR             = 0xc6,
    DEC_DIR_X           = 0xd6,

    DEX_IMP             = 0xca,

    DEY_IMP             = 0x88,

    EOR_ABS             = 0x4d,
    EOR_ABS_X           = 0x5d,
    EOR_ABS_Y           = 0x59,
    EOR_ABSL            = 0x4f,
    EOR_ABSL_X          = 0x5f,
    EOR_DIR             = 0x45,
    EOR_S_REL           = 0x43,
    EOR_DIR_X           = 0x55,
    EOR_DIR_IND         = 0x52,
    EOR_DIR_INDL        = 0x47,
    EOR_S_REL_IND_Y     = 0x53,
    EOR_DIR_X_IND       = 0x41,
    EOR_DIR_IND_Y       = 0x51,
    EOR_DIR_INDL_Y      = 0x57,
    EOR_IMM             = 0x49,

    INC_ABS             = 0xee,
    INC_ACC             = 0x1a,
    INC_ABS_X           = 0xfe,
    INC_DIR             = 0xe6,
    INC_DIR_X           = 0xf6,

    INX_IMP             = 0xe8,

    INY_IMP             = 0xc8,

    JML_ABS_IND         = 0xdc,

    JMP_ABS             = 0x4c,
    JMP_ABSL            = 0x5c,
    JMP_ABS_IND         = 0x6c,
    JMP_ABS_X_IND       = 0x7c,

    JSL_ABSL            = 0x22,

    JSR_ABS             = 0x20,
    JSR_ABS_X_IND       = 0xfc,

    LDA_ABS             = 0xad,
    LDA_ABS_X           = 0xbd,
    LDA_ABS_Y           = 0xb9,
    LDA_ABSL            = 0xaf,
    LDA_ABSL_X          = 0xbf,
    LDA_DIR             = 0xa5,
    LDA_S_REL           = 0xa3,
    LDA_DIR_X           = 0xb5,
    LDA_DIR_IND         = 0xb2,
    LDA_DIR_INDL        = 0xa7,
    LDA_S_REL_IND_Y     = 0xb3,
    LDA_DIR_X_IND       = 0xa1,
    LDA_DIR_IND_Y       = 0xb1,
    LDA_DIR_INDL_Y      = 0xb7,
    LDA_IMM             = 0xa9,

    LDX_ABS             = 0xae,
    LDX_ABS_Y           = 0xbe,
    LDX_DIR             = 0xa6,
    LDX_DIR_Y           = 0xb6,
    LDX_IMM             = 0xa2,

    LDY_ABS             = 0xac,
    LDY_ABS_X           = 0xbc,
    LDY_DIR             = 0xa4,
    LDY_DIR_X           = 0xb4,
    LDY_IMM             = 0xa0,

    LSR_ABS             = 0x4e,
    LSR_ACC             = 0x4a,
    LSR_ABS_X           = 0x5e,
    LSR_DIR             = 0x46,
    LSR_DIR_X           = 0x56,

    MVN_BLK             = 0x54,

    MVP_BLK             = 0x44,

    NOP_IMP             = 0xea,

    ORA_ABS             = 0x0d,
    ORA_ABS_X           = 0x1d,
    ORA_ABS_Y           = 0x19,
    ORA_ABSL            = 0x0f,
    ORA_ABSL_X          = 0x1f,
    ORA_DIR             = 0x05,
    ORA_S_REL           = 0x03,
    ORA_DIR_X           = 0x15,
    ORA_DIR_IND         = 0x12,
    ORA_DIR_INDL        = 0x07,
    ORA_S_REL_IND_Y     = 0x13,
    ORA_DIR_X_IND       = 0x01,
    ORA_DIR_IND_Y       = 0x11,
    ORA_DIR_INDL_Y      = 0x17,
    ORA_IMM             = 0x09,

    PEA_S               = 0xf4,

    PEI_S               = 0xd4,

    PER_S               = 0x62,

    PHA_S               = 0x48,

    PHB_S               = 0x8b,

    PHD_S               = 0x0b,

    PHK_S               = 0x4b,

    PHP_S               = 0x08,

    PHX_S               = 0xda,

    PHY_S               = 0x5a,

    PLA_S               = 0x68,

    PLB_S               = 0xab,

    PLD_S               = 0x2b,

    PLP_S               = 0x28,

    PLX_S               = 0xfa,

    PLY_S               = 0x7a,

    REP_IMM             = 0xc2,

    ROL_ABS             = 0x2e,
    ROL_ACC             = 0x2a,
    ROL_ABS_X           = 0x3e,
    ROL_DIR             = 0x26,
    ROL_DIR_X           = 0x36,

    ROR_ABS             = 0x6e,
    ROR_ACC             = 0x6a,
    ROR_ABS_X           = 0x7e,
    ROR_DIR             = 0x66,
    ROR_DIR_X           = 0x76,

    RTI_S               = 0x40,

    RTL_S               = 0x6b,

    RTS_S               = 0x60,

    SBC_ABS             = 0xed,
    SBC_ABS_X           = 0xfd,
    SBC_ABS_Y           = 0xf9,
    SBC_ABSL_X          = 0xff,
    SBC_DIR             = 0xe5,
    SBC_S_REL           = 0xe3,
    SBC_DIR_X           = 0xf5,
    SBC_DIR_IND         = 0xf2,
    SBC_DIR_INDL        = 0xe7,
    SBC_S_REL_IND_Y     = 0xf3,
    SBC_DIR_X_IND       = 0xe1,
    SBC_DIR_IND_Y       = 0xf1,
    SBC_DIR_INDL_Y      = 0xf7,
    SBC_IMM             = 0xe9,

    SEC_IMP             = 0x38,

    SED_IMP             = 0xf8,

    SEI_IMP             = 0x78,

    SEP_IMM             = 0xe2,

    STA_ABS             = 0x8d,
    STA_ABS_X           = 0x9d,
    STA_ABSL            = 0x8f,
    STA_ABSL_X          = 0x9f,
    STA_DIR             = 0x85,
    STA_S_REL           = 0x83,
    STA_DIR_X           = 0x95,
    STA_DIR_IND         = 0x92,
    STA_DIR_INDL        = 0x87,
    STA_S_REL_IND_Y     = 0x93,
    STA_DIR_X_IND       = 0x81,
    STA_DIR_IND_Y       = 0x91,
    STA_DIR_INDL_Y      = 0x97,

    STX_ABS             = 0x8e,
    STX_DIR             = 0x86,
    STX_DIR_Y           = 0x96,

    STY_ABS             = 0x8c,
    STY_DIR             = 0x84,
    STY_DIR_X           = 0x94,

    STZ_ABS             = 0x9c,
    STZ_ABS_X           = 0x9e,
    STZ_DIR             = 0x64,
    STZ_DIR_X           = 0x74,

    TAX_IMP             = 0xaa,

    TAY_IMP             = 0xa8,

    TCD_IMP             = 0x5b,

    TCS_IMP             = 0x1b,

    TDC_IMP             = 0x7b,

    TRB_DIR             = 0x14,

    TSB_DIR             = 0x04,

    TSC_ACC             = 0x3b,

    TSX_ACC             = 0xba,

    TXA_ACC             = 0x8a,

    TXS_ACC             = 0x9a,

    TXY_ACC             = 0x9b,

    TYA_ACC             = 0x98,

    TYX_ACC             = 0xbb,

    WAI_ACC             = 0xcb,

    WDM_ACC             = 0x42,

    XBA_ACC             = 0xeb,

    XCE_ACC             = 0xfb,

    FETCH,
};

enum CPU_REGS
{
    CPU_REG_JOYWR                           = 0x4016,
    CPU_REG_JOYA                            = 0x4016,
    CPU_REG_JOYB                            = 0x4017,
    CPU_REG_NMITIMEN                        = 0x4200,
    CPU_REG_WRIO                            = 0x4201,
    CPU_REG_WRMPYA                          = 0x4202,
    CPU_REG_WRMPYB                          = 0x4203,
    CPU_REG_WRDIVL                          = 0x4204,
    CPU_REG_WRDIVH                          = 0x4205,
    CPU_REG_WRDIVB                          = 0x4206,
    CPU_REG_HTIMEL                          = 0x4207,
    CPU_REG_HTIMEH                          = 0x4208,
    CPU_REG_VTIMEL                          = 0x4209,
    CPU_REG_VTIMEH                          = 0x420a,
    CPU_REG_MDMAEN                          = 0x420b,

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

    CPU_REG_HDMAEN                          = 0x420c,
    CPU_REG_MEMSEL                          = 0x420d,
    CPU_REG_RDNMI                           = 0x4210,
    CPU_REG_TIMEUP                          = 0x4211,
    CPU_REG_HVBJOY                          = 0x4212,
    CPU_REG_RDIO                            = 0x4213,
    CPU_REG_RDDIVL                          = 0x4214,
    CPU_REG_RDDIVH                          = 0x4215,
    CPU_REG_RDMPYL                          = 0x4216,
    CPU_REG_RDMPYH                          = 0x4217,
    CPU_REG_STDCTRL1L                       = 0x4218,
    CPU_REG_STDCTRL1H                       = 0x4219,
    CPU_REG_STDCTRL2L                       = 0x421a,
    CPU_REG_STDCTRL2H                       = 0x421b,
    CPU_REG_STDCTRL3L                       = 0x421c,
    CPU_REG_STDCTRL3H                       = 0x421d,
    CPU_REG_STDCTRL4L                       = 0x421e,
    CPU_REG_STDCTRL4H                       = 0x421f,

    CPU_REG_DMA0_PARAM                      = 0x4300,
    CPU_REG_DMA0_BBUS_ADDR                  = 0x4301,

    CPU_REG_HDMA0_ATAB_DMA0_ADDRL           = 0x4302,
    CPU_REG_HDMA0_ATAB_DMA0_ADDRH           = 0x4303,
    CPU_REG_HDMA0_ATAB_DMA0_BANK            = 0x4304,

    /* not really mentioned anywhere, but I guess those should be incremented (except the bank) after
    every byte written, just like happens with <43X8> and <43X9>...? */
    CPU_REG_HDMA0_IND_ADDR_DMA0_COUNTL      = 0x4305,
    CPU_REG_HDMA0_IND_ADDR_DMA0_COUNTH      = 0x4306,
    CPU_REG_HDMA0_IND_ADDR_BANK             = 0x4307,

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

    CPU_REG_HDMA0_DIR_ADDRL                 = 0x4308,
    CPU_REG_HDMA0_DIR_ADDRH                 = 0x4309,


    CPU_REG_HDMA0_CUR_LINE_COUNT            = 0x430a,
};

enum CPU_NMITIMEN_FLAGS
{
    CPU_NMITIMEN_FLAG_STD_CTRL_EN   = 1,
    CPU_NMITIMEN_FLAG_HTIMER_EN     = 1 << 4,
    CPU_NMITIMEN_FLAG_VTIMER_EN     = 1 << 5,
    CPU_NMITIMEN_FLAG_NMI_ENABLE    = 1 << 7,
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

enum REGS
{
    REG_ACCUM,
    REG_X,
    REG_Y,
    REG_D,
    REG_S,
    REG_PBR,
    REG_DBR,
    REG_INST,
    REG_PC,
    REG_P,
    REG_TEMP,
    REG_ADDR,
    REG_BANK,
    REG_ZERO,
    REG_LAST,
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

typedef uint32_t (*uop_func_t)(uint32_t arg);

struct uop_t
{
    uop_func_t      func;
    uint32_t        arg;
};

struct inst_t
{
    struct uop_t    uops[24];
};

#define MEM_SPEED_FAST_CYCLES   6
#define MEM_SPEED_MED_CYCLES    8
#define MEM_SPEED_SLOW_CYCLES   12

enum STATUS_FLAGS
{
    STATUS_FLAG_C,
    STATUS_FLAG_Z,
    STATUS_FLAG_I,
    STATUS_FLAG_D,
    STATUS_FLAG_X,
    STATUS_FLAG_M,
    STATUS_FLAG_V,
    STATUS_FLAG_N,
    STATUS_FLAG_E,
    STATUS_FLAG_B,
    /* extra flag set when address computations cross a bank */
    STATUS_FLAG_BANK,
    /* extra flag set when address computations cross a page */
    STATUS_FLAG_PAGE,
    /* extra flag set when reg D LSB is not zero */
    STATUS_FLAG_DL,
    STATUS_FLAG_AZ,
    STATUS_FLAG_LAST,
};

enum CPU_INTS
{
    CPU_INT_BRK = 0,
    CPU_INT_IRQ,
    CPU_INT_NMI,
    CPU_INT_LAST
};

enum CPU_PINS
{
    CPU_PIN_RDY,
    CPU_PIN_IRQ,
    CPU_PIN_NMI,
    CPU_PIN_LAST,
};

struct cpu_state_t
{
    uint8_t pins[CPU_PIN_LAST];
    uint8_t interrupts[CPU_INT_LAST];
//    uint8_t irq;
    uint8_t wai;
//    uint8_t nmi;
    uint8_t cur_interrupt;

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
        };

        uint8_t     flags[STATUS_FLAG_LAST];
    }reg_p;

    struct uop_t *      uop;
    uint32_t            last_uop;
    uint32_t            uop_index;
    int32_t             uop_cycles;
    struct inst_t *     instruction;
    int32_t             instruction_cycles;
    uint32_t            instruction_address;

    struct reg_t        regs[REG_LAST];
};

enum CPU_HVBJOY_FLAGS
{
    CPU_HVBJOY_FLAG_VBLANK = 1 << 7,
    CPU_HVBJOY_FLAG_HBLANK = 1 << 6,
};

#define CPU_MASTER_CYCLES 6

// enum CPU_SIGNALS
// {
//     CPU_SIGNAL_RDY = 0,
//     CPU_SIGNAL_IRQ,
//     CPU_SIGNAL_NMI,
// };


char *instruction_str(uint32_t effective_address);

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

void assert_pin(uint32_t pin);

void deassert_pin(uint32_t pin);

// void set_signal(uint32_t signal, uint8_t value);

// uint8_t get_signal(uint32_t signal);

void load_instruction();

void load_uop();

void next_uop();

void step_cpu(int32_t cycle_count);

uint8_t io_read(uint32_t effective_address);

void io_write(uint32_t effective_address, uint8_t value);

void nmitimen_write(uint32_t effective_address, uint8_t value);

uint8_t timeup_read(uint32_t effective_address);

// void step_cpu(int32_t cycle_count);




#endif // CPU_H






