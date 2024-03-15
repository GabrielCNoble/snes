#include "apu.h"
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>

// struct apu_io_reg_t io_regs[4];


#define APU_APU_TO_MASTER_CYCLE_RATIO (1024000.0f / 21477270.0f)
#define APU_MASTER_TO_APU_CYCLE_RATIO (21477270.0f / 1024000.0f)

uint32_t apu_alu_zero_masks[] = {
    [0] = 0x000000ff,
    [1] = 0x0000ffff,
};

uint32_t apu_alu_sign_masks[] = {
    [0] = 0x00000080,
    [1] = 0x00008000,
};

uint32_t apu_alu_carry_masks[] = {
    [0] = 0x00000100,        
    [1] = 0x00010000,
};

uint32_t apu_alu_half_carry_masks[] = {
    [0] = 0x00000010,        
    [1] = 0x00001000,
};

uint32_t apu_alu_carry_shifts[] = {
    [0] = 8,
    [1] = 16,
};

// struct apu_port_t           apu_ports[4];
uint32_t                    apu_transfer_state = APU_STATE_IDLE;
uint32_t                    end_transfer_timer = 0;
uint8_t *                   apu_ram;
struct apu_io_reg_funcs_t   apu_io_regs[APU_IO_REG_COUNT] = {
    [APU_IO_REG_TEST]       = { .read = apu_IoTestRead,     .write = apu_IoTestWrite },
    [APU_IO_REG_CONTROL]    = { .read = apu_IoControlRead,  .write = apu_IoControlWrite },
    [APU_IO_REG_PORT0]      = { .read = apu_IoPortRead,     .write = apu_IoPortWrite },
    [APU_IO_REG_PORT1]      = { .read = apu_IoPortRead,     .write = apu_IoPortWrite },
    [APU_IO_REG_PORT2]      = { .read = apu_IoPortRead,     .write = apu_IoPortWrite },
    [APU_IO_REG_PORT3]      = { .read = apu_IoPortRead,     .write = apu_IoPortWrite },
    [APU_IO_REG_AUX04]      = {},
    [APU_IO_REG_AUX05]      = {},
    [APU_IO_REG_T0DIV]      = { .read = apu_IoTimerDivRead, .write = apu_IoTimerDivWrite },
    [APU_IO_REG_T1DIV]      = { .read = apu_IoTimerDivRead, .write = apu_IoTimerDivWrite },
    [APU_IO_REG_T2DIV]      = { .read = apu_IoTimerDivRead, .write = apu_IoTimerDivWrite },
    [APU_IO_REG_T0OUT]      = { .read = apu_IoTimerOutRead, .write = apu_IoTimerOutWrite },
    [APU_IO_REG_T1OUT]      = { .read = apu_IoTimerOutRead, .write = apu_IoTimerOutWrite },
    [APU_IO_REG_T2OUT]      = { .read = apu_IoTimerOutRead, .write = apu_IoTimerOutWrite },
};

struct apu_state_t apu_state;

/* from Anomie's SPC700 doc */
uint8_t apu_ipl_boot_rom[] = {
    0xcd, 0xef, 0xbd, 0xe8, 
    0x00, 0xc6, 0x1d, 0xd0, 
    0xfc, 0x8f, 0xaa, 0xf4, 
    0x8f, 0xbb, 0xf5, 0x78,
    0xcc, 0xf4, 0xd0, 0xfb, 
    0x2f, 0x19, 0xeb, 0xf4, 
    0xd0, 0xfc, 0x7e, 0xf4, 
    0xd0, 0x0b, 0xe4, 0xf5,
    0xcb, 0xf4, 0xd7, 0x00, 
    0xfc, 0xd0, 0xf3, 0xab, 
    0x01, 0x10, 0xef, 0x7e, 
    0xf4, 0x10, 0xeb, 0xba,
    0xf6, 0xda, 0x00, 0xba, 
    0xf4, 0xc4, 0xf4, 0xdd, 
    0x5d, 0xd0, 0xdb, 0x1f, 
    0x00, 0x00, 0xc0, 0xff
};

#define UOP(fn, args) ((struct apu_uop_t){.func = (fn), .arg = (args)})

enum MOV_BYTES
{
    MOV_LSB = 0,
    MOV_MSB
};

uint32_t apu_decode(uint32_t arg);
#define DECODE UOP(apu_decode, 0)

uint32_t apu_off_pc_f(uint32_t arg);
#define OFFSET_PC_F(offset) UOP(apu_off_pc_f, offset)

uint32_t apu_off_pc_r(uint32_t arg);
#define OFFSET_PC_R(offset_reg) UOP(apu_off_pc_r, offset_reg)

uint32_t apu_mov_lpc(uint32_t arg);
#define MOV_LPC(dst_reg, byte) UOP(apu_mov_lpc, ((uint32_t)dst_reg << 24) | ((uint32_t)byte << 16))

uint32_t apu_mov_l(uint32_t arg);
#define MOV_L(dst_reg, byte, addr_reg) UOP(apu_mov_l, ((uint32_t)dst_reg << 24) | ((uint32_t)byte << 16) | ((uint32_t)addr_reg))

uint32_t apu_mov_s(uint32_t arg);
#define MOV_S(src_reg, byte, addr_reg) UOP(apu_mov_s, ((uint32_t)src_reg << 24) | ((uint32_t)byte << 16) | ((uint32_t)addr_reg))

enum MOV_RR_WIDTH
{
    MOV_RR_BYTE = 1,
    MOV_RR_WORD = 0,
};

uint32_t apu_mov_rr(uint32_t args);
#define MOV_RR(dst_reg, src_reg, width) UOP(apu_mov_rr, (dst_reg << 24) | (src_reg << 16) | (width << 8))

uint32_t apu_io(uint32_t arg);
#define IO UOP(apu_io, 0)

uint32_t apu_skips(uint32_t arg);
/* skip [count] uops if [flag] is set */
#define SKIPS(count, flag) UOP(apu_skips, ((uint32_t)flag << 8) | ((uint32_t)count))

uint32_t apu_skipc(uint32_t arg);
/* skip [count] uops if [flag] is clear */
#define SKIPC(count, flag) UOP(apu_skipc, ((uint32_t)flag << 8) | ((uint32_t)count))

uint32_t apu_chk_zn(uint32_t arg);
/* test register [reg] and set Z and N flags */
#define CHK_ZN(reg, width) UOP(apu_chk_zn, ((uint32_t)reg) | ((uint32_t)width << 8))

uint32_t apu_alu_op(uint32_t arg);
/* perform alu op [op], with width [width], between register operands [operand_a] and [operand_b]*/
#define ALU_OP(op, operand_a, operand_b, width) UOP(apu_alu_op, ((uint32_t)operand_b << 24) | ((uint32_t)operand_a << 16) | ((uint32_t)op << 8) | ((uint32_t)width))


struct apu_inst_t apu_instructions[APU_OPCODE_LAST] = {

    /**************************************************************************************/
    /*                                      BCC                                           */
    /**************************************************************************************/

    [APU_OPCODE_BCC_PC_REL] = {
        MOV_LPC     (APU_REG_TEMP, MOV_LSB),
        SKIPC       (2, APU_STATUS_FLAG_C),
        IO,
        IO,
        OFFSET_PC_R (APU_REG_TEMP)
    },

    /**************************************************************************************/
    /*                                      BCS                                           */
    /**************************************************************************************/

    [APU_OPCODE_BCS_PC_REL] = {
        MOV_LPC     (APU_REG_TEMP, MOV_LSB),
        SKIPS       (2, APU_STATUS_FLAG_C),
        IO,
        IO,
        OFFSET_PC_R (APU_REG_TEMP)
    },

    /**************************************************************************************/
    /*                                      BEQ                                           */
    /**************************************************************************************/

    [APU_OPCODE_BEQ_PC_REL] = {
        MOV_LPC     (APU_REG_TEMP, MOV_LSB),
        SKIPS       (2, APU_STATUS_FLAG_Z),
        IO,
        IO,
        OFFSET_PC_R (APU_REG_TEMP)
    },

    /**************************************************************************************/
    /*                                      BMI                                           */
    /**************************************************************************************/
    [APU_OPCODE_BMI_PC_REL] = {
        MOV_LPC     (APU_REG_TEMP, MOV_LSB),
        SKIPC       (2, APU_STATUS_FLAG_N),
        IO,
        IO,
        OFFSET_PC_R (APU_REG_TEMP)
    },

    /**************************************************************************************/
    /*                                      BNE                                           */
    /**************************************************************************************/

    [APU_OPCODE_BNE_PC_REL] = {
        MOV_LPC     (APU_REG_TEMP, MOV_LSB),
        SKIPC       (2, APU_STATUS_FLAG_Z),
        IO,
        IO,
        OFFSET_PC_R (APU_REG_TEMP)
    },

    /**************************************************************************************/
    /*                                      BPL                                           */
    /**************************************************************************************/

    [APU_OPCODE_BPL_PC_REL] = {
        MOV_LPC     (APU_REG_TEMP, MOV_LSB),
        SKIPC       (2, APU_STATUS_FLAG_N),
        IO,
        IO,
        OFFSET_PC_R (APU_REG_TEMP)
    },

    /**************************************************************************************/
    /*                                      BVC                                           */
    /**************************************************************************************/

    [APU_OPCODE_BVC_PC_REL] = {
        MOV_LPC     (APU_REG_TEMP, MOV_LSB),
        SKIPC       (2, APU_STATUS_FLAG_V),
        IO,
        IO,
        OFFSET_PC_R (APU_REG_TEMP)
    },

    /**************************************************************************************/
    /*                                      BVS                                           */
    /**************************************************************************************/

    [APU_OPCODE_BVS_PC_REL] = {
        MOV_LPC     (APU_REG_TEMP, MOV_LSB),
        SKIPS       (2, APU_STATUS_FLAG_V),
        IO,
        IO,
        OFFSET_PC_R (APU_REG_TEMP)
    },

    /**************************************************************************************/
    /*                                      BRA                                           */
    /**************************************************************************************/

    [APU_OPCODE_BRA_PC_REL] = {
        MOV_LPC     (APU_REG_TEMP, MOV_LSB),
        IO,
        IO,
        OFFSET_PC_R (APU_REG_TEMP)
    },

    /**************************************************************************************/
    /*                                      DEC                                           */
    /**************************************************************************************/

    [APU_OPCODE_DEC_A_IMP] = {
        IO,
        ALU_OP      (APU_ALU_OP_DEC, APU_REG_A, APU_REG_TEMP, APU_ALU_WIDTH_BYTE),
    },

    [APU_OPCODE_DEC_X_IMP] = {
        IO,
        ALU_OP      (APU_ALU_OP_DEC, APU_REG_X, APU_REG_TEMP, APU_ALU_WIDTH_BYTE),
    },

    [APU_OPCODE_DEC_Y_IMP] = {
        IO,
        ALU_OP      (APU_ALU_OP_DEC, APU_REG_X, APU_REG_TEMP, APU_ALU_WIDTH_BYTE),
    },

    // [APU_OPCODE_DEC_DP] = {
    //     MOV_LPC     (APU_REG_TEMP, MOV_LSB),

    // },

    /**************************************************************************************/
    /*                                      INC                                           */
    /**************************************************************************************/

    [APU_OPCODE_INC_A_IMP] = {
        IO,
        ALU_OP      (APU_ALU_OP_INC, APU_REG_A, APU_REG_TEMP, APU_ALU_WIDTH_BYTE),
    },

    [APU_OPCODE_INC_X_IMP] = {
        IO,
        ALU_OP      (APU_ALU_OP_INC, APU_REG_X, APU_REG_TEMP, APU_ALU_WIDTH_BYTE),
    },

    [APU_OPCODE_INC_Y_IMP] = {
        IO,
        ALU_OP      (APU_ALU_OP_INC, APU_REG_X, APU_REG_TEMP, APU_ALU_WIDTH_BYTE),
    },

    /**************************************************************************************/
    /*                                      MOV                                           */
    /**************************************************************************************/

    [APU_OPCODE_MOV_A_IMM] = {
        MOV_LPC     (APU_REG_TEMP, MOV_LSB),
        IO,
        MOV_RR      (APU_REG_A, APU_REG_TEMP, MOV_RR_BYTE),
        CHK_ZN      (APU_REG_A, APU_ALU_WIDTH_BYTE)
    },

    [APU_OPCODE_MOV_X_IMM] = { 
        MOV_LPC     (APU_REG_TEMP, MOV_LSB),
        IO,
        MOV_RR      (APU_REG_X, APU_REG_TEMP, MOV_RR_BYTE),
        CHK_ZN      (APU_REG_X, APU_ALU_WIDTH_BYTE)
    },

    [APU_OPCODE_MOV_SP_X_IMP] = {
        IO,
        MOV_RR      (APU_REG_SP, APU_REG_X, MOV_RR_BYTE),
    },

    [APU_OPCODE_MOV_DP_X_A] = {
        IO,
        IO,
        IO,
        MOV_S       (APU_REG_TEMP, MOV_LSB, APU_REG_X),
    },

    [APU_OPCODE_FETCH] = {
        MOV_LPC     (APU_REG_INST, MOV_LSB),
        DECODE
    },

    [APU_OPCODE_RESET] = {
        MOV_LPC     (APU_REG_TEMP, MOV_LSB),
        MOV_LPC     (APU_REG_TEMP, MOV_MSB),
        MOV_RR      (APU_REG_PC, APU_REG_TEMP, MOV_RR_WORD)
    }
};

char *apu_instruction_strs[] = {
    [APU_INST_ADC]   = "ADC",
    [APU_INST_ADDW]  = "ADDW",
    [APU_INST_AND]   = "AND",
    [APU_INST_ASL]   = "ASL",
    [APU_INST_BBC]   = "BBC",
    [APU_INST_BBS]   = "BBS",
    [APU_INST_BCC]   = "BCC",
    [APU_INST_BCS]   = "BCS",
    [APU_INST_BEQ]   = "BEQ",
    [APU_INST_BMI]   = "BMI",
    [APU_INST_BNE]   = "BNE",
    [APU_INST_BPL]   = "BPL",
    [APU_INST_BVC]   = "BVC",
    [APU_INST_BVS]   = "BVS",
    [APU_INST_BRA]   = "BRA",
    [APU_INST_BRK]   = "BRK",
    [APU_INST_CALL]  = "CALL",
    [APU_INST_CBNE]  = "CBNE",
    [APU_INST_CLR1]  = "CLR1",
    [APU_INST_CLRC]  = "CLRC",
    [APU_INST_CLRP]  = "CLRP",
    [APU_INST_CLRV]  = "CLRV",
    [APU_INST_CMP]   = "CMP",
    [APU_INST_DAA]   = "DAA",
    [APU_INST_DAS]   = "DAS",
    [APU_INST_DBNZ]  = "DBNZ",
    [APU_INST_DEC]   = "DEC",
    [APU_INST_DI]    = "DI",
    [APU_INST_DIV]   = "DIV",
    [APU_INST_EI]    = "EI",
    [APU_INST_EOR]   = "EOR",
    [APU_INST_INC]   = "INC",
    [APU_INST_JMP]   = "JMP",
    [APU_INST_LSR]   = "LSR",
    [APU_INST_MOV]   = "MOV",
    [APU_INST_MUL]   = "MUL",
    [APU_INST_NOP]   = "NOP",
    [APU_INST_NOT1]  = "NOT1",
    [APU_INST_NOTC]  = "NOTC",
    [APU_INST_OR]    = "OR",
    [APU_INST_PCALL] = "PCALL",
    [APU_INST_POP]   = "POP",
    [APU_INST_PUSH]  = "PUSH",
    [APU_INST_RET]   = "RET",
    [APU_INST_RETI]  = "RETI",
    [APU_INST_ROL]   = "ROL",
    [APU_INST_ROR]   = "ROR",
    [APU_INST_SBC]   = "SBC",
    [APU_INST_SUBW]  = "SUBW",
    [APU_INST_SET1]  = "SET1",
    [APU_INST_SETC]  = "SETC",
    [APU_INST_SETP]  = "SETP",
    [APU_INST_SLEEP] = "SLEEP",
    [APU_INST_STOP]  = "STOP",
    [APU_INST_TCALL] = "TCALL",
    [APU_INST_TCLR1] = "TCLR1",
    [APU_INST_TSET1] = "TSET1",
    [APU_INST_XCN]   = "XCN"
};

char *apu_reg_strs[] = {
    [APU_REG_X]     = "X",
    [APU_REG_Y]     = "Y",
    [APU_REG_SP]    = "SP",
    [APU_REG_PSW]   = "PSW",
    [APU_REG_A]     = "A",
    [APU_REG_PC]    = "PC",
};

char *apu_addr_mode_strs[] = {
    [APU_ADDR_MODE_IMM]         = "#%02x",
    [APU_ADDR_MODE_X]           = "(X) = %02x",
    [APU_ADDR_MODE_X_INCR]      = "(X)+ = %02x",
    [APU_ADDR_MODE_DP]          = "(dp(%02x) = %02x)",
    [APU_ADDR_MODE_DP_BIT]      = "(dp(%02x) = %02x).%d",
    [APU_ADDR_MODE_DP_X]        = "(dp(%02x) + X) = %02x",
    [APU_ADDR_MODE_DP_Y]        = "(dp(%02x) + Y) = %02x",
    [APU_ADDR_MODE_ABS]         = "(!abs(%04x))",
    [APU_ADDR_MODE_ABS_BIT]     = "(!abs(%04x)).%d",
    [APU_ADDR_MODE_ABS_X]       = "(!abs(%04x) + X(%02x))",
    [APU_ADDR_MODE_ABS_Y]       = "(!abs(%04x) + Y(%02x))",
    [APU_ADDR_MODE_DP_X_IND]    = "([dp(%02x) + X(%02x)])",
    [APU_ADDR_MODE_DP_IND_Y]    = "([dp(%02x)] + Y(%02x))",
    [APU_ADDR_MODE_X_Y]         = "",
    [APU_ADDR_MODE_DP_DP]       = "",
    [APU_ADDR_MODE_DP_IMM]      = "",
    [APU_ADDR_MODE_IMP]         = "",
    [APU_ADDR_MODE_PC_REL]      = "PC(%04x) + r(%02x)",
    [APU_ADDR_MODE_S]           = "",
};

enum APU_OPERANDS
{
    APU_OPERAND_REG_A   = APU_REG_A,
    APU_OPERAND_REG_X   = APU_REG_X,
    APU_OPERAND_REG_Y   = APU_REG_Y,
    APU_OPERAND_REG_SP  = APU_REG_SP,
    APU_OPERAND_MEM     = APU_REG_LAST
};

struct apu_inst_info_t apu_instruction_info[] = {
    [APU_OPCODE_ADC_X_Y]        = {.instruction = APU_INST_ADC,   .addr_mode = APU_ADDR_MODE_X_Y,      .operands = {APU_OPERAND_MEM,    APU_OPERAND_MEM},    .width = 1},
    [APU_OPCODE_ADC_A_IMM]      = {.instruction = APU_INST_ADC,   .addr_mode = APU_ADDR_MODE_IMM,      .operands = {APU_OPERAND_REG_A,  APU_OPERAND_MEM},    .width = 2},
    [APU_OPCODE_ADC_A_X]        = {.instruction = APU_INST_ADC,   .addr_mode = APU_ADDR_MODE_X,        .operands = {APU_OPERAND_REG_A,  APU_OPERAND_REG_X},  .width = 1},
    [APU_OPCODE_ADC_A_DP_IND_Y] = {.instruction = APU_INST_ADC,   .addr_mode = APU_ADDR_MODE_DP_IND_Y, .operands = {APU_OPERAND_REG_A,  APU_OPERAND_MEM},    .width = 2},
    [APU_OPCODE_ADC_A_DP_X_IND] = {.instruction = APU_INST_ADC,   .addr_mode = APU_ADDR_MODE_DP_X_IND, .operands = {APU_OPERAND_REG_A,  APU_OPERAND_MEM},    .width = 2},
    [APU_OPCODE_ADC_A_DP]       = {.instruction = APU_INST_ADC,   .addr_mode = APU_ADDR_MODE_DP,       .operands = {APU_OPERAND_REG_A,  APU_OPERAND_MEM},    .width = 2},
    [APU_OPCODE_ADC_A_DP_X]     = {.instruction = APU_INST_ADC,   .addr_mode = APU_ADDR_MODE_DP_X,     .operands = {APU_OPERAND_REG_A,  APU_OPERAND_MEM},    .width = 2},
    [APU_OPCODE_ADC_A_ABS]      = {.instruction = APU_INST_ADC,   .addr_mode = APU_ADDR_MODE_ABS,      .operands = {APU_OPERAND_REG_A,  APU_OPERAND_MEM},    .width = 3},
    [APU_OPCODE_ADC_A_ABX_X]    = {.instruction = APU_INST_ADC,   .addr_mode = APU_ADDR_MODE_ABS_X,    .operands = {APU_OPERAND_REG_A,  APU_OPERAND_MEM},    .width = 3},
    [APU_OPCODE_ADC_A_ABS_Y]    = {.instruction = APU_INST_ADC,   .addr_mode = APU_ADDR_MODE_ABS_Y,    .operands = {APU_OPERAND_REG_A,  APU_OPERAND_MEM},    .width = 3},
    [APU_OPCODE_ADC_DP_DP]      = {.instruction = APU_INST_ADC,   .addr_mode = APU_ADDR_MODE_DP_DP,    .operands = {APU_OPERAND_MEM,    APU_OPERAND_MEM},    .width = 3},
    [APU_OPCODE_ADC_DP_IMM]     = {.instruction = APU_INST_ADC,   .addr_mode = APU_ADDR_MODE_DP_IMM,   .operands = {APU_OPERAND_MEM,    APU_OPERAND_MEM},    .width = 3},
    [APU_OPCODE_ADDW_DP]        = {.instruction = APU_INST_ADDW,  .addr_mode = APU_ADDR_MODE_DP,       .operands = {APU_OPERAND_REG_A,  APU_OPERAND_MEM},    .width = 2},

    [APU_OPCODE_AND_X_Y]        = {.instruction = APU_INST_AND,   .addr_mode = APU_ADDR_MODE_X_Y,       .width = 1},
    [APU_OPCODE_AND_A_IMM]      = {.instruction = APU_INST_AND,   .addr_mode = APU_ADDR_MODE_IMM,       .width = 2},
    [APU_OPCODE_AND_A_X]        = {.instruction = APU_INST_AND,   .addr_mode = APU_ADDR_MODE_X,         .width = 1},
    [APU_OPCODE_AND_A_DP_IND_Y] = {.instruction = APU_INST_AND,   .addr_mode = APU_ADDR_MODE_DP_IND_Y,  .width = 2},
    [APU_OPCODE_AND_A_DP_X_IND] = {.instruction = APU_INST_AND,   .addr_mode = APU_ADDR_MODE_DP_X_IND,  .width = 2},
    [APU_OPCODE_AND_A_DP]       = {.instruction = APU_INST_AND,   .addr_mode = APU_ADDR_MODE_DP,        .width = 2},
    [APU_OPCODE_AND_A_DP_X]     = {.instruction = APU_INST_AND,   .addr_mode = APU_ADDR_MODE_DP_X,      .width = 2},
    [APU_OPCODE_AND_A_ABS]      = {.instruction = APU_INST_AND,   .addr_mode = APU_ADDR_MODE_ABS,       .width = 3},
    [APU_OPCODE_AND_A_ABS_X]    = {.instruction = APU_INST_AND,   .addr_mode = APU_ADDR_MODE_ABS_X,     .width = 3},
    [APU_OPCODE_AND_A_ABS_Y]    = {.instruction = APU_INST_AND,   .addr_mode = APU_ADDR_MODE_ABS_Y,     .width = 3},
    [APU_OPCODE_AND_DP_DP]      = {.instruction = APU_INST_AND,   .addr_mode = APU_ADDR_MODE_DP_DP,     .width = 3},
    [APU_OPCODE_AND_DP_IMM]     = {.instruction = APU_INST_AND,   .addr_mode = APU_ADDR_MODE_DP_IMM,    .width = 3},
    [APU_OPCODE_AND1N_DP]       = {.instruction = APU_INST_AND,   .addr_mode = APU_ADDR_MODE_DP,        .width = 3},
    [APU_OPCODE_AND1_DP_BIT]    = {.instruction = APU_INST_AND,   .addr_mode = APU_ADDR_MODE_DP_BIT,    .width = 3},

    [APU_OPCODE_ASL_A_IMP]      = {.instruction = APU_INST_ASL,   .addr_mode = APU_ADDR_MODE_IMP,       .width = 1},
    [APU_OPCODE_ASL_DP]         = {.instruction = APU_INST_ASL,   .addr_mode = APU_ADDR_MODE_DP,        .width = 2},
    [APU_OPCODE_ASL_DP_X]       = {.instruction = APU_INST_ASL,   .addr_mode = APU_ADDR_MODE_DP_X,      .width = 2},
    [APU_OPCODE_ASL_ABS]        = {.instruction = APU_INST_ASL,   .addr_mode = APU_ADDR_MODE_ABS,       .width = 3},

    [APU_OPCODE_BBC0_PC_REL]    = {.instruction = APU_INST_BCC,   .addr_mode = APU_ADDR_MODE_PC_REL,    .width = 3},
    [APU_OPCODE_BBC1_PC_REL]    = {.instruction = APU_INST_BCC,   .addr_mode = APU_ADDR_MODE_PC_REL,    .width = 3},
    [APU_OPCODE_BBC2_PC_REL]    = {.instruction = APU_INST_BCC,   .addr_mode = APU_ADDR_MODE_PC_REL,    .width = 3},
    [APU_OPCODE_BBC3_PC_REL]    = {.instruction = APU_INST_BCC,   .addr_mode = APU_ADDR_MODE_PC_REL,    .width = 3},
    [APU_OPCODE_BBC4_PC_REL]    = {.instruction = APU_INST_BCC,   .addr_mode = APU_ADDR_MODE_PC_REL,    .width = 3},
    [APU_OPCODE_BBC5_PC_REL]    = {.instruction = APU_INST_BCC,   .addr_mode = APU_ADDR_MODE_PC_REL,    .width = 3},
    [APU_OPCODE_BBC6_PC_REL]    = {.instruction = APU_INST_BCC,   .addr_mode = APU_ADDR_MODE_PC_REL,    .width = 3},
    [APU_OPCODE_BBC7_PC_REL]    = {.instruction = APU_INST_BCC,   .addr_mode = APU_ADDR_MODE_PC_REL,    .width = 3},

    [APU_OPCODE_BBS0_PC_REL]    = {.instruction = APU_INST_BCS,   .addr_mode = APU_ADDR_MODE_PC_REL,    .width = 3},
    [APU_OPCODE_BBS1_PC_REL]    = {.instruction = APU_INST_BCS,   .addr_mode = APU_ADDR_MODE_PC_REL,    .width = 3},
    [APU_OPCODE_BBS2_PC_REL]    = {.instruction = APU_INST_BCS,   .addr_mode = APU_ADDR_MODE_PC_REL,    .width = 3},
    [APU_OPCODE_BBS3_PC_REL]    = {.instruction = APU_INST_BCS,   .addr_mode = APU_ADDR_MODE_PC_REL,    .width = 3},
    [APU_OPCODE_BBS4_PC_REL]    = {.instruction = APU_INST_BCS,   .addr_mode = APU_ADDR_MODE_PC_REL,    .width = 3},
    [APU_OPCODE_BBS5_PC_REL]    = {.instruction = APU_INST_BCS,   .addr_mode = APU_ADDR_MODE_PC_REL,    .width = 3},
    [APU_OPCODE_BBS6_PC_REL]    = {.instruction = APU_INST_BCS,   .addr_mode = APU_ADDR_MODE_PC_REL,    .width = 3},
    [APU_OPCODE_BBS7_PC_REL]    = {.instruction = APU_INST_BCS,   .addr_mode = APU_ADDR_MODE_PC_REL,    .width = 3},

    [APU_OPCODE_BCC_PC_REL]     = {.instruction = APU_INST_BCC,   .addr_mode = APU_ADDR_MODE_PC_REL,    .width = 2},
    [APU_OPCODE_BCS_PC_REL]     = {.instruction = APU_INST_BCS,   .addr_mode = APU_ADDR_MODE_PC_REL,    .width = 2},

    [APU_OPCODE_BEQ_PC_REL]     = {.instruction = APU_INST_BEQ,   .addr_mode = APU_ADDR_MODE_PC_REL,    .width = 2},
    [APU_OPCODE_BMI_PC_REL]     = {.instruction = APU_INST_BMI,   .addr_mode = APU_ADDR_MODE_PC_REL,    .width = 2},
    [APU_OPCODE_BNE_PC_REL]     = {.instruction = APU_INST_BNE,   .addr_mode = APU_ADDR_MODE_PC_REL,    .width = 2},
    [APU_OPCODE_BPL_PC_REL]     = {.instruction = APU_INST_BPL,   .addr_mode = APU_ADDR_MODE_PC_REL,    .width = 2},
    [APU_OPCODE_BVC_PC_REL]     = {.instruction = APU_INST_BVC,   .addr_mode = APU_ADDR_MODE_PC_REL,    .width = 2},
    [APU_OPCODE_BVS_PC_REL]     = {.instruction = APU_INST_BVS,   .addr_mode = APU_ADDR_MODE_PC_REL,    .width = 2},
    [APU_OPCODE_BRA_PC_REL]     = {.instruction = APU_INST_BRA,   .addr_mode = APU_ADDR_MODE_PC_REL,    .width = 2},

    [APU_OPCODE_BRK_IMP]        = {.instruction = APU_INST_BRK,   .addr_mode = APU_ADDR_MODE_IMP,       .width = 1},

    [APU_OPCODE_CALL_ABS]       = {.instruction = APU_INST_CALL,  .addr_mode = APU_ADDR_MODE_ABS,       .width = 3},

    [APU_OPCODE_CBNE_DP_X]      = {.instruction = APU_INST_CBNE,  .addr_mode = APU_ADDR_MODE_DP_X,      .width = 3},
    [APU_OPCODE_CBNE_DP]        = {.instruction = APU_INST_CBNE,  .addr_mode = APU_ADDR_MODE_DP,        .width = 3},

    [APU_OPCODE_CLR10_DP]       = {.instruction = APU_INST_CLR1,  .addr_mode = APU_ADDR_MODE_DP,        .width = 2},
    [APU_OPCODE_CLR11_DP]       = {.instruction = APU_INST_CLR1,  .addr_mode = APU_ADDR_MODE_DP,        .width = 2},
    [APU_OPCODE_CLR12_DP]       = {.instruction = APU_INST_CLR1,  .addr_mode = APU_ADDR_MODE_DP,        .width = 2},
    [APU_OPCODE_CLR13_DP]       = {.instruction = APU_INST_CLR1,  .addr_mode = APU_ADDR_MODE_DP,        .width = 2},
    [APU_OPCODE_CLR14_DP]       = {.instruction = APU_INST_CLR1,  .addr_mode = APU_ADDR_MODE_DP,        .width = 2},
    [APU_OPCODE_CLR15_DP]       = {.instruction = APU_INST_CLR1,  .addr_mode = APU_ADDR_MODE_DP,        .width = 2},
    [APU_OPCODE_CLR16_DP]       = {.instruction = APU_INST_CLR1,  .addr_mode = APU_ADDR_MODE_DP,        .width = 2},
    [APU_OPCODE_CLR17_DP]       = {.instruction = APU_INST_CLR1,  .addr_mode = APU_ADDR_MODE_DP,        .width = 2},

    [APU_OPCODE_CLRC_IMP]       = {.instruction = APU_INST_CLRC,  .addr_mode = APU_ADDR_MODE_IMP,       .width = 1},
    [APU_OPCODE_CLRP_IMP]       = {.instruction = APU_INST_CLRP,  .addr_mode = APU_ADDR_MODE_IMP,       .width = 1},
    [APU_OPCODE_CLRV_IMP]       = {.instruction = APU_INST_CLRV,  .addr_mode = APU_ADDR_MODE_IMP,       .width = 1},

    [APU_OPCODE_CMP_X_Y]        = {.instruction = APU_INST_CMP,   .addr_mode = APU_ADDR_MODE_X_Y,       .width = 1},
    [APU_OPCODE_CMP_A_IMM]      = {.instruction = APU_INST_CMP,   .addr_mode = APU_ADDR_MODE_IMM,       .width = 2},
    [APU_OPCODE_CMP_A_X]        = {.instruction = APU_INST_CMP,   .addr_mode = APU_ADDR_MODE_X,         .width = 1},
    [APU_OPCODE_CMP_A_DP_IND_Y] = {.instruction = APU_INST_CMP,   .addr_mode = APU_ADDR_MODE_DP_IND_Y,  .width = 2},
    [APU_OPCODE_CMP_A_DP_X_IND] = {.instruction = APU_INST_CMP,   .addr_mode = APU_ADDR_MODE_DP_X_IND,  .width = 2},
    [APU_OPCODE_CMP_A_DP]       = {.instruction = APU_INST_CMP,   .addr_mode = APU_ADDR_MODE_DP,        .width = 2},
    [APU_OPCODE_CMP_A_DP_X]     = {.instruction = APU_INST_CMP,   .addr_mode = APU_ADDR_MODE_DP_X,      .width = 2},
    [APU_OPCODE_CMP_A_ABS]      = {.instruction = APU_INST_CMP,   .addr_mode = APU_ADDR_MODE_ABS,       .width = 3},
    [APU_OPCODE_CMP_A_ABS_X]    = {.instruction = APU_INST_CMP,   .addr_mode = APU_ADDR_MODE_ABS_X,     .width = 3},
    [APU_OPCODE_CMP_A_ABS_Y]    = {.instruction = APU_INST_CMP,   .addr_mode = APU_ADDR_MODE_ABS_Y,     .width = 3},
    [APU_OPCODE_CMP_X_IMM]      = {.instruction = APU_INST_CMP,   .addr_mode = APU_ADDR_MODE_IMM,       .width = 2},
    [APU_OPCODE_CMP_X_DP]       = {.instruction = APU_INST_CMP,   .addr_mode = APU_ADDR_MODE_DP,        .width = 2},
    [APU_OPCODE_CMP_X_ABS]      = {.instruction = APU_INST_CMP,   .addr_mode = APU_ADDR_MODE_ABS,       .width = 3},
    [APU_OPCODE_CMP_Y_IMM]      = {.instruction = APU_INST_CMP,   .addr_mode = APU_ADDR_MODE_IMM,       .width = 2},
    [APU_OPCODE_CMP_Y_DP]       = {.instruction = APU_INST_CMP,   .addr_mode = APU_ADDR_MODE_DP,        .width = 2},
    [APU_OPCODE_CMP_Y_ABS]      = {.instruction = APU_INST_CMP,   .addr_mode = APU_ADDR_MODE_ABS,       .width = 3},
    [APU_OPCODE_CMP_DP_DP]      = {.instruction = APU_INST_CMP,   .addr_mode = APU_ADDR_MODE_DP_DP,     .width = 3},
    [APU_OPCODE_CMP_DP_IMM]     = {.instruction = APU_INST_CMP,   .addr_mode = APU_ADDR_MODE_DP_IMM,    .width = 3},
    [APU_OPCODE_CMPW_YA_DP]     = {.instruction = APU_INST_CMP,   .addr_mode = APU_ADDR_MODE_DP,        .width = 2},

    [APU_OPCODE_DAA_IMP]        = {.instruction = APU_INST_DAA,   .addr_mode = APU_ADDR_MODE_IMP,       .width = 1},
    [APU_OPCODE_DAS_IMP]        = {.instruction = APU_INST_DAS,   .addr_mode = APU_ADDR_MODE_IMP,       .width = 1},

    [APU_OPCODE_DBNZ_Y_IMP]     = {.instruction = APU_INST_DBNZ,  .addr_mode = APU_ADDR_MODE_IMP,       .width = 2},
    [APU_OPCODE_DBNZ_DP]        = {.instruction = APU_INST_DBNZ,  .addr_mode = APU_ADDR_MODE_DP,        .width = 3},

    [APU_OPCODE_DEC_A_IMP]      = {.instruction = APU_INST_DEC,   .addr_mode = APU_ADDR_MODE_IMP,       .width = 1},
    [APU_OPCODE_DEC_X_IMP]      = {.instruction = APU_INST_DEC,   .addr_mode = APU_ADDR_MODE_IMP,       .width = 1},
    [APU_OPCODE_DEC_Y_IMP]      = {.instruction = APU_INST_DEC,   .addr_mode = APU_ADDR_MODE_IMP,       .width = 1},
    [APU_OPCODE_DEC_DP]         = {.instruction = APU_INST_DEC,   .addr_mode = APU_ADDR_MODE_DP,        .width = 2},
    [APU_OPCODE_DEC_DP_X]       = {.instruction = APU_INST_DEC,   .addr_mode = APU_ADDR_MODE_DP_X,      .width = 2},
    [APU_OPCODE_DEC_ABS]        = {.instruction = APU_INST_DEC,   .addr_mode = APU_ADDR_MODE_ABS,       .width = 3},
    [APU_OPCODE_DECW_DP]        = {.instruction = APU_INST_DEC,   .addr_mode = APU_ADDR_MODE_DP,        .width = 2},

    [APU_OPCODE_DI_IMP]         = {.instruction = APU_INST_DI,    .addr_mode = APU_ADDR_MODE_IMP,       .width = 1},

    [APU_OPCODE_DIV_IMP]        = {.instruction = APU_INST_DIV,   .addr_mode = APU_ADDR_MODE_IMP,       .width = 1},

    [APU_OPCODE_EI_IMP]         = {.instruction = APU_INST_EI,    .addr_mode = APU_ADDR_MODE_IMP,       .width = 1},

    [APU_OPCODE_EOR_X_Y]        = {.instruction = APU_INST_EOR,   .addr_mode = APU_ADDR_MODE_X_Y,       .width = 1},
    [APU_OPCODE_EOR_A_IMM]      = {.instruction = APU_INST_EOR,   .addr_mode = APU_ADDR_MODE_IMM,       .width = 2},
    [APU_OPCODE_EOR_A_X]        = {.instruction = APU_INST_EOR,   .addr_mode = APU_ADDR_MODE_X,         .width = 1},
    [APU_OPCODE_EOR_A_DP_IND_Y] = {.instruction = APU_INST_EOR,   .addr_mode = APU_ADDR_MODE_DP_IND_Y,  .width = 2},
    [APU_OPCODE_EOR_A_DP_X_IND] = {.instruction = APU_INST_EOR,   .addr_mode = APU_ADDR_MODE_DP_X_IND,  .width = 2},
    [APU_OPCODE_EOR_A_DP]       = {.instruction = APU_INST_EOR,   .addr_mode = APU_ADDR_MODE_DP,        .width = 2},
    [APU_OPCODE_EOR_A_DP_X]     = {.instruction = APU_INST_EOR,   .addr_mode = APU_ADDR_MODE_DP_X,      .width = 2},
    [APU_OPCODE_EOR_A_ABS]      = {.instruction = APU_INST_EOR,   .addr_mode = APU_ADDR_MODE_ABS,       .width = 3},
    [APU_OPCODE_EOR_A_ABS_X]    = {.instruction = APU_INST_EOR,   .addr_mode = APU_ADDR_MODE_ABS_X,     .width = 3},
    [APU_OPCODE_EOR_A_ABS_Y]    = {.instruction = APU_INST_EOR,   .addr_mode = APU_ADDR_MODE_ABS_Y,     .width = 3},
    [APU_OPCODE_EOR_DP_DP]      = {.instruction = APU_INST_EOR,   .addr_mode = APU_ADDR_MODE_DP_DP,     .width = 3},
    [APU_OPCODE_EOR_DP_IMM]     = {.instruction = APU_INST_EOR,   .addr_mode = APU_ADDR_MODE_DP_IMM,    .width = 3},
    [APU_OPCODE_EOR1_DP_BIT]    = {.instruction = APU_INST_EOR,   .addr_mode = APU_ADDR_MODE_DP_BIT,    .width = 3},

    [APU_OPCODE_INC_A_IMP]      = {.instruction = APU_INST_INC,   .addr_mode = APU_ADDR_MODE_IMP,       .width = 1},
    [APU_OPCODE_INC_X_IMP]      = {.instruction = APU_INST_INC,   .addr_mode = APU_ADDR_MODE_IMP,       .width = 1},
    [APU_OPCODE_INC_Y_IMP]      = {.instruction = APU_INST_INC,   .addr_mode = APU_ADDR_MODE_IMP,       .width = 1},
    [APU_OPCODE_INC_DP]         = {.instruction = APU_INST_INC,   .addr_mode = APU_ADDR_MODE_DP,        .width = 2},
    [APU_OPCODE_INC_DP_X]       = {.instruction = APU_INST_INC,   .addr_mode = APU_ADDR_MODE_DP_X,      .width = 2},
    [APU_OPCODE_INC_ABS]        = {.instruction = APU_INST_INC,   .addr_mode = APU_ADDR_MODE_ABS,       .width = 3},
    [APU_OPCODE_INCW_DP]        = {.instruction = APU_INST_INC,   .addr_mode = APU_ADDR_MODE_DP,        .width = 2},

    [APU_OPCODE_JMP_ABS]        = {.instruction = APU_INST_JMP,   .addr_mode = APU_ADDR_MODE_ABS,       .width = 3},
    [APU_OPCODE_JMP_ABS_X]      = {.instruction = APU_INST_JMP,   .addr_mode = APU_ADDR_MODE_ABS_X,     .width = 3},

    [APU_OPCODE_LSR_A_IMP]      = {.instruction = APU_INST_LSR,   .addr_mode = APU_ADDR_MODE_IMP,       .width = 1},
    [APU_OPCODE_LSR_DP]         = {.instruction = APU_INST_LSR,   .addr_mode = APU_ADDR_MODE_DP,        .width = 2},
    [APU_OPCODE_LSR_DP_X]       = {.instruction = APU_INST_LSR,   .addr_mode = APU_ADDR_MODE_DP_X,      .width = 2},
    [APU_OPCODE_LSR_ABS]        = {.instruction = APU_INST_LSR,   .addr_mode = APU_ADDR_MODE_ABS,       .width = 3},

    [APU_OPCODE_MOV_X_INCR_A]   = {.instruction = APU_INST_MOV,   .addr_mode = APU_ADDR_MODE_X_INCR,   .operands = {APU_OPERAND_MEM, APU_OPERAND_REG_A}, .width = 1},
    [APU_OPCODE_MOV_X_A]        = {.instruction = APU_INST_MOV,   .addr_mode = APU_ADDR_MODE_X,        .operands = {APU_OPERAND_MEM, APU_OPERAND_REG_A}, .width = 1},
    [APU_OPCODE_MOV_DP_IND_Y_A] = {.instruction = APU_INST_MOV,   .addr_mode = APU_ADDR_MODE_DP_IND_Y, .operands = {APU_OPERAND_MEM, APU_OPERAND_REG_A}, .width = 2},
    [APU_OPCODE_MOV_DP_X_IND_A] = {.instruction = APU_INST_MOV,   .addr_mode = APU_ADDR_MODE_DP_X_IND, .operands = {APU_OPERAND_MEM, APU_OPERAND_REG_A}, .width = 2},
    [APU_OPCODE_MOV_A_IMM]      = {.instruction = APU_INST_MOV,   .addr_mode = APU_ADDR_MODE_IMM,      .operands = {APU_OPERAND_REG_A, APU_OPERAND_MEM}, .width = 2},
    [APU_OPCODE_MOV_A_X]        = {.instruction = APU_INST_MOV,   .addr_mode = APU_ADDR_MODE_X,        .operands = {APU_OPERAND_REG_A, APU_OPERAND_MEM}, .width = 1},
    [APU_OPCODE_MOV_A_X_INCR]   = {.instruction = APU_INST_MOV,   .addr_mode = APU_ADDR_MODE_X_INCR,   .operands = {APU_OPERAND_REG_A, APU_OPERAND_MEM}, .width = 1},
    [APU_OPCODE_MOV_A_DP_IND_Y] = {.instruction = APU_INST_MOV,   .addr_mode = APU_ADDR_MODE_DP_IND_Y, .operands = {APU_OPERAND_REG_A, APU_OPERAND_MEM}, .width = 2},
    [APU_OPCODE_MOV_A_DP_X_IND] = {.instruction = APU_INST_MOV,   .addr_mode = APU_ADDR_MODE_DP_X_IND, .operands = {APU_OPERAND_REG_A, APU_OPERAND_MEM}, .width = 2},
    [APU_OPCODE_MOV_A_X_IMP]    = {.instruction = APU_INST_MOV,   .addr_mode = APU_ADDR_MODE_IMP,      .operands = {APU_OPERAND_REG_A, APU_OPERAND_REG_X}, .width = 1},
    [APU_OPCODE_MOV_A_Y_IMP]    = {.instruction = APU_INST_MOV,   .addr_mode = APU_ADDR_MODE_IMP,      .operands = {APU_OPERAND_REG_A, APU_OPERAND_REG_Y}, .width = 1},
    [APU_OPCODE_MOV_A_DP]       = {.instruction = APU_INST_MOV,   .addr_mode = APU_ADDR_MODE_DP,       .operands = {APU_OPERAND_REG_A, APU_OPERAND_MEM}, .width = 2},
    [APU_OPCODE_MOV_A_DP_X]     = {.instruction = APU_INST_MOV,   .addr_mode = APU_ADDR_MODE_DP_X,     .operands = {APU_OPERAND_REG_A, APU_OPERAND_MEM}, .width = 2},
    [APU_OPCODE_MOV_A_ABS]      = {.instruction = APU_INST_MOV,   .addr_mode = APU_ADDR_MODE_ABS,      .operands = {APU_OPERAND_REG_A, APU_OPERAND_MEM}, .width = 3},
    [APU_OPCODE_MOV_A_ABS_X]    = {.instruction = APU_INST_MOV,   .addr_mode = APU_ADDR_MODE_ABS_X,    .operands = {APU_OPERAND_REG_A, APU_OPERAND_MEM}, .width = 3},
    [APU_OPCODE_MOV_A_ABS_Y]    = {.instruction = APU_INST_MOV,   .addr_mode = APU_ADDR_MODE_ABS_Y,    .operands = {APU_OPERAND_REG_A, APU_OPERAND_MEM}, .width = 3},
    [APU_OPCODE_MOV_SP_X_IMP]   = {.instruction = APU_INST_MOV,   .addr_mode = APU_ADDR_MODE_IMP,      .operands = {APU_OPERAND_REG_SP, APU_OPERAND_REG_X}, .width = 1},
    [APU_OPCODE_MOV_X_IMM]      = {.instruction = APU_INST_MOV,   .addr_mode = APU_ADDR_MODE_IMM,      .operands = {APU_OPERAND_REG_X, APU_OPERAND_MEM}, .width = 2},
    [APU_OPCODE_MOV_X_A_IMP]    = {.instruction = APU_INST_MOV,   .addr_mode = APU_ADDR_MODE_IMP,      .operands = {APU_OPERAND_REG_X, APU_OPERAND_REG_A}, .width = 1},
    [APU_OPCODE_MOV_X_SP_IMP]   = {.instruction = APU_INST_MOV,   .addr_mode = APU_ADDR_MODE_IMP,      .operands = {APU_OPERAND_REG_X, APU_OPERAND_REG_SP}, .width = 1},
    [APU_OPCODE_MOV_X_DP]       = {.instruction = APU_INST_MOV,   .addr_mode = APU_ADDR_MODE_DP,       .operands = {APU_OPERAND_REG_X, APU_OPERAND_MEM}, .width = 2},
    [APU_OPCODE_MOV_X_DP_Y]     = {.instruction = APU_INST_MOV,   .addr_mode = APU_ADDR_MODE_DP_Y,     .operands = {APU_OPERAND_REG_X, APU_OPERAND_MEM}, .width = 2},
    [APU_OPCODE_MOV_X_ABS]      = {.instruction = APU_INST_MOV,   .addr_mode = APU_ADDR_MODE_ABS,      .operands = {APU_OPERAND_REG_X, APU_OPERAND_MEM}, .width = 3},
    [APU_OPCODE_MOV_Y_IMM]      = {.instruction = APU_INST_MOV,   .addr_mode = APU_ADDR_MODE_IMM,      .operands = {APU_OPERAND_REG_Y, APU_OPERAND_MEM}, .width = 2},
    [APU_OPCODE_MOV_Y_A_IMP]    = {.instruction = APU_INST_MOV,   .addr_mode = APU_ADDR_MODE_IMP,      .operands = {APU_OPERAND_REG_Y, APU_OPERAND_MEM}, .width = 1},
    [APU_OPCODE_MOV_Y_DP]       = {.instruction = APU_INST_MOV,   .addr_mode = APU_ADDR_MODE_DP,       .operands = {APU_OPERAND_REG_Y, APU_OPERAND_MEM}, .width = 2},
    [APU_OPCODE_MOV_Y_DP_X]     = {.instruction = APU_INST_MOV,   .addr_mode = APU_ADDR_MODE_DP_X,     .operands = {APU_OPERAND_REG_Y, APU_OPERAND_MEM}, .width = 2},
    [APU_OPCODE_MOV_Y_ABS]      = {.instruction = APU_INST_MOV,   .addr_mode = APU_ADDR_MODE_ABS,      .operands = {APU_OPERAND_REG_Y, APU_OPERAND_MEM}, .width = 3},
    [APU_OPCODE_MOV_DP_DP]      = {.instruction = APU_INST_MOV,   .addr_mode = APU_ADDR_MODE_DP_DP,    .operands = {APU_OPERAND_MEM, APU_OPERAND_MEM}, .width = 3},
    [APU_OPCODE_MOV_DP_X_A]     = {.instruction = APU_INST_MOV,   .addr_mode = APU_ADDR_MODE_DP_X,     .operands = {APU_OPERAND_MEM, APU_OPERAND_REG_A}, .width = 2},
    [APU_OPCODE_MOV_DP_X_Y]     = {.instruction = APU_INST_MOV,   .addr_mode = APU_ADDR_MODE_DP_X,     .operands = {APU_OPERAND_MEM, APU_OPERAND_REG_Y}, .width = 2},
    [APU_OPCODE_MOV_DP_Y_X]     = {.instruction = APU_INST_MOV,   .addr_mode = APU_ADDR_MODE_DP_Y,     .operands = {APU_OPERAND_MEM, APU_OPERAND_REG_X}, .width = 2},
    [APU_OPCODE_MOV_DP_IMM]     = {.instruction = APU_INST_MOV,   .addr_mode = APU_ADDR_MODE_DP_IMM,   .operands = {APU_OPERAND_MEM, APU_OPERAND_MEM}, .width = 3},
    [APU_OPCODE_MOV_DP_A]       = {.instruction = APU_INST_MOV,   .addr_mode = APU_ADDR_MODE_DP,       .operands = {APU_OPERAND_MEM, APU_OPERAND_REG_A}, .width = 2},
    [APU_OPCODE_MOV_DP_X]       = {.instruction = APU_INST_MOV,   .addr_mode = APU_ADDR_MODE_DP,       .operands = {APU_OPERAND_MEM, APU_OPERAND_REG_X}, .width = 2},
    [APU_OPCODE_MOV_DP_Y]       = {.instruction = APU_INST_MOV,   .addr_mode = APU_ADDR_MODE_DP,       .operands = {APU_OPERAND_MEM, APU_OPERAND_REG_Y}, .width = 2},
    [APU_OPCODE_MOV_ABS_X_A]    = {.instruction = APU_INST_MOV,   .addr_mode = APU_ADDR_MODE_ABS_X,    .operands = {APU_OPERAND_MEM, APU_OPERAND_REG_A}, .width = 3},
    [APU_OPCODE_MOV_ABS_Y_A]    = {.instruction = APU_INST_MOV,   .addr_mode = APU_ADDR_MODE_ABS_Y,    .operands = {APU_OPERAND_MEM, APU_OPERAND_REG_A}, .width = 3},
    [APU_OPCODE_MOV_ABS_A]      = {.instruction = APU_INST_MOV,   .addr_mode = APU_ADDR_MODE_ABS,      .operands = {APU_OPERAND_MEM, APU_OPERAND_REG_A}, .width = 3},
    [APU_OPCODE_MOV_ABS_X]      = {.instruction = APU_INST_MOV,   .addr_mode = APU_ADDR_MODE_ABS,      .operands = {APU_OPERAND_MEM, APU_OPERAND_REG_X}, .width = 3},
    [APU_OPCODE_MOV_ABS_Y]      = {.instruction = APU_INST_MOV,   .addr_mode = APU_ADDR_MODE_ABS,      .operands = {APU_OPERAND_MEM, APU_OPERAND_REG_Y}, .width = 3},
    [APU_OPCODE_MOV1_C_ABS_BIT] = {.instruction = APU_INST_MOV,   .addr_mode = APU_ADDR_MODE_ABS_BIT,  .operands = {}, .width = 3},
    [APU_OPCODE_MOV1_ABS_BIT_C] = {.instruction = APU_INST_MOV,   .addr_mode = APU_ADDR_MODE_ABS_BIT,  .operands = {}, .width = 3},
    [APU_OPCODE_MOVW_YA_DP]     = {.instruction = APU_INST_MOV,   .addr_mode = APU_ADDR_MODE_DP,       .operands = {}, .width = 2},
    [APU_OPCODE_MOVW_DP_YA]     = {.instruction = APU_INST_MOV,   .addr_mode = APU_ADDR_MODE_DP,       .operands = {}, .width = 2},

    [APU_OPCODE_MUL_IMP]        = {.instruction = APU_INST_MUL,   .addr_mode = APU_ADDR_MODE_IMP,       .width = 1},

    [APU_OPCODE_NOP_IMP]        = {.instruction = APU_INST_NOP,   .addr_mode = APU_ADDR_MODE_IMP,       .width = 1},

    [APU_OPCODE_NOT1_DP_BIT]    = {.instruction = APU_INST_NOT1,  .addr_mode = APU_ADDR_MODE_DP_BIT,    .width = 3},
    [APU_OPCODE_NOTC_IMP]       = {.instruction = APU_INST_NOTC,  .addr_mode = APU_ADDR_MODE_IMP,       .width = 1},

    [APU_OPCODE_OR_X_Y]         = {.instruction = APU_INST_OR,    .addr_mode = APU_ADDR_MODE_X_Y,       .width = 1},
    [APU_OPCODE_OR_A_IMM]       = {.instruction = APU_INST_OR,    .addr_mode = APU_ADDR_MODE_IMM,       .width = 2},
    [APU_OPCODE_OR_A_X]         = {.instruction = APU_INST_OR,    .addr_mode = APU_ADDR_MODE_X,         .width = 1},
    [APU_OPCODE_OR_A_DP_IND_Y]  = {.instruction = APU_INST_OR,    .addr_mode = APU_ADDR_MODE_DP_IND_Y,  .width = 2},
    [APU_OPCODE_OR_A_DP_X_IND]  = {.instruction = APU_INST_OR,    .addr_mode = APU_ADDR_MODE_DP_X_IND,  .width = 2},
    [APU_OPCODE_OR_A_DP]        = {.instruction = APU_INST_OR,    .addr_mode = APU_ADDR_MODE_DP,        .width = 2},
    [APU_OPCODE_OR_A_DP_X]      = {.instruction = APU_INST_OR,    .addr_mode = APU_ADDR_MODE_DP_X,      .width = 2},
    [APU_OPCODE_OR_A_ABS]       = {.instruction = APU_INST_OR,    .addr_mode = APU_ADDR_MODE_ABS,       .width = 3},
    [APU_OPCODE_OR_A_ABS_X]     = {.instruction = APU_INST_OR,    .addr_mode = APU_ADDR_MODE_ABS_X,     .width = 3},
    [APU_OPCODE_OR_A_ABS_Y]     = {.instruction = APU_INST_OR,    .addr_mode = APU_ADDR_MODE_ABS_Y,     .width = 3},
    [APU_OPCODE_OR_DP_DP]       = {.instruction = APU_INST_OR,    .addr_mode = APU_ADDR_MODE_DP_DP,     .width = 3},
    [APU_OPCODE_OR_DP_IMM]      = {.instruction = APU_INST_OR,    .addr_mode = APU_ADDR_MODE_DP_IMM,    .width = 3},
    [APU_OPCODE_OR1N_ABS_BIT]   = {.instruction = APU_INST_OR,    .addr_mode = APU_ADDR_MODE_DP,        .width = 3},
    [APU_OPCODE_OR1_ABS_BIT]    = {.instruction = APU_INST_OR,    .addr_mode = APU_ADDR_MODE_DP_BIT,    .width = 3},

    [APU_OPCODE_PCALL_IMM]      = {.instruction = APU_INST_PCALL, .addr_mode = APU_ADDR_MODE_IMM,       .width = 2},

    [APU_OPCODE_POP_A_S]        = {.instruction = APU_INST_POP,   .addr_mode = APU_ADDR_MODE_S,         .width = 1},
    [APU_OPCODE_POP_PSW_S]      = {.instruction = APU_INST_POP,   .addr_mode = APU_ADDR_MODE_S,         .width = 1},
    [APU_OPCODE_POP_X_S]        = {.instruction = APU_INST_POP,   .addr_mode = APU_ADDR_MODE_S,         .width = 1},
    [APU_OPCODE_POP_Y_S]        = {.instruction = APU_INST_POP,   .addr_mode = APU_ADDR_MODE_S,         .width = 1},

    [APU_OPCODE_PUSH_A_S]       = {.instruction = APU_INST_PUSH,  .addr_mode = APU_ADDR_MODE_S,         .width = 1},
    [APU_OPCODE_PUSH_PSW_S]     = {.instruction = APU_INST_PUSH,  .addr_mode = APU_ADDR_MODE_S,         .width = 1},
    [APU_OPCODE_PUSH_X_S]       = {.instruction = APU_INST_PUSH,  .addr_mode = APU_ADDR_MODE_S,         .width = 1},
    [APU_OPCODE_PUSH_Y_S]       = {.instruction = APU_INST_PUSH,  .addr_mode = APU_ADDR_MODE_S,         .width = 1},

    [APU_OPCODE_RET_S]          = {.instruction = APU_INST_RET,   .addr_mode = APU_ADDR_MODE_S,         .width = 1},
    [APU_OPCODE_RETI_S]         = {.instruction = APU_INST_RETI,  .addr_mode = APU_ADDR_MODE_S,         .width = 1},

    [APU_OPCODE_ROL_A_IMP]      = {.instruction = APU_INST_ROL,   .addr_mode = APU_ADDR_MODE_IMP,       .width = 1},
    [APU_OPCODE_ROL_DP]         = {.instruction = APU_INST_ROL,   .addr_mode = APU_ADDR_MODE_DP,        .width = 2},
    [APU_OPCODE_ROL_DP_X]       = {.instruction = APU_INST_ROL,   .addr_mode = APU_ADDR_MODE_DP_X,      .width = 2},
    [APU_OPCODE_ROL_ABS]        = {.instruction = APU_INST_ROL,   .addr_mode = APU_ADDR_MODE_ABS,       .width = 3},

    [APU_OPCODE_ROR_A_IMP]      = {.instruction = APU_INST_ROR,   .addr_mode = APU_ADDR_MODE_IMP,       .width = 1},
    [APU_OPCODE_ROR_DP]         = {.instruction = APU_INST_ROR,   .addr_mode = APU_ADDR_MODE_DP,        .width = 2},
    [APU_OPCODE_ROR_DP_X]       = {.instruction = APU_INST_ROR,   .addr_mode = APU_ADDR_MODE_DP_X,      .width = 2},
    [APU_OPCODE_ROR_ABS]        = {.instruction = APU_INST_ROR,   .addr_mode = APU_ADDR_MODE_ABS,       .width = 3},

    [APU_OPCODE_SBC_X_Y]        = {.instruction = APU_INST_SBC,   .addr_mode = APU_ADDR_MODE_X_Y,       .width = 1},
    [APU_OPCODE_SBC_A_IMM]      = {.instruction = APU_INST_SBC,   .addr_mode = APU_ADDR_MODE_IMM,       .width = 2},
    [APU_OPCODE_SBC_A_X]        = {.instruction = APU_INST_SBC,   .addr_mode = APU_ADDR_MODE_X,         .width = 1},
    [APU_OPCODE_SBC_A_DP_IND_Y] = {.instruction = APU_INST_SBC,   .addr_mode = APU_ADDR_MODE_DP_IND_Y,  .width = 2},
    [APU_OPCODE_SBC_A_DP_X_IND] = {.instruction = APU_INST_SBC,   .addr_mode = APU_ADDR_MODE_DP_X_IND,  .width = 2},
    [APU_OPCODE_SBC_A_DP]       = {.instruction = APU_INST_SBC,   .addr_mode = APU_ADDR_MODE_DP,        .width = 2},
    [APU_OPCODE_SBC_A_DP_X]     = {.instruction = APU_INST_SBC,   .addr_mode = APU_ADDR_MODE_DP_X,      .width = 2},
    [APU_OPCODE_SBC_A_ABS]      = {.instruction = APU_INST_SBC,   .addr_mode = APU_ADDR_MODE_ABS,       .width = 3},
    [APU_OPCODE_SBC_A_ABX_X]    = {.instruction = APU_INST_SBC,   .addr_mode = APU_ADDR_MODE_ABS_X,     .width = 3},
    [APU_OPCODE_SBC_A_ABS_Y]    = {.instruction = APU_INST_SBC,   .addr_mode = APU_ADDR_MODE_ABS_Y,     .width = 3},
    [APU_OPCODE_SBC_DP_DP]      = {.instruction = APU_INST_SBC,   .addr_mode = APU_ADDR_MODE_DP_DP,     .width = 3},
    [APU_OPCODE_SBC_DP_IMM]     = {.instruction = APU_INST_SBC,   .addr_mode = APU_ADDR_MODE_DP_IMM,    .width = 3},
    [APU_OPCODE_SUBW_DP]        = {.instruction = APU_INST_SUBW,  .addr_mode = APU_ADDR_MODE_DP,        .width = 2},

    [APU_OPCODE_SET10_DP]       = {.instruction = APU_INST_SET1,  .addr_mode = APU_ADDR_MODE_DP,        .width = 2},
    [APU_OPCODE_SET11_DP]       = {.instruction = APU_INST_SET1,  .addr_mode = APU_ADDR_MODE_DP,        .width = 2},
    [APU_OPCODE_SET12_DP]       = {.instruction = APU_INST_SET1,  .addr_mode = APU_ADDR_MODE_DP,        .width = 2},
    [APU_OPCODE_SET13_DP]       = {.instruction = APU_INST_SET1,  .addr_mode = APU_ADDR_MODE_DP,        .width = 2},
    [APU_OPCODE_SET14_DP]       = {.instruction = APU_INST_SET1,  .addr_mode = APU_ADDR_MODE_DP,        .width = 2},
    [APU_OPCODE_SET15_DP]       = {.instruction = APU_INST_SET1,  .addr_mode = APU_ADDR_MODE_DP,        .width = 2},
    [APU_OPCODE_SET16_DP]       = {.instruction = APU_INST_SET1,  .addr_mode = APU_ADDR_MODE_DP,        .width = 2},
    [APU_OPCODE_SET17_DP]       = {.instruction = APU_INST_SET1,  .addr_mode = APU_ADDR_MODE_DP,        .width = 2},

    [APU_OPCODE_SETC_IMP]       = {.instruction = APU_INST_SETC,  .addr_mode = APU_ADDR_MODE_IMP,       .width = 1},
    [APU_OPCODE_SETP_IMP]       = {.instruction = APU_INST_SETP,  .addr_mode = APU_ADDR_MODE_IMP,       .width = 1},

    [APU_OPCODE_SLEEP_IMP]      = {.instruction = APU_INST_SLEEP, .addr_mode = APU_ADDR_MODE_IMP,       .width = 1},
    [APU_OPCODE_STOP_IMP]       = {.instruction = APU_INST_STOP,  .addr_mode = APU_ADDR_MODE_IMP,       .width = 1},

    [APU_OPCODE_TCALL0_IMP]     = {.instruction = APU_INST_TCALL, .addr_mode = APU_ADDR_MODE_IMP,       .width = 1},
    [APU_OPCODE_TCALL1_IMP]     = {.instruction = APU_INST_TCALL, .addr_mode = APU_ADDR_MODE_IMP,       .width = 1},
    [APU_OPCODE_TCALL2_IMP]     = {.instruction = APU_INST_TCALL, .addr_mode = APU_ADDR_MODE_IMP,       .width = 1},
    [APU_OPCODE_TCALL3_IMP]     = {.instruction = APU_INST_TCALL, .addr_mode = APU_ADDR_MODE_IMP,       .width = 1},
    [APU_OPCODE_TCALL4_IMP]     = {.instruction = APU_INST_TCALL, .addr_mode = APU_ADDR_MODE_IMP,       .width = 1},
    [APU_OPCODE_TCALL5_IMP]     = {.instruction = APU_INST_TCALL, .addr_mode = APU_ADDR_MODE_IMP,       .width = 1},
    [APU_OPCODE_TCALL6_IMP]     = {.instruction = APU_INST_TCALL, .addr_mode = APU_ADDR_MODE_IMP,       .width = 1},
    [APU_OPCODE_TCALL7_IMP]     = {.instruction = APU_INST_TCALL, .addr_mode = APU_ADDR_MODE_IMP,       .width = 1},
    [APU_OPCODE_TCALL8_IMP]     = {.instruction = APU_INST_TCALL, .addr_mode = APU_ADDR_MODE_IMP,       .width = 1},
    [APU_OPCODE_TCALL9_IMP]     = {.instruction = APU_INST_TCALL, .addr_mode = APU_ADDR_MODE_IMP,       .width = 1},
    [APU_OPCODE_TCALL10_IMP]    = {.instruction = APU_INST_TCALL, .addr_mode = APU_ADDR_MODE_IMP,       .width = 1},
    [APU_OPCODE_TCALL11_IMP]    = {.instruction = APU_INST_TCALL, .addr_mode = APU_ADDR_MODE_IMP,       .width = 1},
    [APU_OPCODE_TCALL12_IMP]    = {.instruction = APU_INST_TCALL, .addr_mode = APU_ADDR_MODE_IMP,       .width = 1},
    [APU_OPCODE_TCALL13_IMP]    = {.instruction = APU_INST_TCALL, .addr_mode = APU_ADDR_MODE_IMP,       .width = 1},
    [APU_OPCODE_TCALL14_IMP]    = {.instruction = APU_INST_TCALL, .addr_mode = APU_ADDR_MODE_IMP,       .width = 1},
    [APU_OPCODE_TCALL15_IMP]    = {.instruction = APU_INST_TCALL, .addr_mode = APU_ADDR_MODE_IMP,       .width = 1},

    [APU_OPCODE_TCLR1_ABS]      = {.instruction = APU_INST_TCLR1, .addr_mode = APU_ADDR_MODE_ABS,       .width = 3},
    [APU_OPCODE_TSET1_ABS]      = {.instruction = APU_INST_TSET1, .addr_mode = APU_ADDR_MODE_ABS,       .width = 3},

    [APU_OPCODE_XCN_IMP]        = {.instruction = APU_INST_XCN,   .addr_mode = APU_ADDR_MODE_IMP,       .width = 1},
};

void apu_Init()
{
    // io_regs[0].read = 0xaa;
    // io_regs[1].read = 0xbb;

    apu_ram = calloc(1, APU_RAM_SIZE);

    apu_Reset();

    apu_state.ports[0].read = 0xaa;
    apu_state.ports[1].read = 0xbb;
} 

void apu_Reset()
{
    /* Motorola MCM51L832F12 SRAM behavior, as described in nocash's doc */
    for(uint32_t offset = 0; offset < APU_RAM_SIZE;)
    {
        for(uint32_t index = 0; index < 32; index++)
        {
            apu_ram[offset] = 0x00;
            offset++;
        }

        for(uint32_t index = 0; index < 32; index++)
        {
            apu_ram[offset] = 0xff;
            offset++;
        }
    }

    memcpy(apu_ram + APU_ROM_START, apu_ipl_boot_rom, sizeof(apu_ipl_boot_rom));

    apu_state.regs[APU_REG_PC].word = 0xfffe;
    apu_state.regs[APU_REG_SP].word = 0x01ff;
    apu_state.regs[APU_REG_INST].word = APU_OPCODE_RESET;
    // apu_state.regs[APU_REG_DP].word = 0;
    apu_state.reg_psw.c = 0;
    apu_state.reg_psw.z = 0;
    apu_state.reg_psw.h = 0;
    apu_state.reg_psw.p = 0;
    apu_state.reg_psw.v = 0;
    apu_state.reg_psw.n = 0;

    apu_state.ports[0].read = 0;
    apu_state.ports[0].write = 0;
    apu_state.ports[1].read = 0;
    apu_state.ports[1].write = 0;
    apu_state.ports[2].read = 0;
    apu_state.ports[2].write = 0;
    apu_state.ports[3].read = 0;
    apu_state.ports[3].write = 0;

    apu_state.master_cycles = 0;
    apu_state.master_cycle_remainder = 0.0f;
    
    apu_LoadInstruction();
}

void apu_Step(int32_t master_cycle_count)
{
    // apu_state.master_cycles += master_cycle_count;
    // float fract_master_cycles = (float)apu_state.master_cycles + apu_state.master_cycle_remainder;
    // apu_state.uop_cycles += (int32_t)floorf(fract_master_cycles * APU_APU_TO_MASTER_CYCLE_RATIO);

    // while(apu_state.uop_cycles > 0)
    // {
    //     int32_t apu_master_cycles = (int32_t)floorf((float)apu_state.uop_cycles * APU_MASTER_TO_APU_CYCLE_RATIO);
    //     apu_state.master_cycles -= apu_master_cycles;
    //     apu_state.master_cycle_remainder = fract_master_cycles - (float)apu_master_cycles;

    //     while(apu_state.uop->func != NULL)
    //     {
    //         if(!apu_state.uop->func(apu_state.uop->arg))
    //         {
    //             break;
    //         }

    //         apu_NextUop();
    //     }

    //     if(apu_state.uop->func == NULL)
    //     {
    //         apu_state.regs[APU_REG_INST].word = APU_OPCODE_FETCH;
    //         apu_state.instruction_address = apu_state.regs[APU_REG_PC].word;
    //         apu_LoadInstruction();
    //     }
    // }    

    switch(apu_transfer_state)
    {
        case APU_STATE_IDLE:
            if(apu_state.ports[0].read == 0xcc && apu_state.ports[1].read != 0x00)
            {
                apu_transfer_state = APU_STATE_START_TRANSFER;
                apu_state.ports[0].write = 0xcc;
            }
            else
            {
                apu_state.ports[0].write = 0xaa;
                apu_state.ports[1].write = 0xbb;
            }
        break;

        case APU_STATE_START_TRANSFER:
            if(apu_state.ports[0].read == 0x00)
            {
                apu_state.ports[0].write = 0x00;
                apu_transfer_state = APU_STATE_TRANSFER;
            }
        break;

        case APU_STATE_TRANSFER:
            uint8_t status = apu_state.ports[0].read - apu_state.ports[0].write;
            if(status == 1)
            {
                apu_state.ports[0].write = apu_state.ports[0].read;
            }
            else if(status > 1)
            {
                if(apu_state.ports[1].read == 0x00)
                {
                    apu_transfer_state = APU_STATE_END_TRANSFER;
                    end_transfer_timer = 0;
                }
                else
                {
                    apu_transfer_state = APU_STATE_START_TRANSFER;
                }
                apu_state.ports[0].write = apu_state.ports[0].read;
            }
        break;

        case APU_STATE_END_TRANSFER:
            end_transfer_timer++;
            if(end_transfer_timer >= 0x3f)
            {
                apu_state.ports[0].read = 0x00;
                apu_transfer_state = APU_STATE_IDLE;
            }
        break;
    }
}

void apu_LoadInstruction()
{
    apu_state.instruction = apu_instructions + apu_state.regs[APU_REG_INST].word;
    apu_state.uop_index = 0;
    apu_LoadUop();
}


void apu_LoadUop()
{
    apu_state.uop = apu_state.instruction->uops + apu_state.uop_index;
    apu_state.last_uop = apu_state.instruction->uops[apu_state.uop_index + 1].func == NULL;
}

void apu_NextUop()
{
    apu_state.uop_index++;
    apu_LoadUop();
}

void apu_InitDisasm(struct apu_disasm_state_t *disasm_state)
{
    disasm_state->reg_pc = apu_state.instruction_address;
}

void apu_Disasm(struct apu_disasm_state_t *disasm_state, struct apu_disasm_inst_t *instruction)
{
    uint8_t opcode = apu_ram[disasm_state->reg_pc];
    struct apu_inst_info_t *info = apu_instruction_info + opcode;

    instruction->width = info->width;
    instruction->opcode_str = apu_instruction_strs[info->instruction];
    instruction->bytes[0] = opcode;
    instruction->operand_str[0][0] = '\0';
    instruction->operand_str[1][0] = '\0';

    for(uint32_t index = 1; index < info->width; index++)
    {
        instruction->bytes[index] = apu_ram[disasm_state->reg_pc + index];
    }

    switch(info->addr_mode)
    {
        case APU_ADDR_MODE_X_Y:
        {
            sprintf(instruction->operand_str[0], "(X) = %02x", apu_ram[apu_state.regs[APU_REG_X].byte[0]]);
            sprintf(instruction->operand_str[1], "(Y) = %02x", apu_ram[apu_state.regs[APU_REG_Y].byte[0]]);
        }
        break;

        case APU_ADDR_MODE_DP_DP:
        {
            sprintf(instruction->operand_str[0], "(d(%02x)) = %02x", instruction->bytes[1], apu_ram[instruction->bytes[1]]);
            sprintf(instruction->operand_str[1], "(s(%02x)) = %02x", instruction->bytes[2], apu_ram[instruction->bytes[2]]);
        }
        break;

        case APU_ADDR_MODE_DP_IMM:
        {
            sprintf(instruction->operand_str[0], apu_addr_mode_strs[APU_ADDR_MODE_DP], instruction->bytes[1], apu_ram[instruction->bytes[1]]);
            sprintf(instruction->operand_str[1], apu_addr_mode_strs[APU_ADDR_MODE_IMM], instruction->bytes[2]);
        }
        break;

        default:
        {
            uint32_t byte_offset = 1;
            for(uint32_t operand_index = 0; operand_index < 2; operand_index++)
            {
                if(info->operands[operand_index] == APU_OPERAND_MEM)
                {
                    switch(info->addr_mode)
                    {
                        // case APU_ADDR_MODE_IMM:
                        // {
                        //     sprintf(instruction->operand_str[operand_index], apu_addr_mode_strs[info->addr_mode], instruction->bytes[])
                        // }
                        // break;
                    }
                }
                else
                {
                    sprintf(instruction->operand_str[operand_index], apu_reg_strs[info->operands[operand_index]], apu_state.regs[info->operands[operand_index]].word);
                }
            }
        }
        break;
    }

    disasm_state->reg_pc += info->width;
}

uint8_t apu_ReadByte(uint16_t address)
{
    if(address >= APU_IO_REGS_START && address <= APU_IO_REGS_END)
    {
        uint32_t reg_index = address - APU_IO_REGS_START;

        if(apu_io_regs[reg_index].read != NULL)
        {
            return apu_io_regs[reg_index].read(address);
        }
    }

    return apu_ram[address];
}

void apu_WriteByte(uint16_t address, uint8_t value)
{
    if(address >= APU_IO_REGS_START && address <= APU_IO_REGS_END)
    {
        uint32_t reg_index = address - APU_IO_REGS_START;
        
        if(apu_io_regs[reg_index].read != NULL)
        {
            apu_io_regs[reg_index].write(address, value);
        }
    }

    /* writes to io registers also affect ram */
    if(address < APU_ROM_START)
    {
        apu_ram[address] = value;
    }
}

void apu_IoTestWrite(uint16_t address, uint8_t value)
{

}

uint8_t apu_IoTestRead(uint16_t address)
{

}

void apu_IoControlWrite(uint16_t address, uint8_t value)
{
    if(value & APU_IO_CONTROL_REG_FLAG_PC10)
    {
        apu_state.ports[0].read = 0;
        apu_state.ports[0].write = 0;
        apu_state.ports[1].read = 0;
        apu_state.ports[1].write = 0;
    }

    if(value & APU_IO_CONTROL_REG_FLAG_PC23)
    {
        apu_state.ports[2].read = 0;
        apu_state.ports[2].write = 0;
        apu_state.ports[3].read = 0;
        apu_state.ports[3].write = 0;
    }
}

uint8_t apu_IoControlRead(uint16_t address)
{

}

void apu_IoDspAddrWrite(uint16_t address, uint8_t value)
{

}

uint8_t apu_IoDspAddrRead(uint16_t address, uint8_t value)
{

}

void apu_IoDspDataWrite(uint16_t address, uint8_t value)
{

}

uint8_t apu_IoDspDataRead(uint16_t address)
{

}

void apu_IoPortWrite(uint16_t address, uint8_t value)
{
    uint32_t port_index = address - APU_IO_REGS_START;
    apu_state.ports[port_index].write = value;
}

uint8_t apu_IoPortRead(uint16_t address)
{
    uint32_t port_index = address - APU_IO_REGS_START;
    return apu_state.ports[port_index].read;
}

void apu_IoTimerDivWrite(uint16_t address, uint8_t value)
{

}

uint8_t apu_IoTimerDivRead(uint16_t address)
{

}

void apu_IoTimerOutWrite(uint16_t address, uint8_t value)
{

}

uint8_t apu_IoTimerOutRead(uint16_t address)
{

}


uint8_t apuio_read(uint32_t effective_address)
{
    uint32_t reg = (effective_address & 0xffff) - APU_MEM_REG_IO0;
    return apu_state.ports[reg].write;
}

void apuio_write(uint32_t effective_address, uint8_t data)
{
    uint32_t reg = (effective_address & 0xffff) - APU_MEM_REG_IO0;
    apu_state.ports[reg].read = data;
}

uint32_t apu_decode(uint32_t arg)
{
    apu_state.regs[APU_REG_INST].byte[1] = 0;
    apu_LoadInstruction();
    apu_state.uop_index--;
    return 1;
}

uint32_t apu_off_pc_r(uint32_t arg)
{
    uint32_t offset_reg = arg & 0xff;
    uint16_t offset = apu_state.regs[offset_reg].byte[0];

    if(offset & 0x80)
    {
        offset |= 0xff00;
    }

    apu_state.regs[APU_REG_PC].word += offset;
}

uint32_t apu_mov_lpc(uint32_t arg)
{
    arg |= (uint32_t)APU_REG_PC;
    uint32_t done = apu_mov_l(arg);
    apu_state.regs[APU_REG_PC].word += done;
    return done;
}

uint32_t apu_mov_l(uint32_t arg)
{
    uint32_t addr_reg = arg & 0xff;
    uint32_t dst_reg  = (arg >> 24) & 0xff;
    uint32_t byte = (arg >> 16) & 0xff;

    // uint32_t address = EFFECTIVE_ADDRESS(cpu_state.regs[bank_reg].byte[0], cpu_state.regs[addr_reg].word);
    // int32_t read_cycles = cpu_mem_cycles(address);

    uint16_t address = apu_state.regs[addr_reg].word;

    if(apu_state.uop_cycles > 0)
    {
        apu_state.uop_cycles--;
        apu_state.regs[dst_reg].byte[byte] = apu_ReadByte(address);
        return 1;
    }

    return 0;
}

uint32_t apu_mov_s(uint32_t arg)
{
    uint32_t addr_reg = arg & 0xff;
    uint32_t src_reg  = (arg >> 24) & 0xff;
    uint32_t byte = (arg >> 16) & 0xff;

    uint16_t address = apu_state.regs[addr_reg].word;

    if(apu_state.uop_cycles > 0)
    {
        apu_state.uop_cycles--;
        apu_WriteByte(address, apu_state.regs[src_reg].byte[byte]);
        return 1;
    }

    return 0;
}

uint32_t apu_mov_rr(uint32_t arg)
{
    uint32_t dst_reg = (arg >> 24) & 0xff;
    uint32_t src_reg = (arg >> 16) & 0xff;
    uint32_t width = (arg >> 8) & 0xff;

    if(width == MOV_RR_BYTE)
    {
        apu_state.regs[dst_reg].byte[0] = apu_state.regs[src_reg].byte[0];
    }
    else
    {
        apu_state.regs[dst_reg].word = apu_state.regs[src_reg].word;
    }

    return 1;
}

uint32_t apu_io(uint32_t arg)
{
    if(apu_state.uop_cycles > 0)
    {
        apu_state.uop_cycles--;
        return 1;
    }

    return 0;
}

uint32_t apu_skips(uint32_t arg)
{
    uint32_t count = arg & 0xff;
    uint32_t flag = (arg >> 8) & 0xff;

    if(apu_state.reg_psw.flags[flag])
    {
        apu_state.uop_index += count;
        apu_LoadUop();
    }

    return 1;
}

uint32_t apu_skipc(uint32_t arg)
{
    uint32_t count = arg & 0xff;
    uint32_t flag = (arg >> 8) & 0xff;

    if(!apu_state.reg_psw.flags[flag])
    {
        apu_state.uop_index += count;
        apu_LoadUop();
    }

    return 1;
}

uint32_t apu_chk_zn(uint32_t arg)
{
    uint32_t reg_index = arg & 0xff;
    uint32_t width = (arg >> 8) & 0xff; 
    union apu_reg_t *reg = apu_state.regs + reg_index;
    apu_state.reg_psw.z = !(reg->word & apu_alu_zero_masks[width]);
    apu_state.reg_psw.n = (reg->word & apu_alu_sign_masks[width]) && 1;
    return 1;
}

uint32_t apu_alu_op(uint32_t arg)
{
    uint32_t operand_b_reg = (arg >> 24) & 0xff;
    uint32_t operand_a_reg = (arg >> 16) & 0xff;
    uint32_t operation = (arg >> 8) & 0xff;
    // uint32_t width_flag = arg & 0xff;
    uint32_t width = arg & 0xff;
//    uint8_t flags = cpu_state.regs[CPU_REG_P].byte[cpu_state.reg_e];

    uint32_t sign_mask = apu_alu_sign_masks[width];
    uint32_t carry_mask = apu_alu_carry_masks[width];
    uint32_t half_carry_mask = apu_alu_half_carry_masks[width];
    uint32_t zero_mask = apu_alu_zero_masks[width];
    uint32_t carry = apu_state.reg_psw.c;
    uint32_t carry_shift = apu_alu_carry_shifts[width];

    uint32_t operand0 = apu_state.regs[operand_a_reg].word & zero_mask;
    uint32_t operand1 = apu_state.regs[operand_b_reg].word & zero_mask;
    uint32_t result;

    switch(operation)
    {
        case APU_ALU_OP_CMP:
            carry = 1;
            operand1 ^= zero_mask;
            result = operand0 + operand1 + carry;
        break;

        case APU_ALU_OP_SUB:
            operand1 ^= zero_mask;
        case APU_ALU_OP_ADD:
            result = operand0 + operand1 + carry;
            /* if operand0 and operand1 have the same sign, and the result have a different
            sign than the operands, then overflow ocurred */
            apu_state.reg_psw.v = (((~(operand0 ^ operand1)) & (operand1 ^ result)) & sign_mask) && 1;
        break;

        /* AND/OR/XOR/INC/DEC doesn't affect the carry flag, so we just copy it to the final result to 
        reuse the code that tests for carries */
        case APU_ALU_OP_AND:
            result = (operand0 & operand1) | (carry << carry_shift);
        break;

        case APU_ALU_OP_OR:
            result = (operand0 | operand1) | (carry << carry_shift);
        break;

        case APU_ALU_OP_XOR:
            result = (operand0 ^ operand1) | (carry << carry_shift);
        break;
    
        case APU_ALU_OP_INC:
            result = ((operand0 + 1) & zero_mask) | (carry << carry_shift);
        break;

        case APU_ALU_OP_DEC:
            result = ((operand0 - 1) & zero_mask) | (carry << carry_shift);
        break;

        case APU_ALU_OP_SHL:
            /* shift left shifts msb into the carry */
            carry = 0;
        case APU_ALU_OP_ROL:
            /* rotate left shifts the carry into the lsb and the
            msb into the carry */
            result = (operand0 << 1) | carry;
        break;


        case APU_ALU_OP_ROR:
            /* rotate right shifts the carry into the msb and the lsb
            into the carry */
            operand0 |= carry << carry_shift;
        case APU_ALU_OP_SHR:
            carry = (operand0 & 1) << carry_shift;
            /* shift right shifts lsb into the carry */
            result = (operand0 >> 1) | carry;
        break;

        // case APU_ALU_OP_TRB:
        //     apu_state.regs[APU_REG_TEMP].word = (uint32_t)((~apu_state.regs[APU_REG_A].word) & zero_mask) & operand0;
        //     apu_state.reg_psw.z = !((apu_state.regs[APU_REG_ACCUM].word & operand0) & zero_mask);
        //     return 1;
        // break;

        // case ALU_OP_TSB:
        //     cpu_state.regs[CPU_REG_TEMP].word = (uint32_t)(cpu_state.regs[CPU_REG_ACCUM].word & zero_mask) | operand0;
        //     cpu_state.reg_p.z = !((cpu_state.regs[CPU_REG_ACCUM].word & operand0) & zero_mask);
        //     return 1;
        // break;

        // case ALU_OP_BIT:
        //     cpu_state.reg_p.n = (operand0 & sign_mask) && 1;
        //     cpu_state.reg_p.v = (operand0 & (sign_mask >> 1)) && 1;
        // case ALU_OP_BIT_IMM:
        //     cpu_state.reg_p.z = !((cpu_state.regs[CPU_REG_ACCUM].word & operand0) & zero_mask);
        //     return 1;
        // break;
    }

    // if(alu_op_carry_flag[operation])
    // {
    //     cpu_state.reg_p.c = (result & carry_mask) && 1;
    // }

//    cpu_state.reg_p.c = (result & carry_mask) && 1;
    apu_state.reg_psw.c = result > zero_mask;
    apu_state.regs[APU_REG_TEMP].word = result & zero_mask;
    apu_chk_zn((APU_REG_TEMP) | (width << 8));
    return 1;
}