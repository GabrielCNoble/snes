#include "cpu.h"
#include "ppu.h"
#include "cart.h"
#include "mem.h"
// #include "uop.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <stdint.h>
#include <ctype.h>

unsigned long step_count = 0;

struct cpu_state_t cpu_state = {
//    .in_irqb = 1,
//    .in_rdy = 1,
//    .in_resb = 1,
//    .in_abortb = 1,
//    .in_nmib = 1,
    .regs = (struct reg_t []) {
        [CPU_REG_ACCUM]         = { .flag = CPU_STATUS_FLAG_M },
        [CPU_REG_X]             = { .flag = CPU_STATUS_FLAG_X },
        [CPU_REG_Y]             = { .flag = CPU_STATUS_FLAG_X },
        [CPU_REG_D]             = { },
        [CPU_REG_S]             = { },
        [CPU_REG_PBR]           = { },
        [CPU_REG_DBR]           = { },
        [CPU_REG_INST]          = { },
        [CPU_REG_PC]            = { },
        // [CPU_REG_P]             = { .flag = 0x0000},
        [CPU_REG_ADDR]          = { },
        [CPU_REG_BANK]          = { },
        [CPU_REG_ZERO]          = { },
    },
};

uint32_t mem_speed_map[][2] = {
    {MEM_SPEED_MED_CYCLES,  MEM_SPEED_FAST_CYCLES},
    {MEM_SPEED_MED_CYCLES,  MEM_SPEED_MED_CYCLES},
    {MEM_SPEED_FAST_CYCLES, MEM_SPEED_FAST_CYCLES},
    {MEM_SPEED_SLOW_CYCLES, MEM_SPEED_SLOW_CYCLES},
    {MEM_SPEED_FAST_CYCLES, MEM_SPEED_FAST_CYCLES},
    {MEM_SPEED_MED_CYCLES,  MEM_SPEED_MED_CYCLES}
};

uint16_t interrupt_vectors[][2] = {
    [CPU_INT_BRK] = {0xffe6, 0xfffe},
    [CPU_INT_IRQ] = {0xffee, 0xfffe},
    [CPU_INT_NMI] = {0xffea, 0xfffa},
    [CPU_INT_COP] = {0xffe4, 0xfff4},
    [CPU_INT_RES] = {0xfffc, 0xfffc}
};

uint32_t            cpu_cycle_count = 0;
extern uint64_t     master_cycles;
extern uint8_t *    ram1_regs;
extern uint8_t      last_bus_value;
extern uint16_t     vcounter;
extern uint16_t     hcounter;
// extern uint32_t     scanline_cycles;

#define ALU_WIDTH_WORD 0
#define ALU_WIDTH_BYTE 1

uint8_t alu_op_carry_flag[ALU_OP_LAST] =
{
    [ALU_OP_ADD] = 1,
    [ALU_OP_SUB] = 1,
    [ALU_OP_SHL] = 0,
    [ALU_OP_SHR] = 0,
    [ALU_OP_ROR] = 0,
    [ALU_OP_ROL] = 0,
    [ALU_OP_CMP] = 1,
};

uint32_t alu_zero_masks[] = {
    [0] = 0x0000ffff,
    [1] = 0x000000ff
};

uint32_t alu_sign_masks[] = {
    [0] = 0x00008000,
    [1] = 0x00000080
};

uint32_t alu_carry_masks[] = {
    [0] = 0x00010000,
    [1] = 0x00000100
};

uint32_t alu_carry_shifts[] = {
    [0] = 16,
    [1] = 8
};

/* cpu uops */

#define UOP(fn, arg) ((struct uop_t){(fn), (arg)})

uint32_t inc_pc(uint32_t arg);
/* increment PC register by [inc] */
#define INC_PC(inc) UOP(inc_pc, inc)

uint32_t decode(uint32_t arg);
/* decode instruction at PBR:PC */
#define DECODE UOP(decode, 0)

enum MOV_BYTES
{
    MOV_LSB = 0,
    MOV_MSB
};

uint32_t mov_lpc(uint32_t arg);
/* load [dst_reg] low/high byte ([byte_index]) with the value at PBR:PC, and then increment PC */
#define MOV_LPC(byte_index, dst_reg) UOP(mov_lpc, ((uint32_t)dst_reg << 24) | ((uint32_t)byte_index))

uint32_t mov_l(uint32_t arg);
/* load [dst_reg] low/high byte ([byte_index]) from [bank_reg]:[addr_reg] */
#define MOV_L(byte_index, addr_reg, bank_reg, dst_reg) UOP(mov_l, (((uint32_t)dst_reg << 24)) | ((uint32_t)bank_reg << 16) | ((uint32_t)addr_reg << 8) | ((uint32_t)byte_index))

uint32_t mov_s(uint32_t arg);
/* store [dst_reg] low/high byte ([byte_index]) at [bank_reg]:[addr_reg] */
#define MOV_S(byte_index, addr_reg, bank_reg, src_reg) UOP(mov_s, ((uint32_t)src_reg << 24) | ((uint32_t)bank_reg << 16) | ((uint32_t)addr_reg << 8) | ((uint32_t)byte_index))

enum MOV_RRW_WIDTH
{
    MOV_RRW_BYTE = 1,
    MOV_RRW_WORD = 0,
};

uint32_t mov_rrw(uint32_t arg);
/* mov [src] to [dst], explicit [width] */
#define MOV_RRW(src, dst, width) UOP(mov_rrw, ((uint32_t)(width) << 16) | ((uint32_t)(src) << 8) | ((uint32_t)(dst)))

uint32_t mov_rr(uint32_t arg);
/* mov [src] to [dst], implicit width based on MX flags */
#define MOV_RR(src, dst) UOP(mov_rr, ((uint32_t)(src) << 8) | ((uint32_t)(dst)))

uint32_t mov_p(uint32_t arg);
/* load/store the P register */
#define MOV_P(src, dst) UOP(mov_p, ((uint32_t)(src) << 8) | ((uint32_t)(dst)))

uint32_t decs(uint32_t arg);
/* decrement the S register */
#define DECS UOP(decs, 0)

uint32_t dec_rw(uint32_t arg);
/* decrement word register [reg] */
#define DEC_RW(reg) UOP(dec_rw, reg)

uint32_t dec_rb(uint32_t arg);
/* decrement byte register [reg] */
#define DEC_RB(reg) UOP(dec_rb, reg)

uint32_t incs(uint32_t arg);
/* increment the S register */
#define INCS UOP(incs, 0)

uint32_t inc_rw(uint32_t arg);
/* increment word register [reg] */
#define INC_RW(reg) UOP(inc_rw, reg)

uint32_t inc_rb(uint32_t arg);
/* increment byte register [reg] */
#define INC_RB(reg) UOP(inc_rb, reg)

uint32_t zext(uint32_t arg);
/* zero extend byte register [reg] */
#define ZEXT(reg) UOP(zext, reg)

uint32_t sext(uint32_t arg);
/* sign extend byte register [reg] */
#define SEXT(reg) UOP(sext, reg)

uint32_t set_p(uint32_t arg);
/* set flags [flag] */
#define SET_P(flag) UOP(set_p, flag)

uint32_t clr_p(uint32_t arg);
/* clear flags [flag] */
#define CLR_P(flag) UOP(clr_p, flag)

uint32_t xce(uint32_t arg);
/* XCE */
#define XCE UOP(xce, 0)

uint32_t xba(uint32_t arg);
/* XBA */
#define XBA UOP(xba, 0)

uint32_t wai(uint32_t);
/* WAI */
#define WAI UOP(wai, 0)

uint32_t io(uint32_t arg);
/* arbitrary internal operation (idles the cpu) */
#define IO UOP(io, 0)


enum ADDR_OFF_BANK
{
    ADDR_OFF_BANK_NEXT = 1,
    ADDR_OFF_BANK_WRAP = 0,
};

enum ADDR_OFF_SIGN
{
    ADDR_OFF_SIGNED = 1,
    ADDR_OFF_UNSIGNED = 0
};

uint32_t addr_offr(uint32_t arg);
/*  offset ADDR register by word register [addr_reg], with explicit
offset sign ([offset_sign]) with or without bank wrapping ([bank_wrap]) */
#define ADDR_OFFRS(addr_reg, bank_wrap, offset_sign) UOP(addr_offr, ((uint32_t)offset_sign << 16) | ((uint32_t)bank_wrap << 8) | ((uint32_t)addr_reg))
/* unsigned offset ADDR register by word register [addr_reg], with or without bank wrapping ([bank_wrap])*/
#define ADDR_OFFR(addr_reg, bank_wrap) ADDR_OFFRS(addr_reg, bank_wrap, ADDR_OFF_UNSIGNED)

uint32_t addr_offi(uint32_t arg);
/*  offset ADDR register by word constant [offset], with explicit
offset sign ([offset_sign]) with or without bank wrapping ([bank_wrap]) */
#define ADDR_OFFIS(offset, bank_wrap, offset_sign) UOP(addr_offi, ((uint32_t)offset_sign << 24) | ((uint32_t)bank_wrap << 16) | ((uint32_t)offset))
/* unsigned offset ADDR register by word constant [offset], with or without bank wrapping ([bank_wrap])*/
#define ADDR_OFFI(offset, bank_wrap) ADDR_OFFIS(offset, bank_wrap, ADDR_OFF_UNSIGNED)

uint32_t skips(uint32_t arg);
/* skip [count] uops if [flag] is set */
#define SKIPS(count, flag) UOP(skips, ((uint32_t)flag << 8) | ((uint32_t)count))

uint32_t skipc(uint32_t arg);
/* skip [count] uops if [flag] is clear */
#define SKIPC(count, flag) UOP(skipc, ((uint32_t)flag << 8) | ((uint32_t)count))

enum CHK_ZW_WIDTH
{
    CHK_ZW_BYTE = 1,
    CHK_ZW_WORD = 0
};

uint32_t chk_znw(uint32_t arg);
/* test register [reg] and set Z and N flags, with explicit width [width]  */
#define CHK_ZNW(reg, width) UOP(chk_znw, ((uint32_t)width << 8) | ((uint32_t)reg))

uint32_t chk_zn(uint32_t arg);
/* test register [reg] and set Z and N flags, with implicit width based on MX flags  */
#define CHK_ZN(reg) UOP(chk_zn, reg)

uint32_t alu_op(uint32_t arg);
/* perform alu op [op], with width [width], between register operands [operand_a] and [operand_b]*/
#define ALU_OP(op, width_flag, operand_a, operand_b) UOP(alu_op, ((uint32_t)operand_b << 24) | ((uint32_t)operand_a << 16) | ((uint32_t)op << 8) | ((uint32_t)width_flag))

uint32_t brk(uint32_t arg);
/* BRK */
#define BRK UOP(brk, 0)

uint32_t cop(uint32_t arg);
/* COP */
#define COP UOP(cop, 0)

uint32_t stp(uint32_t arg);
/* STP */
#define STP UOP(stp, 0)

/* hangs the cpu when encountering an unknown instruction */
uint32_t unimplemented(uint32_t arg);
#define UNIMPLEMENTED UOP(unimplemented, 0);



#define OPCODE(op, cat, addr_mod) {.opcode = op, .address_mode = addr_mod, .opcode_class = cat}

struct inst_t fetch_inst = {
    .uops = {
        MOV_LPC         (MOV_LSB, CPU_REG_INST),
        DECODE
    }
};

struct inst_t instructions[] = {

    /**************************************************************************************/
    /*                                      ADC                                           */
    /**************************************************************************************/

    [CPU_OPCODE_ADC_ABS] = {
        .uops = {
            MOV_LPC     (MOV_LSB, CPU_REG_ADDR),
            MOV_LPC     (MOV_MSB, CPU_REG_ADDR),
            MOV_RRW     (CPU_REG_DBR, CPU_REG_BANK, MOV_RRW_BYTE),
            MOV_L       (MOV_LSB, CPU_REG_ADDR, CPU_REG_BANK, CPU_REG_TEMP),
            SKIPS       (2, CPU_STATUS_FLAG_M),
            ADDR_OFFI   (1, ADDR_OFF_BANK_NEXT),
            MOV_L       (MOV_MSB, CPU_REG_ADDR, CPU_REG_BANK, CPU_REG_TEMP),
            ALU_OP      (ALU_OP_ADD, CPU_STATUS_FLAG_M, CPU_REG_ACCUM, CPU_REG_TEMP),
            MOV_RR      (CPU_REG_TEMP, CPU_REG_ACCUM),
            // CHK_ZN      (CPU_REG_ACCUM),
        }
    },
    [CPU_OPCODE_ADC_ABS_X] = {
        .uops = {
            MOV_LPC     (MOV_LSB, CPU_REG_ADDR),
            MOV_LPC     (MOV_MSB, CPU_REG_ADDR),
            MOV_RRW     (CPU_REG_DBR, CPU_REG_BANK, MOV_RRW_BYTE),
            ADDR_OFFR   (CPU_REG_X, ADDR_OFF_BANK_NEXT),
            SKIPC       (2, CPU_STATUS_FLAG_PAGE),
            SKIPS       (1, CPU_STATUS_FLAG_X),
            IO          /* X=0 or page boundary */,
            MOV_L       (MOV_LSB, CPU_REG_ADDR, CPU_REG_BANK, CPU_REG_TEMP),
            SKIPS       (2, CPU_STATUS_FLAG_M),
            ADDR_OFFI   (1, ADDR_OFF_BANK_NEXT),
            MOV_L       (MOV_MSB, CPU_REG_ADDR, CPU_REG_BANK, CPU_REG_TEMP),
            ALU_OP      (ALU_OP_ADD, CPU_STATUS_FLAG_M, CPU_REG_ACCUM, CPU_REG_TEMP),
            MOV_RR      (CPU_REG_TEMP, CPU_REG_ACCUM),
            // CHK_ZN      (CPU_REG_ACCUM),
        }
    },
    [CPU_OPCODE_ADC_ABS_Y] = {
        .uops = {
            MOV_LPC     (MOV_LSB, CPU_REG_ADDR),
            MOV_LPC     (MOV_MSB, CPU_REG_ADDR),
            MOV_RRW     (CPU_REG_DBR, CPU_REG_BANK, MOV_RRW_BYTE),
            ADDR_OFFR   (CPU_REG_Y, ADDR_OFF_BANK_NEXT),
            SKIPC       (2, CPU_STATUS_FLAG_PAGE),
            SKIPS       (1, CPU_STATUS_FLAG_X),
            IO          /* X=0 or page boundary */,
            MOV_L       (MOV_LSB, CPU_REG_ADDR, CPU_REG_BANK, CPU_REG_TEMP),
            SKIPS       (2, CPU_STATUS_FLAG_M),
            ADDR_OFFI   (1, ADDR_OFF_BANK_NEXT),
            MOV_L       (MOV_MSB, CPU_REG_ADDR, CPU_REG_BANK, CPU_REG_TEMP),
            ALU_OP      (ALU_OP_ADD, CPU_STATUS_FLAG_M, CPU_REG_ACCUM, CPU_REG_TEMP),
            MOV_RR      (CPU_REG_TEMP, CPU_REG_ACCUM),
            // CHK_ZN      (CPU_REG_ACCUM),
        }
    },
    [CPU_OPCODE_ADC_ABSL] = {
        .uops = {
            MOV_LPC     (MOV_LSB, CPU_REG_ADDR),
            MOV_LPC     (MOV_MSB, CPU_REG_ADDR),
            MOV_LPC     (MOV_LSB, CPU_REG_BANK),
            MOV_L       (MOV_LSB, CPU_REG_ADDR, CPU_REG_BANK, CPU_REG_TEMP),
            SKIPS       (2, CPU_STATUS_FLAG_M),
            ADDR_OFFI   (1, ADDR_OFF_BANK_NEXT),
            MOV_L       (MOV_MSB, CPU_REG_ADDR, CPU_REG_BANK, CPU_REG_TEMP),
            ALU_OP      (ALU_OP_ADD, CPU_STATUS_FLAG_M, CPU_REG_ACCUM, CPU_REG_TEMP),
            MOV_RR      (CPU_REG_TEMP, CPU_REG_ACCUM),
            // CHK_ZN      (CPU_REG_ACCUM),
        }
    },
    [CPU_OPCODE_ADC_ABSL_X] = {
        .uops = {
            MOV_LPC     (MOV_LSB, CPU_REG_ADDR),
            MOV_LPC     (MOV_MSB, CPU_REG_ADDR),
            MOV_LPC     (MOV_LSB, CPU_REG_BANK),
            ADDR_OFFR   (CPU_REG_X, ADDR_OFF_BANK_NEXT),
            MOV_L       (MOV_LSB, CPU_REG_ADDR, CPU_REG_BANK, CPU_REG_TEMP),
            SKIPS       (2, CPU_STATUS_FLAG_M),
            ADDR_OFFI   (1, ADDR_OFF_BANK_NEXT),
            MOV_L       (MOV_MSB, CPU_REG_ADDR, CPU_REG_BANK, CPU_REG_TEMP),
            ALU_OP      (ALU_OP_ADD, CPU_STATUS_FLAG_M, CPU_REG_ACCUM, CPU_REG_TEMP),
            MOV_RR      (CPU_REG_TEMP, CPU_REG_ACCUM),
            // CHK_ZN      (CPU_REG_ACCUM),
        }
    },
    [CPU_OPCODE_ADC_DIR] = {
        .uops = {
            MOV_LPC     (MOV_LSB, CPU_REG_ADDR),
            ZEXT        (CPU_REG_ADDR),
            ADDR_OFFR   (CPU_REG_D, ADDR_OFF_BANK_WRAP),
            SKIPC       (1, CPU_STATUS_FLAG_DL),
            IO          /* DL != 0 */,
            MOV_RRW     (CPU_REG_ZERO, CPU_REG_BANK, MOV_RRW_BYTE),
            MOV_L       (MOV_LSB, CPU_REG_ADDR, CPU_REG_BANK, CPU_REG_TEMP),
            SKIPS       (2, CPU_STATUS_FLAG_M),
            ADDR_OFFI   (1, ADDR_OFF_BANK_NEXT),
            MOV_L       (MOV_MSB, CPU_REG_ADDR, CPU_REG_BANK, CPU_REG_TEMP),
            ALU_OP      (ALU_OP_ADD, CPU_STATUS_FLAG_M, CPU_REG_ACCUM, CPU_REG_TEMP),
            MOV_RR      (CPU_REG_TEMP, CPU_REG_ACCUM),
            // CHK_ZN      (CPU_REG_ACCUM),
        }
    },
    [CPU_OPCODE_ADC_S_REL] = {
        .uops = {
            MOV_LPC     (MOV_LSB, CPU_REG_ADDR),
            ZEXT        (CPU_REG_ADDR),
            ADDR_OFFR   (CPU_REG_S, ADDR_OFF_BANK_WRAP),
            IO,
            MOV_L       (MOV_LSB, CPU_REG_ADDR, CPU_REG_ZERO, CPU_REG_TEMP),
            SKIPS       (2, CPU_STATUS_FLAG_M),
            ADDR_OFFI   (1, ADDR_OFF_BANK_WRAP),
            MOV_L       (MOV_MSB, CPU_REG_ADDR, CPU_REG_ZERO, CPU_REG_TEMP),
            ALU_OP      (ALU_OP_ADD, CPU_STATUS_FLAG_M, CPU_REG_ACCUM, CPU_REG_TEMP),
            MOV_RR      (CPU_REG_TEMP, CPU_REG_ACCUM),
            // CHK_ZN      (CPU_REG_ACCUM),
        },
    },
    [CPU_OPCODE_ADC_DIR_X] = {
        .uops = {
            MOV_LPC     (MOV_LSB, CPU_REG_ADDR),
            ZEXT        (CPU_REG_ADDR),
            SKIPC       (1, CPU_STATUS_FLAG_DL),
            IO          /* DL != 0 */,
            ADDR_OFFR   (CPU_REG_D, ADDR_OFF_BANK_WRAP),
            ADDR_OFFR   (CPU_REG_X, ADDR_OFF_BANK_WRAP),
            IO,
            MOV_L       (MOV_LSB, CPU_REG_ADDR, CPU_REG_ZERO, CPU_REG_TEMP),
            SKIPS       (2, CPU_STATUS_FLAG_M),
            ADDR_OFFI   (1, ADDR_OFF_BANK_WRAP),
            MOV_L       (MOV_MSB, CPU_REG_ADDR, CPU_REG_ZERO, CPU_REG_TEMP),
            ALU_OP      (ALU_OP_ADD, CPU_STATUS_FLAG_M, CPU_REG_ACCUM, CPU_REG_TEMP),
            MOV_RR      (CPU_REG_TEMP, CPU_REG_ACCUM),
            // CHK_ZN      (CPU_REG_ACCUM),
        }
    },

    [CPU_OPCODE_ADC_DIR_IND] = {
        .uops = {
            MOV_LPC     (MOV_LSB, CPU_REG_ADDR),
            ZEXT        (CPU_REG_ADDR),
            SKIPC       (1, CPU_STATUS_FLAG_DL),
            IO          /* DL != 0 */,
            ADDR_OFFR   (CPU_REG_D, ADDR_OFF_BANK_WRAP),
            MOV_RRW     (CPU_REG_ZERO, CPU_REG_BANK, MOV_RRW_BYTE),
            MOV_L       (MOV_LSB, CPU_REG_ADDR, CPU_REG_BANK, CPU_REG_TEMP),
            ADDR_OFFI   (1, ADDR_OFF_BANK_NEXT),
            MOV_L       (MOV_MSB, CPU_REG_ADDR, CPU_REG_BANK, CPU_REG_TEMP),
            MOV_RRW     (CPU_REG_TEMP, CPU_REG_ADDR, MOV_RRW_WORD),
            MOV_RRW     (CPU_REG_DBR, CPU_REG_BANK, MOV_RRW_BYTE),
            MOV_L       (MOV_LSB, CPU_REG_ADDR, CPU_REG_BANK, CPU_REG_TEMP),
            SKIPS       (2, CPU_STATUS_FLAG_M),
            ADDR_OFFI   (1, ADDR_OFF_BANK_NEXT),
            MOV_L       (MOV_MSB, CPU_REG_ADDR, CPU_REG_BANK, CPU_REG_TEMP),
            ALU_OP      (ALU_OP_ADD, CPU_STATUS_FLAG_M, CPU_REG_ACCUM, CPU_REG_TEMP),
            MOV_RR      (CPU_REG_TEMP, CPU_REG_ACCUM),
            // CHK_ZN      (CPU_REG_ACCUM),
        }
    },

    [CPU_OPCODE_ADC_DIR_INDL] = {
        .uops = {
            MOV_LPC     (MOV_LSB, CPU_REG_ADDR),
            ZEXT        (CPU_REG_ADDR),
            SKIPC       (1, CPU_STATUS_FLAG_DL),
            IO          /* DL != 0 */,
            ADDR_OFFR   (CPU_REG_D, ADDR_OFF_BANK_WRAP),
            MOV_RRW     (CPU_REG_ZERO, CPU_REG_BANK, MOV_RRW_BYTE),
            MOV_L       (MOV_LSB, CPU_REG_ADDR, CPU_REG_BANK, CPU_REG_TEMP),
            ADDR_OFFI   (1, ADDR_OFF_BANK_NEXT),
            MOV_L       (MOV_MSB, CPU_REG_ADDR, CPU_REG_BANK, CPU_REG_TEMP),
            ADDR_OFFI   (1, ADDR_OFF_BANK_NEXT),
            MOV_L       (MOV_LSB, CPU_REG_ADDR, CPU_REG_BANK, CPU_REG_BANK),
            MOV_RRW     (CPU_REG_TEMP, CPU_REG_ADDR, MOV_RRW_WORD),
            MOV_L       (MOV_LSB, CPU_REG_ADDR, CPU_REG_BANK, CPU_REG_TEMP),
            SKIPS       (2, CPU_STATUS_FLAG_M),
            ADDR_OFFI   (1, ADDR_OFF_BANK_NEXT),
            MOV_L       (MOV_MSB, CPU_REG_ADDR, CPU_REG_BANK, CPU_REG_TEMP),
            ALU_OP      (ALU_OP_ADD, CPU_STATUS_FLAG_M, CPU_REG_ACCUM, CPU_REG_TEMP),
            MOV_RR      (CPU_REG_TEMP, CPU_REG_ACCUM),
            // CHK_ZN      (CPU_REG_ACCUM),
        }
    },

    [CPU_OPCODE_ADC_S_REL_IND_Y] = {
        .uops = {
            MOV_LPC     (MOV_LSB, CPU_REG_ADDR),
            ZEXT        (CPU_REG_ADDR),
            IO,
            ADDR_OFFR   (CPU_REG_S, ADDR_OFF_BANK_WRAP),
            MOV_L       (MOV_LSB, CPU_REG_ADDR, CPU_REG_ZERO, CPU_REG_TEMP),
            ADDR_OFFI   (1, ADDR_OFF_BANK_WRAP),
            MOV_L       (MOV_MSB, CPU_REG_ADDR, CPU_REG_ZERO, CPU_REG_TEMP),
            MOV_RRW     (CPU_REG_TEMP, CPU_REG_ADDR, MOV_RRW_WORD),
            MOV_RRW     (CPU_REG_DBR, CPU_REG_BANK, MOV_RRW_BYTE),
            IO,
            ADDR_OFFR   (CPU_REG_Y, ADDR_OFF_BANK_NEXT),
            MOV_L       (MOV_LSB, CPU_REG_ADDR, CPU_REG_BANK, CPU_REG_TEMP),
            SKIPS       (2, CPU_STATUS_FLAG_M),
            ADDR_OFFI   (1, ADDR_OFF_BANK_NEXT),
            MOV_L       (MOV_MSB, CPU_REG_ADDR, CPU_REG_BANK, CPU_REG_TEMP),
            ALU_OP      (ALU_OP_ADD, CPU_STATUS_FLAG_M, CPU_REG_ACCUM, CPU_REG_TEMP),
            MOV_RR      (CPU_REG_TEMP, CPU_REG_ACCUM),
            // CHK_ZN      (CPU_REG_ACCUM),

        }
    },

    [CPU_OPCODE_ADC_DIR_X_IND] = {
        .uops = {
            MOV_LPC     (MOV_LSB, CPU_REG_ADDR),
            ZEXT        (CPU_REG_ADDR),
            SKIPC       (1, CPU_STATUS_FLAG_DL),
            IO          /* DL != 0 */,
            ADDR_OFFR   (CPU_REG_D, ADDR_OFF_BANK_WRAP),
            ADDR_OFFR   (CPU_REG_X, ADDR_OFF_BANK_WRAP),
            IO,
            MOV_L       (MOV_LSB, CPU_REG_ADDR, CPU_REG_ZERO, CPU_REG_TEMP),
            ADDR_OFFI   (1, ADDR_OFF_BANK_WRAP),
            MOV_L       (MOV_MSB, CPU_REG_ADDR, CPU_REG_ZERO, CPU_REG_TEMP),
            MOV_RRW     (CPU_REG_TEMP, CPU_REG_ADDR, MOV_RRW_WORD),
            MOV_RRW     (CPU_REG_DBR, CPU_REG_BANK, MOV_RRW_BYTE),
            MOV_L       (MOV_LSB, CPU_REG_ADDR, CPU_REG_BANK, CPU_REG_TEMP),
            SKIPS       (2, CPU_STATUS_FLAG_M),
            ADDR_OFFI   (1, ADDR_OFF_BANK_NEXT),
            MOV_L       (MOV_MSB, CPU_REG_ADDR, CPU_REG_BANK, CPU_REG_TEMP),
            ALU_OP      (ALU_OP_ADD, CPU_STATUS_FLAG_M, CPU_REG_ACCUM, CPU_REG_TEMP),
            MOV_RR      (CPU_REG_TEMP, CPU_REG_ACCUM),
            // CHK_ZN      (CPU_REG_ACCUM),
        }
    },

    [CPU_OPCODE_ADC_DIR_IND_Y] = {
        .uops = {
            MOV_LPC     (MOV_LSB, CPU_REG_ADDR),
            ZEXT        (CPU_REG_ADDR),
            SKIPC       (1, CPU_STATUS_FLAG_DL),
            IO          /* DL != 0 */,
            ADDR_OFFR   (CPU_REG_D, ADDR_OFF_BANK_WRAP),
            MOV_RRW     (CPU_REG_ZERO, CPU_REG_BANK, MOV_RRW_BYTE),
            MOV_L       (MOV_LSB, CPU_REG_ADDR, CPU_REG_BANK, CPU_REG_TEMP),
            ADDR_OFFI   (1, ADDR_OFF_BANK_NEXT),
            MOV_L       (MOV_MSB, CPU_REG_ADDR, CPU_REG_BANK, CPU_REG_TEMP),
            MOV_RRW     (CPU_REG_TEMP, CPU_REG_ADDR, MOV_RRW_WORD),
            MOV_RRW     (CPU_REG_DBR, CPU_REG_BANK, MOV_RRW_BYTE),
            ADDR_OFFR   (CPU_REG_Y, ADDR_OFF_BANK_NEXT),
            SKIPS       (2, CPU_STATUS_FLAG_X),
            SKIPC       (1, CPU_STATUS_FLAG_PAGE),
            IO          /* X = 0 or page crossed */,
            MOV_L       (MOV_LSB, CPU_REG_ADDR, CPU_REG_BANK, CPU_REG_TEMP),
            SKIPS       (2, CPU_STATUS_FLAG_M),
            ADDR_OFFI   (1, ADDR_OFF_BANK_NEXT),
            MOV_L       (MOV_MSB, CPU_REG_ADDR, CPU_REG_BANK, CPU_REG_TEMP),
            ALU_OP      (ALU_OP_ADD, CPU_STATUS_FLAG_M, CPU_REG_ACCUM, CPU_REG_TEMP),
            MOV_RR      (CPU_REG_TEMP, CPU_REG_ACCUM),
            // CHK_ZN      (CPU_REG_ACCUM),
        }
    },

    [CPU_OPCODE_ADC_DIR_INDL_Y] = {
        .uops = {
            MOV_LPC     (MOV_LSB, CPU_REG_ADDR),
            ZEXT        (CPU_REG_ADDR),
            ADDR_OFFR   (CPU_REG_D, 0),
            SKIPC       (1, CPU_STATUS_FLAG_DL),
            IO          /* DL != 0 */,
            MOV_RRW     (CPU_REG_ZERO, CPU_REG_BANK, MOV_RRW_BYTE),
            MOV_L       (MOV_LSB, CPU_REG_ADDR, CPU_REG_BANK, CPU_REG_TEMP),
            ADDR_OFFI   (1, ADDR_OFF_BANK_NEXT),
            MOV_L       (MOV_MSB, CPU_REG_ADDR, CPU_REG_BANK, CPU_REG_TEMP),
            ADDR_OFFI   (1, ADDR_OFF_BANK_NEXT),
            MOV_L       (MOV_LSB, CPU_REG_ADDR, CPU_REG_BANK, CPU_REG_BANK),
            MOV_RRW     (CPU_REG_TEMP, CPU_REG_ADDR, MOV_RRW_WORD),
            ADDR_OFFR   (CPU_REG_Y, ADDR_OFF_BANK_NEXT),
            MOV_L       (MOV_LSB, CPU_REG_ADDR, CPU_REG_BANK, CPU_REG_TEMP),
            SKIPS       (2, CPU_STATUS_FLAG_M),
            ADDR_OFFI   (1, ADDR_OFF_BANK_NEXT),
            MOV_L       (MOV_MSB, CPU_REG_ADDR, CPU_REG_BANK, CPU_REG_TEMP),
            ALU_OP      (ALU_OP_ADD, CPU_STATUS_FLAG_M, CPU_REG_ACCUM, CPU_REG_TEMP),
            MOV_RR      (CPU_REG_TEMP, CPU_REG_ACCUM),
            // CHK_ZN      (CPU_REG_ACCUM),
        }
    },

    [CPU_OPCODE_ADC_IMM] = {
        .uops = {
            MOV_LPC     (MOV_LSB, CPU_REG_TEMP),
            SKIPS       (1, CPU_STATUS_FLAG_M),
            MOV_LPC     (MOV_MSB, CPU_REG_TEMP),
            ALU_OP      (ALU_OP_ADD, CPU_STATUS_FLAG_M, CPU_REG_ACCUM, CPU_REG_TEMP),
            MOV_RR      (CPU_REG_TEMP, CPU_REG_ACCUM),
            // CHK_ZN      (CPU_REG_ACCUM),
        }
    },

    /**************************************************************************************/
    /*                                      AND                                           */
    /**************************************************************************************/

    [CPU_OPCODE_AND_ABS] = {
        .uops = {
            MOV_LPC     (MOV_LSB, CPU_REG_ADDR),
            MOV_LPC     (MOV_MSB, CPU_REG_ADDR),
            MOV_RRW     (CPU_REG_DBR, CPU_REG_BANK, MOV_RRW_BYTE),
            MOV_L       (MOV_LSB, CPU_REG_ADDR, CPU_REG_BANK, CPU_REG_TEMP),
            SKIPS       (2, CPU_STATUS_FLAG_M),
            ADDR_OFFI   (1, ADDR_OFF_BANK_NEXT),
            MOV_L       (MOV_MSB, CPU_REG_ADDR, CPU_REG_BANK, CPU_REG_TEMP),
            ALU_OP      (ALU_OP_AND, CPU_STATUS_FLAG_M, CPU_REG_ACCUM, CPU_REG_TEMP),
            MOV_RR      (CPU_REG_TEMP, CPU_REG_ACCUM),
            // CHK_ZN      (CPU_REG_ACCUM),
        }
    },
    [CPU_OPCODE_AND_ABS_X] = {
        .uops = {
            MOV_LPC     (MOV_LSB, CPU_REG_ADDR),
            MOV_LPC     (MOV_MSB, CPU_REG_ADDR),
            MOV_RRW     (CPU_REG_DBR, CPU_REG_BANK, MOV_RRW_BYTE),
            ADDR_OFFR   (CPU_REG_X, ADDR_OFF_BANK_NEXT),
            SKIPC       (2, CPU_STATUS_FLAG_PAGE),
            SKIPS       (1, CPU_STATUS_FLAG_X),
            IO          /* X=0 or page boundary */,
            MOV_L       (MOV_LSB, CPU_REG_ADDR, CPU_REG_BANK, CPU_REG_TEMP),
            SKIPS       (2, CPU_STATUS_FLAG_M),
            ADDR_OFFI   (1, ADDR_OFF_BANK_NEXT),
            MOV_L       (MOV_MSB, CPU_REG_ADDR, CPU_REG_BANK, CPU_REG_TEMP),
            ALU_OP      (ALU_OP_AND, CPU_STATUS_FLAG_M, CPU_REG_ACCUM, CPU_REG_TEMP),
            MOV_RR      (CPU_REG_TEMP, CPU_REG_ACCUM),
            // CHK_ZN      (CPU_REG_ACCUM),
        }
    },
    [CPU_OPCODE_AND_ABS_Y] = {
        .uops = {
            MOV_LPC     (MOV_LSB, CPU_REG_ADDR),
            MOV_LPC     (MOV_MSB, CPU_REG_ADDR),
            MOV_RRW     (CPU_REG_DBR, CPU_REG_BANK, MOV_RRW_BYTE),
            ADDR_OFFR   (CPU_REG_Y, ADDR_OFF_BANK_NEXT),
            SKIPC       (2, CPU_STATUS_FLAG_PAGE),
            SKIPS       (1, CPU_STATUS_FLAG_X),
            IO          /* X=0 or page boundary */,
            MOV_L       (MOV_LSB, CPU_REG_ADDR, CPU_REG_BANK, CPU_REG_TEMP),
            SKIPS       (2, CPU_STATUS_FLAG_M),
            ADDR_OFFI   (1, ADDR_OFF_BANK_NEXT),
            MOV_L       (MOV_MSB, CPU_REG_ADDR, CPU_REG_BANK, CPU_REG_TEMP),
            ALU_OP      (ALU_OP_AND, CPU_STATUS_FLAG_M, CPU_REG_ACCUM, CPU_REG_TEMP),
            MOV_RR      (CPU_REG_TEMP, CPU_REG_ACCUM),
            // CHK_ZN      (CPU_REG_ACCUM),
        }
    },
    [CPU_OPCODE_AND_ABSL] = {
        .uops = {
            MOV_LPC     (MOV_LSB, CPU_REG_ADDR),
            MOV_LPC     (MOV_MSB, CPU_REG_ADDR),
            MOV_LPC     (MOV_LSB, CPU_REG_BANK),
            MOV_L       (MOV_LSB, CPU_REG_ADDR, CPU_REG_BANK, CPU_REG_TEMP),
            SKIPS       (2, CPU_STATUS_FLAG_M),
            ADDR_OFFI   (1, ADDR_OFF_BANK_NEXT),
            MOV_L       (MOV_MSB, CPU_REG_ADDR, CPU_REG_BANK, CPU_REG_TEMP),
            ALU_OP      (ALU_OP_AND, CPU_STATUS_FLAG_M, CPU_REG_ACCUM, CPU_REG_TEMP),
            MOV_RR      (CPU_REG_TEMP, CPU_REG_ACCUM),
            // CHK_ZN      (CPU_REG_ACCUM),
        }
    },
    [CPU_OPCODE_AND_ABSL_X] = {
        .uops = {
            MOV_LPC     (MOV_LSB, CPU_REG_ADDR),
            MOV_LPC     (MOV_MSB, CPU_REG_ADDR),
            MOV_LPC     (MOV_LSB, CPU_REG_BANK),
            ADDR_OFFR   (CPU_REG_X, ADDR_OFF_BANK_NEXT),
            MOV_L       (MOV_LSB, CPU_REG_ADDR, CPU_REG_BANK, CPU_REG_TEMP),
            SKIPS       (2, CPU_STATUS_FLAG_M),
            ADDR_OFFI   (1, ADDR_OFF_BANK_NEXT),
            MOV_L       (MOV_MSB, CPU_REG_ADDR, CPU_REG_BANK, CPU_REG_TEMP),
            ALU_OP      (ALU_OP_AND, CPU_STATUS_FLAG_M, CPU_REG_ACCUM, CPU_REG_TEMP),
            MOV_RR      (CPU_REG_TEMP, CPU_REG_ACCUM),
            // CHK_ZN      (CPU_REG_ACCUM),
        }
    },
    [CPU_OPCODE_AND_DIR] = {
        .uops = {
            MOV_LPC     (MOV_LSB, CPU_REG_ADDR),
            ZEXT        (CPU_REG_ADDR),
            ADDR_OFFR   (CPU_REG_D, ADDR_OFF_BANK_WRAP),
            SKIPC       (1, CPU_STATUS_FLAG_DL),
            IO          /* DL != 0 */,
            MOV_RRW     (CPU_REG_ZERO, CPU_REG_BANK, MOV_RRW_BYTE),
            MOV_L       (MOV_LSB, CPU_REG_ADDR, CPU_REG_BANK, CPU_REG_TEMP),
            SKIPS       (2, CPU_STATUS_FLAG_M),
            ADDR_OFFI   (1, ADDR_OFF_BANK_NEXT),
            MOV_L       (MOV_MSB, CPU_REG_ADDR, CPU_REG_BANK, CPU_REG_TEMP),
            ALU_OP      (ALU_OP_AND, CPU_STATUS_FLAG_M, CPU_REG_ACCUM, CPU_REG_TEMP),
            MOV_RR      (CPU_REG_TEMP, CPU_REG_ACCUM),
            // CHK_ZN      (CPU_REG_ACCUM),
        }
    },
    [CPU_OPCODE_AND_S_REL] = {
        .uops = {
            MOV_LPC     (MOV_LSB, CPU_REG_ADDR),
            ZEXT        (CPU_REG_ADDR),
            ADDR_OFFR   (CPU_REG_S, ADDR_OFF_BANK_WRAP),
            IO,
            MOV_L       (MOV_LSB, CPU_REG_ADDR, CPU_REG_ZERO, CPU_REG_TEMP),
            SKIPS       (2, CPU_STATUS_FLAG_M),
            ADDR_OFFI   (1, ADDR_OFF_BANK_WRAP),
            MOV_L       (MOV_MSB, CPU_REG_ADDR, CPU_REG_ZERO, CPU_REG_TEMP),
            ALU_OP      (ALU_OP_AND, CPU_STATUS_FLAG_M, CPU_REG_ACCUM, CPU_REG_TEMP),
            MOV_RR      (CPU_REG_TEMP, CPU_REG_ACCUM),
            // CHK_ZN      (CPU_REG_ACCUM),
        },
    },
    [CPU_OPCODE_AND_DIR_X] = {
        .uops = {
            MOV_LPC     (MOV_LSB, CPU_REG_ADDR),
            ZEXT        (CPU_REG_ADDR),
            SKIPC       (1, CPU_STATUS_FLAG_DL),
            IO          /* DL != 0 */,
            ADDR_OFFR   (CPU_REG_D, ADDR_OFF_BANK_WRAP),
            ADDR_OFFR   (CPU_REG_X, ADDR_OFF_BANK_WRAP),
            IO,
            MOV_L       (MOV_LSB, CPU_REG_ADDR, CPU_REG_ZERO, CPU_REG_TEMP),
            SKIPS       (2, CPU_STATUS_FLAG_M),
            ADDR_OFFI   (1, ADDR_OFF_BANK_WRAP),
            MOV_L       (MOV_MSB, CPU_REG_ADDR, CPU_REG_ZERO, CPU_REG_TEMP),
            ALU_OP      (ALU_OP_AND, CPU_STATUS_FLAG_M, CPU_REG_ACCUM, CPU_REG_TEMP),
            MOV_RR      (CPU_REG_TEMP, CPU_REG_ACCUM),
            // CHK_ZN      (CPU_REG_ACCUM),
        }
    },

    [CPU_OPCODE_AND_DIR_IND] = {
        .uops = {
            MOV_LPC     (MOV_LSB, CPU_REG_ADDR),
            ZEXT        (CPU_REG_ADDR),
            SKIPC       (1, CPU_STATUS_FLAG_DL),
            IO          /* DL != 0 */,
            ADDR_OFFR   (CPU_REG_D, ADDR_OFF_BANK_WRAP),
            MOV_RRW     (CPU_REG_ZERO, CPU_REG_BANK, MOV_RRW_BYTE),
            MOV_L       (MOV_LSB, CPU_REG_ADDR, CPU_REG_BANK, CPU_REG_TEMP),
            ADDR_OFFI   (1, ADDR_OFF_BANK_NEXT),
            MOV_L       (MOV_MSB, CPU_REG_ADDR, CPU_REG_BANK, CPU_REG_TEMP),
            MOV_RRW     (CPU_REG_TEMP, CPU_REG_ADDR, MOV_RRW_WORD),
            MOV_RRW     (CPU_REG_DBR, CPU_REG_BANK, MOV_RRW_BYTE),
            MOV_L       (MOV_LSB, CPU_REG_ADDR, CPU_REG_BANK, CPU_REG_TEMP),
            SKIPS       (2, CPU_STATUS_FLAG_M),
            ADDR_OFFI   (1, ADDR_OFF_BANK_NEXT),
            MOV_L       (MOV_MSB, CPU_REG_ADDR, CPU_REG_BANK, CPU_REG_TEMP),
            ALU_OP      (ALU_OP_AND, CPU_STATUS_FLAG_M, CPU_REG_ACCUM, CPU_REG_TEMP),
            MOV_RR      (CPU_REG_TEMP, CPU_REG_ACCUM),
            // CHK_ZN      (CPU_REG_ACCUM),
        }
    },

    [CPU_OPCODE_AND_DIR_INDL] = {
        .uops = {
            MOV_LPC     (MOV_LSB, CPU_REG_ADDR),
            ZEXT        (CPU_REG_ADDR),
            SKIPC       (1, CPU_STATUS_FLAG_DL),
            IO          /* DL != 0 */,
            ADDR_OFFR   (CPU_REG_D, ADDR_OFF_BANK_WRAP),
            MOV_RRW     (CPU_REG_ZERO, CPU_REG_BANK, MOV_RRW_BYTE),
            MOV_L       (MOV_LSB, CPU_REG_ADDR, CPU_REG_BANK, CPU_REG_TEMP),
            ADDR_OFFI   (1, ADDR_OFF_BANK_NEXT),
            MOV_L       (MOV_MSB, CPU_REG_ADDR, CPU_REG_BANK, CPU_REG_TEMP),
            ADDR_OFFI   (1, ADDR_OFF_BANK_NEXT),
            MOV_L       (MOV_LSB, CPU_REG_ADDR, CPU_REG_BANK, CPU_REG_BANK),
            MOV_RRW     (CPU_REG_TEMP, CPU_REG_ADDR, MOV_RRW_WORD),
            MOV_L       (MOV_LSB, CPU_REG_ADDR, CPU_REG_BANK, CPU_REG_TEMP),
            SKIPS       (2, CPU_STATUS_FLAG_M),
            ADDR_OFFI   (1, ADDR_OFF_BANK_NEXT),
            MOV_L       (MOV_MSB, CPU_REG_ADDR, CPU_REG_BANK, CPU_REG_TEMP),
            ALU_OP      (ALU_OP_AND, CPU_STATUS_FLAG_M, CPU_REG_ACCUM, CPU_REG_TEMP),
            MOV_RR      (CPU_REG_TEMP, CPU_REG_ACCUM),
            // CHK_ZN      (CPU_REG_ACCUM),
        }
    },

    [CPU_OPCODE_AND_S_REL_IND_Y] = {
        .uops = {
            MOV_LPC     (MOV_LSB, CPU_REG_ADDR),
            ZEXT        (CPU_REG_ADDR),
            IO,
            ADDR_OFFR   (CPU_REG_S, ADDR_OFF_BANK_WRAP),
            MOV_L       (MOV_LSB, CPU_REG_ADDR, CPU_REG_ZERO, CPU_REG_TEMP),
            ADDR_OFFI   (1, ADDR_OFF_BANK_WRAP),
            MOV_L       (MOV_MSB, CPU_REG_ADDR, CPU_REG_ZERO, CPU_REG_TEMP),
            MOV_RRW     (CPU_REG_TEMP, CPU_REG_ADDR, MOV_RRW_WORD),
            MOV_RRW     (CPU_REG_DBR, CPU_REG_BANK, MOV_RRW_BYTE),
            IO,
            ADDR_OFFR   (CPU_REG_Y, ADDR_OFF_BANK_NEXT),
            MOV_L       (MOV_LSB, CPU_REG_ADDR, CPU_REG_BANK, CPU_REG_TEMP),
            SKIPS       (2, CPU_STATUS_FLAG_M),
            ADDR_OFFI   (1, ADDR_OFF_BANK_NEXT),
            MOV_L       (MOV_MSB, CPU_REG_ADDR, CPU_REG_BANK, CPU_REG_TEMP),
            ALU_OP      (ALU_OP_AND, CPU_STATUS_FLAG_M, CPU_REG_ACCUM, CPU_REG_TEMP),
            MOV_RR      (CPU_REG_TEMP, CPU_REG_ACCUM),
            // CHK_ZN      (CPU_REG_ACCUM),

        }
    },

    [CPU_OPCODE_AND_DIR_X_IND] = {
        .uops = {
            MOV_LPC     (MOV_LSB, CPU_REG_ADDR),
            ZEXT        (CPU_REG_ADDR),
            SKIPC       (1, CPU_STATUS_FLAG_DL),
            IO          /* DL != 0 */,
            ADDR_OFFR   (CPU_REG_D, ADDR_OFF_BANK_WRAP),
            ADDR_OFFR   (CPU_REG_X, ADDR_OFF_BANK_WRAP),
            IO,
            MOV_L       (MOV_LSB, CPU_REG_ADDR, CPU_REG_ZERO, CPU_REG_TEMP),
            ADDR_OFFI   (1, ADDR_OFF_BANK_WRAP),
            MOV_L       (MOV_MSB, CPU_REG_ADDR, CPU_REG_ZERO, CPU_REG_TEMP),
            MOV_RRW     (CPU_REG_TEMP, CPU_REG_ADDR, MOV_RRW_WORD),
            MOV_RRW     (CPU_REG_DBR, CPU_REG_BANK, MOV_RRW_BYTE),
            MOV_L       (MOV_LSB, CPU_REG_ADDR, CPU_REG_BANK, CPU_REG_TEMP),
            SKIPS       (2, CPU_STATUS_FLAG_M),
            ADDR_OFFI   (1, ADDR_OFF_BANK_NEXT),
            MOV_L       (MOV_MSB, CPU_REG_ADDR, CPU_REG_BANK, CPU_REG_TEMP),
            ALU_OP      (ALU_OP_AND, CPU_STATUS_FLAG_M, CPU_REG_ACCUM, CPU_REG_TEMP),
            MOV_RR      (CPU_REG_TEMP, CPU_REG_ACCUM),
            // CHK_ZN      (CPU_REG_ACCUM),
        }
    },

    [CPU_OPCODE_AND_DIR_IND_Y] = {
        .uops = {
            MOV_LPC     (MOV_LSB, CPU_REG_ADDR),
            ZEXT        (CPU_REG_ADDR),
            SKIPC       (1, CPU_STATUS_FLAG_DL),
            IO          /* DL != 0 */,
            ADDR_OFFR   (CPU_REG_D, 0),
            MOV_RRW     (CPU_REG_ZERO, CPU_REG_BANK, MOV_RRW_BYTE),
            MOV_L       (MOV_LSB, CPU_REG_ADDR, CPU_REG_BANK, CPU_REG_TEMP),
            ADDR_OFFI   (1, ADDR_OFF_BANK_NEXT),
            MOV_L       (MOV_MSB, CPU_REG_ADDR, CPU_REG_BANK, CPU_REG_TEMP),
            MOV_RRW     (CPU_REG_TEMP, CPU_REG_ADDR, MOV_RRW_WORD),
            MOV_RRW     (CPU_REG_DBR, CPU_REG_BANK, MOV_RRW_BYTE),
            ADDR_OFFR   (CPU_REG_Y, ADDR_OFF_BANK_NEXT),
            SKIPS       (2, CPU_STATUS_FLAG_X),
            SKIPC       (1, CPU_STATUS_FLAG_PAGE),
            IO          /* X = 0 or page crossed */,
            MOV_L       (MOV_LSB, CPU_REG_ADDR, CPU_REG_BANK, CPU_REG_TEMP),
            SKIPS       (2, CPU_STATUS_FLAG_M),
            ADDR_OFFI   (1, ADDR_OFF_BANK_NEXT),
            MOV_L       (MOV_MSB, CPU_REG_ADDR, CPU_REG_BANK, CPU_REG_TEMP),
            ALU_OP      (ALU_OP_AND, CPU_STATUS_FLAG_M, CPU_REG_ACCUM, CPU_REG_TEMP),
            MOV_RR      (CPU_REG_TEMP, CPU_REG_ACCUM),
            // CHK_ZN      (CPU_REG_ACCUM),
        }
    },

    [CPU_OPCODE_AND_DIR_INDL_Y] = {
        .uops = {
            MOV_LPC     (MOV_LSB, CPU_REG_ADDR),
            ZEXT        (CPU_REG_ADDR),
            ADDR_OFFR   (CPU_REG_D, 0),
            SKIPC       (1, CPU_STATUS_FLAG_DL),
            IO          /* DL != 0 */,
            MOV_RRW     (CPU_REG_ZERO, CPU_REG_BANK, MOV_RRW_BYTE),
            MOV_L       (MOV_LSB, CPU_REG_ADDR, CPU_REG_BANK, CPU_REG_TEMP),
            ADDR_OFFI   (1, ADDR_OFF_BANK_NEXT),
            MOV_L       (MOV_MSB, CPU_REG_ADDR, CPU_REG_BANK, CPU_REG_TEMP),
            ADDR_OFFI   (1, ADDR_OFF_BANK_NEXT),
            MOV_L       (MOV_LSB, CPU_REG_ADDR, CPU_REG_BANK, CPU_REG_BANK),
            MOV_RRW     (CPU_REG_TEMP, CPU_REG_ADDR, MOV_RRW_WORD),
            ADDR_OFFR   (CPU_REG_Y, ADDR_OFF_BANK_NEXT),
            MOV_L       (MOV_LSB, CPU_REG_ADDR, CPU_REG_BANK, CPU_REG_TEMP),
            SKIPS       (2, CPU_STATUS_FLAG_M),
            ADDR_OFFI   (1, ADDR_OFF_BANK_NEXT),
            MOV_L       (MOV_MSB, CPU_REG_ADDR, CPU_REG_BANK, CPU_REG_TEMP),
            ALU_OP      (ALU_OP_AND, CPU_STATUS_FLAG_M, CPU_REG_ACCUM, CPU_REG_TEMP),
            MOV_RR      (CPU_REG_TEMP, CPU_REG_ACCUM),
            // CHK_ZN      (CPU_REG_ACCUM),
        }
    },

    [CPU_OPCODE_AND_IMM] = {
        .uops = {
            MOV_LPC     (MOV_LSB, CPU_REG_TEMP),
            SKIPS       (1, CPU_STATUS_FLAG_M),
            MOV_LPC     (MOV_MSB, CPU_REG_TEMP),
            ALU_OP      (ALU_OP_AND, CPU_STATUS_FLAG_M, CPU_REG_ACCUM, CPU_REG_TEMP),
            MOV_RR      (CPU_REG_TEMP, CPU_REG_ACCUM),
            // CHK_ZN      (CPU_REG_ACCUM),
        }
    },

    /**************************************************************************************/
    /*                                      ASL                                           */
    /**************************************************************************************/

    [CPU_OPCODE_ASL_ABS] = {
        .uops = {
            MOV_LPC     (MOV_LSB, CPU_REG_ADDR),
            MOV_LPC     (MOV_MSB, CPU_REG_ADDR),
            MOV_RRW     (CPU_REG_DBR, CPU_REG_BANK, MOV_RRW_BYTE),
            MOV_L       (MOV_LSB, CPU_REG_ADDR, CPU_REG_BANK, CPU_REG_TEMP),
            SKIPS       (2, CPU_STATUS_FLAG_M),
            ADDR_OFFI   (1, ADDR_OFF_BANK_NEXT),
            MOV_L       (MOV_MSB, CPU_REG_ADDR, CPU_REG_BANK, CPU_REG_TEMP),
            IO,
            ALU_OP      (ALU_OP_SHL, CPU_STATUS_FLAG_M, CPU_REG_TEMP, CPU_REG_ZERO),
            // CHK_ZNW     (CPU_REG_TEMP, 0),
            SKIPS       (2, CPU_STATUS_FLAG_M),
            MOV_S       (MOV_MSB, CPU_REG_ADDR, CPU_REG_BANK, CPU_REG_TEMP),
            ADDR_OFFIS  (0xffff, ADDR_OFF_BANK_NEXT, ADDR_OFF_SIGNED),
            MOV_S       (MOV_LSB, CPU_REG_ADDR, CPU_REG_BANK, CPU_REG_TEMP),
        }
    },

    [CPU_OPCODE_ASL_ACC] = {
        .uops = {
            IO,
            ALU_OP      (ALU_OP_SHL, CPU_STATUS_FLAG_M, CPU_REG_ACCUM, CPU_REG_ZERO),
            MOV_RR      (CPU_REG_TEMP, CPU_REG_ACCUM),
            // CHK_ZN      (CPU_REG_ACCUM),
        }
    },

    [CPU_OPCODE_ASL_ABS_X] = {
        .uops = {
            MOV_LPC     (MOV_LSB, CPU_REG_ADDR),
            MOV_LPC     (MOV_MSB, CPU_REG_ADDR),
            MOV_RRW     (CPU_REG_DBR, CPU_REG_BANK, MOV_RRW_BYTE),
            IO,
            ADDR_OFFR   (CPU_REG_X, ADDR_OFF_BANK_NEXT),
            MOV_L       (MOV_LSB, CPU_REG_ADDR, CPU_REG_BANK, CPU_REG_TEMP),
            SKIPS       (2, CPU_STATUS_FLAG_M),
            ADDR_OFFI   (1, ADDR_OFF_BANK_NEXT),
            MOV_L       (MOV_MSB, CPU_REG_ADDR, CPU_REG_BANK, CPU_REG_TEMP),
            ALU_OP      (ALU_OP_SHL, CPU_STATUS_FLAG_M, CPU_REG_TEMP, CPU_REG_ZERO),
            // CHK_ZNW     (CPU_REG_TEMP, 0),
            SKIPS       (2, CPU_STATUS_FLAG_M),
            MOV_S       (MOV_MSB, CPU_REG_ADDR, CPU_REG_BANK, CPU_REG_TEMP),
            ADDR_OFFIS  (0xffff, ADDR_OFF_BANK_WRAP, ADDR_OFF_SIGNED),
            MOV_S       (MOV_LSB, CPU_REG_ADDR, CPU_REG_BANK, CPU_REG_TEMP),
        }
    },

    [CPU_OPCODE_ASL_DIR] = {
        .uops = {
            MOV_LPC     (MOV_LSB, CPU_REG_ADDR),
            ZEXT        (CPU_REG_ADDR),
            SKIPC       (1, CPU_STATUS_FLAG_DL),
            IO          /* DL != 0 */,
            ADDR_OFFR   (CPU_REG_D, ADDR_OFF_BANK_WRAP),
            MOV_RRW     (CPU_REG_ZERO, CPU_REG_BANK, MOV_RRW_BYTE),
            MOV_L       (MOV_LSB, CPU_REG_ADDR, CPU_REG_BANK, CPU_REG_TEMP),
            SKIPS       (2, CPU_STATUS_FLAG_M),
            ADDR_OFFI   (1, ADDR_OFF_BANK_NEXT),
            MOV_L       (MOV_MSB, CPU_REG_ADDR, CPU_REG_BANK, CPU_REG_TEMP),
            IO,
            ALU_OP      (ALU_OP_SHL, CPU_STATUS_FLAG_M, CPU_REG_TEMP, CPU_REG_ZERO),
            // CHK_ZNW     (CPU_REG_TEMP, 0),
            SKIPS       (2, CPU_STATUS_FLAG_M),
            MOV_S       (MOV_MSB, CPU_REG_ADDR, CPU_REG_ZERO, CPU_REG_TEMP),
            ADDR_OFFIS  (0xffff, ADDR_OFF_BANK_WRAP, ADDR_OFF_SIGNED),
            MOV_S       (MOV_LSB, CPU_REG_ADDR, CPU_REG_ZERO, CPU_REG_TEMP),
        }
    },

    [CPU_OPCODE_ASL_DIR_X] = {
        .uops = {
            MOV_LPC     (MOV_LSB, CPU_REG_ADDR),
            ZEXT        (CPU_REG_ADDR),
            SKIPC       (1, CPU_STATUS_FLAG_DL),
            IO          /* DL != 0 */,
            IO,
            ADDR_OFFR   (CPU_REG_D, ADDR_OFF_BANK_WRAP),
            ADDR_OFFR   (CPU_REG_X, ADDR_OFF_BANK_WRAP),
            MOV_L       (MOV_LSB, CPU_REG_ADDR, CPU_REG_ZERO, CPU_REG_TEMP),
            SKIPS       (2, CPU_STATUS_FLAG_M),
            ADDR_OFFI   (1, ADDR_OFF_BANK_WRAP),
            MOV_L       (MOV_MSB, CPU_REG_ADDR, CPU_REG_ZERO, CPU_REG_TEMP),
            IO,
            ALU_OP      (ALU_OP_SHL, CPU_STATUS_FLAG_M, CPU_REG_TEMP, CPU_REG_ZERO),
            // CHK_ZNW     (CPU_REG_TEMP, 0),
            SKIPS       (2, CPU_STATUS_FLAG_M),
            MOV_S       (MOV_MSB, CPU_REG_ADDR, CPU_REG_ZERO, CPU_REG_TEMP),
            ADDR_OFFIS  (0xffff, ADDR_OFF_BANK_WRAP, ADDR_OFF_SIGNED),
            MOV_S       (MOV_LSB, CPU_REG_ADDR, CPU_REG_ZERO, CPU_REG_TEMP),
        }
    },

    /**************************************************************************************/
    /*                                      BCC                                           */
    /**************************************************************************************/

    [CPU_OPCODE_BCC_PC_REL] = {
        .uops = {
            MOV_LPC     (MOV_LSB, CPU_REG_ADDR),
            SEXT        (CPU_REG_ADDR),
            ADDR_OFFRS  (CPU_REG_PC, ADDR_OFF_BANK_WRAP, ADDR_OFF_SIGNED),
            SKIPS       (2, CPU_STATUS_FLAG_C),
            IO          /* branch taken */,
            MOV_RRW     (CPU_REG_ADDR, CPU_REG_PC, MOV_RRW_WORD),
        }
    },

    /**************************************************************************************/
    /*                                      BCS                                           */
    /**************************************************************************************/

    [CPU_OPCODE_BCS_PC_REL] = {
        .uops = {
            MOV_LPC     (MOV_LSB, CPU_REG_ADDR),
            SEXT        (CPU_REG_ADDR),
            ADDR_OFFRS  (CPU_REG_PC, ADDR_OFF_BANK_WRAP, ADDR_OFF_SIGNED),
            SKIPC       (2, CPU_STATUS_FLAG_C),
            IO          /* branch taken */,
            MOV_RRW     (CPU_REG_ADDR, CPU_REG_PC, MOV_RRW_WORD),
        }
    },

    /**************************************************************************************/
    /*                                      BEQ                                           */
    /**************************************************************************************/

    [CPU_OPCODE_BEQ_PC_REL] = {
        .uops = {
            MOV_LPC     (MOV_LSB, CPU_REG_ADDR),
            SEXT        (CPU_REG_ADDR),
            ADDR_OFFRS  (CPU_REG_PC, ADDR_OFF_BANK_WRAP, ADDR_OFF_SIGNED),
            SKIPC       (2, CPU_STATUS_FLAG_Z),
            IO          /* branch taken */,
            MOV_RRW     (CPU_REG_ADDR, CPU_REG_PC, 0),
        }
    },

    /**************************************************************************************/
    /*                                      BIT                                           */
    /**************************************************************************************/

    [CPU_OPCODE_BIT_ABS] = {
        .uops = {
            MOV_LPC     (MOV_LSB, CPU_REG_ADDR),
            MOV_LPC     (MOV_MSB, CPU_REG_ADDR),
            MOV_RRW     (CPU_REG_DBR, CPU_REG_BANK, MOV_RRW_BYTE),
            MOV_L       (MOV_LSB, CPU_REG_ADDR, CPU_REG_BANK, CPU_REG_TEMP),
            SKIPS       (2, CPU_STATUS_FLAG_M),
            ADDR_OFFI   (1, ADDR_OFF_BANK_NEXT),
            MOV_L       (MOV_MSB, CPU_REG_ADDR, CPU_REG_BANK, CPU_REG_TEMP),
            ALU_OP      (ALU_OP_BIT, CPU_STATUS_FLAG_M, CPU_REG_TEMP, CPU_REG_ZERO),
        }
    },

    [CPU_OPCODE_BIT_ABS_X] = {
        .uops = {
            MOV_LPC     (MOV_LSB, CPU_REG_ADDR),
            MOV_LPC     (MOV_MSB, CPU_REG_ADDR),
            MOV_RRW     (CPU_REG_DBR, CPU_REG_BANK, MOV_RRW_BYTE),
            ADDR_OFFR   (CPU_REG_X, ADDR_OFF_BANK_NEXT),
            SKIPC       (2, CPU_STATUS_FLAG_PAGE),
            SKIPS       (1, CPU_STATUS_FLAG_X),
            IO          /* X=0 or page boundary */,
            MOV_L       (MOV_LSB, CPU_REG_ADDR, CPU_REG_BANK, CPU_REG_TEMP),
            SKIPS       (2, CPU_STATUS_FLAG_M),
            ADDR_OFFI   (1, ADDR_OFF_BANK_NEXT),
            MOV_L       (MOV_MSB, CPU_REG_ADDR, CPU_REG_BANK, CPU_REG_TEMP),
            ALU_OP      (ALU_OP_BIT, CPU_STATUS_FLAG_M, CPU_REG_TEMP, CPU_REG_ZERO),
        }
    },

    [CPU_OPCODE_BIT_DIR] = {
        .uops = {
            MOV_LPC     (MOV_LSB, CPU_REG_ADDR),
            ZEXT        (CPU_REG_ADDR),
            ADDR_OFFR   (CPU_REG_D, ADDR_OFF_BANK_WRAP),
            SKIPC       (1, CPU_STATUS_FLAG_DL),
            IO          /* DL != 0 */,
            MOV_RRW     (CPU_REG_ZERO, CPU_REG_BANK, MOV_RRW_BYTE),
            MOV_L       (MOV_LSB, CPU_REG_ADDR, CPU_REG_BANK, CPU_REG_TEMP),
            SKIPS       (2, CPU_STATUS_FLAG_M),
            ADDR_OFFI   (1, ADDR_OFF_BANK_NEXT),
            MOV_L       (MOV_MSB, CPU_REG_ADDR, CPU_REG_BANK, CPU_REG_TEMP),
            ALU_OP      (ALU_OP_BIT, CPU_STATUS_FLAG_M, CPU_REG_TEMP, CPU_REG_ZERO),
        }
    },

    [CPU_OPCODE_BIT_DIR_X] = {
        .uops = {
            MOV_LPC     (MOV_LSB, CPU_REG_ADDR),
            ZEXT        (CPU_REG_ADDR),
            SKIPC       (1, CPU_STATUS_FLAG_DL),
            IO          /* DL != 0 */,
            ADDR_OFFR   (CPU_REG_D, ADDR_OFF_BANK_WRAP),
            ADDR_OFFR   (CPU_REG_X, ADDR_OFF_BANK_WRAP),
            IO,
            MOV_L       (MOV_LSB, CPU_REG_ADDR, CPU_REG_ZERO, CPU_REG_TEMP),
            SKIPS       (2, CPU_STATUS_FLAG_M),
            ADDR_OFFI   (1, ADDR_OFF_BANK_WRAP),
            MOV_L       (MOV_MSB, CPU_REG_ADDR, CPU_REG_ZERO, CPU_REG_TEMP),
            ALU_OP      (ALU_OP_BIT, CPU_STATUS_FLAG_M, CPU_REG_TEMP, CPU_REG_ZERO),
        }
    },

    [CPU_OPCODE_BIT_IMM] = {
        .uops = {
            MOV_LPC     (MOV_LSB, CPU_REG_TEMP),
            SKIPS       (1, CPU_STATUS_FLAG_M),
            MOV_LPC     (MOV_MSB, CPU_REG_TEMP),
            ALU_OP      (ALU_OP_BIT_IMM, CPU_STATUS_FLAG_M, CPU_REG_TEMP, CPU_REG_ZERO),
        }
    },

    /**************************************************************************************/
    /*                                      BMI                                           */
    /**************************************************************************************/

    [CPU_OPCODE_BMI_PC_REL] = {
        .uops = {
            MOV_LPC     (MOV_LSB, CPU_REG_ADDR),
            SEXT        (CPU_REG_ADDR),
            ADDR_OFFRS  (CPU_REG_PC, ADDR_OFF_BANK_WRAP, ADDR_OFF_SIGNED),
            SKIPC       (2, CPU_STATUS_FLAG_N),
            IO          /* branch taken */,
            MOV_RRW     (CPU_REG_ADDR, CPU_REG_PC, MOV_RRW_WORD),
        }
    },

    /**************************************************************************************/
    /*                                      BNE                                           */
    /**************************************************************************************/

    [CPU_OPCODE_BNE_PC_REL] = {
        .uops = {
            MOV_LPC     (MOV_LSB, CPU_REG_ADDR),
            SEXT        (CPU_REG_ADDR),
            ADDR_OFFRS  (CPU_REG_PC, ADDR_OFF_BANK_WRAP, ADDR_OFF_SIGNED),
            SKIPS       (2, CPU_STATUS_FLAG_Z),
            IO          /* branch taken */,
            MOV_RRW     (CPU_REG_ADDR, CPU_REG_PC, MOV_RRW_WORD),
        }
    },

    /**************************************************************************************/
    /*                                      BPL                                           */
    /**************************************************************************************/

    [CPU_OPCODE_BPL_PC_REL] = {
        .uops = {
            MOV_LPC     (MOV_LSB, CPU_REG_ADDR),
            SEXT        (CPU_REG_ADDR),
            ADDR_OFFRS  (CPU_REG_PC, ADDR_OFF_BANK_WRAP, ADDR_OFF_SIGNED),
            SKIPS       (2, CPU_STATUS_FLAG_N),
            IO          /* branch taken */,
            MOV_RRW     (CPU_REG_ADDR, CPU_REG_PC, MOV_RRW_WORD),
        }
    },

    /**************************************************************************************/
    /*                                      BRA                                           */
    /**************************************************************************************/

    [CPU_OPCODE_BRA_PC_REL] = {
        .uops = {
            MOV_LPC     (MOV_LSB, CPU_REG_ADDR),
            SEXT        (CPU_REG_ADDR),
            ADDR_OFFRS  (CPU_REG_PC, ADDR_OFF_BANK_WRAP, ADDR_OFF_SIGNED),
            IO          /* branch taken */,
            MOV_RRW     (CPU_REG_ADDR, CPU_REG_PC, MOV_RRW_WORD),
        }
    },

    /**************************************************************************************/
    /*                                      BRK                                           */
    /**************************************************************************************/

    [CPU_OPCODE_BRK_S] = {
        .uops = {
            MOV_LPC     (MOV_LSB, CPU_REG_TEMP),
            /* this sets the B flag in the status register (when in emulation
            mode) and also loads the appropriate interrupt vector into CPU_REG_ADDR */
            BRK,

            /* when in native mode... */
            SKIPS       (2, CPU_STATUS_FLAG_E),
            /* the program bank register gets pushed onto the stack */
            MOV_S       (MOV_LSB, CPU_REG_S, CPU_REG_ZERO, CPU_REG_PBR),
            DECS,

            MOV_S       (MOV_MSB, CPU_REG_S, CPU_REG_ZERO, CPU_REG_PC),
            DECS,
            MOV_S       (MOV_LSB, CPU_REG_S, CPU_REG_ZERO, CPU_REG_PC),
            DECS,
            MOV_P       (CPU_REG_P, CPU_REG_TEMP),
            MOV_S       (MOV_LSB, CPU_REG_S, CPU_REG_ZERO, CPU_REG_TEMP),
            DECS,

            SET_P       (CPU_STATUS_FLAG_I),

            /* when in native mode... */
            SKIPS       (1, CPU_STATUS_FLAG_E),
            /* the D flag gets cleared */
            CLR_P       (CPU_STATUS_FLAG_D),

            MOV_L       (MOV_LSB, CPU_REG_ADDR, CPU_REG_ZERO, CPU_REG_TEMP),
            ADDR_OFFI   (1, ADDR_OFF_BANK_WRAP),
            MOV_L       (MOV_MSB, CPU_REG_ADDR, CPU_REG_ZERO, CPU_REG_TEMP),
            MOV_RRW     (CPU_REG_TEMP, CPU_REG_PC, MOV_RRW_WORD),
            MOV_RRW     (CPU_REG_ZERO, CPU_REG_PBR, MOV_RRW_BYTE),
            // MOV_LPC     (MOV_LSB, CPU_REG_INST),
            // DECODE
        }
    },

    /**************************************************************************************/
    /*                                      BRL                                           */
    /**************************************************************************************/

    [CPU_OPCODE_BRL_PC_RELL] = {
        .uops = {
            MOV_LPC     (MOV_LSB, CPU_REG_ADDR),
            MOV_LPC     (MOV_MSB, CPU_REG_ADDR),
            ADDR_OFFR   (CPU_REG_PC, ADDR_OFF_BANK_WRAP),
            IO          /* branch taken */,
            MOV_RRW     (CPU_REG_ADDR, CPU_REG_PC, MOV_RRW_WORD),
        }
    },

    /**************************************************************************************/
    /*                                      BVC                                           */
    /**************************************************************************************/

    [CPU_OPCODE_BVC_PC_REL] = {
        .uops = {
            MOV_LPC     (MOV_LSB, CPU_REG_ADDR),
            SEXT        (CPU_REG_ADDR),
            ADDR_OFFRS  (CPU_REG_PC, ADDR_OFF_BANK_WRAP, ADDR_OFF_SIGNED),
            SKIPS       (2, CPU_STATUS_FLAG_V),
            IO          /* branch taken */,
            MOV_RRW     (CPU_REG_ADDR, CPU_REG_PC, MOV_RRW_WORD),
        }
    },

    /**************************************************************************************/
    /*                                      BVS                                           */
    /**************************************************************************************/

    [CPU_OPCODE_BVS_PC_REL] = {
        .uops = {
            MOV_LPC     (MOV_LSB, CPU_REG_ADDR),
            SEXT        (CPU_REG_ADDR),
            ADDR_OFFRS  (CPU_REG_PC, ADDR_OFF_BANK_WRAP, ADDR_OFF_SIGNED),
            SKIPC       (2, CPU_STATUS_FLAG_V),
            IO          /* branch taken */,
            MOV_RRW     (CPU_REG_ADDR, CPU_REG_PC, MOV_RRW_WORD),
        }
    },

    /**************************************************************************************/
    /*                                      CLC                                           */
    /**************************************************************************************/

    [CPU_OPCODE_CLC_IMP] = {
        .uops = {
            IO,
            CLR_P       (CPU_STATUS_FLAG_C)
        }
    },

    /**************************************************************************************/
    /*                                      CLD                                           */
    /**************************************************************************************/

    [CPU_OPCODE_CLD_IMP] = {
        .uops = {
            IO,
            CLR_P       (CPU_STATUS_FLAG_D)
        }
    },

    /**************************************************************************************/
    /*                                      CLI                                           */
    /**************************************************************************************/

    [CPU_OPCODE_CLI_IMP] = {
        .uops = {
            IO,
            CLR_P       (CPU_STATUS_FLAG_I)
        }
    },

    /**************************************************************************************/
    /*                                      CLV                                           */
    /**************************************************************************************/

    [CPU_OPCODE_CLV_IMP] = {
        .uops = {
            IO,
            CLR_P       (CPU_STATUS_FLAG_V)
        }
    },

    /**************************************************************************************/
    /*                                      CMP                                           */
    /**************************************************************************************/

    [CPU_OPCODE_CMP_ABS] = {
        .uops = {
            MOV_LPC     (MOV_LSB, CPU_REG_ADDR),
            MOV_LPC     (MOV_MSB, CPU_REG_ADDR),
            MOV_RRW     (CPU_REG_DBR, CPU_REG_BANK, MOV_RRW_BYTE),
            MOV_L       (MOV_LSB, CPU_REG_ADDR, CPU_REG_BANK, CPU_REG_TEMP),
            SKIPS       (2, CPU_STATUS_FLAG_M),
            ADDR_OFFI   (1, ADDR_OFF_BANK_NEXT),
            MOV_L       (MOV_MSB, CPU_REG_ADDR, CPU_REG_BANK, CPU_REG_TEMP),
            ALU_OP      (ALU_OP_CMP, CPU_STATUS_FLAG_M, CPU_REG_ACCUM, CPU_REG_TEMP),
            // CHK_ZNW     (CPU_REG_TEMP, 0),
        }
    },
    [CPU_OPCODE_CMP_ABS_X] = {
        .uops = {
            MOV_LPC     (MOV_LSB, CPU_REG_ADDR),
            MOV_LPC     (MOV_MSB, CPU_REG_ADDR),
            MOV_RRW     (CPU_REG_DBR, CPU_REG_BANK, MOV_RRW_BYTE),
            ADDR_OFFR   (CPU_REG_X, ADDR_OFF_BANK_NEXT),
            SKIPC       (2, CPU_STATUS_FLAG_PAGE),
            SKIPS       (1, CPU_STATUS_FLAG_X),
            IO          /* X=0 or page boundary */,
            MOV_L       (MOV_LSB, CPU_REG_ADDR, CPU_REG_BANK, CPU_REG_TEMP),
            SKIPS       (2, CPU_STATUS_FLAG_M),
            ADDR_OFFI   (1, ADDR_OFF_BANK_NEXT),
            MOV_L       (MOV_MSB, CPU_REG_ADDR, CPU_REG_BANK, CPU_REG_TEMP),
            ALU_OP      (ALU_OP_CMP, CPU_STATUS_FLAG_M, CPU_REG_ACCUM, CPU_REG_TEMP),
            // CHK_ZNW     (CPU_REG_TEMP, 0),
        }
    },
    [CPU_OPCODE_CMP_ABS_Y] = {
        .uops = {
            MOV_LPC     (MOV_LSB, CPU_REG_ADDR),
            MOV_LPC     (MOV_MSB, CPU_REG_ADDR),
            MOV_RRW     (CPU_REG_DBR, CPU_REG_BANK, MOV_RRW_BYTE),
            ADDR_OFFR   (CPU_REG_Y, ADDR_OFF_BANK_NEXT),
            SKIPC       (2, CPU_STATUS_FLAG_PAGE),
            SKIPS       (1, CPU_STATUS_FLAG_X),
            IO          /* X=0 or page boundary */,
            MOV_L       (MOV_LSB, CPU_REG_ADDR, CPU_REG_BANK, CPU_REG_TEMP),
            SKIPS       (2, CPU_STATUS_FLAG_M),
            ADDR_OFFI   (1, ADDR_OFF_BANK_NEXT),
            MOV_L       (MOV_MSB, CPU_REG_ADDR, CPU_REG_BANK, CPU_REG_TEMP),
            ALU_OP      (ALU_OP_CMP, CPU_STATUS_FLAG_M, CPU_REG_ACCUM, CPU_REG_TEMP),
            // CHK_ZNW     (CPU_REG_TEMP, 0),
        }
    },
    [CPU_OPCODE_CMP_ABSL] = {
        .uops = {
            MOV_LPC     (MOV_LSB, CPU_REG_ADDR),
            MOV_LPC     (MOV_MSB, CPU_REG_ADDR),
            MOV_LPC     (MOV_LSB, CPU_REG_BANK),
            MOV_L       (MOV_LSB, CPU_REG_ADDR, CPU_REG_BANK, CPU_REG_TEMP),
            SKIPS       (2, CPU_STATUS_FLAG_M),
            ADDR_OFFI   (1, ADDR_OFF_BANK_NEXT),
            MOV_L       (MOV_MSB, CPU_REG_ADDR, CPU_REG_BANK, CPU_REG_TEMP),
            ALU_OP      (ALU_OP_CMP, CPU_STATUS_FLAG_M, CPU_REG_ACCUM, CPU_REG_TEMP),
            // CHK_ZNW     (CPU_REG_TEMP, 0),
        }
    },
    [CPU_OPCODE_CMP_ABSL_X] = {
        .uops = {
            MOV_LPC     (MOV_LSB, CPU_REG_ADDR),
            MOV_LPC     (MOV_MSB, CPU_REG_ADDR),
            MOV_LPC     (MOV_LSB, CPU_REG_BANK),
            ADDR_OFFR   (CPU_REG_X, ADDR_OFF_BANK_NEXT),
            MOV_L       (MOV_LSB, CPU_REG_ADDR, CPU_REG_BANK, CPU_REG_TEMP),
            SKIPS       (2, CPU_STATUS_FLAG_M),
            ADDR_OFFI   (1, ADDR_OFF_BANK_NEXT),
            MOV_L       (MOV_MSB, CPU_REG_ADDR, CPU_REG_BANK, CPU_REG_TEMP),
            ALU_OP      (ALU_OP_CMP, CPU_STATUS_FLAG_M, CPU_REG_ACCUM, CPU_REG_TEMP),
            // CHK_ZNW     (CPU_REG_TEMP, 0),
        }
    },
    [CPU_OPCODE_CMP_DIR] = {
        .uops = {
            MOV_LPC     (MOV_LSB, CPU_REG_ADDR),
            ZEXT        (CPU_REG_ADDR),
            ADDR_OFFR   (CPU_REG_D, ADDR_OFF_BANK_WRAP),
            SKIPC       (1, CPU_STATUS_FLAG_DL),
            IO          /* DL != 0 */,
            MOV_RRW     (CPU_REG_ZERO, CPU_REG_BANK, MOV_RRW_BYTE),
            MOV_L       (MOV_LSB, CPU_REG_ADDR, CPU_REG_BANK, CPU_REG_TEMP),
            SKIPS       (2, CPU_STATUS_FLAG_M),
            ADDR_OFFI   (1, ADDR_OFF_BANK_NEXT),
            MOV_L       (MOV_MSB, CPU_REG_ADDR, CPU_REG_BANK, CPU_REG_TEMP),
            ALU_OP      (ALU_OP_CMP, CPU_STATUS_FLAG_M, CPU_REG_ACCUM, CPU_REG_TEMP),
            // CHK_ZNW     (CPU_REG_TEMP, 0),
        }
    },
    [CPU_OPCODE_CMP_S_REL] = {
        .uops = {
            MOV_LPC     (MOV_LSB, CPU_REG_ADDR),
            ZEXT        (CPU_REG_ADDR),
            ADDR_OFFR   (CPU_REG_S, ADDR_OFF_BANK_WRAP),
            IO,
            MOV_L       (MOV_LSB, CPU_REG_ADDR, CPU_REG_ZERO, CPU_REG_TEMP),
            SKIPS       (2, CPU_STATUS_FLAG_M),
            ADDR_OFFI   (1, ADDR_OFF_BANK_WRAP),
            MOV_L       (MOV_MSB, CPU_REG_ADDR, CPU_REG_ZERO, CPU_REG_TEMP),
            ALU_OP      (ALU_OP_CMP, CPU_STATUS_FLAG_M, CPU_REG_ACCUM, CPU_REG_TEMP),
            // CHK_ZNW     (CPU_REG_TEMP, 0),
        },
    },
    [CPU_OPCODE_CMP_DIR_X] = {
        .uops = {
            MOV_LPC     (MOV_LSB, CPU_REG_ADDR),
            ZEXT        (CPU_REG_ADDR),
            SKIPC       (1, CPU_STATUS_FLAG_DL),
            IO          /* DL != 0 */,
            ADDR_OFFR   (CPU_REG_D, ADDR_OFF_BANK_WRAP),
            ADDR_OFFR   (CPU_REG_X, ADDR_OFF_BANK_WRAP),
            IO,
            MOV_L       (MOV_LSB, CPU_REG_ADDR, CPU_REG_ZERO, CPU_REG_TEMP),
            SKIPS       (2, CPU_STATUS_FLAG_M),
            ADDR_OFFI   (1, ADDR_OFF_BANK_WRAP),
            MOV_L       (MOV_MSB, CPU_REG_ADDR, CPU_REG_ZERO, CPU_REG_TEMP),
            ALU_OP      (ALU_OP_CMP, CPU_STATUS_FLAG_M, CPU_REG_ACCUM, CPU_REG_TEMP),
            // CHK_ZNW     (CPU_REG_TEMP, 0),
        }
    },

    [CPU_OPCODE_CMP_DIR_IND] = {
        .uops = {
            MOV_LPC     (MOV_LSB, CPU_REG_ADDR),
            ZEXT        (CPU_REG_ADDR),
            SKIPC       (1, CPU_STATUS_FLAG_DL),
            IO          /* DL != 0 */,
            ADDR_OFFR   (CPU_REG_D, ADDR_OFF_BANK_WRAP),
            MOV_RRW     (CPU_REG_ZERO, CPU_REG_BANK, MOV_RRW_BYTE),
            MOV_L       (MOV_LSB, CPU_REG_ADDR, CPU_REG_BANK, CPU_REG_TEMP),
            ADDR_OFFI   (1, ADDR_OFF_BANK_NEXT),
            MOV_L       (MOV_MSB, CPU_REG_ADDR, CPU_REG_BANK, CPU_REG_TEMP),
            MOV_RRW     (CPU_REG_TEMP, CPU_REG_ADDR, MOV_RRW_WORD),
            MOV_RRW     (CPU_REG_DBR, CPU_REG_BANK, MOV_RRW_BYTE),
            MOV_L       (MOV_LSB, CPU_REG_ADDR, CPU_REG_BANK, CPU_REG_TEMP),
            SKIPS       (2, CPU_STATUS_FLAG_M),
            ADDR_OFFI   (1, ADDR_OFF_BANK_NEXT),
            MOV_L       (MOV_MSB, CPU_REG_ADDR, CPU_REG_BANK, CPU_REG_TEMP),
            ALU_OP      (ALU_OP_CMP, CPU_STATUS_FLAG_M, CPU_REG_ACCUM, CPU_REG_TEMP),
            // CHK_ZNW     (CPU_REG_TEMP, 0),
        }
    },

    [CPU_OPCODE_CMP_DIR_INDL] = {
        .uops = {
            MOV_LPC     (MOV_LSB, CPU_REG_ADDR),
            ZEXT        (CPU_REG_ADDR),
            SKIPC       (1, CPU_STATUS_FLAG_DL),
            IO          /* DL != 0 */,
            ADDR_OFFR   (CPU_REG_D, ADDR_OFF_BANK_WRAP),
            MOV_RRW     (CPU_REG_ZERO, CPU_REG_BANK, MOV_RRW_BYTE),
            MOV_L       (MOV_LSB, CPU_REG_ADDR, CPU_REG_BANK, CPU_REG_TEMP),
            ADDR_OFFI   (1, ADDR_OFF_BANK_NEXT),
            MOV_L       (MOV_MSB, CPU_REG_ADDR, CPU_REG_BANK, CPU_REG_TEMP),
            ADDR_OFFI   (1, ADDR_OFF_BANK_NEXT),
            MOV_L       (MOV_LSB, CPU_REG_ADDR, CPU_REG_BANK, CPU_REG_BANK),
            MOV_RRW     (CPU_REG_TEMP, CPU_REG_ADDR, MOV_RRW_WORD),
            MOV_L       (MOV_LSB, CPU_REG_ADDR, CPU_REG_BANK, CPU_REG_TEMP),
            SKIPS       (2, CPU_STATUS_FLAG_M),
            ADDR_OFFI   (1, ADDR_OFF_BANK_NEXT),
            MOV_L       (MOV_MSB, CPU_REG_ADDR, CPU_REG_BANK, CPU_REG_TEMP),
            ALU_OP      (ALU_OP_CMP, CPU_STATUS_FLAG_M, CPU_REG_ACCUM, CPU_REG_TEMP),
            // CHK_ZNW     (CPU_REG_TEMP, 0),
        }
    },

    [CPU_OPCODE_CMP_S_REL_IND_Y] = {
        .uops = {
            MOV_LPC     (MOV_LSB, CPU_REG_ADDR),
            ZEXT        (CPU_REG_ADDR),
            IO,
            ADDR_OFFR   (CPU_REG_S, ADDR_OFF_BANK_WRAP),
            MOV_L       (MOV_LSB, CPU_REG_ADDR, CPU_REG_ZERO, CPU_REG_TEMP),
            ADDR_OFFI   (1, ADDR_OFF_BANK_WRAP),
            MOV_L       (MOV_MSB, CPU_REG_ADDR, CPU_REG_ZERO, CPU_REG_TEMP),
            MOV_RRW     (CPU_REG_TEMP, CPU_REG_ADDR, MOV_RRW_WORD),
            MOV_RRW     (CPU_REG_DBR, CPU_REG_BANK, MOV_RRW_BYTE),
            IO,
            ADDR_OFFR   (CPU_REG_Y, ADDR_OFF_BANK_NEXT),
            MOV_L       (MOV_LSB, CPU_REG_ADDR, CPU_REG_BANK, CPU_REG_TEMP),
            SKIPS       (2, CPU_STATUS_FLAG_M),
            ADDR_OFFI   (1, ADDR_OFF_BANK_NEXT),
            MOV_L       (MOV_MSB, CPU_REG_ADDR, CPU_REG_BANK, CPU_REG_TEMP),
            ALU_OP      (ALU_OP_CMP, CPU_STATUS_FLAG_M, CPU_REG_ACCUM, CPU_REG_TEMP),
            // CHK_ZNW     (CPU_REG_TEMP, 0),
        }
    },

    [CPU_OPCODE_CMP_DIR_X_IND] = {
        .uops = {
            MOV_LPC     (MOV_LSB, CPU_REG_ADDR),
            ZEXT        (CPU_REG_ADDR),
            SKIPC       (1, CPU_STATUS_FLAG_DL),
            IO          /* DL != 0 */,
            ADDR_OFFR   (CPU_REG_D, ADDR_OFF_BANK_WRAP),
            ADDR_OFFR   (CPU_REG_X, ADDR_OFF_BANK_WRAP),
            IO,
            MOV_L       (MOV_LSB, CPU_REG_ADDR, CPU_REG_ZERO, CPU_REG_TEMP),
            ADDR_OFFI   (1, ADDR_OFF_BANK_WRAP),
            MOV_L       (MOV_MSB, CPU_REG_ADDR, CPU_REG_ZERO, CPU_REG_TEMP),
            MOV_RRW     (CPU_REG_TEMP, CPU_REG_ADDR, MOV_RRW_WORD),
            MOV_RRW     (CPU_REG_DBR, CPU_REG_BANK, MOV_RRW_BYTE),
            MOV_L       (MOV_LSB, CPU_REG_ADDR, CPU_REG_BANK, CPU_REG_TEMP),
            SKIPS       (2, CPU_STATUS_FLAG_M),
            ADDR_OFFI   (1, ADDR_OFF_BANK_NEXT),
            MOV_L       (MOV_MSB, CPU_REG_ADDR, CPU_REG_BANK, CPU_REG_TEMP),
            ALU_OP      (ALU_OP_CMP, CPU_STATUS_FLAG_M, CPU_REG_ACCUM, CPU_REG_TEMP),
            // CHK_ZNW     (CPU_REG_TEMP, 0),
        }
    },

    [CPU_OPCODE_CMP_DIR_IND_Y] = {
        .uops = {
            MOV_LPC     (MOV_LSB, CPU_REG_ADDR),
            ZEXT        (CPU_REG_ADDR),
            SKIPC       (1, CPU_STATUS_FLAG_DL),
            IO          /* DL != 0 */,
            ADDR_OFFR   (CPU_REG_D, ADDR_OFF_BANK_WRAP),
            MOV_RRW     (CPU_REG_ZERO, CPU_REG_BANK, MOV_RRW_BYTE),
            MOV_L       (MOV_LSB, CPU_REG_ADDR, CPU_REG_BANK, CPU_REG_TEMP),
            ADDR_OFFI   (1, ADDR_OFF_BANK_NEXT),
            MOV_L       (MOV_MSB, CPU_REG_ADDR, CPU_REG_BANK, CPU_REG_TEMP),
            MOV_RRW     (CPU_REG_TEMP, CPU_REG_ADDR, MOV_RRW_WORD),
            MOV_RRW     (CPU_REG_DBR, CPU_REG_BANK, MOV_RRW_BYTE),
            ADDR_OFFR   (CPU_REG_Y, ADDR_OFF_BANK_NEXT),
            SKIPS       (2, CPU_STATUS_FLAG_X),
            SKIPC       (1, CPU_STATUS_FLAG_PAGE),
            IO          /* X = 0 or page crossed */,
            MOV_L       (MOV_LSB, CPU_REG_ADDR, CPU_REG_BANK, CPU_REG_TEMP),
            SKIPS       (2, CPU_STATUS_FLAG_M),
            ADDR_OFFI   (1, ADDR_OFF_BANK_NEXT),
            MOV_L       (MOV_MSB, CPU_REG_ADDR, CPU_REG_BANK, CPU_REG_TEMP),
            ALU_OP      (ALU_OP_CMP, CPU_STATUS_FLAG_M, CPU_REG_ACCUM, CPU_REG_TEMP),
            // CHK_ZNW     (CPU_REG_TEMP, 0),
        }
    },

    [CPU_OPCODE_CMP_DIR_INDL_Y] = {
        .uops = {
            MOV_LPC     (MOV_LSB, CPU_REG_ADDR),
            ZEXT        (CPU_REG_ADDR),
            ADDR_OFFR   (CPU_REG_D, 0),
            SKIPC       (1, CPU_STATUS_FLAG_DL),
            IO          /* DL != 0 */,
            MOV_RRW     (CPU_REG_ZERO, CPU_REG_BANK, MOV_RRW_BYTE),
            MOV_L       (MOV_LSB, CPU_REG_ADDR, CPU_REG_BANK, CPU_REG_TEMP),
            ADDR_OFFI   (1, ADDR_OFF_BANK_NEXT),
            MOV_L       (MOV_MSB, CPU_REG_ADDR, CPU_REG_BANK, CPU_REG_TEMP),
            ADDR_OFFI   (1, ADDR_OFF_BANK_NEXT),
            MOV_L       (MOV_LSB, CPU_REG_ADDR, CPU_REG_BANK, CPU_REG_BANK),
            MOV_RRW     (CPU_REG_TEMP, CPU_REG_ADDR, MOV_RRW_WORD),
            // MOV_RRW     (CPU_REG_DBR, CPU_REG_BANK, MOV_RRW_BYTE),
            ADDR_OFFR   (CPU_REG_Y, ADDR_OFF_BANK_NEXT),
            MOV_L       (MOV_LSB, CPU_REG_ADDR, CPU_REG_BANK, CPU_REG_TEMP),
            SKIPS       (2, CPU_STATUS_FLAG_M),
            ADDR_OFFI   (1, ADDR_OFF_BANK_NEXT),
            MOV_L       (MOV_MSB, CPU_REG_ADDR, CPU_REG_BANK, CPU_REG_TEMP),
            ALU_OP      (ALU_OP_CMP, CPU_STATUS_FLAG_M, CPU_REG_ACCUM, CPU_REG_TEMP),
            // CHK_ZNW     (CPU_REG_TEMP, 0),
        }
    },

    [CPU_OPCODE_CMP_IMM] = {
        .uops = {
            MOV_LPC     (MOV_LSB, CPU_REG_TEMP),
            SKIPS       (1, CPU_STATUS_FLAG_M),
            MOV_LPC     (MOV_MSB, CPU_REG_TEMP),
            ALU_OP      (ALU_OP_CMP, CPU_STATUS_FLAG_M, CPU_REG_ACCUM, CPU_REG_TEMP),
            // CHK_ZNW     (CPU_REG_TEMP, 0),
        }
    },

    /**************************************************************************************/
    /*                                      COP                                           */
    /**************************************************************************************/

    [CPU_OPCODE_COP_S] = {
        .uops = {
            COP,
        }
    },

    /**************************************************************************************/
    /*                                      CPX                                           */
    /**************************************************************************************/

    [CPU_OPCODE_CPX_ABS] = {
        .uops = {
            MOV_LPC     (MOV_LSB, CPU_REG_ADDR),
            MOV_LPC     (MOV_MSB, CPU_REG_ADDR),
            MOV_RRW     (CPU_REG_DBR, CPU_REG_BANK, MOV_RRW_BYTE),
            MOV_L       (MOV_LSB, CPU_REG_ADDR, CPU_REG_BANK, CPU_REG_TEMP),
            SKIPS       (2, CPU_STATUS_FLAG_X),
            ADDR_OFFI   (1, ADDR_OFF_BANK_NEXT),
            MOV_L       (MOV_MSB, CPU_REG_ADDR, CPU_REG_BANK, CPU_REG_TEMP),
            ALU_OP      (ALU_OP_CMP, CPU_STATUS_FLAG_X, CPU_REG_X, CPU_REG_TEMP),
            // CHK_ZNW     (CPU_REG_TEMP, 0),
        }
    },

    [CPU_OPCODE_CPX_DIR] = {
        .uops = {
            MOV_LPC     (MOV_LSB, CPU_REG_ADDR),
            ZEXT        (CPU_REG_ADDR),
            ADDR_OFFR   (CPU_REG_D, ADDR_OFF_BANK_WRAP),
            SKIPC       (1, CPU_STATUS_FLAG_DL),
            IO          /* DL != 0 */,
            MOV_RRW     (CPU_REG_ZERO, CPU_REG_BANK, MOV_RRW_BYTE),
            MOV_L       (MOV_LSB, CPU_REG_ADDR, CPU_REG_BANK, CPU_REG_TEMP),
            SKIPS       (2, CPU_STATUS_FLAG_X),
            ADDR_OFFI   (1, ADDR_OFF_BANK_NEXT),
            MOV_L       (MOV_MSB, CPU_REG_ADDR, CPU_REG_BANK, CPU_REG_TEMP),
            ALU_OP      (ALU_OP_CMP, CPU_STATUS_FLAG_X, CPU_REG_X, CPU_REG_TEMP),
            // CHK_ZN      (CPU_REG_TEMP),
        }
    },

    [CPU_OPCODE_CPX_IMM] = {
        .uops = {
            MOV_LPC     (MOV_LSB, CPU_REG_TEMP),
            SKIPS       (1, CPU_STATUS_FLAG_X),
            MOV_LPC     (MOV_MSB, CPU_REG_TEMP),
            ALU_OP      (ALU_OP_CMP, CPU_STATUS_FLAG_X, CPU_REG_X, CPU_REG_TEMP),
            // CHK_ZNW     (CPU_REG_TEMP, 0),
        }
    },

    /**************************************************************************************/
    /*                                      CPY                                           */
    /**************************************************************************************/

    [CPU_OPCODE_CPY_ABS] = {
        .uops = {
            MOV_LPC     (MOV_LSB, CPU_REG_ADDR),
            MOV_LPC     (MOV_MSB, CPU_REG_ADDR),
            MOV_RRW     (CPU_REG_DBR, CPU_REG_BANK, MOV_RRW_BYTE),
            MOV_L       (MOV_LSB, CPU_REG_ADDR, CPU_REG_BANK, CPU_REG_TEMP),
            SKIPS       (2, CPU_STATUS_FLAG_X),
            ADDR_OFFI   (1, ADDR_OFF_BANK_NEXT),
            MOV_L       (MOV_MSB, CPU_REG_ADDR, CPU_REG_BANK, CPU_REG_TEMP),
            ALU_OP      (ALU_OP_CMP, CPU_STATUS_FLAG_X, CPU_REG_Y, CPU_REG_TEMP),
            // CHK_ZNW     (CPU_REG_TEMP, 0),
        }
    },

    [CPU_OPCODE_CPY_DIR] = {
        .uops = {
            MOV_LPC     (MOV_LSB, CPU_REG_ADDR),
            ZEXT        (CPU_REG_ADDR),
            ADDR_OFFR   (CPU_REG_D, ADDR_OFF_BANK_WRAP),
            SKIPC       (1, CPU_STATUS_FLAG_DL),
            IO          /* DL != 0 */,
            MOV_RRW     (CPU_REG_ZERO, CPU_REG_BANK, MOV_RRW_BYTE),
            MOV_L       (MOV_LSB, CPU_REG_ADDR, CPU_REG_BANK, CPU_REG_TEMP),
            SKIPS       (2, CPU_STATUS_FLAG_X),
            ADDR_OFFI   (1, ADDR_OFF_BANK_NEXT),
            MOV_L       (MOV_MSB, CPU_REG_ADDR, CPU_REG_BANK, CPU_REG_TEMP),
            ALU_OP      (ALU_OP_CMP, CPU_STATUS_FLAG_X, CPU_REG_Y, CPU_REG_TEMP),
            // CHK_ZN      (CPU_REG_TEMP),
        }
    },

    [CPU_OPCODE_CPY_IMM] = {
        .uops = {
            MOV_LPC     (MOV_LSB, CPU_REG_TEMP),
            SKIPS       (1, CPU_STATUS_FLAG_X),
            MOV_LPC     (MOV_MSB, CPU_REG_TEMP),
            ALU_OP      (ALU_OP_CMP, CPU_STATUS_FLAG_X, CPU_REG_Y, CPU_REG_TEMP),
            // CHK_ZNW     (CPU_REG_TEMP, 0),
        }
    },

    /**************************************************************************************/
    /*                                      DEC                                           */
    /**************************************************************************************/

    [CPU_OPCODE_DEC_ABS] = {
        .uops = {
            MOV_LPC     (MOV_LSB, CPU_REG_ADDR),
            MOV_LPC     (MOV_MSB, CPU_REG_ADDR),
            MOV_RRW     (CPU_REG_DBR, CPU_REG_BANK, MOV_RRW_BYTE),
            MOV_L       (MOV_LSB, CPU_REG_ADDR, CPU_REG_BANK, CPU_REG_TEMP),
            SKIPS       (2, CPU_STATUS_FLAG_M),
            ADDR_OFFI   (1, ADDR_OFF_BANK_NEXT),
            MOV_L       (MOV_MSB, CPU_REG_ADDR, CPU_REG_BANK, CPU_REG_TEMP),
            IO,
            ALU_OP      (ALU_OP_DEC, CPU_STATUS_FLAG_M, CPU_REG_TEMP, CPU_REG_ZERO),
            // CHK_ZNW     (CPU_REG_TEMP, 0),
            SKIPS       (2, CPU_STATUS_FLAG_M),
            MOV_S       (MOV_MSB, CPU_REG_ADDR, CPU_REG_BANK, CPU_REG_TEMP),
            ADDR_OFFIS  (0xffff, ADDR_OFF_BANK_NEXT, ADDR_OFF_SIGNED),
            MOV_S       (MOV_LSB, CPU_REG_ADDR, CPU_REG_BANK, CPU_REG_TEMP),
        }
    },

    [CPU_OPCODE_DEC_ACC] = {
        .uops = {
            IO,
            ALU_OP      (ALU_OP_DEC, CPU_STATUS_FLAG_M, CPU_REG_ACCUM, CPU_REG_ZERO),
            MOV_RR      (CPU_REG_TEMP, CPU_REG_ACCUM),
            // CHK_ZN      (CPU_REG_ACCUM),
        }
    },

    [CPU_OPCODE_DEC_ABS_X] = {
        .uops = {
            MOV_LPC     (MOV_LSB, CPU_REG_ADDR),
            MOV_LPC     (MOV_MSB, CPU_REG_ADDR),
            MOV_RRW     (CPU_REG_DBR, CPU_REG_BANK, MOV_RRW_BYTE),
            IO,
            ADDR_OFFR   (CPU_REG_X, ADDR_OFF_BANK_NEXT),
            MOV_L       (MOV_LSB, CPU_REG_ADDR, CPU_REG_BANK, CPU_REG_TEMP),
            SKIPS       (2, CPU_STATUS_FLAG_M),
            ADDR_OFFI   (1, ADDR_OFF_BANK_NEXT),
            MOV_L       (MOV_MSB, CPU_REG_ADDR, CPU_REG_BANK, CPU_REG_TEMP),
            ALU_OP      (ALU_OP_DEC, CPU_STATUS_FLAG_M, CPU_REG_TEMP, CPU_REG_ZERO),
            // CHK_ZNW     (CPU_REG_TEMP, 0),
            SKIPS       (2, CPU_STATUS_FLAG_M),
            MOV_S       (MOV_MSB, CPU_REG_ADDR, CPU_REG_BANK, CPU_REG_TEMP),
            ADDR_OFFIS  (0xffff, ADDR_OFF_BANK_WRAP, ADDR_OFF_SIGNED),
            MOV_S       (MOV_LSB, CPU_REG_ADDR, CPU_REG_BANK, CPU_REG_TEMP),
        }
    },

    [CPU_OPCODE_DEC_DIR] = {
        .uops = {
            MOV_LPC     (MOV_LSB, CPU_REG_ADDR),
            ZEXT        (CPU_REG_ADDR),
            SKIPC       (1, CPU_STATUS_FLAG_DL),
            IO          /* DL != 0 */,
            ADDR_OFFR   (CPU_REG_D, ADDR_OFF_BANK_WRAP),
            MOV_RRW     (CPU_REG_ZERO, CPU_REG_BANK, MOV_RRW_BYTE),
            MOV_L       (MOV_LSB, CPU_REG_ADDR, CPU_REG_BANK, CPU_REG_TEMP),
            SKIPS       (2, CPU_STATUS_FLAG_M),
            ADDR_OFFI   (1, ADDR_OFF_BANK_NEXT),
            MOV_L       (MOV_MSB, CPU_REG_ADDR, CPU_REG_BANK, CPU_REG_TEMP),
            IO,
            ALU_OP      (ALU_OP_DEC, CPU_STATUS_FLAG_M, CPU_REG_TEMP, CPU_REG_ZERO),
            // CHK_ZNW     (CPU_REG_TEMP, 0),
            SKIPS       (2, CPU_STATUS_FLAG_M),
            MOV_S       (MOV_MSB, CPU_REG_ADDR, CPU_REG_BANK, CPU_REG_TEMP),
            ADDR_OFFIS  (0xffff, ADDR_OFF_BANK_NEXT, ADDR_OFF_SIGNED),
            MOV_S       (MOV_LSB, CPU_REG_ADDR, CPU_REG_BANK, CPU_REG_TEMP),
        }
    },

    [CPU_OPCODE_DEC_DIR_X] = {
        .uops = {
            MOV_LPC     (MOV_LSB, CPU_REG_ADDR),
            ZEXT        (CPU_REG_ADDR),
            SKIPC       (1, CPU_STATUS_FLAG_DL),
            IO          /* DL != 0 */,
            IO,
            ADDR_OFFR   (CPU_REG_D, ADDR_OFF_BANK_WRAP),
            ADDR_OFFR   (CPU_REG_X, ADDR_OFF_BANK_WRAP),
            MOV_L       (MOV_LSB, CPU_REG_ADDR, CPU_REG_ZERO, CPU_REG_TEMP),
            SKIPS       (2, CPU_STATUS_FLAG_M),
            ADDR_OFFI   (1, ADDR_OFF_BANK_WRAP),
            MOV_L       (MOV_MSB, CPU_REG_ADDR, CPU_REG_ZERO, CPU_REG_TEMP),
            IO,
            ALU_OP      (ALU_OP_DEC, CPU_STATUS_FLAG_M, CPU_REG_TEMP, CPU_REG_ZERO),
            // CHK_ZNW     (CPU_REG_TEMP, 0),
            SKIPS       (2, CPU_STATUS_FLAG_M),
            MOV_S       (MOV_MSB, CPU_REG_ADDR, CPU_REG_ZERO, CPU_REG_TEMP),
            ADDR_OFFIS  (0xffff, ADDR_OFF_BANK_WRAP, ADDR_OFF_SIGNED),
            MOV_S       (MOV_LSB, CPU_REG_ADDR, CPU_REG_ZERO, CPU_REG_TEMP),
        }
    },

    /**************************************************************************************/
    /*                                      DEX                                           */
    /**************************************************************************************/

    [CPU_OPCODE_DEX_IMP] = {
        .uops = {
            IO,
            ALU_OP      (ALU_OP_DEC, CPU_STATUS_FLAG_X, CPU_REG_X, CPU_REG_ZERO),
            MOV_RR      (CPU_REG_TEMP, CPU_REG_X),
            // CHK_ZN      (CPU_REG_X),
        }
    },

    /**************************************************************************************/
    /*                                      DEY                                           */
    /**************************************************************************************/

    [CPU_OPCODE_DEY_IMP] = {
        .uops = {
            IO,
            ALU_OP      (ALU_OP_DEC, CPU_STATUS_FLAG_X, CPU_REG_Y, CPU_REG_ZERO),
            MOV_RR      (CPU_REG_TEMP, CPU_REG_Y),
            // CHK_ZN      (CPU_REG_Y),
        }
    },

    /**************************************************************************************/
    /*                                      EOR                                           */
    /**************************************************************************************/

    [CPU_OPCODE_EOR_ABS] = {
        .uops = {
            MOV_LPC     (MOV_LSB, CPU_REG_ADDR),
            MOV_LPC     (MOV_MSB, CPU_REG_ADDR),
            MOV_RRW     (CPU_REG_DBR, CPU_REG_BANK, MOV_RRW_BYTE),
            MOV_L       (MOV_LSB, CPU_REG_ADDR, CPU_REG_BANK, CPU_REG_TEMP),
            SKIPS       (2, CPU_STATUS_FLAG_M),
            ADDR_OFFI   (1, ADDR_OFF_BANK_NEXT),
            MOV_L       (MOV_MSB, CPU_REG_ADDR, CPU_REG_BANK, CPU_REG_TEMP),
            ALU_OP      (ALU_OP_XOR, CPU_STATUS_FLAG_M, CPU_REG_ACCUM, CPU_REG_TEMP),
            MOV_RR      (CPU_REG_TEMP, CPU_REG_ACCUM),
            // CHK_ZN      (CPU_REG_ACCUM),
        }
    },
    [CPU_OPCODE_EOR_ABS_X] = {
        .uops = {
            MOV_LPC     (MOV_LSB, CPU_REG_ADDR),
            MOV_LPC     (MOV_MSB, CPU_REG_ADDR),
            MOV_RRW     (CPU_REG_DBR, CPU_REG_BANK, MOV_RRW_BYTE),
            ADDR_OFFR   (CPU_REG_X, ADDR_OFF_BANK_NEXT),
            SKIPC       (2, CPU_STATUS_FLAG_PAGE),
            SKIPS       (1, CPU_STATUS_FLAG_X),
            IO          /* X=0 or page boundary */,
            MOV_L       (MOV_LSB, CPU_REG_ADDR, CPU_REG_BANK, CPU_REG_TEMP),
            SKIPS       (2, CPU_STATUS_FLAG_M),
            ADDR_OFFI   (1, ADDR_OFF_BANK_NEXT),
            MOV_L       (MOV_MSB, CPU_REG_ADDR, CPU_REG_BANK, CPU_REG_TEMP),
            ALU_OP      (ALU_OP_XOR, CPU_STATUS_FLAG_M, CPU_REG_ACCUM, CPU_REG_TEMP),
            MOV_RR      (CPU_REG_TEMP, CPU_REG_ACCUM),
            // CHK_ZN      (CPU_REG_ACCUM),
        }
    },
    [CPU_OPCODE_EOR_ABS_Y] = {
        .uops = {
            MOV_LPC     (MOV_LSB, CPU_REG_ADDR),
            MOV_LPC     (MOV_MSB, CPU_REG_ADDR),
            MOV_RRW     (CPU_REG_DBR, CPU_REG_BANK, MOV_RRW_BYTE),
            ADDR_OFFR   (CPU_REG_Y, ADDR_OFF_BANK_NEXT),
            SKIPC       (2, CPU_STATUS_FLAG_PAGE),
            SKIPS       (1, CPU_STATUS_FLAG_X),
            IO          /* X=0 or page boundary */,
            MOV_L       (MOV_LSB, CPU_REG_ADDR, CPU_REG_BANK, CPU_REG_TEMP),
            SKIPS       (2, CPU_STATUS_FLAG_M),
            ADDR_OFFI   (1, ADDR_OFF_BANK_NEXT),
            MOV_L       (MOV_MSB, CPU_REG_ADDR, CPU_REG_BANK, CPU_REG_TEMP),
            ALU_OP      (ALU_OP_XOR, CPU_STATUS_FLAG_M, CPU_REG_ACCUM, CPU_REG_TEMP),
            MOV_RR      (CPU_REG_TEMP, CPU_REG_ACCUM),
            // CHK_ZN      (CPU_REG_ACCUM),
        }
    },
    [CPU_OPCODE_EOR_ABSL] = {
        .uops = {
            MOV_LPC     (MOV_LSB, CPU_REG_ADDR),
            MOV_LPC     (MOV_MSB, CPU_REG_ADDR),
            MOV_LPC     (MOV_LSB, CPU_REG_BANK),
            MOV_L       (MOV_LSB, CPU_REG_ADDR, CPU_REG_BANK, CPU_REG_TEMP),
            SKIPS       (2, CPU_STATUS_FLAG_M),
            ADDR_OFFI   (1, ADDR_OFF_BANK_NEXT),
            MOV_L       (MOV_MSB, CPU_REG_ADDR, CPU_REG_BANK, CPU_REG_TEMP),
            ALU_OP      (ALU_OP_XOR, CPU_STATUS_FLAG_M, CPU_REG_ACCUM, CPU_REG_TEMP),
            MOV_RR      (CPU_REG_TEMP, CPU_REG_ACCUM),
            // CHK_ZN      (CPU_REG_ACCUM),
        }
    },
    [CPU_OPCODE_EOR_ABSL_X] = {
        .uops = {
            MOV_LPC     (MOV_LSB, CPU_REG_ADDR),
            MOV_LPC     (MOV_MSB, CPU_REG_ADDR),
            MOV_LPC     (MOV_LSB, CPU_REG_BANK),
            ADDR_OFFR   (CPU_REG_X, ADDR_OFF_BANK_NEXT),
            MOV_L       (MOV_LSB, CPU_REG_ADDR, CPU_REG_BANK, CPU_REG_TEMP),
            SKIPS       (2, CPU_STATUS_FLAG_M),
            ADDR_OFFI   (1, ADDR_OFF_BANK_NEXT),
            MOV_L       (MOV_MSB, CPU_REG_ADDR, CPU_REG_BANK, CPU_REG_TEMP),
            ALU_OP      (ALU_OP_XOR, CPU_STATUS_FLAG_M, CPU_REG_ACCUM, CPU_REG_TEMP),
            MOV_RR      (CPU_REG_TEMP, CPU_REG_ACCUM),
            // CHK_ZN      (CPU_REG_ACCUM),
        }
    },

    [CPU_OPCODE_EOR_DIR] = {
        .uops = {
            MOV_LPC     (MOV_LSB, CPU_REG_ADDR),
            ZEXT        (CPU_REG_ADDR),
            ADDR_OFFR   (CPU_REG_D, ADDR_OFF_BANK_WRAP),
            SKIPC       (1, CPU_STATUS_FLAG_DL),
            IO          /* DL != 0 */,
            MOV_RRW     (CPU_REG_ZERO, CPU_REG_BANK, MOV_RRW_BYTE),
            MOV_L       (MOV_LSB, CPU_REG_ADDR, CPU_REG_BANK, CPU_REG_TEMP),
            SKIPS       (2, CPU_STATUS_FLAG_M),
            ADDR_OFFI   (1, ADDR_OFF_BANK_NEXT),
            MOV_L       (MOV_MSB, CPU_REG_ADDR, CPU_REG_BANK, CPU_REG_TEMP),
            ALU_OP      (ALU_OP_XOR, CPU_STATUS_FLAG_M, CPU_REG_ACCUM, CPU_REG_TEMP),
            MOV_RR      (CPU_REG_TEMP, CPU_REG_ACCUM),
            // CHK_ZN      (CPU_REG_ACCUM),
        }
    },
    [CPU_OPCODE_EOR_S_REL] = {
        .uops = {
            MOV_LPC     (MOV_LSB, CPU_REG_ADDR),
            ZEXT        (CPU_REG_ADDR),
            ADDR_OFFR   (CPU_REG_S, ADDR_OFF_BANK_WRAP),
            IO,
            MOV_L       (MOV_LSB, CPU_REG_ADDR, CPU_REG_ZERO, CPU_REG_TEMP),
            SKIPS       (2, CPU_STATUS_FLAG_M),
            ADDR_OFFI   (1, ADDR_OFF_BANK_WRAP),
            MOV_L       (MOV_MSB, CPU_REG_ADDR, CPU_REG_ZERO, CPU_REG_TEMP),
            ALU_OP      (ALU_OP_XOR, CPU_STATUS_FLAG_M, CPU_REG_ACCUM, CPU_REG_TEMP),
            MOV_RR      (CPU_REG_TEMP, CPU_REG_ACCUM),
            // CHK_ZN      (CPU_REG_ACCUM),
        },
    },
    [CPU_OPCODE_EOR_DIR_X] = {
        .uops = {
            MOV_LPC     (MOV_LSB, CPU_REG_ADDR),
            ZEXT        (CPU_REG_ADDR),
            SKIPC       (1, CPU_STATUS_FLAG_DL),
            IO          /* DL != 0 */,
            ADDR_OFFR   (CPU_REG_D, ADDR_OFF_BANK_WRAP),
            ADDR_OFFR   (CPU_REG_X, ADDR_OFF_BANK_WRAP),
            IO,
            MOV_L       (MOV_LSB, CPU_REG_ADDR, CPU_REG_ZERO, CPU_REG_TEMP),
            SKIPS       (2, CPU_STATUS_FLAG_M),
            ADDR_OFFI   (1, ADDR_OFF_BANK_WRAP),
            MOV_L       (MOV_MSB, CPU_REG_ADDR, CPU_REG_ZERO, CPU_REG_TEMP),
            ALU_OP      (ALU_OP_XOR, CPU_STATUS_FLAG_M, CPU_REG_ACCUM, CPU_REG_TEMP),
            MOV_RR      (CPU_REG_TEMP, CPU_REG_ACCUM),
            // CHK_ZN      (CPU_REG_ACCUM),
        }
    },

    [CPU_OPCODE_EOR_DIR_IND] = {
        .uops = {
            MOV_LPC     (MOV_LSB, CPU_REG_ADDR),
            ZEXT        (CPU_REG_ADDR),
            SKIPC       (1, CPU_STATUS_FLAG_DL),
            IO          /* DL != 0 */,
            ADDR_OFFR   (CPU_REG_D, ADDR_OFF_BANK_WRAP),
            MOV_RRW     (CPU_REG_ZERO, CPU_REG_BANK, MOV_RRW_BYTE),
            MOV_L       (MOV_LSB, CPU_REG_ADDR, CPU_REG_BANK, CPU_REG_TEMP),
            ADDR_OFFI   (1, ADDR_OFF_BANK_NEXT),
            MOV_L       (MOV_MSB, CPU_REG_ADDR, CPU_REG_BANK, CPU_REG_TEMP),
            MOV_RRW     (CPU_REG_TEMP, CPU_REG_ADDR, MOV_RRW_WORD),
            MOV_RRW     (CPU_REG_DBR, CPU_REG_BANK, MOV_RRW_BYTE),
            MOV_L       (MOV_LSB, CPU_REG_ADDR, CPU_REG_BANK, CPU_REG_TEMP),
            SKIPS       (2, CPU_STATUS_FLAG_M),
            ADDR_OFFI   (1, ADDR_OFF_BANK_NEXT),
            MOV_L       (MOV_MSB, CPU_REG_ADDR, CPU_REG_BANK, CPU_REG_TEMP),
            ALU_OP      (ALU_OP_XOR, CPU_STATUS_FLAG_M, CPU_REG_ACCUM, CPU_REG_TEMP),
            MOV_RR      (CPU_REG_TEMP, CPU_REG_ACCUM),
            // CHK_ZN      (CPU_REG_ACCUM),
        }
    },

    [CPU_OPCODE_EOR_DIR_INDL] = {
        .uops = {
            MOV_LPC     (MOV_LSB, CPU_REG_ADDR),
            ZEXT        (CPU_REG_ADDR),
            SKIPC       (1, CPU_STATUS_FLAG_DL),
            IO          /* DL != 0 */,
            ADDR_OFFR   (CPU_REG_D, ADDR_OFF_BANK_WRAP),
            MOV_RRW     (CPU_REG_ZERO, CPU_REG_BANK, MOV_RRW_BYTE),
            MOV_L       (MOV_LSB, CPU_REG_ADDR, CPU_REG_BANK, CPU_REG_TEMP),
            ADDR_OFFI   (1, ADDR_OFF_BANK_NEXT),
            MOV_L       (MOV_MSB, CPU_REG_ADDR, CPU_REG_BANK, CPU_REG_TEMP),
            ADDR_OFFI   (1, ADDR_OFF_BANK_NEXT),
            MOV_L       (MOV_LSB, CPU_REG_ADDR, CPU_REG_BANK, CPU_REG_BANK),
            MOV_RRW     (CPU_REG_TEMP, CPU_REG_ADDR, MOV_RRW_WORD),
            MOV_L       (MOV_LSB, CPU_REG_ADDR, CPU_REG_BANK, CPU_REG_TEMP),
            SKIPS       (2, CPU_STATUS_FLAG_M),
            ADDR_OFFI   (1, ADDR_OFF_BANK_NEXT),
            MOV_L       (MOV_MSB, CPU_REG_ADDR, CPU_REG_BANK, CPU_REG_TEMP),
            ALU_OP      (ALU_OP_XOR, CPU_STATUS_FLAG_M, CPU_REG_ACCUM, CPU_REG_TEMP),
            MOV_RR      (CPU_REG_TEMP, CPU_REG_ACCUM),
            // CHK_ZN      (CPU_REG_ACCUM),
        }
    },

    [CPU_OPCODE_EOR_S_REL_IND_Y] = {
        .uops = {
            MOV_LPC     (MOV_LSB, CPU_REG_ADDR),
            ZEXT        (CPU_REG_ADDR),
            IO,
            ADDR_OFFR   (CPU_REG_S, ADDR_OFF_BANK_WRAP),
            MOV_L       (MOV_LSB, CPU_REG_ADDR, CPU_REG_ZERO, CPU_REG_TEMP),
            ADDR_OFFI   (1, ADDR_OFF_BANK_WRAP),
            MOV_L       (MOV_MSB, CPU_REG_ADDR, CPU_REG_ZERO, CPU_REG_TEMP),
            MOV_RRW     (CPU_REG_TEMP, CPU_REG_ADDR, MOV_RRW_WORD),
            MOV_RRW     (CPU_REG_DBR, CPU_REG_BANK, MOV_RRW_BYTE),
            IO,
            ADDR_OFFR   (CPU_REG_Y, ADDR_OFF_BANK_NEXT),
            MOV_L       (MOV_LSB, CPU_REG_ADDR, CPU_REG_BANK, CPU_REG_TEMP),
            SKIPS       (2, CPU_STATUS_FLAG_M),
            ADDR_OFFI   (1, ADDR_OFF_BANK_NEXT),
            MOV_L       (MOV_MSB, CPU_REG_ADDR, CPU_REG_BANK, CPU_REG_TEMP),
            ALU_OP      (ALU_OP_XOR, CPU_STATUS_FLAG_M, CPU_REG_ACCUM, CPU_REG_TEMP),
            MOV_RR      (CPU_REG_TEMP, CPU_REG_ACCUM),
            // CHK_ZN      (CPU_REG_ACCUM),

        }
    },

    [CPU_OPCODE_EOR_DIR_X_IND] = {
        .uops = {
            MOV_LPC     (MOV_LSB, CPU_REG_ADDR),
            ZEXT        (CPU_REG_ADDR),
            SKIPC       (1, CPU_STATUS_FLAG_DL),
            IO          /* DL != 0 */,
            ADDR_OFFR   (CPU_REG_D, ADDR_OFF_BANK_WRAP),
            ADDR_OFFR   (CPU_REG_X, ADDR_OFF_BANK_WRAP),
            IO,
            MOV_L       (MOV_LSB, CPU_REG_ADDR, CPU_REG_ZERO, CPU_REG_TEMP),
            ADDR_OFFI   (1, ADDR_OFF_BANK_WRAP),
            MOV_L       (MOV_MSB, CPU_REG_ADDR, CPU_REG_ZERO, CPU_REG_TEMP),
            MOV_RRW     (CPU_REG_TEMP, CPU_REG_ADDR, MOV_RRW_WORD),
            MOV_RRW     (CPU_REG_DBR, CPU_REG_BANK, MOV_RRW_BYTE),
            MOV_L       (MOV_LSB, CPU_REG_ADDR, CPU_REG_BANK, CPU_REG_TEMP),
            SKIPS       (2, CPU_STATUS_FLAG_M),
            ADDR_OFFI   (1, ADDR_OFF_BANK_NEXT),
            MOV_L       (MOV_MSB, CPU_REG_ADDR, CPU_REG_BANK, CPU_REG_TEMP),
            ALU_OP      (ALU_OP_XOR, CPU_STATUS_FLAG_M, CPU_REG_ACCUM, CPU_REG_TEMP),
            MOV_RR      (CPU_REG_TEMP, CPU_REG_ACCUM),
            // CHK_ZN      (CPU_REG_ACCUM),
        }
    },

    [CPU_OPCODE_EOR_DIR_IND_Y] = {
        .uops = {
            MOV_LPC     (MOV_LSB, CPU_REG_ADDR),
            ZEXT        (CPU_REG_ADDR),
            SKIPC       (1, CPU_STATUS_FLAG_DL),
            IO          /* DL != 0 */,
            ADDR_OFFR   (CPU_REG_D, ADDR_OFF_BANK_WRAP),
            MOV_RRW     (CPU_REG_ZERO, CPU_REG_BANK, MOV_RRW_BYTE),
            MOV_L       (MOV_LSB, CPU_REG_ADDR, CPU_REG_BANK, CPU_REG_TEMP),
            ADDR_OFFI   (1, ADDR_OFF_BANK_NEXT),
            MOV_L       (MOV_MSB, CPU_REG_ADDR, CPU_REG_BANK, CPU_REG_TEMP),
            MOV_RRW     (CPU_REG_TEMP, CPU_REG_ADDR, MOV_RRW_WORD),
            MOV_RRW     (CPU_REG_DBR, CPU_REG_BANK, MOV_RRW_BYTE),
            ADDR_OFFR   (CPU_REG_Y, ADDR_OFF_BANK_NEXT),
            SKIPS       (2, CPU_STATUS_FLAG_X),
            SKIPC       (1, CPU_STATUS_FLAG_PAGE),
            IO          /* X = 0 or page crossed */,
            MOV_L       (MOV_LSB, CPU_REG_ADDR, CPU_REG_BANK, CPU_REG_TEMP),
            SKIPS       (2, CPU_STATUS_FLAG_M),
            ADDR_OFFI   (1, ADDR_OFF_BANK_NEXT),
            MOV_L       (MOV_MSB, CPU_REG_ADDR, CPU_REG_BANK, CPU_REG_TEMP),
            ALU_OP      (ALU_OP_XOR, CPU_STATUS_FLAG_M, CPU_REG_ACCUM, CPU_REG_TEMP),
            MOV_RR      (CPU_REG_TEMP, CPU_REG_ACCUM),
            // CHK_ZN      (CPU_REG_ACCUM),
        }
    },

    [CPU_OPCODE_EOR_DIR_INDL_Y] = {
        .uops = {
            MOV_LPC     (MOV_LSB, CPU_REG_ADDR),
            ZEXT        (CPU_REG_ADDR),
            ADDR_OFFR   (CPU_REG_D, ADDR_OFF_BANK_WRAP),
            SKIPC       (1, CPU_STATUS_FLAG_DL),
            IO          /* DL != 0 */,
            MOV_RRW     (CPU_REG_ZERO, CPU_REG_BANK, MOV_RRW_BYTE),
            MOV_L       (MOV_LSB, CPU_REG_ADDR, CPU_REG_BANK, CPU_REG_TEMP),
            ADDR_OFFI   (1, ADDR_OFF_BANK_NEXT),
            MOV_L       (MOV_MSB, CPU_REG_ADDR, CPU_REG_BANK, CPU_REG_TEMP),
            ADDR_OFFI   (1, ADDR_OFF_BANK_NEXT),
            MOV_L       (MOV_LSB, CPU_REG_ADDR, CPU_REG_BANK, CPU_REG_BANK),
            MOV_RRW     (CPU_REG_TEMP, CPU_REG_ADDR, MOV_RRW_WORD),
            // MOV_RRW     (CPU_REG_DBR, CPU_REG_BANK, MOV_RRW_BYTE),
            ADDR_OFFR   (CPU_REG_Y, ADDR_OFF_BANK_NEXT),
            MOV_L       (MOV_LSB, CPU_REG_ADDR, CPU_REG_BANK, CPU_REG_TEMP),
            SKIPS       (2, CPU_STATUS_FLAG_M),
            ADDR_OFFI   (1, ADDR_OFF_BANK_NEXT),
            MOV_L       (MOV_MSB, CPU_REG_ADDR, CPU_REG_BANK, CPU_REG_TEMP),
            ALU_OP      (ALU_OP_XOR, CPU_STATUS_FLAG_M, CPU_REG_ACCUM, CPU_REG_TEMP),
            MOV_RR      (CPU_REG_TEMP, CPU_REG_ACCUM),
            // CHK_ZN      (CPU_REG_ACCUM),
        }
    },

    [CPU_OPCODE_EOR_IMM] = {
        .uops = {
            MOV_LPC     (MOV_LSB, CPU_REG_TEMP),
            SKIPS       (1, CPU_STATUS_FLAG_M),
            MOV_LPC     (MOV_MSB, CPU_REG_TEMP),
            ALU_OP      (ALU_OP_XOR, CPU_STATUS_FLAG_M, CPU_REG_ACCUM, CPU_REG_TEMP),
            MOV_RR      (CPU_REG_TEMP, CPU_REG_ACCUM),
            // CHK_ZN      (CPU_REG_ACCUM),
        }
    },

    /**************************************************************************************/
    /*                                      INC                                           */
    /**************************************************************************************/

    [CPU_OPCODE_INC_ABS] = {
        .uops = {
            MOV_LPC     (MOV_LSB, CPU_REG_ADDR),
            MOV_LPC     (MOV_MSB, CPU_REG_ADDR),
            MOV_RRW     (CPU_REG_DBR, CPU_REG_BANK, MOV_RRW_BYTE),
            MOV_L       (MOV_LSB, CPU_REG_ADDR, CPU_REG_BANK, CPU_REG_TEMP),
            SKIPS       (2, CPU_STATUS_FLAG_M),
            ADDR_OFFI   (1, ADDR_OFF_BANK_NEXT),
            MOV_L       (MOV_MSB, CPU_REG_ADDR, CPU_REG_BANK, CPU_REG_TEMP),
            IO,
            ALU_OP      (ALU_OP_INC, CPU_STATUS_FLAG_M, CPU_REG_TEMP, CPU_REG_ZERO),
            // CHK_ZNW     (CPU_REG_TEMP, 0),
            SKIPS       (2, CPU_STATUS_FLAG_M),
            MOV_S       (MOV_MSB, CPU_REG_ADDR, CPU_REG_BANK, CPU_REG_TEMP),
            ADDR_OFFIS  (0xffff, ADDR_OFF_BANK_NEXT, ADDR_OFF_SIGNED),
            MOV_S       (MOV_LSB, CPU_REG_ADDR, CPU_REG_BANK, CPU_REG_TEMP),
        }
    },

    [CPU_OPCODE_INC_ACC] = {
        .uops = {
            IO,
            ALU_OP      (ALU_OP_INC, CPU_STATUS_FLAG_M, CPU_REG_ACCUM, CPU_REG_ZERO),
            MOV_RR      (CPU_REG_TEMP, CPU_REG_ACCUM),
            // CHK_ZN      (CPU_REG_ACCUM),
        }
    },

    [CPU_OPCODE_INC_ABS_X] = {
        .uops = {
            MOV_LPC     (MOV_LSB, CPU_REG_ADDR),
            MOV_LPC     (MOV_MSB, CPU_REG_ADDR),
            MOV_RRW     (CPU_REG_DBR, CPU_REG_BANK, MOV_RRW_BYTE),
            IO,
            ADDR_OFFR   (CPU_REG_X, ADDR_OFF_BANK_NEXT),
            MOV_L       (MOV_LSB, CPU_REG_ADDR, CPU_REG_BANK, CPU_REG_TEMP),
            SKIPS       (2, CPU_STATUS_FLAG_M),
            ADDR_OFFI   (1, ADDR_OFF_BANK_NEXT),
            MOV_L       (MOV_MSB, CPU_REG_ADDR, CPU_REG_BANK, CPU_REG_TEMP),
            ALU_OP      (ALU_OP_INC, CPU_STATUS_FLAG_M, CPU_REG_TEMP, CPU_REG_ZERO),
            // CHK_ZNW     (CPU_REG_TEMP, 0),
            SKIPS       (2, CPU_STATUS_FLAG_M),
            MOV_S       (MOV_MSB, CPU_REG_ADDR, CPU_REG_BANK, CPU_REG_TEMP),
            ADDR_OFFIS  (0xffff, ADDR_OFF_BANK_WRAP, ADDR_OFF_SIGNED),
            MOV_S       (MOV_LSB, CPU_REG_ADDR, CPU_REG_BANK, CPU_REG_TEMP),
        }
    },

    [CPU_OPCODE_INC_DIR] = {
        .uops = {
            MOV_LPC     (MOV_LSB, CPU_REG_ADDR),
            ZEXT        (CPU_REG_ADDR),
            SKIPC       (1, CPU_STATUS_FLAG_DL),
            IO          /* DL != 0 */,
            ADDR_OFFR   (CPU_REG_D, ADDR_OFF_BANK_WRAP),
            MOV_RRW     (CPU_REG_ZERO, CPU_REG_BANK, MOV_RRW_BYTE),
            MOV_L       (MOV_LSB, CPU_REG_ADDR, CPU_REG_BANK, CPU_REG_TEMP),
            SKIPS       (2, CPU_STATUS_FLAG_M),
            ADDR_OFFI   (1, ADDR_OFF_BANK_NEXT),
            MOV_L       (MOV_MSB, CPU_REG_ADDR, CPU_REG_BANK, CPU_REG_TEMP),
            IO,
            ALU_OP      (ALU_OP_INC, CPU_STATUS_FLAG_M, CPU_REG_TEMP, CPU_REG_ZERO),
            // CHK_ZNW     (CPU_REG_TEMP, 0),
            SKIPS       (2, CPU_STATUS_FLAG_M),
            MOV_S       (MOV_MSB, CPU_REG_ADDR, CPU_REG_BANK, CPU_REG_TEMP),
            ADDR_OFFIS  (0xffff, ADDR_OFF_BANK_NEXT, ADDR_OFF_SIGNED),
            MOV_S       (MOV_LSB, CPU_REG_ADDR, CPU_REG_BANK, CPU_REG_TEMP),
        }
    },

    [CPU_OPCODE_INC_DIR_X] = {
        .uops = {
            MOV_LPC     (MOV_LSB, CPU_REG_ADDR),
            ZEXT        (CPU_REG_ADDR),
            SKIPC       (1, CPU_STATUS_FLAG_DL),
            IO          /* DL != 0 */,
            IO,
            ADDR_OFFR   (CPU_REG_D, ADDR_OFF_BANK_WRAP),
            ADDR_OFFR   (CPU_REG_X, ADDR_OFF_BANK_WRAP),
            MOV_L       (MOV_LSB, CPU_REG_ADDR, CPU_REG_ZERO, CPU_REG_TEMP),
            SKIPS       (2, CPU_STATUS_FLAG_M),
            ADDR_OFFI   (1, ADDR_OFF_BANK_WRAP),
            MOV_L       (MOV_MSB, CPU_REG_ADDR, CPU_REG_ZERO, CPU_REG_TEMP),
            IO,
            ALU_OP      (ALU_OP_INC, CPU_STATUS_FLAG_M, CPU_REG_TEMP, CPU_REG_ZERO),
            // CHK_ZNW     (CPU_REG_TEMP, 0),
            SKIPS       (2, CPU_STATUS_FLAG_M),
            MOV_S       (MOV_MSB, CPU_REG_ADDR, CPU_REG_ZERO, CPU_REG_TEMP),
            ADDR_OFFIS  (0xffff, ADDR_OFF_BANK_WRAP, ADDR_OFF_SIGNED),
            MOV_S       (MOV_LSB, CPU_REG_ADDR, CPU_REG_ZERO, CPU_REG_TEMP),
        }
    },

    /**************************************************************************************/
    /*                                      INX                                           */
    /**************************************************************************************/

    [CPU_OPCODE_INX_IMP] = {
        .uops = {
            IO,
            ALU_OP      (ALU_OP_INC, CPU_STATUS_FLAG_X, CPU_REG_X, CPU_REG_ZERO),
            MOV_RR      (CPU_REG_TEMP, CPU_REG_X),
            // CHK_ZN      (CPU_REG_X),
        }
    },

    /**************************************************************************************/
    /*                                      INY                                           */
    /**************************************************************************************/

    [CPU_OPCODE_INY_IMP] = {
        .uops = {
            IO,
            ALU_OP      (ALU_OP_INC, CPU_STATUS_FLAG_X, CPU_REG_Y, CPU_REG_ZERO),
            MOV_RR      (CPU_REG_TEMP, CPU_REG_Y),
            // CHK_ZN      (CPU_REG_Y),
        }
    },

    /**************************************************************************************/
    /*                                      JML                                           */
    /**************************************************************************************/

    [CPU_OPCODE_JML_ABS_IND] = {
        .uops = {
            MOV_LPC     (MOV_LSB, CPU_REG_ADDR),
            MOV_LPC     (MOV_MSB, CPU_REG_ADDR),
            MOV_L       (MOV_LSB, CPU_REG_ADDR, CPU_REG_ZERO, CPU_REG_TEMP),
            ADDR_OFFI   (1, ADDR_OFF_BANK_WRAP),
            MOV_L       (MOV_MSB, CPU_REG_ADDR, CPU_REG_ZERO, CPU_REG_TEMP),
            ADDR_OFFI   (1, ADDR_OFF_BANK_WRAP),
            MOV_L       (MOV_LSB, CPU_REG_ADDR, CPU_REG_ZERO, CPU_REG_PBR),
            MOV_RRW     (CPU_REG_TEMP, CPU_REG_PC, MOV_RRW_WORD),
        }
    },

    /**************************************************************************************/
    /*                                      JMP                                           */
    /**************************************************************************************/

    [CPU_OPCODE_JMP_ABS] = {
        .uops = {
            MOV_LPC     (MOV_LSB, CPU_REG_ADDR),
            MOV_LPC     (MOV_MSB, CPU_REG_ADDR),
            MOV_RRW     (CPU_REG_ADDR, CPU_REG_PC, MOV_RRW_WORD),
        }
    },

    [CPU_OPCODE_JMP_ABSL] = {
        .uops = {
            MOV_LPC     (MOV_LSB, CPU_REG_ADDR),
            MOV_LPC     (MOV_MSB, CPU_REG_ADDR),
            MOV_LPC     (MOV_LSB, CPU_REG_BANK),
            MOV_RRW     (CPU_REG_ADDR, CPU_REG_PC, MOV_RRW_WORD),
            MOV_RRW     (CPU_REG_BANK, CPU_REG_PBR, MOV_RRW_BYTE),
        }
    },

    [CPU_OPCODE_JMP_ABS_IND] = {
        .uops = {
            MOV_LPC     (MOV_LSB, CPU_REG_ADDR),
            MOV_LPC     (MOV_MSB, CPU_REG_ADDR),
            MOV_L       (MOV_LSB, CPU_REG_ADDR, CPU_REG_ZERO, CPU_REG_TEMP),
            ADDR_OFFI   (1, ADDR_OFF_BANK_WRAP),
            MOV_L       (MOV_MSB, CPU_REG_ADDR, CPU_REG_ZERO, CPU_REG_TEMP),
            MOV_RRW      (CPU_REG_TEMP, CPU_REG_PC, MOV_RRW_WORD),
        }
    },

    [CPU_OPCODE_JMP_ABS_X_IND] = {
        .uops = {
            MOV_LPC     (MOV_LSB, CPU_REG_ADDR),
            MOV_LPC     (MOV_MSB, CPU_REG_ADDR),
            IO,
            ADDR_OFFR   (CPU_REG_X, ADDR_OFF_BANK_WRAP),
            MOV_L       (MOV_LSB, CPU_REG_ADDR, CPU_REG_PBR, CPU_REG_TEMP),
            ADDR_OFFI   (1, ADDR_OFF_BANK_WRAP),
            MOV_L       (MOV_MSB, CPU_REG_ADDR, CPU_REG_PBR, CPU_REG_TEMP),
            MOV_RRW     (CPU_REG_TEMP, CPU_REG_PC, MOV_RRW_WORD),
        }
    },

    /**************************************************************************************/
    /*                                      JSL                                           */
    /**************************************************************************************/

    [CPU_OPCODE_JSL_ABSL] = {
        .uops = {
            MOV_LPC     (MOV_LSB, CPU_REG_ADDR),
            MOV_LPC     (MOV_MSB, CPU_REG_ADDR),
            MOV_S       (MOV_LSB, CPU_REG_S, CPU_REG_ZERO, CPU_REG_PBR),
            DECS,
            IO,
            MOV_L       (MOV_LSB, CPU_REG_PC, CPU_REG_PBR, CPU_REG_BANK),
            MOV_S       (MOV_MSB, CPU_REG_S, CPU_REG_ZERO, CPU_REG_PC),
            DECS,
            MOV_S       (MOV_LSB, CPU_REG_S, CPU_REG_ZERO, CPU_REG_PC),
            DECS,
            MOV_RRW     (CPU_REG_ADDR, CPU_REG_PC, MOV_RRW_WORD),
            MOV_RRW     (CPU_REG_BANK, CPU_REG_PBR, MOV_RRW_BYTE),
        }
    },

    /**************************************************************************************/
    /*                                      JSR                                           */
    /**************************************************************************************/

    [CPU_OPCODE_JSR_ABS] = {
        .uops = {
            MOV_LPC     (MOV_LSB, CPU_REG_ADDR),
            MOV_L       (MOV_MSB, CPU_REG_PC, CPU_REG_PBR, CPU_REG_ADDR),
            IO,
            MOV_S       (MOV_MSB, CPU_REG_S, CPU_REG_ZERO, CPU_REG_PC),
            DECS,
            MOV_S       (MOV_LSB, CPU_REG_S, CPU_REG_ZERO, CPU_REG_PC),
            DECS,
            MOV_RRW     (CPU_REG_ADDR, CPU_REG_PC, MOV_RRW_WORD),
        }
    },

    [CPU_OPCODE_JSR_ABS_X_IND] = {
        .uops = {
            MOV_LPC     (MOV_LSB, CPU_REG_ADDR),
            MOV_S       (MOV_MSB, CPU_REG_S, CPU_REG_ZERO, CPU_REG_PC),
            DECS,
            MOV_S       (MOV_LSB, CPU_REG_S, CPU_REG_ZERO, CPU_REG_PC),
            DECS,
            MOV_L       (MOV_MSB, CPU_REG_PC, CPU_REG_PBR, CPU_REG_ADDR),
            IO,
            ADDR_OFFR   (CPU_REG_X, ADDR_OFF_BANK_WRAP),
            MOV_L       (MOV_LSB, CPU_REG_ADDR, CPU_REG_PBR, CPU_REG_TEMP),
            ADDR_OFFI   (1, ADDR_OFF_BANK_WRAP),
            MOV_L       (MOV_MSB, CPU_REG_ADDR, CPU_REG_PBR, CPU_REG_TEMP),
            MOV_RRW     (CPU_REG_TEMP, CPU_REG_PC, MOV_RRW_WORD),
        }
    },

    /**************************************************************************************/
    /*                                      LDA                                           */
    /**************************************************************************************/


    [CPU_OPCODE_LDA_ABS] = {
        .uops = {
            MOV_LPC     (MOV_LSB, CPU_REG_ADDR),
            MOV_LPC     (MOV_MSB, CPU_REG_ADDR),
            MOV_RRW     (CPU_REG_DBR, CPU_REG_BANK, MOV_RRW_BYTE),
            MOV_L       (MOV_LSB, CPU_REG_ADDR, CPU_REG_BANK, CPU_REG_TEMP),
            SKIPS       (2, CPU_STATUS_FLAG_M),
            ADDR_OFFI   (1, ADDR_OFF_BANK_NEXT),
            MOV_L       (MOV_MSB, CPU_REG_ADDR, CPU_REG_BANK, CPU_REG_TEMP),
            MOV_RR      (CPU_REG_TEMP, CPU_REG_ACCUM),
            CHK_ZN      (CPU_REG_ACCUM),
        }
    },

    [CPU_OPCODE_LDA_ABS_X] = {
        .uops = {
            MOV_LPC     (MOV_LSB, CPU_REG_ADDR),
            MOV_LPC     (MOV_MSB, CPU_REG_ADDR),
            MOV_RRW     (CPU_REG_DBR, CPU_REG_BANK, MOV_RRW_BYTE),
            ADDR_OFFR   (CPU_REG_X, ADDR_OFF_BANK_NEXT),
            SKIPC       (2, CPU_STATUS_FLAG_PAGE),
            SKIPS       (1, CPU_STATUS_FLAG_X),
            IO          /* X=0 or page boundary */,
            MOV_L       (MOV_LSB, CPU_REG_ADDR, CPU_REG_BANK, CPU_REG_TEMP),
            SKIPS       (2, CPU_STATUS_FLAG_M),
            ADDR_OFFI   (1, ADDR_OFF_BANK_NEXT),
            MOV_L       (MOV_MSB, CPU_REG_ADDR, CPU_REG_BANK, CPU_REG_TEMP),
            MOV_RR      (CPU_REG_TEMP, CPU_REG_ACCUM),
            CHK_ZN      (CPU_REG_ACCUM),
        }
    },
    [CPU_OPCODE_LDA_ABS_Y] = {
        .uops = {
            MOV_LPC     (MOV_LSB, CPU_REG_ADDR),
            MOV_LPC     (MOV_MSB, CPU_REG_ADDR),
            MOV_RRW     (CPU_REG_DBR, CPU_REG_BANK, MOV_RRW_BYTE),
            ADDR_OFFR   (CPU_REG_Y, ADDR_OFF_BANK_NEXT),
            SKIPC       (2, CPU_STATUS_FLAG_PAGE),
            SKIPS       (1, CPU_STATUS_FLAG_X),
            IO          /* X=0 or page boundary */,
            MOV_L       (MOV_LSB, CPU_REG_ADDR, CPU_REG_BANK, CPU_REG_TEMP),
            SKIPS       (2, CPU_STATUS_FLAG_M),
            ADDR_OFFI   (1, ADDR_OFF_BANK_NEXT),
            MOV_L       (MOV_MSB, CPU_REG_ADDR, CPU_REG_BANK, CPU_REG_TEMP),
            MOV_RR      (CPU_REG_TEMP, CPU_REG_ACCUM),
            CHK_ZN      (CPU_REG_ACCUM),
        }
    },

    [CPU_OPCODE_LDA_ABSL] = {
        .uops = {
            MOV_LPC     (MOV_LSB, CPU_REG_ADDR),
            MOV_LPC     (MOV_MSB, CPU_REG_ADDR),
            MOV_LPC     (MOV_LSB, CPU_REG_BANK),
            MOV_L       (MOV_LSB, CPU_REG_ADDR, CPU_REG_BANK, CPU_REG_TEMP),
            SKIPS       (2, CPU_STATUS_FLAG_M),
            ADDR_OFFI   (1, ADDR_OFF_BANK_NEXT),
            MOV_L       (MOV_MSB, CPU_REG_ADDR, CPU_REG_BANK, CPU_REG_TEMP),
            MOV_RR      (CPU_REG_TEMP, CPU_REG_ACCUM),
            CHK_ZN      (CPU_REG_ACCUM),
        }
    },

    [CPU_OPCODE_LDA_ABSL_X] = {
        .uops = {
            MOV_LPC     (MOV_LSB, CPU_REG_ADDR),
            MOV_LPC     (MOV_MSB, CPU_REG_ADDR),
            MOV_LPC     (MOV_LSB, CPU_REG_BANK),
            ADDR_OFFR   (CPU_REG_X, ADDR_OFF_BANK_NEXT),
            MOV_L       (MOV_LSB, CPU_REG_ADDR, CPU_REG_BANK, CPU_REG_TEMP),
            SKIPS       (2, CPU_STATUS_FLAG_M),
            ADDR_OFFI   (1, ADDR_OFF_BANK_NEXT),
            MOV_L       (MOV_MSB, CPU_REG_ADDR, CPU_REG_BANK, CPU_REG_TEMP),
            MOV_RR      (CPU_REG_TEMP, CPU_REG_ACCUM),
            CHK_ZN      (CPU_REG_ACCUM),
        }
    },

    [CPU_OPCODE_LDA_DIR] = {
        .uops = {
            MOV_LPC     (MOV_LSB, CPU_REG_ADDR),
            ZEXT        (CPU_REG_ADDR),
            ADDR_OFFR   (CPU_REG_D, ADDR_OFF_BANK_WRAP),
            SKIPC       (1, CPU_STATUS_FLAG_DL),
            IO          /* DL != 0 */,
            MOV_RRW     (CPU_REG_ZERO, CPU_REG_BANK, MOV_RRW_BYTE),
            MOV_L       (MOV_LSB, CPU_REG_ADDR, CPU_REG_BANK, CPU_REG_TEMP),
            SKIPS       (2, CPU_STATUS_FLAG_M),
            ADDR_OFFI   (1, ADDR_OFF_BANK_NEXT),
            MOV_L       (MOV_MSB, CPU_REG_ADDR, CPU_REG_BANK, CPU_REG_TEMP),
            MOV_RR      (CPU_REG_TEMP, CPU_REG_ACCUM),
            CHK_ZN      (CPU_REG_ACCUM),
        }
    },

    [CPU_OPCODE_LDA_S_REL] = {
        .uops = {
            MOV_LPC     (MOV_LSB, CPU_REG_ADDR),
            ZEXT        (CPU_REG_ADDR),
            ADDR_OFFR   (CPU_REG_S, ADDR_OFF_BANK_WRAP),
            IO,
            MOV_L       (MOV_LSB, CPU_REG_ADDR, CPU_REG_ZERO, CPU_REG_TEMP),
            SKIPS       (2, CPU_STATUS_FLAG_M),
            ADDR_OFFI   (1, ADDR_OFF_BANK_WRAP),
            MOV_L       (MOV_MSB, CPU_REG_ADDR, CPU_REG_ZERO, CPU_REG_TEMP),
            MOV_RR      (CPU_REG_TEMP, CPU_REG_ACCUM),
            CHK_ZN      (CPU_REG_ACCUM),
        },
    },

    [CPU_OPCODE_LDA_DIR_X] = {
        .uops = {
            MOV_LPC     (MOV_LSB, CPU_REG_ADDR),
            ZEXT        (CPU_REG_ADDR),
            SKIPC       (1, CPU_STATUS_FLAG_DL),
            IO          /* DL != 0 */,
            ADDR_OFFR   (CPU_REG_D, ADDR_OFF_BANK_WRAP),
            ADDR_OFFR   (CPU_REG_X, ADDR_OFF_BANK_WRAP),
            IO,
            MOV_L       (MOV_LSB, CPU_REG_ADDR, CPU_REG_ZERO, CPU_REG_TEMP),
            SKIPS       (2, CPU_STATUS_FLAG_M),
            ADDR_OFFI   (1, ADDR_OFF_BANK_WRAP),
            MOV_L       (MOV_MSB, CPU_REG_ADDR, CPU_REG_ZERO, CPU_REG_TEMP),
            MOV_RR      (CPU_REG_TEMP, CPU_REG_ACCUM),
            CHK_ZN      (CPU_REG_ACCUM),
        }
    },

    [CPU_OPCODE_LDA_DIR_IND] = {
        .uops = {
            MOV_LPC     (MOV_LSB, CPU_REG_ADDR),
            ZEXT        (CPU_REG_ADDR),
            SKIPC       (1, CPU_STATUS_FLAG_DL),
            IO          /* DL != 0 */,
            ADDR_OFFR   (CPU_REG_D, ADDR_OFF_BANK_WRAP),
            MOV_RRW     (CPU_REG_ZERO, CPU_REG_BANK, MOV_RRW_BYTE),
            MOV_L       (MOV_LSB, CPU_REG_ADDR, CPU_REG_BANK, CPU_REG_TEMP),
            ADDR_OFFI   (1, ADDR_OFF_BANK_NEXT),
            MOV_L       (MOV_MSB, CPU_REG_ADDR, CPU_REG_BANK, CPU_REG_TEMP),
            MOV_RRW     (CPU_REG_TEMP, CPU_REG_ADDR, MOV_RRW_WORD),
            MOV_RRW     (CPU_REG_DBR, CPU_REG_BANK, MOV_RRW_BYTE),
            MOV_L       (MOV_LSB, CPU_REG_ADDR, CPU_REG_BANK, CPU_REG_TEMP),
            SKIPS       (2, CPU_STATUS_FLAG_M),
            ADDR_OFFI   (1, ADDR_OFF_BANK_NEXT),
            MOV_L       (MOV_MSB, CPU_REG_ADDR, CPU_REG_BANK, CPU_REG_TEMP),
            MOV_RR      (CPU_REG_TEMP, CPU_REG_ACCUM),
            CHK_ZN      (CPU_REG_ACCUM),
        }
    },

    [CPU_OPCODE_LDA_DIR_INDL] = {
        .uops = {
            MOV_LPC     (MOV_LSB, CPU_REG_ADDR),
            ZEXT        (CPU_REG_ADDR),
            SKIPC       (1, CPU_STATUS_FLAG_DL),
            IO          /* DL != 0 */,
            ADDR_OFFR   (CPU_REG_D, ADDR_OFF_BANK_WRAP),
            MOV_RRW     (CPU_REG_ZERO, CPU_REG_BANK, MOV_RRW_BYTE),
            MOV_L       (MOV_LSB, CPU_REG_ADDR, CPU_REG_BANK, CPU_REG_TEMP),
            ADDR_OFFI   (1, ADDR_OFF_BANK_NEXT),
            MOV_L       (MOV_MSB, CPU_REG_ADDR, CPU_REG_BANK, CPU_REG_TEMP),
            ADDR_OFFI   (1, ADDR_OFF_BANK_NEXT),
            MOV_L       (MOV_LSB, CPU_REG_ADDR, CPU_REG_BANK, CPU_REG_BANK),
            MOV_RRW     (CPU_REG_TEMP, CPU_REG_ADDR, MOV_RRW_WORD),
            MOV_L       (MOV_LSB, CPU_REG_ADDR, CPU_REG_BANK, CPU_REG_TEMP),
            SKIPS       (2, CPU_STATUS_FLAG_M),
            ADDR_OFFI   (1, ADDR_OFF_BANK_NEXT),
            MOV_L       (MOV_MSB, CPU_REG_ADDR, CPU_REG_BANK, CPU_REG_TEMP),
            MOV_RR      (CPU_REG_TEMP, CPU_REG_ACCUM),
            CHK_ZN      (CPU_REG_ACCUM),
        }
    },

    [CPU_OPCODE_LDA_S_REL_IND_Y] = {
        .uops = {
            MOV_LPC     (MOV_LSB, CPU_REG_ADDR),
            ZEXT        (CPU_REG_ADDR),
            IO,
            ADDR_OFFR   (CPU_REG_S, ADDR_OFF_BANK_WRAP),
            MOV_L       (MOV_LSB, CPU_REG_ADDR, CPU_REG_ZERO, CPU_REG_TEMP),
            ADDR_OFFI   (1, ADDR_OFF_BANK_WRAP),
            MOV_L       (MOV_MSB, CPU_REG_ADDR, CPU_REG_ZERO, CPU_REG_TEMP),
            MOV_RRW     (CPU_REG_TEMP, CPU_REG_ADDR, MOV_RRW_WORD),
            MOV_RRW     (CPU_REG_DBR, CPU_REG_BANK, MOV_RRW_BYTE),
            IO,
            ADDR_OFFR   (CPU_REG_Y, ADDR_OFF_BANK_NEXT),
            MOV_L       (MOV_LSB, CPU_REG_ADDR, CPU_REG_BANK, CPU_REG_TEMP),
            SKIPS       (2, CPU_STATUS_FLAG_M),
            ADDR_OFFI   (1, ADDR_OFF_BANK_NEXT),
            MOV_L       (MOV_MSB, CPU_REG_ADDR, CPU_REG_BANK, CPU_REG_TEMP),
            MOV_RR      (CPU_REG_TEMP, CPU_REG_ACCUM),
            CHK_ZN      (CPU_REG_ACCUM),

        }
    },

    [CPU_OPCODE_LDA_DIR_X_IND] = {
        .uops = {
            MOV_LPC     (MOV_LSB, CPU_REG_ADDR),
            ZEXT        (CPU_REG_ADDR),
            SKIPC       (1, CPU_STATUS_FLAG_DL),
            IO          /* DL != 0 */,
            ADDR_OFFR   (CPU_REG_D, ADDR_OFF_BANK_WRAP),
            ADDR_OFFR   (CPU_REG_X, ADDR_OFF_BANK_WRAP),
            IO,
            MOV_L       (MOV_LSB, CPU_REG_ADDR, CPU_REG_ZERO, CPU_REG_TEMP),
            ADDR_OFFI   (1, ADDR_OFF_BANK_WRAP),
            MOV_L       (MOV_MSB, CPU_REG_ADDR, CPU_REG_ZERO, CPU_REG_TEMP),
            MOV_RRW     (CPU_REG_TEMP, CPU_REG_ADDR, MOV_RRW_WORD),
            MOV_RRW     (CPU_REG_DBR, CPU_REG_BANK, MOV_RRW_BYTE),
            MOV_L       (MOV_LSB, CPU_REG_ADDR, CPU_REG_BANK, CPU_REG_TEMP),
            SKIPS       (2, CPU_STATUS_FLAG_M),
            ADDR_OFFI   (1, ADDR_OFF_BANK_NEXT),
            MOV_L       (MOV_MSB, CPU_REG_ADDR, CPU_REG_BANK, CPU_REG_TEMP),
            MOV_RR      (CPU_REG_TEMP, CPU_REG_ACCUM),
            CHK_ZN      (CPU_REG_ACCUM),
        }
    },

    [CPU_OPCODE_LDA_DIR_IND_Y] = {
        .uops = {
            MOV_LPC     (MOV_LSB, CPU_REG_ADDR),
            ZEXT        (CPU_REG_ADDR),
            SKIPC       (1, CPU_STATUS_FLAG_DL),
            IO          /* DL != 0 */,
            ADDR_OFFR   (CPU_REG_D, 0),
            MOV_RRW     (CPU_REG_ZERO, CPU_REG_BANK, MOV_RRW_BYTE),
            MOV_L       (MOV_LSB, CPU_REG_ADDR, CPU_REG_BANK, CPU_REG_TEMP),
            ADDR_OFFI   (1, ADDR_OFF_BANK_NEXT),
            MOV_L       (MOV_MSB, CPU_REG_ADDR, CPU_REG_BANK, CPU_REG_TEMP),
            MOV_RRW     (CPU_REG_TEMP, CPU_REG_ADDR, MOV_RRW_WORD),
            MOV_RRW     (CPU_REG_DBR, CPU_REG_BANK, MOV_RRW_BYTE),
            ADDR_OFFR   (CPU_REG_Y, ADDR_OFF_BANK_NEXT),
            SKIPS       (2, CPU_STATUS_FLAG_X),
            SKIPC       (1, CPU_STATUS_FLAG_PAGE),
            IO          /* X = 0 or page crossed */,
            MOV_L       (MOV_LSB, CPU_REG_ADDR, CPU_REG_BANK, CPU_REG_TEMP),
            SKIPS       (2, CPU_STATUS_FLAG_M),
            ADDR_OFFI   (1, ADDR_OFF_BANK_NEXT),
            MOV_L       (MOV_MSB, CPU_REG_ADDR, CPU_REG_BANK, CPU_REG_TEMP),
            MOV_RR      (CPU_REG_TEMP, CPU_REG_ACCUM),
            CHK_ZN      (CPU_REG_ACCUM),
        }
    },

    [CPU_OPCODE_LDA_DIR_INDL_Y] = {
        .uops = {
            MOV_LPC     (MOV_LSB, CPU_REG_ADDR),
            ZEXT        (CPU_REG_ADDR),
            ADDR_OFFR   (CPU_REG_D, 0),
            SKIPC       (1, CPU_STATUS_FLAG_DL),
            IO          /* DL != 0 */,
            MOV_RRW     (CPU_REG_ZERO, CPU_REG_BANK, MOV_RRW_BYTE),
            MOV_L       (MOV_LSB, CPU_REG_ADDR, CPU_REG_BANK, CPU_REG_TEMP),
            ADDR_OFFI   (1, ADDR_OFF_BANK_NEXT),
            MOV_L       (MOV_MSB, CPU_REG_ADDR, CPU_REG_BANK, CPU_REG_TEMP),
            ADDR_OFFI   (1, ADDR_OFF_BANK_NEXT),
            MOV_L       (MOV_LSB, CPU_REG_ADDR, CPU_REG_BANK, CPU_REG_BANK),
            MOV_RRW     (CPU_REG_TEMP, CPU_REG_ADDR, MOV_RRW_WORD),
            // MOV_RRW     (CPU_REG_DBR, CPU_REG_BANK, MOV_RRW_BYTE),
            ADDR_OFFR   (CPU_REG_Y, ADDR_OFF_BANK_NEXT),
            MOV_L       (MOV_LSB, CPU_REG_ADDR, CPU_REG_BANK, CPU_REG_TEMP),
            SKIPS       (2, CPU_STATUS_FLAG_M),
            ADDR_OFFI   (1, ADDR_OFF_BANK_NEXT),
            MOV_L       (MOV_MSB, CPU_REG_ADDR, CPU_REG_BANK, CPU_REG_TEMP),
            MOV_RR      (CPU_REG_TEMP, CPU_REG_ACCUM),
            CHK_ZN      (CPU_REG_ACCUM),
        }
    },

    [CPU_OPCODE_LDA_IMM] = {
        .uops = {
            MOV_LPC     (MOV_LSB, CPU_REG_TEMP),
            SKIPS       (1, CPU_STATUS_FLAG_M),
            MOV_LPC     (MOV_MSB, CPU_REG_TEMP),
            MOV_RR      (CPU_REG_TEMP, CPU_REG_ACCUM),
            CHK_ZN      (CPU_REG_ACCUM)
        }
    },

    /**************************************************************************************/
    /*                                      LDX                                           */
    /**************************************************************************************/

    [CPU_OPCODE_LDX_ABS] = {
        .uops = {
            MOV_LPC     (MOV_LSB, CPU_REG_ADDR),
            MOV_LPC     (MOV_MSB, CPU_REG_ADDR),
            MOV_RRW     (CPU_REG_DBR, CPU_REG_BANK, MOV_RRW_BYTE),
            MOV_L       (MOV_LSB, CPU_REG_ADDR, CPU_REG_BANK, CPU_REG_TEMP),
            SKIPS       (2, CPU_STATUS_FLAG_X),
            ADDR_OFFI   (1, ADDR_OFF_BANK_NEXT),
            MOV_L       (MOV_MSB, CPU_REG_ADDR, CPU_REG_BANK, CPU_REG_TEMP),
            MOV_RR      (CPU_REG_TEMP, CPU_REG_X),
            CHK_ZN      (CPU_REG_X),
        }
    },

    [CPU_OPCODE_LDX_ABS_Y] = {
        .uops = {
            MOV_LPC     (MOV_LSB, CPU_REG_ADDR),
            MOV_LPC     (MOV_MSB, CPU_REG_ADDR),
            MOV_RRW     (CPU_REG_DBR, CPU_REG_BANK, MOV_RRW_BYTE),
            ADDR_OFFR   (CPU_REG_Y, ADDR_OFF_BANK_NEXT),
            SKIPC       (2, CPU_STATUS_FLAG_PAGE),
            SKIPS       (1, CPU_STATUS_FLAG_X),
            IO          /* X=0 or page boundary */,
            MOV_L       (MOV_LSB, CPU_REG_ADDR, CPU_REG_BANK, CPU_REG_TEMP),
            SKIPS       (2, CPU_STATUS_FLAG_X),
            ADDR_OFFI   (1, ADDR_OFF_BANK_NEXT),
            MOV_L       (MOV_MSB, CPU_REG_ADDR, CPU_REG_BANK, CPU_REG_TEMP),
            MOV_RR      (CPU_REG_TEMP, CPU_REG_X),
            CHK_ZN      (CPU_REG_X),
        }
    },

    [CPU_OPCODE_LDX_DIR] = {
        .uops = {
            MOV_LPC     (MOV_LSB, CPU_REG_ADDR),
            ZEXT        (CPU_REG_ADDR),
            ADDR_OFFR   (CPU_REG_D, ADDR_OFF_BANK_WRAP),
            SKIPC       (1, CPU_STATUS_FLAG_DL),
            IO          /* DL != 0 */,
            MOV_RRW     (CPU_REG_ZERO, CPU_REG_BANK, MOV_RRW_BYTE),
            MOV_L       (MOV_LSB, CPU_REG_ADDR, CPU_REG_BANK, CPU_REG_TEMP),
            SKIPS       (2, CPU_STATUS_FLAG_X),
            ADDR_OFFI   (1, ADDR_OFF_BANK_NEXT),
            MOV_L       (MOV_MSB, CPU_REG_ADDR, CPU_REG_BANK, CPU_REG_TEMP),
            MOV_RR      (CPU_REG_TEMP, CPU_REG_X),
            CHK_ZN      (CPU_REG_X),
        }
    },

    [CPU_OPCODE_LDX_DIR_Y] = {
        .uops = {
            MOV_LPC     (MOV_LSB, CPU_REG_ADDR),
            ZEXT        (CPU_REG_ADDR),
            SKIPC       (1, CPU_STATUS_FLAG_DL),
            IO          /* DL != 0 */,
            ADDR_OFFR   (CPU_REG_D, ADDR_OFF_BANK_WRAP),
            ADDR_OFFR   (CPU_REG_Y, ADDR_OFF_BANK_WRAP),
            IO,
            MOV_L       (MOV_LSB, CPU_REG_ADDR, CPU_REG_ZERO, CPU_REG_TEMP),
            SKIPS       (2, CPU_STATUS_FLAG_X),
            ADDR_OFFI   (1, ADDR_OFF_BANK_WRAP),
            MOV_L       (MOV_MSB, CPU_REG_ADDR, CPU_REG_ZERO, CPU_REG_TEMP),
            MOV_RR      (CPU_REG_TEMP, CPU_REG_X),
            CHK_ZN      (CPU_REG_X),
        }
    },

    [CPU_OPCODE_LDX_IMM] = {
        .uops = {
            MOV_LPC     (MOV_LSB, CPU_REG_TEMP),
            SKIPS       (1, CPU_STATUS_FLAG_X),
            MOV_LPC     (MOV_MSB, CPU_REG_TEMP),
            MOV_RR      (CPU_REG_TEMP, CPU_REG_X),
            CHK_ZN      (CPU_REG_X)
        }
    },

    /**************************************************************************************/
    /*                                      LDY                                           */
    /**************************************************************************************/

    [CPU_OPCODE_LDY_ABS] = {
        .uops = {
            MOV_LPC     (MOV_LSB, CPU_REG_ADDR),
            MOV_LPC     (MOV_MSB, CPU_REG_ADDR),
            MOV_RRW     (CPU_REG_DBR, CPU_REG_BANK, MOV_RRW_BYTE),
            MOV_L       (MOV_LSB, CPU_REG_ADDR, CPU_REG_BANK, CPU_REG_TEMP),
            SKIPS       (2, CPU_STATUS_FLAG_X),
            ADDR_OFFI   (1, ADDR_OFF_BANK_NEXT),
            MOV_L       (MOV_MSB, CPU_REG_ADDR, CPU_REG_BANK, CPU_REG_TEMP),
            MOV_RR      (CPU_REG_TEMP, CPU_REG_Y),
            CHK_ZN      (CPU_REG_Y),
        }
    },

    [CPU_OPCODE_LDY_ABS_X] = {
        .uops = {
            MOV_LPC     (MOV_LSB, CPU_REG_ADDR),
            MOV_LPC     (MOV_MSB, CPU_REG_ADDR),
            MOV_RRW     (CPU_REG_DBR, CPU_REG_BANK, MOV_RRW_BYTE),
            ADDR_OFFR   (CPU_REG_X, ADDR_OFF_BANK_NEXT),
            SKIPC       (2, CPU_STATUS_FLAG_PAGE),
            SKIPS       (1, CPU_STATUS_FLAG_X),
            IO          /* X=0 or page boundary */,
            MOV_L       (MOV_LSB, CPU_REG_ADDR, CPU_REG_BANK, CPU_REG_TEMP),
            SKIPS       (2, CPU_STATUS_FLAG_X),
            ADDR_OFFI   (1, ADDR_OFF_BANK_NEXT),
            MOV_L       (MOV_MSB, CPU_REG_ADDR, CPU_REG_BANK, CPU_REG_TEMP),
            MOV_RR      (CPU_REG_TEMP, CPU_REG_Y),
            CHK_ZN      (CPU_REG_Y),
        }
    },

    [CPU_OPCODE_LDY_DIR] = {
        .uops = {
            MOV_LPC     (MOV_LSB, CPU_REG_ADDR),
            ZEXT        (CPU_REG_ADDR),
            ADDR_OFFR   (CPU_REG_D, ADDR_OFF_BANK_WRAP),
            SKIPC       (1, CPU_STATUS_FLAG_DL),
            IO          /* DL != 0 */,
            MOV_RRW     (CPU_REG_ZERO, CPU_REG_BANK, MOV_RRW_BYTE),
            MOV_L       (MOV_LSB, CPU_REG_ADDR, CPU_REG_BANK, CPU_REG_TEMP),
            SKIPS       (2, CPU_STATUS_FLAG_X),
            ADDR_OFFI   (1, ADDR_OFF_BANK_NEXT),
            MOV_L       (MOV_MSB, CPU_REG_ADDR, CPU_REG_BANK, CPU_REG_TEMP),
            MOV_RR      (CPU_REG_TEMP, CPU_REG_Y),
            CHK_ZN      (CPU_REG_Y),
        }
    },

    [CPU_OPCODE_LDY_DIR_X] = {
        .uops = {
            MOV_LPC     (MOV_LSB, CPU_REG_ADDR),
            ZEXT        (CPU_REG_ADDR),
            SKIPC       (1, CPU_STATUS_FLAG_DL),
            IO          /* DL != 0 */,
            ADDR_OFFR   (CPU_REG_D, ADDR_OFF_BANK_WRAP),
            ADDR_OFFR   (CPU_REG_X, ADDR_OFF_BANK_WRAP),
            IO,
            MOV_L       (MOV_LSB, CPU_REG_ADDR, CPU_REG_ZERO, CPU_REG_TEMP),
            SKIPS       (2, CPU_STATUS_FLAG_X),
            ADDR_OFFI   (1, ADDR_OFF_BANK_WRAP),
            MOV_L       (MOV_MSB, CPU_REG_ADDR, CPU_REG_ZERO, CPU_REG_TEMP),
            MOV_RR      (CPU_REG_TEMP, CPU_REG_Y),
            CHK_ZN      (CPU_REG_Y),
        }
    },

    [CPU_OPCODE_LDY_IMM] = {
        .uops = {
            MOV_LPC     (MOV_LSB, CPU_REG_TEMP),
            SKIPS       (1, CPU_STATUS_FLAG_X),
            MOV_LPC     (MOV_MSB, CPU_REG_TEMP),
            MOV_RR      (CPU_REG_TEMP, CPU_REG_Y),
            CHK_ZN      (CPU_REG_Y)
        }
    },

    /**************************************************************************************/
    /*                                      LSR                                           */
    /**************************************************************************************/

    [CPU_OPCODE_LSR_ABS] = {
        .uops = {
            MOV_LPC     (MOV_LSB, CPU_REG_ADDR),
            MOV_LPC     (MOV_MSB, CPU_REG_ADDR),
            MOV_RRW     (CPU_REG_DBR, CPU_REG_BANK, MOV_RRW_BYTE),
            MOV_L       (MOV_LSB, CPU_REG_ADDR, CPU_REG_BANK, CPU_REG_TEMP),
            SKIPS       (2, CPU_STATUS_FLAG_M),
            ADDR_OFFI   (1, ADDR_OFF_BANK_NEXT),
            MOV_L       (MOV_MSB, CPU_REG_ADDR, CPU_REG_BANK, CPU_REG_TEMP),
            IO,
            ALU_OP      (ALU_OP_SHR, CPU_STATUS_FLAG_M, CPU_REG_TEMP, CPU_REG_ZERO),
            // CHK_ZNW     (CPU_REG_TEMP, 0),
            SKIPS       (2, CPU_STATUS_FLAG_M),
            MOV_S       (MOV_MSB, CPU_REG_ADDR, CPU_REG_BANK, CPU_REG_TEMP),
            ADDR_OFFIS  (0xffff, ADDR_OFF_BANK_NEXT, ADDR_OFF_SIGNED),
            MOV_S       (MOV_LSB, CPU_REG_ADDR, CPU_REG_BANK, CPU_REG_TEMP),
        }
    },

    [CPU_OPCODE_LSR_ACC] = {
        .uops = {
            IO,
            ALU_OP      (ALU_OP_SHR, CPU_STATUS_FLAG_M, CPU_REG_ACCUM, CPU_REG_ZERO),
            MOV_RR      (CPU_REG_TEMP, CPU_REG_ACCUM),
            // CHK_ZN      (CPU_REG_ACCUM),
        }
    },

    [CPU_OPCODE_LSR_ABS_X] = {
        .uops = {
            MOV_LPC     (MOV_LSB, CPU_REG_ADDR),
            MOV_LPC     (MOV_MSB, CPU_REG_ADDR),
            MOV_RRW     (CPU_REG_DBR, CPU_REG_BANK, MOV_RRW_BYTE),
            IO,
            ADDR_OFFR   (CPU_REG_X, ADDR_OFF_BANK_NEXT),
            MOV_L       (MOV_LSB, CPU_REG_ADDR, CPU_REG_BANK, CPU_REG_TEMP),
            SKIPS       (2, CPU_STATUS_FLAG_M),
            ADDR_OFFI   (1, ADDR_OFF_BANK_NEXT),
            MOV_L       (MOV_MSB, CPU_REG_ADDR, CPU_REG_BANK, CPU_REG_TEMP),
            ALU_OP      (ALU_OP_SHR, CPU_STATUS_FLAG_M, CPU_REG_TEMP, CPU_REG_ZERO),
            // CHK_ZNW     (CPU_REG_TEMP, 0),
            SKIPS       (2, CPU_STATUS_FLAG_M),
            MOV_S       (MOV_MSB, CPU_REG_ADDR, CPU_REG_BANK, CPU_REG_TEMP),
            ADDR_OFFIS  (0xffff, ADDR_OFF_BANK_WRAP, ADDR_OFF_SIGNED),
            MOV_S       (MOV_LSB, CPU_REG_ADDR, CPU_REG_BANK, CPU_REG_TEMP),
        }
    },

    [CPU_OPCODE_LSR_DIR] = {
        .uops = {
            MOV_LPC     (MOV_LSB, CPU_REG_ADDR),
            ZEXT        (CPU_REG_ADDR),
            SKIPC       (1, CPU_STATUS_FLAG_DL),
            IO          /* DL != 0 */,
            ADDR_OFFR   (CPU_REG_D, ADDR_OFF_BANK_WRAP),
            MOV_RRW     (CPU_REG_ZERO, CPU_REG_BANK, MOV_RRW_BYTE),
            MOV_L       (MOV_LSB, CPU_REG_ADDR, CPU_REG_BANK, CPU_REG_TEMP),
            SKIPS       (2, CPU_STATUS_FLAG_M),
            ADDR_OFFI   (1, ADDR_OFF_BANK_NEXT),
            MOV_L       (MOV_MSB, CPU_REG_ADDR, CPU_REG_BANK, CPU_REG_TEMP),
            IO,
            ALU_OP      (ALU_OP_SHR, CPU_STATUS_FLAG_M, CPU_REG_TEMP, CPU_REG_ZERO),
            // CHK_ZNW     (CPU_REG_TEMP, 0),
            SKIPS       (2, CPU_STATUS_FLAG_M),
            MOV_S       (MOV_MSB, CPU_REG_ADDR, CPU_REG_BANK, CPU_REG_TEMP),
            ADDR_OFFIS  (0xffff, ADDR_OFF_BANK_NEXT, ADDR_OFF_SIGNED),
            MOV_S       (MOV_LSB, CPU_REG_ADDR, CPU_REG_BANK, CPU_REG_TEMP),
        }
    },

    [CPU_OPCODE_LSR_DIR_X] = {
        .uops = {
            MOV_LPC     (MOV_LSB, CPU_REG_ADDR),
            ZEXT        (CPU_REG_ADDR),
            SKIPC       (1, CPU_STATUS_FLAG_DL),
            IO          /* DL != 0 */,
            IO,
            ADDR_OFFR   (CPU_REG_D, ADDR_OFF_BANK_WRAP),
            ADDR_OFFR   (CPU_REG_X, ADDR_OFF_BANK_WRAP),
            MOV_L       (MOV_LSB, CPU_REG_ADDR, CPU_REG_ZERO, CPU_REG_TEMP),
            SKIPS       (2, CPU_STATUS_FLAG_M),
            ADDR_OFFI   (1, ADDR_OFF_BANK_WRAP),
            MOV_L       (MOV_MSB, CPU_REG_ADDR, CPU_REG_ZERO, CPU_REG_TEMP),
            IO,
            ALU_OP      (ALU_OP_SHR, CPU_STATUS_FLAG_M, CPU_REG_TEMP, CPU_REG_ZERO),
            // CHK_ZNW     (CPU_REG_TEMP, 0),
            SKIPS       (2, CPU_STATUS_FLAG_M),
            MOV_S       (MOV_MSB, CPU_REG_ADDR, CPU_REG_ZERO, CPU_REG_TEMP),
            ADDR_OFFIS  (0xffff, ADDR_OFF_BANK_WRAP, ADDR_OFF_SIGNED),
            MOV_S       (MOV_LSB, CPU_REG_ADDR, CPU_REG_ZERO, CPU_REG_TEMP),
        }
    },

    /**************************************************************************************/
    /*                                      MVN                                           */
    /**************************************************************************************/

    [CPU_OPCODE_MVN_BLK] = {
        .uops = {
            MOV_LPC     (MOV_LSB, CPU_REG_BANK),
            MOV_LPC     (MOV_LSB, CPU_REG_ADDR),
            MOV_L       (MOV_LSB, CPU_REG_X, CPU_REG_ADDR, CPU_REG_TEMP),
            MOV_S       (MOV_LSB, CPU_REG_Y, CPU_REG_BANK, CPU_REG_TEMP),
            IO,
            INC_RW      (CPU_REG_X),
            IO,
            INC_RW      (CPU_REG_Y),
            DEC_RW      (CPU_REG_ACCUM),
            SKIPS       (1, CPU_STATUS_FLAG_AM),
            /* we're not done copying stuff yet, so reset PC
            to the start of the instruction */
            INC_PC      (0xfffd)
//            DEC_RW      (CPU_REG_PC),
//            DEC_RW      (CPU_REG_PC),
//            DEC_RW      (CPU_REG_PC),
        }
    },

    /**************************************************************************************/
    /*                                      MVP                                           */
    /**************************************************************************************/
    [CPU_OPCODE_MVP_BLK] = {
        .uops = {
            MOV_LPC     (MOV_LSB, CPU_REG_BANK),
            MOV_LPC     (MOV_LSB, CPU_REG_ADDR),
            MOV_L       (MOV_LSB, CPU_REG_X, CPU_REG_ADDR, CPU_REG_TEMP),
            MOV_S       (MOV_LSB, CPU_REG_Y, CPU_REG_BANK, CPU_REG_TEMP),
            IO,
            DEC_RW      (CPU_REG_X),
            IO,
            DEC_RW      (CPU_REG_Y),
            DEC_RW      (CPU_REG_ACCUM),
            SKIPS       (1, CPU_STATUS_FLAG_AM),
            /* we're not done copying stuff yet, so reset PC
            to the start of the instruction */
            INC_PC      (0xfffd)
//            DEC_RW      (CPU_REG_PC),
//            DEC_RW      (CPU_REG_PC),
//            DEC_RW      (CPU_REG_PC),
        }
    },
    /**************************************************************************************/
    /*                                      NOP                                           */
    /**************************************************************************************/

    [CPU_OPCODE_NOP_IMP] = {
        .uops = {
            IO
        }
    },

    /**************************************************************************************/
    /*                                      ORA                                           */
    /**************************************************************************************/

    [CPU_OPCODE_ORA_ABS] = {
        .uops = {
            MOV_LPC     (MOV_LSB, CPU_REG_ADDR),
            MOV_LPC     (MOV_MSB, CPU_REG_ADDR),
            MOV_RRW     (CPU_REG_DBR, CPU_REG_BANK, MOV_RRW_BYTE),
            MOV_L       (MOV_LSB, CPU_REG_ADDR, CPU_REG_BANK, CPU_REG_TEMP),
            SKIPS       (2, CPU_STATUS_FLAG_M),
            ADDR_OFFI   (1, ADDR_OFF_BANK_NEXT),
            MOV_L       (MOV_MSB, CPU_REG_ADDR, CPU_REG_BANK, CPU_REG_TEMP),
            ALU_OP      (ALU_OP_OR, CPU_STATUS_FLAG_M, CPU_REG_ACCUM, CPU_REG_TEMP),
            MOV_RR      (CPU_REG_TEMP, CPU_REG_ACCUM),
            // CHK_ZN      (CPU_REG_ACCUM),
        }
    },
    [CPU_OPCODE_ORA_ABS_X] = {
        .uops = {
            MOV_LPC     (MOV_LSB, CPU_REG_ADDR),
            MOV_LPC     (MOV_MSB, CPU_REG_ADDR),
            MOV_RRW     (CPU_REG_DBR, CPU_REG_BANK, MOV_RRW_BYTE),
            ADDR_OFFR   (CPU_REG_X, ADDR_OFF_BANK_NEXT),
            SKIPC       (2, CPU_STATUS_FLAG_PAGE),
            SKIPS       (1, CPU_STATUS_FLAG_X),
            IO          /* X=0 or page boundary */,
            MOV_L       (MOV_LSB, CPU_REG_ADDR, CPU_REG_BANK, CPU_REG_TEMP),
            SKIPS       (2, CPU_STATUS_FLAG_M),
            ADDR_OFFI   (1, ADDR_OFF_BANK_NEXT),
            MOV_L       (MOV_MSB, CPU_REG_ADDR, CPU_REG_BANK, CPU_REG_TEMP),
            ALU_OP      (ALU_OP_OR, CPU_STATUS_FLAG_M, CPU_REG_ACCUM, CPU_REG_TEMP),
            MOV_RR      (CPU_REG_TEMP, CPU_REG_ACCUM),
            // CHK_ZN      (CPU_REG_ACCUM),
        }
    },
    [CPU_OPCODE_ORA_ABS_Y] = {
        .uops = {
            MOV_LPC     (MOV_LSB, CPU_REG_ADDR),
            MOV_LPC     (MOV_MSB, CPU_REG_ADDR),
            MOV_RRW     (CPU_REG_DBR, CPU_REG_BANK, MOV_RRW_BYTE),
            ADDR_OFFR   (CPU_REG_Y, ADDR_OFF_BANK_NEXT),
            SKIPC       (2, CPU_STATUS_FLAG_PAGE),
            SKIPS       (1, CPU_STATUS_FLAG_X),
            IO          /* X=0 or page boundary */,
            MOV_L       (MOV_LSB, CPU_REG_ADDR, CPU_REG_BANK, CPU_REG_TEMP),
            SKIPS       (2, CPU_STATUS_FLAG_M),
            ADDR_OFFI   (1, ADDR_OFF_BANK_NEXT),
            MOV_L       (MOV_MSB, CPU_REG_ADDR, CPU_REG_BANK, CPU_REG_TEMP),
            ALU_OP      (ALU_OP_OR, CPU_STATUS_FLAG_M, CPU_REG_ACCUM, CPU_REG_TEMP),
            MOV_RR      (CPU_REG_TEMP, CPU_REG_ACCUM),
            // CHK_ZN      (CPU_REG_ACCUM),
        }
    },
    [CPU_OPCODE_ORA_ABSL] = {
        .uops = {
            MOV_LPC     (MOV_LSB, CPU_REG_ADDR),
            MOV_LPC     (MOV_MSB, CPU_REG_ADDR),
            MOV_LPC     (MOV_LSB, CPU_REG_BANK),
            MOV_L       (MOV_LSB, CPU_REG_ADDR, CPU_REG_BANK, CPU_REG_TEMP),
            SKIPS       (2, CPU_STATUS_FLAG_M),
            ADDR_OFFI   (1, ADDR_OFF_BANK_NEXT),
            MOV_L       (MOV_MSB, CPU_REG_ADDR, CPU_REG_BANK, CPU_REG_TEMP),
            ALU_OP      (ALU_OP_OR, CPU_STATUS_FLAG_M, CPU_REG_ACCUM, CPU_REG_TEMP),
            MOV_RR      (CPU_REG_TEMP, CPU_REG_ACCUM),
            // CHK_ZN      (CPU_REG_ACCUM),
        }
    },
    [CPU_OPCODE_ORA_ABSL_X] = {
        .uops = {
            MOV_LPC     (MOV_LSB, CPU_REG_ADDR),
            MOV_LPC     (MOV_MSB, CPU_REG_ADDR),
            MOV_LPC     (MOV_LSB, CPU_REG_BANK),
            ADDR_OFFR   (CPU_REG_X, ADDR_OFF_BANK_NEXT),
            MOV_L       (MOV_LSB, CPU_REG_ADDR, CPU_REG_BANK, CPU_REG_TEMP),
            SKIPS       (2, CPU_STATUS_FLAG_M),
            ADDR_OFFI   (1, ADDR_OFF_BANK_NEXT),
            MOV_L       (MOV_MSB, CPU_REG_ADDR, CPU_REG_BANK, CPU_REG_TEMP),
            ALU_OP      (ALU_OP_OR, CPU_STATUS_FLAG_M, CPU_REG_ACCUM, CPU_REG_TEMP),
            MOV_RR      (CPU_REG_TEMP, CPU_REG_ACCUM),
            // CHK_ZN      (CPU_REG_ACCUM),
        }
    },

    [CPU_OPCODE_ORA_DIR] = {
        .uops = {
            MOV_LPC     (MOV_LSB, CPU_REG_ADDR),
            ZEXT        (CPU_REG_ADDR),
            ADDR_OFFR   (CPU_REG_D, ADDR_OFF_BANK_WRAP),
            SKIPC       (1, CPU_STATUS_FLAG_DL),
            IO          /* DL != 0 */,
            MOV_RRW     (CPU_REG_ZERO, CPU_REG_BANK, MOV_RRW_BYTE),
            MOV_L       (MOV_LSB, CPU_REG_ADDR, CPU_REG_BANK, CPU_REG_TEMP),
            SKIPS       (2, CPU_STATUS_FLAG_M),
            ADDR_OFFI   (1, ADDR_OFF_BANK_NEXT),
            MOV_L       (MOV_MSB, CPU_REG_ADDR, CPU_REG_BANK, CPU_REG_TEMP),
            ALU_OP      (ALU_OP_OR, CPU_STATUS_FLAG_M, CPU_REG_ACCUM, CPU_REG_TEMP),
            MOV_RR      (CPU_REG_TEMP, CPU_REG_ACCUM),
            // CHK_ZN      (CPU_REG_ACCUM),
        }
    },
    [CPU_OPCODE_ORA_S_REL] = {
        .uops = {
            MOV_LPC     (MOV_LSB, CPU_REG_ADDR),
            ZEXT        (CPU_REG_ADDR),
            ADDR_OFFR   (CPU_REG_S, ADDR_OFF_BANK_WRAP),
            IO,
            MOV_L       (MOV_LSB, CPU_REG_ADDR, CPU_REG_ZERO, CPU_REG_TEMP),
            SKIPS       (2, CPU_STATUS_FLAG_M),
            ADDR_OFFI   (1, ADDR_OFF_BANK_WRAP),
            MOV_L       (MOV_MSB, CPU_REG_ADDR, CPU_REG_ZERO, CPU_REG_TEMP),
            ALU_OP      (ALU_OP_OR, CPU_STATUS_FLAG_M, CPU_REG_ACCUM, CPU_REG_TEMP),
            MOV_RR      (CPU_REG_TEMP, CPU_REG_ACCUM),
            // CHK_ZN      (CPU_REG_ACCUM),
        },
    },
    [CPU_OPCODE_ORA_DIR_X] = {
        .uops = {
            MOV_LPC     (MOV_LSB, CPU_REG_ADDR),
            ZEXT        (CPU_REG_ADDR),
            SKIPC       (1, CPU_STATUS_FLAG_DL),
            IO          /* DL != 0 */,
            ADDR_OFFR   (CPU_REG_D, ADDR_OFF_BANK_WRAP),
            ADDR_OFFR   (CPU_REG_X, ADDR_OFF_BANK_WRAP),
            IO,
            MOV_L       (MOV_LSB, CPU_REG_ADDR, CPU_REG_ZERO, CPU_REG_TEMP),
            SKIPS       (2, CPU_STATUS_FLAG_M),
            ADDR_OFFI   (1, ADDR_OFF_BANK_WRAP),
            MOV_L       (MOV_MSB, CPU_REG_ADDR, CPU_REG_ZERO, CPU_REG_TEMP),
            ALU_OP      (ALU_OP_OR, CPU_STATUS_FLAG_M, CPU_REG_ACCUM, CPU_REG_TEMP),
            MOV_RR      (CPU_REG_TEMP, CPU_REG_ACCUM),
            // CHK_ZN      (CPU_REG_ACCUM),
        }
    },

    [CPU_OPCODE_ORA_DIR_IND] = {
        .uops = {
            MOV_LPC     (MOV_LSB, CPU_REG_ADDR),
            ZEXT        (CPU_REG_ADDR),
            SKIPC       (1, CPU_STATUS_FLAG_DL),
            IO          /* DL != 0 */,
            ADDR_OFFR   (CPU_REG_D, ADDR_OFF_BANK_WRAP),
            MOV_RRW     (CPU_REG_ZERO, CPU_REG_BANK, MOV_RRW_BYTE),
            MOV_L       (MOV_LSB, CPU_REG_ADDR, CPU_REG_BANK, CPU_REG_TEMP),
            ADDR_OFFI   (1, ADDR_OFF_BANK_NEXT),
            MOV_L       (MOV_MSB, CPU_REG_ADDR, CPU_REG_BANK, CPU_REG_TEMP),
            MOV_RRW     (CPU_REG_TEMP, CPU_REG_ADDR, MOV_RRW_WORD),
            MOV_RRW     (CPU_REG_DBR, CPU_REG_BANK, MOV_RRW_BYTE),
            MOV_L       (MOV_LSB, CPU_REG_ADDR, CPU_REG_BANK, CPU_REG_TEMP),
            SKIPS       (2, CPU_STATUS_FLAG_M),
            ADDR_OFFI   (1, ADDR_OFF_BANK_NEXT),
            MOV_L       (MOV_MSB, CPU_REG_ADDR, CPU_REG_BANK, CPU_REG_TEMP),
            ALU_OP      (ALU_OP_OR, CPU_STATUS_FLAG_M, CPU_REG_ACCUM, CPU_REG_TEMP),
            MOV_RR      (CPU_REG_TEMP, CPU_REG_ACCUM),
            // CHK_ZN      (CPU_REG_ACCUM),
        }
    },

    [CPU_OPCODE_ORA_DIR_INDL] = {
        .uops = {
            MOV_LPC     (MOV_LSB, CPU_REG_ADDR),
            ZEXT        (CPU_REG_ADDR),
            SKIPC       (1, CPU_STATUS_FLAG_DL),
            IO          /* DL != 0 */,
            ADDR_OFFR   (CPU_REG_D, ADDR_OFF_BANK_WRAP),
            MOV_RRW     (CPU_REG_ZERO, CPU_REG_BANK, MOV_RRW_BYTE),
            MOV_L       (MOV_LSB, CPU_REG_ADDR, CPU_REG_BANK, CPU_REG_TEMP),
            ADDR_OFFI   (1, ADDR_OFF_BANK_NEXT),
            MOV_L       (MOV_MSB, CPU_REG_ADDR, CPU_REG_BANK, CPU_REG_TEMP),
            ADDR_OFFI   (1, ADDR_OFF_BANK_NEXT),
            MOV_L       (MOV_LSB, CPU_REG_ADDR, CPU_REG_BANK, CPU_REG_BANK),
            MOV_RRW     (CPU_REG_TEMP, CPU_REG_ADDR, MOV_RRW_WORD),
            MOV_L       (MOV_LSB, CPU_REG_ADDR, CPU_REG_BANK, CPU_REG_TEMP),
            SKIPS       (2, CPU_STATUS_FLAG_M),
            ADDR_OFFI   (1, ADDR_OFF_BANK_NEXT),
            MOV_L       (MOV_MSB, CPU_REG_ADDR, CPU_REG_BANK, CPU_REG_TEMP),
            ALU_OP      (ALU_OP_OR, CPU_STATUS_FLAG_M, CPU_REG_ACCUM, CPU_REG_TEMP),
            MOV_RR      (CPU_REG_TEMP, CPU_REG_ACCUM),
            // CHK_ZN      (CPU_REG_ACCUM),
        }
    },

    [CPU_OPCODE_ORA_S_REL_IND_Y] = {
        .uops = {
            MOV_LPC     (MOV_LSB, CPU_REG_ADDR),
            ZEXT        (CPU_REG_ADDR),
            IO,
            ADDR_OFFR   (CPU_REG_S, ADDR_OFF_BANK_WRAP),
            MOV_L       (MOV_LSB, CPU_REG_ADDR, CPU_REG_ZERO, CPU_REG_TEMP),
            ADDR_OFFI   (1, ADDR_OFF_BANK_WRAP),
            MOV_L       (MOV_MSB, CPU_REG_ADDR, CPU_REG_ZERO, CPU_REG_TEMP),
            MOV_RRW     (CPU_REG_TEMP, CPU_REG_ADDR, MOV_RRW_WORD),
            MOV_RRW     (CPU_REG_DBR, CPU_REG_BANK, MOV_RRW_BYTE),
            IO,
            ADDR_OFFR   (CPU_REG_Y, ADDR_OFF_BANK_NEXT),
            MOV_L       (MOV_LSB, CPU_REG_ADDR, CPU_REG_BANK, CPU_REG_TEMP),
            SKIPS       (2, CPU_STATUS_FLAG_M),
            ADDR_OFFI   (1, ADDR_OFF_BANK_NEXT),
            MOV_L       (MOV_MSB, CPU_REG_ADDR, CPU_REG_BANK, CPU_REG_TEMP),
            ALU_OP      (ALU_OP_OR, CPU_STATUS_FLAG_M, CPU_REG_ACCUM, CPU_REG_TEMP),
            MOV_RR      (CPU_REG_TEMP, CPU_REG_ACCUM),
            // CHK_ZN      (CPU_REG_ACCUM),

        }
    },

    [CPU_OPCODE_ORA_DIR_X_IND] = {
        .uops = {
            MOV_LPC     (MOV_LSB, CPU_REG_ADDR),
            ZEXT        (CPU_REG_ADDR),
            SKIPC       (1, CPU_STATUS_FLAG_DL),
            IO          /* DL != 0 */,
            ADDR_OFFR   (CPU_REG_D, ADDR_OFF_BANK_WRAP),
            ADDR_OFFR   (CPU_REG_X, ADDR_OFF_BANK_WRAP),
            IO,
            MOV_L       (MOV_LSB, CPU_REG_ADDR, CPU_REG_ZERO, CPU_REG_TEMP),
            ADDR_OFFI   (1, ADDR_OFF_BANK_WRAP),
            MOV_L       (MOV_MSB, CPU_REG_ADDR, CPU_REG_ZERO, CPU_REG_TEMP),
            MOV_RRW     (CPU_REG_TEMP, CPU_REG_ADDR, MOV_RRW_WORD),
            MOV_RRW     (CPU_REG_DBR, CPU_REG_BANK, MOV_RRW_BYTE),
            MOV_L       (MOV_LSB, CPU_REG_ADDR, CPU_REG_BANK, CPU_REG_TEMP),
            SKIPS       (2, CPU_STATUS_FLAG_M),
            ADDR_OFFI   (1, ADDR_OFF_BANK_NEXT),
            MOV_L       (MOV_MSB, CPU_REG_ADDR, CPU_REG_BANK, CPU_REG_TEMP),
            ALU_OP      (ALU_OP_OR, CPU_STATUS_FLAG_M, CPU_REG_ACCUM, CPU_REG_TEMP),
            MOV_RR      (CPU_REG_TEMP, CPU_REG_ACCUM),
            // CHK_ZN      (CPU_REG_ACCUM),
        }
    },

    [CPU_OPCODE_ORA_DIR_IND_Y] = {
        .uops = {
            MOV_LPC     (MOV_LSB, CPU_REG_ADDR),
            ZEXT        (CPU_REG_ADDR),
            SKIPC       (1, CPU_STATUS_FLAG_DL),
            IO          /* DL != 0 */,
            ADDR_OFFR   (CPU_REG_D, ADDR_OFF_BANK_WRAP),
            MOV_RRW     (CPU_REG_ZERO, CPU_REG_BANK, MOV_RRW_BYTE),
            MOV_L       (MOV_LSB, CPU_REG_ADDR, CPU_REG_BANK, CPU_REG_TEMP),
            ADDR_OFFI   (1, ADDR_OFF_BANK_NEXT),
            MOV_L       (MOV_MSB, CPU_REG_ADDR, CPU_REG_BANK, CPU_REG_TEMP),
            MOV_RRW     (CPU_REG_TEMP, CPU_REG_ADDR, MOV_RRW_WORD),
            MOV_RRW     (CPU_REG_DBR, CPU_REG_BANK, MOV_RRW_BYTE),
            ADDR_OFFR   (CPU_REG_Y, ADDR_OFF_BANK_NEXT),
            SKIPS       (2, CPU_STATUS_FLAG_X),
            SKIPC       (1, CPU_STATUS_FLAG_PAGE),
            IO          /* X = 0 or page crossed */,
            MOV_L       (MOV_LSB, CPU_REG_ADDR, CPU_REG_BANK, CPU_REG_TEMP),
            SKIPS       (2, CPU_STATUS_FLAG_M),
            ADDR_OFFI   (1, ADDR_OFF_BANK_NEXT),
            MOV_L       (MOV_MSB, CPU_REG_ADDR, CPU_REG_BANK, CPU_REG_TEMP),
            ALU_OP      (ALU_OP_OR, CPU_STATUS_FLAG_M, CPU_REG_ACCUM, CPU_REG_TEMP),
            MOV_RR      (CPU_REG_TEMP, CPU_REG_ACCUM),
            // CHK_ZN      (CPU_REG_ACCUM),
        }
    },

    [CPU_OPCODE_ORA_DIR_INDL_Y] = {
        .uops = {
            MOV_LPC     (MOV_LSB, CPU_REG_ADDR),
            ZEXT        (CPU_REG_ADDR),
            ADDR_OFFR   (CPU_REG_D, ADDR_OFF_BANK_WRAP),
            SKIPC       (1, CPU_STATUS_FLAG_DL),
            IO          /* DL != 0 */,
            MOV_RRW     (CPU_REG_ZERO, CPU_REG_BANK, MOV_RRW_BYTE),
            MOV_L       (MOV_LSB, CPU_REG_ADDR, CPU_REG_BANK, CPU_REG_TEMP),
            ADDR_OFFI   (1, ADDR_OFF_BANK_NEXT),
            MOV_L       (MOV_MSB, CPU_REG_ADDR, CPU_REG_BANK, CPU_REG_TEMP),
            ADDR_OFFI   (1, ADDR_OFF_BANK_NEXT),
            MOV_L       (MOV_LSB, CPU_REG_ADDR, CPU_REG_BANK, CPU_REG_BANK),
            MOV_RRW     (CPU_REG_TEMP, CPU_REG_ADDR, MOV_RRW_WORD),
            // MOV_RRW     (CPU_REG_DBR, CPU_REG_BANK, MOV_RRW_BYTE),
            ADDR_OFFR   (CPU_REG_Y, ADDR_OFF_BANK_NEXT),
            MOV_L       (MOV_LSB, CPU_REG_ADDR, CPU_REG_BANK, CPU_REG_TEMP),
            SKIPS       (2, CPU_STATUS_FLAG_M),
            ADDR_OFFI   (1, ADDR_OFF_BANK_NEXT),
            MOV_L       (MOV_MSB, CPU_REG_ADDR, CPU_REG_BANK, CPU_REG_TEMP),
            ALU_OP      (ALU_OP_OR, CPU_STATUS_FLAG_M, CPU_REG_ACCUM, CPU_REG_TEMP),
            MOV_RR      (CPU_REG_TEMP, CPU_REG_ACCUM),
            // CHK_ZN      (CPU_REG_ACCUM),
        }
    },

    [CPU_OPCODE_ORA_IMM] = {
        .uops = {
            MOV_LPC     (MOV_LSB, CPU_REG_TEMP),
            SKIPS       (1, CPU_STATUS_FLAG_M),
            MOV_LPC     (MOV_MSB, CPU_REG_TEMP),
            ALU_OP      (ALU_OP_OR, CPU_STATUS_FLAG_M, CPU_REG_ACCUM, CPU_REG_TEMP),
            MOV_RR      (CPU_REG_TEMP, CPU_REG_ACCUM),
            // CHK_ZN      (CPU_REG_ACCUM),
        }
    },

    /**************************************************************************************/
    /*                                      PEA                                           */
    /**************************************************************************************/

    [CPU_OPCODE_PEA_S] = {
        .uops = {
            MOV_LPC     (MOV_LSB, CPU_REG_TEMP),
            MOV_LPC     (MOV_MSB, CPU_REG_TEMP),
            MOV_S       (MOV_MSB, CPU_REG_S, CPU_REG_ZERO, CPU_REG_TEMP),
            DECS,
            MOV_S       (MOV_LSB, CPU_REG_S, CPU_REG_ZERO, CPU_REG_TEMP),
            DECS,
        }
    },

    /**************************************************************************************/
    /*                                      PEI                                           */
    /**************************************************************************************/

    [CPU_OPCODE_PEI_S] = {
        .uops = {
            MOV_LPC     (MOV_LSB, CPU_REG_ADDR),
            ZEXT        (CPU_REG_ADDR),
            SKIPC       (1, CPU_STATUS_FLAG_DL),
            IO,
            ADDR_OFFR   (CPU_REG_D, ADDR_OFF_BANK_WRAP),
            MOV_L       (MOV_LSB, CPU_REG_ADDR, CPU_REG_ZERO, CPU_REG_TEMP),
            ADDR_OFFI   (1, ADDR_OFF_BANK_WRAP),
            MOV_L       (MOV_MSB, CPU_REG_ADDR, CPU_REG_ZERO, CPU_REG_TEMP),
            MOV_S       (MOV_MSB, CPU_REG_S, CPU_REG_ZERO, CPU_REG_TEMP),
            DECS,
            MOV_S       (MOV_LSB, CPU_REG_S, CPU_REG_ZERO, CPU_REG_TEMP),
            DECS,
        }
    },

    /**************************************************************************************/
    /*                                      PER                                           */
    /**************************************************************************************/

    [CPU_OPCODE_PER_S] = {
        .uops = {
            MOV_LPC     (MOV_LSB, CPU_REG_ADDR),
            MOV_LPC     (MOV_MSB, CPU_REG_ADDR),
            ADDR_OFFRS  (CPU_REG_PC, ADDR_OFF_BANK_WRAP, ADDR_OFF_SIGNED),
            MOV_S       (MOV_MSB, CPU_REG_S, CPU_REG_ZERO, CPU_REG_ADDR),
            DECS,
            MOV_S       (MOV_LSB, CPU_REG_S, CPU_REG_ZERO, CPU_REG_ADDR),
            DECS,
        }
    },

    /**************************************************************************************/
    /*                                      PHA                                           */
    /**************************************************************************************/

    [CPU_OPCODE_PHA_S] = {
        .uops = {
            SKIPS       (3, CPU_STATUS_FLAG_M),
            IO          /* M = 0 */,
            MOV_S       (MOV_MSB, CPU_REG_S, CPU_REG_ZERO, CPU_REG_ACCUM),
            DECS,
            MOV_S       (MOV_LSB, CPU_REG_S, CPU_REG_ZERO, CPU_REG_ACCUM),
            DECS,
        }
    },

    /**************************************************************************************/
    /*                                      PHB                                           */
    /**************************************************************************************/

    [CPU_OPCODE_PHB_S] = {
        .uops = {
            IO,
            MOV_S       (MOV_LSB, CPU_REG_S, CPU_REG_ZERO, CPU_REG_DBR),
            DECS,
        }
    },

    /**************************************************************************************/
    /*                                      PHD                                           */
    /**************************************************************************************/

    [CPU_OPCODE_PHD_S] = {
        .uops = {
            IO,
            MOV_S       (MOV_MSB, CPU_REG_S, CPU_REG_ZERO, CPU_REG_D),
            DECS,
            MOV_S       (MOV_LSB, CPU_REG_S, CPU_REG_ZERO, CPU_REG_D),
            DECS,
        }
    },

    /**************************************************************************************/
    /*                                      PHK                                           */
    /**************************************************************************************/

    [CPU_OPCODE_PHK_S] = {
        .uops = {
            IO,
            MOV_S       (MOV_LSB, CPU_REG_S, CPU_REG_ZERO, CPU_REG_PBR),
            DECS,
        }
    },

    /**************************************************************************************/
    /*                                      PHP                                           */
    /**************************************************************************************/

    [CPU_OPCODE_PHP_S] = {
        .uops = {
            IO,
            MOV_P       (CPU_REG_P, CPU_REG_TEMP),
            MOV_S       (MOV_LSB, CPU_REG_S, CPU_REG_ZERO, CPU_REG_TEMP),
            DECS,
        }
    },

    /**************************************************************************************/
    /*                                      PHX                                           */
    /**************************************************************************************/

    [CPU_OPCODE_PHX_S] = {
        .uops = {
            SKIPS       (3, CPU_STATUS_FLAG_X),
            IO          /* X = 0 */,
            MOV_S       (MOV_MSB, CPU_REG_S, CPU_REG_ZERO, CPU_REG_X),
            DECS,
            MOV_S       (MOV_LSB, CPU_REG_S, CPU_REG_ZERO, CPU_REG_X),
            DECS,
        }
    },

    /**************************************************************************************/
    /*                                      PHY                                           */
    /**************************************************************************************/

    [CPU_OPCODE_PHY_S] = {
        .uops = {
            SKIPS       (3, CPU_STATUS_FLAG_X),
            IO          /* X = 0 */,
            MOV_S       (MOV_MSB, CPU_REG_S, CPU_REG_ZERO, CPU_REG_Y),
            DECS,
            MOV_S       (MOV_LSB, CPU_REG_S, CPU_REG_ZERO, CPU_REG_Y),
            DECS,
        }
    },

    /**************************************************************************************/
    /*                                      PLA                                           */
    /**************************************************************************************/

    [CPU_OPCODE_PLA_S] = {
        .uops = {
            IO,
            IO,
            INCS,
            MOV_L       (MOV_LSB, CPU_REG_S, CPU_REG_ZERO, CPU_REG_ACCUM),
            SKIPS       (2, CPU_STATUS_FLAG_M),
            INCS,
            MOV_L       (MOV_MSB, CPU_REG_S, CPU_REG_ZERO, CPU_REG_ACCUM),
            CHK_ZN      (CPU_REG_ACCUM),
        }
    },

    /**************************************************************************************/
    /*                                      PLB                                           */
    /**************************************************************************************/

    [CPU_OPCODE_PLB_S] = {
        .uops = {
            IO,
            IO,
            INCS,
            MOV_L       (MOV_LSB, CPU_REG_S, CPU_REG_ZERO, CPU_REG_DBR),
            CHK_ZN      (CPU_REG_DBR)
        }
    },

    /**************************************************************************************/
    /*                                      PLD                                           */
    /**************************************************************************************/

    [CPU_OPCODE_PLD_S] = {
        .uops = {
            IO,
            IO,
            INCS,
            MOV_L       (MOV_LSB, CPU_REG_S, CPU_REG_ZERO, CPU_REG_D),
            INCS,
            MOV_L       (MOV_MSB, CPU_REG_S, CPU_REG_ZERO, CPU_REG_D),
            CHK_ZN      (CPU_REG_D)
        }
    },

    /**************************************************************************************/
    /*                                      PLP                                           */
    /**************************************************************************************/

    [CPU_OPCODE_PLP_S] = {
        .uops = {
            IO,
            IO,
            INCS,
            MOV_L       (MOV_LSB, CPU_REG_S, CPU_REG_ZERO, CPU_REG_TEMP),
            MOV_P       (CPU_REG_TEMP, CPU_REG_P),
        }
    },

    /**************************************************************************************/
    /*                                      PLX                                           */
    /**************************************************************************************/
    [CPU_OPCODE_PLX_S] = {
        .uops = {
            IO,
            IO,
            INCS,
            MOV_L       (MOV_LSB, CPU_REG_S, CPU_REG_ZERO, CPU_REG_X),
            SKIPS       (2, CPU_STATUS_FLAG_X),
            INCS,
            MOV_L       (MOV_MSB, CPU_REG_S, CPU_REG_ZERO, CPU_REG_X),
            CHK_ZN      (CPU_REG_X)
        }
    },

    /**************************************************************************************/
    /*                                      PLY                                           */
    /**************************************************************************************/

    [CPU_OPCODE_PLY_S] = {
        .uops = {
            IO,
            IO,
            INCS,
            MOV_L       (MOV_LSB, CPU_REG_S, CPU_REG_ZERO, CPU_REG_Y),
            SKIPS       (2, CPU_STATUS_FLAG_X),
            INCS,
            MOV_L       (MOV_MSB, CPU_REG_S, CPU_REG_ZERO, CPU_REG_Y),
            CHK_ZN      (CPU_REG_Y)
        }
    },

    /**************************************************************************************/
    /*                                      REP                                           */
    /**************************************************************************************/

    [CPU_OPCODE_REP_IMM] = {
        .uops = {
            MOV_LPC     (MOV_LSB, CPU_REG_TEMP),
            IO,
            CLR_P       (CPU_STATUS_FLAG_LAST)
        }
    },

    /**************************************************************************************/
    /*                                      ROL                                           */
    /**************************************************************************************/

    [CPU_OPCODE_ROL_ABS] = {
        .uops = {
            MOV_LPC     (MOV_LSB, CPU_REG_ADDR),
            MOV_LPC     (MOV_MSB, CPU_REG_ADDR),
            MOV_RRW     (CPU_REG_DBR, CPU_REG_BANK, MOV_RRW_BYTE),
            MOV_L       (MOV_LSB, CPU_REG_ADDR, CPU_REG_BANK, CPU_REG_TEMP),
            SKIPS       (2, CPU_STATUS_FLAG_M),
            ADDR_OFFI   (1, ADDR_OFF_BANK_NEXT),
            MOV_L       (MOV_MSB, CPU_REG_ADDR, CPU_REG_BANK, CPU_REG_TEMP),
            IO,
            ALU_OP      (ALU_OP_ROL, CPU_STATUS_FLAG_M, CPU_REG_TEMP, CPU_REG_ZERO),
            // CHK_ZNW     (CPU_REG_TEMP, 0),
            SKIPS       (2, CPU_STATUS_FLAG_M),
            MOV_S       (MOV_MSB, CPU_REG_ADDR, CPU_REG_BANK, CPU_REG_TEMP),
            ADDR_OFFIS  (0xffff, ADDR_OFF_BANK_NEXT, ADDR_OFF_SIGNED),
            MOV_S       (MOV_LSB, CPU_REG_ADDR, CPU_REG_BANK, CPU_REG_TEMP),
        }
    },

    [CPU_OPCODE_ROL_ACC] = {
        .uops = {
            IO,
            ALU_OP      (ALU_OP_ROL, CPU_STATUS_FLAG_M, CPU_REG_ACCUM, CPU_REG_ZERO),
            MOV_RR      (CPU_REG_TEMP, CPU_REG_ACCUM),
            // CHK_ZN      (CPU_REG_ACCUM),
        }
    },

    [CPU_OPCODE_ROL_ABS_X] = {
        .uops = {
            MOV_LPC     (MOV_LSB, CPU_REG_ADDR),
            MOV_LPC     (MOV_MSB, CPU_REG_ADDR),
            MOV_RRW     (CPU_REG_DBR, CPU_REG_BANK, MOV_RRW_BYTE),
            IO,
            ADDR_OFFR   (CPU_REG_X, ADDR_OFF_BANK_NEXT),
            MOV_L       (MOV_LSB, CPU_REG_ADDR, CPU_REG_BANK, CPU_REG_TEMP),
            SKIPS       (2, CPU_STATUS_FLAG_M),
            ADDR_OFFI   (1, ADDR_OFF_BANK_NEXT),
            MOV_L       (MOV_MSB, CPU_REG_ADDR, CPU_REG_BANK, CPU_REG_TEMP),
            ALU_OP      (ALU_OP_ROL, CPU_STATUS_FLAG_M, CPU_REG_TEMP, CPU_REG_ZERO),
            // CHK_ZNW     (CPU_REG_TEMP, 0),
            SKIPS       (2, CPU_STATUS_FLAG_M),
            MOV_S       (MOV_MSB, CPU_REG_ADDR, CPU_REG_BANK, CPU_REG_TEMP),
            ADDR_OFFIS  (0xffff, ADDR_OFF_BANK_WRAP, ADDR_OFF_SIGNED),
            MOV_S       (MOV_LSB, CPU_REG_ADDR, CPU_REG_BANK, CPU_REG_TEMP),
        }
    },

    [CPU_OPCODE_ROL_DIR] = {
        .uops = {
            MOV_LPC     (MOV_LSB, CPU_REG_ADDR),
            ZEXT        (CPU_REG_ADDR),
            SKIPC       (1, CPU_STATUS_FLAG_DL),
            IO          /* DL != 0 */,
            ADDR_OFFR   (CPU_REG_D, ADDR_OFF_BANK_WRAP),
            MOV_RRW     (CPU_REG_ZERO, CPU_REG_BANK, MOV_RRW_BYTE),
            MOV_L       (MOV_LSB, CPU_REG_ADDR, CPU_REG_BANK, CPU_REG_TEMP),
            SKIPS       (2, CPU_STATUS_FLAG_M),
            ADDR_OFFI   (1, ADDR_OFF_BANK_NEXT),
            MOV_L       (MOV_MSB, CPU_REG_ADDR, CPU_REG_BANK, CPU_REG_TEMP),
            IO,
            ALU_OP      (ALU_OP_ROL, CPU_STATUS_FLAG_M, CPU_REG_TEMP, CPU_REG_ZERO),
            // CHK_ZNW     (CPU_REG_TEMP, 0),
            SKIPS       (2, CPU_STATUS_FLAG_M),
            MOV_S       (MOV_MSB, CPU_REG_ADDR, CPU_REG_BANK, CPU_REG_TEMP),
            ADDR_OFFIS  (0xffff, ADDR_OFF_BANK_NEXT, ADDR_OFF_SIGNED),
            MOV_S       (MOV_LSB, CPU_REG_ADDR, CPU_REG_BANK, CPU_REG_TEMP),
        }
    },

    [CPU_OPCODE_ROL_DIR_X] = {
        .uops = {
            MOV_LPC     (MOV_LSB, CPU_REG_ADDR),
            ZEXT        (CPU_REG_ADDR),
            SKIPC       (1, CPU_STATUS_FLAG_DL),
            IO          /* DL != 0 */,
            IO,
            ADDR_OFFR   (CPU_REG_D, ADDR_OFF_BANK_WRAP),
            ADDR_OFFR   (CPU_REG_X, ADDR_OFF_BANK_WRAP),
            MOV_L       (MOV_LSB, CPU_REG_ADDR, CPU_REG_ZERO, CPU_REG_TEMP),
            SKIPS       (2, CPU_STATUS_FLAG_M),
            ADDR_OFFI   (1, ADDR_OFF_BANK_WRAP),
            MOV_L       (MOV_MSB, CPU_REG_ADDR, CPU_REG_ZERO, CPU_REG_TEMP),
            IO,
            ALU_OP      (ALU_OP_ROL, CPU_STATUS_FLAG_M, CPU_REG_TEMP, CPU_REG_ZERO),
            // CHK_ZNW     (CPU_REG_TEMP, 0),
            SKIPS       (2, CPU_STATUS_FLAG_M),
            MOV_S       (MOV_MSB, CPU_REG_ADDR, CPU_REG_ZERO, CPU_REG_TEMP),
            ADDR_OFFIS  (0xffff, ADDR_OFF_BANK_WRAP, ADDR_OFF_SIGNED),
            MOV_S       (MOV_LSB, CPU_REG_ADDR, CPU_REG_ZERO, CPU_REG_TEMP),
        }
    },

    /**************************************************************************************/
    /*                                      ROR                                           */
    /**************************************************************************************/

    [CPU_OPCODE_ROR_ABS] = {
        .uops = {
            MOV_LPC     (MOV_LSB, CPU_REG_ADDR),
            MOV_LPC     (MOV_MSB, CPU_REG_ADDR),
            MOV_RRW     (CPU_REG_DBR, CPU_REG_BANK, MOV_RRW_BYTE),
            MOV_L       (MOV_LSB, CPU_REG_ADDR, CPU_REG_BANK, CPU_REG_TEMP),
            SKIPS       (2, CPU_STATUS_FLAG_M),
            ADDR_OFFI   (1, ADDR_OFF_BANK_NEXT),
            MOV_L       (MOV_MSB, CPU_REG_ADDR, CPU_REG_BANK, CPU_REG_TEMP),
            IO,
            ALU_OP      (ALU_OP_ROR, CPU_STATUS_FLAG_M, CPU_REG_TEMP, CPU_REG_ZERO),
            // CHK_ZNW     (CPU_REG_TEMP, 0),
            SKIPS       (2, CPU_STATUS_FLAG_M),
            MOV_S       (MOV_MSB, CPU_REG_ADDR, CPU_REG_BANK, CPU_REG_TEMP),
            ADDR_OFFIS  (0xffff, ADDR_OFF_BANK_NEXT, ADDR_OFF_SIGNED),
            MOV_S       (MOV_LSB, CPU_REG_ADDR, CPU_REG_BANK, CPU_REG_TEMP),
        }
    },

    [CPU_OPCODE_ROR_ACC] = {
        .uops = {
            IO,
            ALU_OP      (ALU_OP_ROR, CPU_STATUS_FLAG_M, CPU_REG_ACCUM, CPU_REG_ZERO),
            MOV_RR      (CPU_REG_TEMP, CPU_REG_ACCUM),
            // CHK_ZN      (CPU_REG_ACCUM),
        }
    },

    [CPU_OPCODE_ROR_ABS_X] = {
        .uops = {
            MOV_LPC     (MOV_LSB, CPU_REG_ADDR),
            MOV_LPC     (MOV_MSB, CPU_REG_ADDR),
            MOV_RRW     (CPU_REG_DBR, CPU_REG_BANK, MOV_RRW_BYTE),
            IO,
            ADDR_OFFR   (CPU_REG_X, ADDR_OFF_BANK_NEXT),
            MOV_L       (MOV_LSB, CPU_REG_ADDR, CPU_REG_BANK, CPU_REG_TEMP),
            SKIPS       (2, CPU_STATUS_FLAG_M),
            ADDR_OFFI   (1, ADDR_OFF_BANK_NEXT),
            MOV_L       (MOV_MSB, CPU_REG_ADDR, CPU_REG_BANK, CPU_REG_TEMP),
            ALU_OP      (ALU_OP_ROR, CPU_STATUS_FLAG_M, CPU_REG_TEMP, CPU_REG_ZERO),
            // CHK_ZNW     (CPU_REG_TEMP, 0),
            SKIPS       (2, CPU_STATUS_FLAG_M),
            MOV_S       (MOV_MSB, CPU_REG_ADDR, CPU_REG_BANK, CPU_REG_TEMP),
            ADDR_OFFIS  (0xffff, ADDR_OFF_BANK_WRAP, ADDR_OFF_SIGNED),
            MOV_S       (MOV_LSB, CPU_REG_ADDR, CPU_REG_BANK, CPU_REG_TEMP),
        }
    },

    [CPU_OPCODE_ROR_DIR] = {
        .uops = {
            MOV_LPC     (MOV_LSB, CPU_REG_ADDR),
            ZEXT        (CPU_REG_ADDR),
            SKIPC       (1, CPU_STATUS_FLAG_DL),
            IO          /* DL != 0 */,
            ADDR_OFFR   (CPU_REG_D, ADDR_OFF_BANK_WRAP),
            MOV_RRW     (CPU_REG_ZERO, CPU_REG_BANK, MOV_RRW_BYTE),
            MOV_L       (MOV_LSB, CPU_REG_ADDR, CPU_REG_BANK, CPU_REG_TEMP),
            SKIPS       (2, CPU_STATUS_FLAG_M),
            ADDR_OFFI   (1, ADDR_OFF_BANK_NEXT),
            MOV_L       (MOV_MSB, CPU_REG_ADDR, CPU_REG_BANK, CPU_REG_TEMP),
            IO,
            ALU_OP      (ALU_OP_ROR, CPU_STATUS_FLAG_M, CPU_REG_TEMP, CPU_REG_ZERO),
            // CHK_ZNW     (CPU_REG_TEMP, 0),
            SKIPS       (2, CPU_STATUS_FLAG_M),
            MOV_S       (MOV_MSB, CPU_REG_ADDR, CPU_REG_BANK, CPU_REG_TEMP),
            ADDR_OFFIS  (0xffff, ADDR_OFF_BANK_NEXT, ADDR_OFF_SIGNED),
            MOV_S       (MOV_LSB, CPU_REG_ADDR, CPU_REG_BANK, CPU_REG_TEMP),
        }
    },

    [CPU_OPCODE_ROR_DIR_X] = {
        .uops = {
            MOV_LPC     (MOV_LSB, CPU_REG_ADDR),
            ZEXT        (CPU_REG_ADDR),
            SKIPC       (1, CPU_STATUS_FLAG_DL),
            IO          /* DL != 0 */,
            IO,
            ADDR_OFFR   (CPU_REG_D, ADDR_OFF_BANK_WRAP),
            ADDR_OFFR   (CPU_REG_X, ADDR_OFF_BANK_WRAP),
            MOV_L       (MOV_LSB, CPU_REG_ADDR, CPU_REG_ZERO, CPU_REG_TEMP),
            SKIPS       (2, CPU_STATUS_FLAG_M),
            ADDR_OFFI   (1, ADDR_OFF_BANK_WRAP),
            MOV_L       (MOV_MSB, CPU_REG_ADDR, CPU_REG_ZERO, CPU_REG_TEMP),
            IO,
            ALU_OP      (ALU_OP_ROR, CPU_STATUS_FLAG_M, CPU_REG_TEMP, CPU_REG_ZERO),
            // CHK_ZNW     (CPU_REG_TEMP, 0),
            SKIPS       (2, CPU_STATUS_FLAG_M),
            MOV_S       (MOV_MSB, CPU_REG_ADDR, CPU_REG_ZERO, CPU_REG_TEMP),
            ADDR_OFFIS  (0xffff, ADDR_OFF_BANK_WRAP, ADDR_OFF_SIGNED),
            MOV_S       (MOV_LSB, CPU_REG_ADDR, CPU_REG_ZERO, CPU_REG_TEMP),
        }
    },

    /**************************************************************************************/
    /*                                      RTI                                           */
    /**************************************************************************************/

    [CPU_OPCODE_RTI_S] = {
        .uops = {
            IO,
            IO,
            INCS,
            MOV_L       (MOV_LSB, CPU_REG_S, CPU_REG_ZERO, CPU_REG_TEMP),
            MOV_P       (CPU_REG_TEMP, CPU_REG_P),
            INCS,
            MOV_L       (MOV_LSB, CPU_REG_S, CPU_REG_ZERO, CPU_REG_TEMP),
            INCS,
            MOV_L       (MOV_MSB, CPU_REG_S, CPU_REG_ZERO, CPU_REG_TEMP),
            MOV_RRW     (CPU_REG_TEMP, CPU_REG_PC, MOV_RRW_WORD),
            SKIPS       (2, CPU_STATUS_FLAG_E),
            INCS,
            MOV_L       (MOV_LSB, CPU_REG_S, CPU_REG_ZERO, CPU_REG_PBR),
        }
    },

    /**************************************************************************************/
    /*                                      RTL                                           */
    /**************************************************************************************/

    [CPU_OPCODE_RTL_S] = {
        .uops = {
            IO,
            IO,
            INCS,
            MOV_L       (MOV_LSB, CPU_REG_S, CPU_REG_ZERO, CPU_REG_ADDR),
            INCS,
            MOV_L       (MOV_MSB, CPU_REG_S, CPU_REG_ZERO, CPU_REG_ADDR),
            INCS,
            MOV_L       (MOV_LSB, CPU_REG_S, CPU_REG_ZERO, CPU_REG_BANK),
            IO,
            INC_RW      (CPU_REG_ADDR),
            MOV_RRW     (CPU_REG_ADDR, CPU_REG_PC, MOV_RRW_WORD),
            MOV_RRW     (CPU_REG_BANK, CPU_REG_PBR, MOV_RRW_BYTE),
        }
    },

    /**************************************************************************************/
    /*                                      RTS                                           */
    /**************************************************************************************/

    [CPU_OPCODE_RTS_S] = {
        .uops = {
            IO,
            IO,
            INCS,
            MOV_L       (MOV_LSB, CPU_REG_S, CPU_REG_ZERO, CPU_REG_ADDR),
            INCS,
            MOV_L       (MOV_MSB, CPU_REG_S, CPU_REG_ZERO, CPU_REG_ADDR),
            IO,
            INC_RW      (CPU_REG_ADDR),
            MOV_RRW     (CPU_REG_ADDR, CPU_REG_PC, MOV_RRW_WORD),
        }
    },

    /**************************************************************************************/
    /*                                      SBC                                           */
    /**************************************************************************************/

    [CPU_OPCODE_SBC_ABS] = {
        .uops = {
            MOV_LPC     (MOV_LSB, CPU_REG_ADDR),
            MOV_LPC     (MOV_MSB, CPU_REG_ADDR),
            MOV_RRW     (CPU_REG_DBR, CPU_REG_BANK, MOV_RRW_BYTE),
            MOV_L       (MOV_LSB, CPU_REG_ADDR, CPU_REG_BANK, CPU_REG_TEMP),
            SKIPS       (2, CPU_STATUS_FLAG_M),
            ADDR_OFFI   (1, ADDR_OFF_BANK_NEXT),
            MOV_L       (MOV_MSB, CPU_REG_ADDR, CPU_REG_BANK, CPU_REG_TEMP),
            ALU_OP      (ALU_OP_SUB, CPU_STATUS_FLAG_M, CPU_REG_ACCUM, CPU_REG_TEMP),
            MOV_RR      (CPU_REG_TEMP, CPU_REG_ACCUM),
            // CHK_ZN      (CPU_REG_ACCUM),
        }
    },
    [CPU_OPCODE_SBC_ABS_X] = {
        .uops = {
            MOV_LPC     (MOV_LSB, CPU_REG_ADDR),
            MOV_LPC     (MOV_MSB, CPU_REG_ADDR),
            MOV_RRW     (CPU_REG_DBR, CPU_REG_BANK, MOV_RRW_BYTE),
            ADDR_OFFR   (CPU_REG_X, ADDR_OFF_BANK_NEXT),
            SKIPC       (2, CPU_STATUS_FLAG_PAGE),
            SKIPS       (1, CPU_STATUS_FLAG_X),
            IO          /* X=0 or page boundary */,
            MOV_L       (MOV_LSB, CPU_REG_ADDR, CPU_REG_BANK, CPU_REG_TEMP),
            SKIPS       (2, CPU_STATUS_FLAG_M),
            ADDR_OFFI   (1, ADDR_OFF_BANK_NEXT),
            MOV_L       (MOV_MSB, CPU_REG_ADDR, CPU_REG_BANK, CPU_REG_TEMP),
            ALU_OP      (ALU_OP_SUB, CPU_STATUS_FLAG_M, CPU_REG_ACCUM, CPU_REG_TEMP),
            MOV_RR      (CPU_REG_TEMP, CPU_REG_ACCUM),
            // CHK_ZN      (CPU_REG_ACCUM),
        }
    },
    [CPU_OPCODE_SBC_ABS_Y] = {
        .uops = {
            MOV_LPC     (MOV_LSB, CPU_REG_ADDR),
            MOV_LPC     (MOV_MSB, CPU_REG_ADDR),
            MOV_RRW     (CPU_REG_DBR, CPU_REG_BANK, MOV_RRW_BYTE),
            ADDR_OFFR   (CPU_REG_Y, ADDR_OFF_BANK_NEXT),
            SKIPC       (2, CPU_STATUS_FLAG_PAGE),
            SKIPS       (1, CPU_STATUS_FLAG_X),
            IO          /* X=0 or page boundary */,
            MOV_L       (MOV_LSB, CPU_REG_ADDR, CPU_REG_BANK, CPU_REG_TEMP),
            SKIPS       (2, CPU_STATUS_FLAG_M),
            ADDR_OFFI   (1, ADDR_OFF_BANK_NEXT),
            MOV_L       (MOV_MSB, CPU_REG_ADDR, CPU_REG_BANK, CPU_REG_TEMP),
            ALU_OP      (ALU_OP_SUB, CPU_STATUS_FLAG_M, CPU_REG_ACCUM, CPU_REG_TEMP),
            MOV_RR      (CPU_REG_TEMP, CPU_REG_ACCUM),
            // CHK_ZN      (CPU_REG_ACCUM),
        }
    },

    [CPU_OPCODE_SBC_ABSL] = {
        .uops = {
            MOV_LPC     (MOV_LSB, CPU_REG_ADDR),
            MOV_LPC     (MOV_MSB, CPU_REG_ADDR),
            MOV_LPC     (MOV_LSB, CPU_REG_BANK),
            MOV_L       (MOV_LSB, CPU_REG_ADDR, CPU_REG_BANK, CPU_REG_TEMP),
            SKIPS       (2, CPU_STATUS_FLAG_M),
            ADDR_OFFI   (1, ADDR_OFF_BANK_NEXT),
            MOV_L       (MOV_MSB, CPU_REG_ADDR, CPU_REG_BANK, CPU_REG_TEMP),
            ALU_OP      (ALU_OP_SUB, CPU_STATUS_FLAG_M, CPU_REG_ACCUM, CPU_REG_TEMP),
            MOV_RR      (CPU_REG_TEMP, CPU_REG_ACCUM),
        }
    },

    [CPU_OPCODE_SBC_ABSL_X] = {
        .uops = {
            MOV_LPC     (MOV_LSB, CPU_REG_ADDR),
            MOV_LPC     (MOV_MSB, CPU_REG_ADDR),
            MOV_LPC     (MOV_LSB, CPU_REG_BANK),
            ADDR_OFFR   (CPU_REG_X, ADDR_OFF_BANK_NEXT),
            MOV_L       (MOV_LSB, CPU_REG_ADDR, CPU_REG_BANK, CPU_REG_TEMP),
            SKIPS       (2, CPU_STATUS_FLAG_M),
            ADDR_OFFI   (1, ADDR_OFF_BANK_NEXT),
            MOV_L       (MOV_MSB, CPU_REG_ADDR, CPU_REG_BANK, CPU_REG_TEMP),
            ALU_OP      (ALU_OP_SUB, CPU_STATUS_FLAG_M, CPU_REG_ACCUM, CPU_REG_TEMP),
            MOV_RR      (CPU_REG_TEMP, CPU_REG_ACCUM),
            // CHK_ZN      (CPU_REG_ACCUM),
        }
    },
    [CPU_OPCODE_SBC_DIR] = {
        .uops = {
            MOV_LPC     (MOV_LSB, CPU_REG_ADDR),
            ZEXT        (CPU_REG_ADDR),
            ADDR_OFFR   (CPU_REG_D, ADDR_OFF_BANK_WRAP),
            SKIPC       (1, CPU_STATUS_FLAG_DL),
            IO          /* DL != 0 */,
            MOV_RRW     (CPU_REG_ZERO, CPU_REG_BANK, MOV_RRW_BYTE),
            MOV_L       (MOV_LSB, CPU_REG_ADDR, CPU_REG_BANK, CPU_REG_TEMP),
            SKIPS       (2, CPU_STATUS_FLAG_M),
            ADDR_OFFI   (1, ADDR_OFF_BANK_NEXT),
            MOV_L       (MOV_MSB, CPU_REG_ADDR, CPU_REG_BANK, CPU_REG_TEMP),
            ALU_OP      (ALU_OP_SUB, CPU_STATUS_FLAG_M, CPU_REG_ACCUM, CPU_REG_TEMP),
            MOV_RR      (CPU_REG_TEMP, CPU_REG_ACCUM),
            // CHK_ZN      (CPU_REG_ACCUM),
        }
    },
    [CPU_OPCODE_SBC_S_REL] = {
        .uops = {
            MOV_LPC     (MOV_LSB, CPU_REG_ADDR),
            ZEXT        (CPU_REG_ADDR),
            ADDR_OFFR   (CPU_REG_S, ADDR_OFF_BANK_WRAP),
            IO,
            MOV_L       (MOV_LSB, CPU_REG_ADDR, CPU_REG_ZERO, CPU_REG_TEMP),
            SKIPS       (2, CPU_STATUS_FLAG_M),
            ADDR_OFFI   (1, ADDR_OFF_BANK_WRAP),
            MOV_L       (MOV_MSB, CPU_REG_ADDR, CPU_REG_ZERO, CPU_REG_TEMP),
            ALU_OP      (ALU_OP_SUB, CPU_STATUS_FLAG_M, CPU_REG_ACCUM, CPU_REG_TEMP),
            MOV_RR      (CPU_REG_TEMP, CPU_REG_ACCUM),
            // CHK_ZN      (CPU_REG_ACCUM),
        },
    },
    [CPU_OPCODE_SBC_DIR_X] = {
        .uops = {
            MOV_LPC     (MOV_LSB, CPU_REG_ADDR),
            ZEXT        (CPU_REG_ADDR),
            SKIPC       (1, CPU_STATUS_FLAG_DL),
            IO          /* DL != 0 */,
            ADDR_OFFR   (CPU_REG_D, ADDR_OFF_BANK_WRAP),
            ADDR_OFFR   (CPU_REG_X, ADDR_OFF_BANK_WRAP),
            IO,
            MOV_L       (MOV_LSB, CPU_REG_ADDR, CPU_REG_ZERO, CPU_REG_TEMP),
            SKIPS       (2, CPU_STATUS_FLAG_M),
            ADDR_OFFI   (1, ADDR_OFF_BANK_WRAP),
            MOV_L       (MOV_MSB, CPU_REG_ADDR, CPU_REG_ZERO, CPU_REG_TEMP),
            ALU_OP      (ALU_OP_SUB, CPU_STATUS_FLAG_M, CPU_REG_ACCUM, CPU_REG_TEMP),
            MOV_RR      (CPU_REG_TEMP, CPU_REG_ACCUM),
            // CHK_ZN      (CPU_REG_ACCUM),
        }
    },

    [CPU_OPCODE_SBC_DIR_IND] = {
        .uops = {
            MOV_LPC     (MOV_LSB, CPU_REG_ADDR),
            ZEXT        (CPU_REG_ADDR),
            SKIPC       (1, CPU_STATUS_FLAG_DL),
            IO          /* DL != 0 */,
            ADDR_OFFR   (CPU_REG_D, 0),
            MOV_RRW     (CPU_REG_ZERO, CPU_REG_BANK, MOV_RRW_BYTE),
            MOV_L       (MOV_LSB, CPU_REG_ADDR, CPU_REG_BANK, CPU_REG_TEMP),
            ADDR_OFFI   (1, ADDR_OFF_BANK_NEXT),
            MOV_L       (MOV_MSB, CPU_REG_ADDR, CPU_REG_BANK, CPU_REG_TEMP),
            MOV_RRW     (CPU_REG_TEMP, CPU_REG_ADDR, MOV_RRW_WORD),
            MOV_RRW     (CPU_REG_DBR, CPU_REG_BANK, MOV_RRW_BYTE),
            MOV_L       (MOV_LSB, CPU_REG_ADDR, CPU_REG_BANK, CPU_REG_TEMP),
            SKIPS       (2, CPU_STATUS_FLAG_M),
            ADDR_OFFI   (1, ADDR_OFF_BANK_NEXT),
            MOV_L       (MOV_MSB, CPU_REG_ADDR, CPU_REG_BANK, CPU_REG_TEMP),
            ALU_OP      (ALU_OP_SUB, CPU_STATUS_FLAG_M, CPU_REG_ACCUM, CPU_REG_TEMP),
            MOV_RR      (CPU_REG_TEMP, CPU_REG_ACCUM),
            // CHK_ZN      (CPU_REG_ACCUM),
        }
    },

    [CPU_OPCODE_SBC_DIR_INDL] = {
        .uops = {
            MOV_LPC     (MOV_LSB, CPU_REG_ADDR),
            ZEXT        (CPU_REG_ADDR),
            SKIPC       (1, CPU_STATUS_FLAG_DL),
            IO          /* DL != 0 */,
            ADDR_OFFR   (CPU_REG_D, ADDR_OFF_BANK_WRAP),
            MOV_RRW     (CPU_REG_ZERO, CPU_REG_BANK, MOV_RRW_BYTE),
            MOV_L       (MOV_LSB, CPU_REG_ADDR, CPU_REG_BANK, CPU_REG_TEMP),
            ADDR_OFFI   (1, ADDR_OFF_BANK_NEXT),
            MOV_L       (MOV_MSB, CPU_REG_ADDR, CPU_REG_BANK, CPU_REG_TEMP),
            ADDR_OFFI   (1, ADDR_OFF_BANK_NEXT),
            MOV_L       (MOV_LSB, CPU_REG_ADDR, CPU_REG_BANK, CPU_REG_BANK),
            MOV_RRW     (CPU_REG_TEMP, CPU_REG_ADDR, MOV_RRW_WORD),
            MOV_L       (MOV_LSB, CPU_REG_ADDR, CPU_REG_BANK, CPU_REG_TEMP),
            SKIPS       (2, CPU_STATUS_FLAG_M),
            ADDR_OFFI   (1, ADDR_OFF_BANK_NEXT),
            MOV_L       (MOV_MSB, CPU_REG_ADDR, CPU_REG_BANK, CPU_REG_TEMP),
            ALU_OP      (ALU_OP_SUB, CPU_STATUS_FLAG_M, CPU_REG_ACCUM, CPU_REG_TEMP),
            MOV_RR      (CPU_REG_TEMP, CPU_REG_ACCUM),
            // CHK_ZN      (CPU_REG_ACCUM),
        }
    },

    [CPU_OPCODE_SBC_S_REL_IND_Y] = {
        .uops = {
            MOV_LPC     (MOV_LSB, CPU_REG_ADDR),
            ZEXT        (CPU_REG_ADDR),
            IO,
            ADDR_OFFR   (CPU_REG_S, ADDR_OFF_BANK_WRAP),
            MOV_L       (MOV_LSB, CPU_REG_ADDR, CPU_REG_ZERO, CPU_REG_TEMP),
            ADDR_OFFI   (1, ADDR_OFF_BANK_WRAP),
            MOV_L       (MOV_MSB, CPU_REG_ADDR, CPU_REG_ZERO, CPU_REG_TEMP),
            MOV_RRW     (CPU_REG_TEMP, CPU_REG_ADDR, MOV_RRW_WORD),
            MOV_RRW     (CPU_REG_DBR, CPU_REG_BANK, MOV_RRW_BYTE),
            IO,
            ADDR_OFFR   (CPU_REG_Y, ADDR_OFF_BANK_NEXT),
            MOV_L       (MOV_LSB, CPU_REG_ADDR, CPU_REG_BANK, CPU_REG_TEMP),
            SKIPS       (2, CPU_STATUS_FLAG_M),
            ADDR_OFFI   (1, ADDR_OFF_BANK_NEXT),
            MOV_L       (MOV_MSB, CPU_REG_ADDR, CPU_REG_BANK, CPU_REG_TEMP),
            ALU_OP      (ALU_OP_SUB, CPU_STATUS_FLAG_M, CPU_REG_ACCUM, CPU_REG_TEMP),
            MOV_RR      (CPU_REG_TEMP, CPU_REG_ACCUM),
            // CHK_ZN      (CPU_REG_ACCUM),

        }
    },

    [CPU_OPCODE_SBC_DIR_X_IND] = {
        .uops = {
            MOV_LPC     (MOV_LSB, CPU_REG_ADDR),
            ZEXT        (CPU_REG_ADDR),
            SKIPC       (1, CPU_STATUS_FLAG_DL),
            IO          /* DL != 0 */,
            ADDR_OFFR   (CPU_REG_D, ADDR_OFF_BANK_WRAP),
            ADDR_OFFR   (CPU_REG_X, ADDR_OFF_BANK_WRAP),
            IO,
            MOV_L       (MOV_LSB, CPU_REG_ADDR, CPU_REG_ZERO, CPU_REG_TEMP),
            ADDR_OFFI   (1, ADDR_OFF_BANK_WRAP),
            MOV_L       (MOV_MSB, CPU_REG_ADDR, CPU_REG_ZERO, CPU_REG_TEMP),
            MOV_RRW     (CPU_REG_TEMP, CPU_REG_ADDR, MOV_RRW_WORD),
            MOV_RRW     (CPU_REG_DBR, CPU_REG_BANK, MOV_RRW_BYTE),
            MOV_L       (MOV_LSB, CPU_REG_ADDR, CPU_REG_BANK, CPU_REG_TEMP),
            SKIPS       (2, CPU_STATUS_FLAG_M),
            ADDR_OFFI   (1, ADDR_OFF_BANK_NEXT),
            MOV_L       (MOV_MSB, CPU_REG_ADDR, CPU_REG_BANK, CPU_REG_TEMP),
            ALU_OP      (ALU_OP_SUB, CPU_STATUS_FLAG_M, CPU_REG_ACCUM, CPU_REG_TEMP),
            MOV_RR      (CPU_REG_TEMP, CPU_REG_ACCUM),
            // CHK_ZN      (CPU_REG_ACCUM),
        }
    },

    [CPU_OPCODE_SBC_DIR_IND_Y] = {
        .uops = {
            MOV_LPC     (MOV_LSB, CPU_REG_ADDR),
            ZEXT        (CPU_REG_ADDR),
            SKIPC       (1, CPU_STATUS_FLAG_DL),
            IO          /* DL != 0 */,
            ADDR_OFFR   (CPU_REG_D, 0),
            MOV_RRW     (CPU_REG_ZERO, CPU_REG_BANK, MOV_RRW_BYTE),
            MOV_L       (MOV_LSB, CPU_REG_ADDR, CPU_REG_BANK, CPU_REG_TEMP),
            ADDR_OFFI   (1, ADDR_OFF_BANK_NEXT),
            MOV_L       (MOV_MSB, CPU_REG_ADDR, CPU_REG_BANK, CPU_REG_TEMP),
            MOV_RRW     (CPU_REG_TEMP, CPU_REG_ADDR, MOV_RRW_WORD),
            MOV_RRW     (CPU_REG_DBR, CPU_REG_BANK, MOV_RRW_BYTE),
            ADDR_OFFR   (CPU_REG_Y, ADDR_OFF_BANK_NEXT),
            SKIPS       (2, CPU_STATUS_FLAG_X),
            SKIPC       (1, CPU_STATUS_FLAG_PAGE),
            IO          /* X = 0 or page crossed */,
            MOV_L       (MOV_LSB, CPU_REG_ADDR, CPU_REG_BANK, CPU_REG_TEMP),
            SKIPS       (2, CPU_STATUS_FLAG_M),
            ADDR_OFFI   (1, ADDR_OFF_BANK_NEXT),
            MOV_L       (MOV_MSB, CPU_REG_ADDR, CPU_REG_BANK, CPU_REG_TEMP),
            ALU_OP      (ALU_OP_SUB, CPU_STATUS_FLAG_M, CPU_REG_ACCUM, CPU_REG_TEMP),
            MOV_RR      (CPU_REG_TEMP, CPU_REG_ACCUM),
            // CHK_ZN      (CPU_REG_ACCUM),
        }
    },

    [CPU_OPCODE_SBC_DIR_INDL_Y] = {
        .uops = {
            MOV_LPC     (MOV_LSB, CPU_REG_ADDR),
            ZEXT        (CPU_REG_ADDR),
            ADDR_OFFR   (CPU_REG_D, 0),
            SKIPC       (1, CPU_STATUS_FLAG_DL),
            IO          /* DL != 0 */,
            MOV_RRW     (CPU_REG_ZERO, CPU_REG_BANK, MOV_RRW_BYTE),
            MOV_L       (MOV_LSB, CPU_REG_ADDR, CPU_REG_BANK, CPU_REG_TEMP),
            ADDR_OFFI   (1, ADDR_OFF_BANK_NEXT),
            MOV_L       (MOV_MSB, CPU_REG_ADDR, CPU_REG_BANK, CPU_REG_TEMP),
            ADDR_OFFI   (1, ADDR_OFF_BANK_NEXT),
            MOV_L       (MOV_LSB, CPU_REG_ADDR, CPU_REG_BANK, CPU_REG_BANK),
            MOV_RRW     (CPU_REG_TEMP, CPU_REG_ADDR, MOV_RRW_WORD),
            // MOV_RRW     (CPU_REG_DBR, CPU_REG_BANK, MOV_RRW_BYTE),
            ADDR_OFFR   (CPU_REG_Y, ADDR_OFF_BANK_NEXT),
            MOV_L       (MOV_LSB, CPU_REG_ADDR, CPU_REG_BANK, CPU_REG_TEMP),
            SKIPS       (2, CPU_STATUS_FLAG_M),
            ADDR_OFFI   (1, ADDR_OFF_BANK_NEXT),
            MOV_L       (MOV_MSB, CPU_REG_ADDR, CPU_REG_BANK, CPU_REG_TEMP),
            ALU_OP      (ALU_OP_SUB, CPU_STATUS_FLAG_M, CPU_REG_ACCUM, CPU_REG_TEMP),
            MOV_RR      (CPU_REG_TEMP, CPU_REG_ACCUM),
            // CHK_ZN      (CPU_REG_ACCUM),
        }
    },

    [CPU_OPCODE_SBC_IMM] = {
        .uops = {
            MOV_LPC     (MOV_LSB, CPU_REG_TEMP),
            SKIPS       (1, CPU_STATUS_FLAG_M),
            MOV_LPC     (MOV_MSB, CPU_REG_TEMP),
            ALU_OP      (ALU_OP_SUB, CPU_STATUS_FLAG_M, CPU_REG_ACCUM, CPU_REG_TEMP),
            MOV_RR      (CPU_REG_TEMP, CPU_REG_ACCUM),
            // CHK_ZN      (CPU_REG_ACCUM),
        }
    },

    /**************************************************************************************/
    /*                                      SEC                                           */
    /**************************************************************************************/

    [CPU_OPCODE_SEC_IMP] = {
        .uops = {
            IO,
            SET_P       (CPU_STATUS_FLAG_C)
        },
    },

    /**************************************************************************************/
    /*                                      SED                                           */
    /**************************************************************************************/

    [CPU_OPCODE_SED_IMP] = {
        .uops = {
            IO,
            SET_P       (CPU_STATUS_FLAG_D)
        },
    },

    /**************************************************************************************/
    /*                                      SEI                                           */
    /**************************************************************************************/

    [CPU_OPCODE_SEI_IMP] = {
        .uops = {
            IO,
            SET_P       (CPU_STATUS_FLAG_I)
        },
    },

    /**************************************************************************************/
    /*                                      SEP                                           */
    /**************************************************************************************/

    [CPU_OPCODE_SEP_IMM] = {
        .uops = {
            MOV_LPC     (MOV_LSB, CPU_REG_TEMP),
            IO,
            SET_P       (CPU_STATUS_FLAG_LAST)
        }
    },

    /**************************************************************************************/
    /*                                      STA                                           */
    /**************************************************************************************/

    [CPU_OPCODE_STA_ABS] = {
        .uops = {
            MOV_LPC     (MOV_LSB, CPU_REG_ADDR),
            MOV_LPC     (MOV_MSB, CPU_REG_ADDR),
            MOV_RRW     (CPU_REG_DBR, CPU_REG_BANK, MOV_RRW_BYTE),
            MOV_S       (MOV_LSB, CPU_REG_ADDR, CPU_REG_BANK, CPU_REG_ACCUM),
            SKIPS       (2, CPU_STATUS_FLAG_M),
            ADDR_OFFI   (1, ADDR_OFF_BANK_NEXT),
            MOV_S       (MOV_MSB, CPU_REG_ADDR, CPU_REG_BANK, CPU_REG_ACCUM),
        }
    },
    [CPU_OPCODE_STA_ABS_X] = {
        .uops = {
            MOV_LPC     (MOV_LSB, CPU_REG_ADDR),
            MOV_LPC     (MOV_MSB, CPU_REG_ADDR),
            MOV_RRW     (CPU_REG_DBR, CPU_REG_BANK, MOV_RRW_BYTE),
            ADDR_OFFR   (CPU_REG_X, ADDR_OFF_BANK_NEXT),
            IO          /* extra cycle due to write */,
            MOV_S       (MOV_LSB, CPU_REG_ADDR, CPU_REG_BANK, CPU_REG_ACCUM),
            SKIPS       (2, CPU_STATUS_FLAG_M),
            ADDR_OFFI   (1, ADDR_OFF_BANK_NEXT),
            MOV_S       (MOV_MSB, CPU_REG_ADDR, CPU_REG_BANK, CPU_REG_ACCUM),
        }
    },
    [CPU_OPCODE_STA_ABS_Y] = {
        .uops = {
            MOV_LPC     (MOV_LSB, CPU_REG_ADDR),
            MOV_LPC     (MOV_MSB, CPU_REG_ADDR),
            MOV_RRW     (CPU_REG_DBR, CPU_REG_BANK, MOV_RRW_BYTE),
            ADDR_OFFR   (CPU_REG_Y, ADDR_OFF_BANK_NEXT),
            IO          /* extra cycle due to write */,
            MOV_S       (MOV_LSB, CPU_REG_ADDR, CPU_REG_BANK, CPU_REG_ACCUM),
            SKIPS       (2, CPU_STATUS_FLAG_M),
            ADDR_OFFI   (1, ADDR_OFF_BANK_NEXT),
            MOV_S       (MOV_MSB, CPU_REG_ADDR, CPU_REG_BANK, CPU_REG_ACCUM),
        }
    },
    [CPU_OPCODE_STA_ABSL] = {
        .uops = {
            MOV_LPC     (MOV_LSB, CPU_REG_ADDR),
            MOV_LPC     (MOV_MSB, CPU_REG_ADDR),
            MOV_LPC     (MOV_LSB, CPU_REG_BANK),
            MOV_S       (MOV_LSB, CPU_REG_ADDR, CPU_REG_BANK, CPU_REG_ACCUM),
            SKIPS       (2, CPU_STATUS_FLAG_M),
            ADDR_OFFI   (1, ADDR_OFF_BANK_NEXT),
            MOV_S       (MOV_MSB, CPU_REG_ADDR, CPU_REG_BANK, CPU_REG_ACCUM),
        }
    },
    [CPU_OPCODE_STA_ABSL_X] = {
        .uops = {
            MOV_LPC     (MOV_LSB, CPU_REG_ADDR),
            MOV_LPC     (MOV_MSB, CPU_REG_ADDR),
            MOV_LPC     (MOV_LSB, CPU_REG_BANK),
            ADDR_OFFR   (CPU_REG_X, ADDR_OFF_BANK_NEXT),
            MOV_S       (MOV_LSB, CPU_REG_ADDR, CPU_REG_BANK, CPU_REG_ACCUM),
            SKIPS       (2, CPU_STATUS_FLAG_M),
            ADDR_OFFI   (1, ADDR_OFF_BANK_NEXT),
            MOV_S       (MOV_MSB, CPU_REG_ADDR, CPU_REG_BANK, CPU_REG_ACCUM),
        }
    },
    [CPU_OPCODE_STA_DIR] = {
        .uops = {
            MOV_LPC     (MOV_LSB, CPU_REG_ADDR),
            ZEXT        (CPU_REG_ADDR),
            ADDR_OFFR   (CPU_REG_D, ADDR_OFF_BANK_WRAP),
            SKIPC       (1, CPU_STATUS_FLAG_DL),
            IO          /* DL != 0 */,
            MOV_RRW     (CPU_REG_ZERO, CPU_REG_BANK, MOV_RRW_BYTE),
            MOV_S       (MOV_LSB, CPU_REG_ADDR, CPU_REG_BANK, CPU_REG_ACCUM),
            SKIPS       (2, CPU_STATUS_FLAG_M),
            ADDR_OFFI   (1, ADDR_OFF_BANK_NEXT),
            MOV_S       (MOV_MSB, CPU_REG_ADDR, CPU_REG_BANK, CPU_REG_ACCUM),
        }
    },
    [CPU_OPCODE_STA_S_REL] = {
        .uops = {
            MOV_LPC     (MOV_LSB, CPU_REG_ADDR),
            ZEXT        (CPU_REG_ADDR),
            ADDR_OFFR   (CPU_REG_S, ADDR_OFF_BANK_WRAP),
            IO,
            MOV_S       (MOV_LSB, CPU_REG_ADDR, CPU_REG_ZERO, CPU_REG_ACCUM),
            SKIPS       (2, CPU_STATUS_FLAG_M),
            ADDR_OFFI   (1, ADDR_OFF_BANK_WRAP),
            MOV_S       (MOV_MSB, CPU_REG_ADDR, CPU_REG_ZERO, CPU_REG_ACCUM),
        },
    },

    [CPU_OPCODE_STA_DIR_X] = {
        .uops = {
            MOV_LPC     (MOV_LSB, CPU_REG_ADDR),
            ZEXT        (CPU_REG_ADDR),
            SKIPC       (1, CPU_STATUS_FLAG_DL),
            IO          /* DL != 0 */,
            ADDR_OFFR   (CPU_REG_D, ADDR_OFF_BANK_WRAP),
            ADDR_OFFR   (CPU_REG_X, ADDR_OFF_BANK_WRAP),
            IO,
            MOV_S       (MOV_LSB, CPU_REG_ADDR, CPU_REG_ZERO, CPU_REG_ACCUM),
            SKIPS       (2, CPU_STATUS_FLAG_M),
            ADDR_OFFI   (1, ADDR_OFF_BANK_WRAP),
            MOV_S       (MOV_MSB, CPU_REG_ADDR, CPU_REG_ZERO, CPU_REG_ACCUM),
        }
    },

    [CPU_OPCODE_STA_DIR_IND] = {
        .uops = {
            MOV_LPC     (MOV_LSB, CPU_REG_ADDR),
            ZEXT        (CPU_REG_ADDR),
            SKIPC       (1, CPU_STATUS_FLAG_DL),
            IO          /* DL != 0 */,
            ADDR_OFFR   (CPU_REG_D, ADDR_OFF_BANK_WRAP),
            MOV_RRW     (CPU_REG_ZERO, CPU_REG_BANK, MOV_RRW_BYTE),
            MOV_L       (MOV_LSB, CPU_REG_ADDR, CPU_REG_BANK, CPU_REG_TEMP),
            ADDR_OFFI   (1, ADDR_OFF_BANK_NEXT),
            MOV_L       (MOV_MSB, CPU_REG_ADDR, CPU_REG_BANK, CPU_REG_TEMP),
            MOV_RRW     (CPU_REG_TEMP, CPU_REG_ADDR, MOV_RRW_WORD),
            MOV_RRW     (CPU_REG_DBR, CPU_REG_BANK, MOV_RRW_BYTE),
            MOV_S       (MOV_LSB, CPU_REG_ADDR, CPU_REG_BANK, CPU_REG_ACCUM),
            SKIPS       (2, CPU_STATUS_FLAG_M),
            ADDR_OFFI   (1, ADDR_OFF_BANK_NEXT),
            MOV_S       (MOV_MSB, CPU_REG_ADDR, CPU_REG_BANK, CPU_REG_ACCUM),
        }
    },

    [CPU_OPCODE_STA_DIR_INDL] = {
        .uops = {
            MOV_LPC     (MOV_LSB, CPU_REG_ADDR),
            ZEXT        (CPU_REG_ADDR),
            SKIPC       (1, CPU_STATUS_FLAG_DL),
            IO          /* DL != 0 */,
            ADDR_OFFR   (CPU_REG_D, ADDR_OFF_BANK_WRAP),
            MOV_RRW     (CPU_REG_ZERO, CPU_REG_BANK, MOV_RRW_BYTE),
            MOV_L       (MOV_LSB, CPU_REG_ADDR, CPU_REG_BANK, CPU_REG_TEMP),
            ADDR_OFFI   (1, ADDR_OFF_BANK_NEXT),
            MOV_L       (MOV_MSB, CPU_REG_ADDR, CPU_REG_BANK, CPU_REG_TEMP),
            ADDR_OFFI   (1, ADDR_OFF_BANK_NEXT),
            MOV_L       (MOV_LSB, CPU_REG_ADDR, CPU_REG_BANK, CPU_REG_BANK),
            MOV_RRW     (CPU_REG_TEMP, CPU_REG_ADDR, MOV_RRW_WORD),
            MOV_S       (MOV_LSB, CPU_REG_ADDR, CPU_REG_BANK, CPU_REG_ACCUM),
            SKIPS       (2, CPU_STATUS_FLAG_M),
            ADDR_OFFI   (1, ADDR_OFF_BANK_NEXT),
            MOV_S       (MOV_MSB, CPU_REG_ADDR, CPU_REG_BANK, CPU_REG_ACCUM),
        }
    },

    [CPU_OPCODE_STA_S_REL_IND_Y] = {
        .uops = {
            MOV_LPC     (MOV_LSB, CPU_REG_ADDR),
            ZEXT        (CPU_REG_ADDR),
            IO,
            ADDR_OFFR   (CPU_REG_S, ADDR_OFF_BANK_WRAP),
            MOV_L       (MOV_LSB, CPU_REG_ADDR, CPU_REG_ZERO, CPU_REG_TEMP),
            ADDR_OFFI   (1, ADDR_OFF_BANK_WRAP),
            MOV_L       (MOV_MSB, CPU_REG_ADDR, CPU_REG_ZERO, CPU_REG_TEMP),
            MOV_RRW     (CPU_REG_TEMP, CPU_REG_ADDR, MOV_RRW_WORD),
            MOV_RRW     (CPU_REG_DBR, CPU_REG_BANK, MOV_RRW_BYTE),
            IO,
            ADDR_OFFR   (CPU_REG_Y, ADDR_OFF_BANK_NEXT),
            MOV_S       (MOV_LSB, CPU_REG_ADDR, CPU_REG_BANK, CPU_REG_ACCUM),
            SKIPS       (2, CPU_STATUS_FLAG_M),
            ADDR_OFFI   (1, ADDR_OFF_BANK_NEXT),
            MOV_S       (MOV_MSB, CPU_REG_ADDR, CPU_REG_BANK, CPU_REG_ACCUM),
        }
    },

    [CPU_OPCODE_STA_DIR_X_IND] = {
        .uops = {
            MOV_LPC     (MOV_LSB, CPU_REG_ADDR),
            ZEXT        (CPU_REG_ADDR),
            SKIPC       (1, CPU_STATUS_FLAG_DL),
            IO          /* DL != 0 */,
            ADDR_OFFR   (CPU_REG_D, ADDR_OFF_BANK_WRAP),
            ADDR_OFFR   (CPU_REG_X, ADDR_OFF_BANK_WRAP),
            IO,
            MOV_L       (MOV_LSB, CPU_REG_ADDR, CPU_REG_ZERO, CPU_REG_TEMP),
            ADDR_OFFI   (1, ADDR_OFF_BANK_WRAP),
            MOV_L       (MOV_MSB, CPU_REG_ADDR, CPU_REG_ZERO, CPU_REG_TEMP),
            MOV_RRW     (CPU_REG_TEMP, CPU_REG_ADDR, MOV_RRW_WORD),
            MOV_RRW     (CPU_REG_DBR, CPU_REG_BANK, MOV_RRW_BYTE),
            MOV_S       (MOV_LSB, CPU_REG_ADDR, CPU_REG_BANK, CPU_REG_ACCUM),
            SKIPS       (2, CPU_STATUS_FLAG_M),
            ADDR_OFFI   (1, ADDR_OFF_BANK_NEXT),
            MOV_S       (MOV_MSB, CPU_REG_ADDR, CPU_REG_BANK, CPU_REG_ACCUM),
        }
    },

    [CPU_OPCODE_STA_DIR_IND_Y] = {
        .uops = {
            MOV_LPC     (MOV_LSB, CPU_REG_ADDR),
            ZEXT        (CPU_REG_ADDR),
            SKIPC       (1, CPU_STATUS_FLAG_DL),
            IO          /* DL != 0 */,
            ADDR_OFFR   (CPU_REG_D, ADDR_OFF_BANK_WRAP),
            MOV_RRW     (CPU_REG_ZERO, CPU_REG_BANK, MOV_RRW_BYTE),
            MOV_L       (MOV_LSB, CPU_REG_ADDR, CPU_REG_BANK, CPU_REG_TEMP),
            ADDR_OFFI   (1, ADDR_OFF_BANK_NEXT),
            MOV_L       (MOV_MSB, CPU_REG_ADDR, CPU_REG_BANK, CPU_REG_TEMP),
            MOV_RRW     (CPU_REG_TEMP, CPU_REG_ADDR, MOV_RRW_WORD),
            MOV_RRW     (CPU_REG_DBR, CPU_REG_BANK, MOV_RRW_BYTE),
            ADDR_OFFR   (CPU_REG_Y, ADDR_OFF_BANK_NEXT),
            SKIPS       (2, CPU_STATUS_FLAG_X),
            SKIPC       (1, CPU_STATUS_FLAG_PAGE),
            IO          /* X = 0 or page crossed */,
            MOV_S       (MOV_LSB, CPU_REG_ADDR, CPU_REG_BANK, CPU_REG_ACCUM),
            SKIPS       (2, CPU_STATUS_FLAG_M),
            ADDR_OFFI   (1, ADDR_OFF_BANK_NEXT),
            MOV_S       (MOV_MSB, CPU_REG_ADDR, CPU_REG_BANK, CPU_REG_ACCUM),
        }
    },

    [CPU_OPCODE_STA_DIR_INDL_Y] = {
        .uops = {
            MOV_LPC     (MOV_LSB, CPU_REG_ADDR),
            ZEXT        (CPU_REG_ADDR),
            ADDR_OFFR   (CPU_REG_D, 0),
            SKIPC       (1, CPU_STATUS_FLAG_DL),
            IO          /* DL != 0 */,
            MOV_RRW     (CPU_REG_ZERO, CPU_REG_BANK, MOV_RRW_BYTE),
            MOV_L       (MOV_LSB, CPU_REG_ADDR, CPU_REG_BANK, CPU_REG_TEMP),
            ADDR_OFFI   (1, ADDR_OFF_BANK_NEXT),
            MOV_L       (MOV_MSB, CPU_REG_ADDR, CPU_REG_BANK, CPU_REG_TEMP),
            ADDR_OFFI   (1, ADDR_OFF_BANK_NEXT),
            MOV_L       (MOV_LSB, CPU_REG_ADDR, CPU_REG_BANK, CPU_REG_BANK),
            MOV_RRW     (CPU_REG_TEMP, CPU_REG_ADDR, MOV_RRW_WORD),
            // MOV_RRW     (CPU_REG_DBR, CPU_REG_BANK, MOV_RRW_BYTE),
            ADDR_OFFR   (CPU_REG_Y, ADDR_OFF_BANK_NEXT),
            MOV_S       (MOV_LSB, CPU_REG_ADDR, CPU_REG_BANK, CPU_REG_ACCUM),
            SKIPS       (2, CPU_STATUS_FLAG_M),
            ADDR_OFFI   (1, ADDR_OFF_BANK_NEXT),
            MOV_S       (MOV_MSB, CPU_REG_ADDR, CPU_REG_BANK, CPU_REG_ACCUM),
        }
    },

    /**************************************************************************************/
    /*                                      STX                                           */
    /**************************************************************************************/

    [CPU_OPCODE_STP_IMP] = {
        .uops = {
            IO,
            IO,
            STP,
        }
    },

    /**************************************************************************************/
    /*                                      STX                                           */
    /**************************************************************************************/

    [CPU_OPCODE_STX_ABS] = {
        .uops = {
            MOV_LPC     (MOV_LSB, CPU_REG_ADDR),
            MOV_LPC     (MOV_MSB, CPU_REG_ADDR),
            MOV_RRW     (CPU_REG_DBR, CPU_REG_BANK, MOV_RRW_BYTE),
            MOV_S       (MOV_LSB, CPU_REG_ADDR, CPU_REG_BANK, CPU_REG_X),
            SKIPS       (2, CPU_STATUS_FLAG_X),
            ADDR_OFFI   (1, ADDR_OFF_BANK_NEXT),
            MOV_S       (MOV_MSB, CPU_REG_ADDR, CPU_REG_BANK, CPU_REG_X),
        }
    },
    [CPU_OPCODE_STX_DIR] = {
        .uops = {
            MOV_LPC     (MOV_LSB, CPU_REG_ADDR),
            ZEXT        (CPU_REG_ADDR),
            ADDR_OFFR   (CPU_REG_D, ADDR_OFF_BANK_WRAP),
            SKIPC       (1, CPU_STATUS_FLAG_DL),
            IO          /* DL != 0  */,
            MOV_RRW     (CPU_REG_ZERO, CPU_REG_BANK, MOV_RRW_BYTE),
            MOV_S       (MOV_LSB, CPU_REG_ADDR, CPU_REG_BANK, CPU_REG_X),
            SKIPS       (2, CPU_STATUS_FLAG_X),
            ADDR_OFFI   (1, ADDR_OFF_BANK_NEXT),
            MOV_S       (MOV_MSB, CPU_REG_ADDR, CPU_REG_BANK, CPU_REG_X),
        }
    },
    [CPU_OPCODE_STX_DIR_Y] = {
        .uops = {
            MOV_LPC     (MOV_LSB, CPU_REG_ADDR),
            ZEXT        (CPU_REG_ADDR),
            ADDR_OFFR   (CPU_REG_D, ADDR_OFF_BANK_WRAP),
            ADDR_OFFR   (CPU_REG_Y, ADDR_OFF_BANK_WRAP),
            SKIPC       (1, CPU_STATUS_FLAG_DL),
            IO          /* DL != 0 */,
            IO,
            MOV_S       (MOV_LSB, CPU_REG_ADDR, CPU_REG_ZERO, CPU_REG_X),
            SKIPS       (2, CPU_STATUS_FLAG_X),
            ADDR_OFFI   (1, ADDR_OFF_BANK_WRAP),
            MOV_S       (MOV_MSB, CPU_REG_ADDR, CPU_REG_ZERO, CPU_REG_X),
        }
    },

    /**************************************************************************************/
    /*                                      STY                                           */
    /**************************************************************************************/

    [CPU_OPCODE_STY_ABS] = {
        .uops = {
            MOV_LPC     (MOV_LSB, CPU_REG_ADDR),
            MOV_LPC     (MOV_MSB, CPU_REG_ADDR),
            MOV_RRW     (CPU_REG_DBR, CPU_REG_BANK, MOV_RRW_BYTE),
            MOV_S       (MOV_LSB, CPU_REG_ADDR, CPU_REG_BANK, CPU_REG_Y),
            SKIPS       (2, CPU_STATUS_FLAG_X),
            ADDR_OFFI   (1, ADDR_OFF_BANK_NEXT),
            MOV_S       (MOV_MSB, CPU_REG_ADDR, CPU_REG_BANK, CPU_REG_Y),
        }
    },
    [CPU_OPCODE_STY_DIR] = {
        .uops = {
            MOV_LPC     (MOV_LSB, CPU_REG_ADDR),
            ZEXT        (CPU_REG_ADDR),
            ADDR_OFFR   (CPU_REG_D, ADDR_OFF_BANK_WRAP),
            SKIPC       (1, CPU_STATUS_FLAG_DL),
            IO          /* DL !=0  */,
            MOV_RRW     (CPU_REG_ZERO, CPU_REG_BANK, MOV_RRW_BYTE),
            MOV_S       (MOV_LSB, CPU_REG_ADDR, CPU_REG_BANK, CPU_REG_Y),
            SKIPS       (2, CPU_STATUS_FLAG_X),
            ADDR_OFFI   (1, ADDR_OFF_BANK_NEXT),
            MOV_S       (MOV_MSB, CPU_REG_ADDR, CPU_REG_BANK, CPU_REG_Y),
        }
    },
    [CPU_OPCODE_STY_DIR_X] = {
        .uops = {
            MOV_LPC     (MOV_LSB, CPU_REG_ADDR),
            ZEXT        (CPU_REG_ADDR),
            ADDR_OFFR   (CPU_REG_D, ADDR_OFF_BANK_WRAP),
            ADDR_OFFR   (CPU_REG_X, ADDR_OFF_BANK_WRAP),
            SKIPC       (1, CPU_STATUS_FLAG_DL),
            IO          /* DL != 0 */,
            IO,
            MOV_S       (MOV_LSB, CPU_REG_ADDR, CPU_REG_ZERO, CPU_REG_Y),
            SKIPS       (2, CPU_STATUS_FLAG_X),
            ADDR_OFFI   (1, ADDR_OFF_BANK_WRAP),
            MOV_S       (MOV_MSB, CPU_REG_ADDR, CPU_REG_ZERO, CPU_REG_Y),
        }
    },

    /**************************************************************************************/
    /*                                      STZ                                           */
    /**************************************************************************************/

    [CPU_OPCODE_STZ_ABS] = {
        .uops = {
            MOV_LPC     (MOV_LSB, CPU_REG_ADDR),
            MOV_LPC     (MOV_MSB, CPU_REG_ADDR),
            MOV_RRW     (CPU_REG_DBR, CPU_REG_BANK, MOV_RRW_BYTE),
            MOV_S       (MOV_LSB, CPU_REG_ADDR, CPU_REG_BANK, CPU_REG_ZERO),
            SKIPS       (2, CPU_STATUS_FLAG_M),
            ADDR_OFFI   (1, ADDR_OFF_BANK_NEXT),
            MOV_S       (MOV_MSB, CPU_REG_ADDR, CPU_REG_BANK, CPU_REG_ZERO),
        }
    },

    [CPU_OPCODE_STZ_ABS_X] = {
        .uops = {
            MOV_LPC     (MOV_LSB, CPU_REG_ADDR),
            MOV_LPC     (MOV_MSB, CPU_REG_ADDR),
            MOV_RRW     (CPU_REG_DBR, CPU_REG_BANK, MOV_RRW_BYTE),
            ADDR_OFFR   (CPU_REG_X, ADDR_OFF_BANK_NEXT),
            IO          /* extra cycle due to write */,
            MOV_S       (MOV_LSB, CPU_REG_ADDR, CPU_REG_BANK, CPU_REG_ZERO),
            SKIPS       (2, CPU_STATUS_FLAG_M),
            ADDR_OFFI   (1, ADDR_OFF_BANK_NEXT),
            MOV_S       (MOV_MSB, CPU_REG_ADDR, CPU_REG_BANK, CPU_REG_ZERO),
        }
    },

    [CPU_OPCODE_STZ_DIR] = {
        .uops = {
            MOV_LPC     (MOV_LSB, CPU_REG_ADDR),
            ZEXT        (CPU_REG_ADDR),
            ADDR_OFFR   (CPU_REG_D, ADDR_OFF_BANK_WRAP),
            SKIPC       (1, CPU_STATUS_FLAG_DL),
            IO          /* DL !=0  */,
            MOV_RRW     (CPU_REG_ZERO, CPU_REG_BANK, MOV_RRW_BYTE),
            MOV_S       (MOV_LSB, CPU_REG_ADDR, CPU_REG_BANK, CPU_REG_ZERO),
            SKIPS       (2, CPU_STATUS_FLAG_M),
            ADDR_OFFI   (1, ADDR_OFF_BANK_NEXT),
            MOV_S       (MOV_MSB, CPU_REG_ADDR, CPU_REG_BANK, CPU_REG_ZERO),
        }
    },

    [CPU_OPCODE_STZ_DIR_X] = {
        .uops = {
            MOV_LPC     (MOV_LSB, CPU_REG_ADDR),
            ZEXT        (CPU_REG_ADDR),
            ADDR_OFFR   (CPU_REG_D, ADDR_OFF_BANK_WRAP),
            ADDR_OFFR   (CPU_REG_X, ADDR_OFF_BANK_WRAP),
            SKIPC       (1, CPU_STATUS_FLAG_DL),
            IO          /* DL != 0 */,
            IO,
            MOV_S       (MOV_LSB, CPU_REG_ADDR, CPU_REG_ZERO, CPU_REG_ZERO),
            SKIPS       (2, CPU_STATUS_FLAG_M),
            ADDR_OFFI   (1, ADDR_OFF_BANK_WRAP),
            MOV_S       (MOV_MSB, CPU_REG_ADDR, CPU_REG_ZERO, CPU_REG_ZERO),
        }
    },

    /**************************************************************************************/
    /*                                      TAX                                           */
    /**************************************************************************************/


    [CPU_OPCODE_TAX_IMP] = {
        .uops = {
            IO,
            MOV_RR      (CPU_REG_ACCUM, CPU_REG_X),
            CHK_ZN      (CPU_REG_X),
        }
    },

    /**************************************************************************************/
    /*                                      TAY                                           */
    /**************************************************************************************/

    [CPU_OPCODE_TAY_IMP] = {
        .uops = {
            IO,
            MOV_RR      (CPU_REG_ACCUM, CPU_REG_Y),
            CHK_ZN      (CPU_REG_Y),
        }
    },

    /**************************************************************************************/
    /*                                      TCD                                           */
    /**************************************************************************************/

    [CPU_OPCODE_TCD_IMP] = {
        .uops = {
            IO,
            MOV_RRW     (CPU_REG_ACCUM, CPU_REG_D, MOV_RRW_WORD),
            CHK_ZNW     (CPU_REG_D, CHK_ZW_WORD),
        }
    },

    /**************************************************************************************/
    /*                                      TRB                                           */
    /**************************************************************************************/

    [CPU_OPCODE_TRB_ABS] = {
        .uops = {
            MOV_LPC     (MOV_LSB, CPU_REG_ADDR),
            MOV_LPC     (MOV_MSB, CPU_REG_ADDR),
            MOV_RRW     (CPU_REG_DBR, CPU_REG_BANK, MOV_RRW_BYTE),
            MOV_L       (MOV_LSB, CPU_REG_ADDR, CPU_REG_BANK, CPU_REG_TEMP),
            SKIPS       (2, CPU_STATUS_FLAG_M),
            ADDR_OFFI   (1, ADDR_OFF_BANK_NEXT),
            MOV_L       (MOV_MSB, CPU_REG_ADDR, CPU_REG_BANK, CPU_REG_TEMP),
            ALU_OP      (ALU_OP_TRB, CPU_STATUS_FLAG_M, CPU_REG_TEMP, CPU_REG_ZERO),
            IO,
            SKIPS       (2, CPU_STATUS_FLAG_M),
            MOV_S       (MOV_MSB, CPU_REG_ADDR, CPU_REG_BANK, CPU_REG_TEMP),
            ADDR_OFFIS  (0xffff, ADDR_OFF_BANK_NEXT, ADDR_OFF_SIGNED),
            MOV_S       (MOV_LSB, CPU_REG_ADDR, CPU_REG_BANK, CPU_REG_TEMP)
        }
    },

    [CPU_OPCODE_TRB_DIR] = {
        .uops = {
            MOV_LPC     (MOV_LSB, CPU_REG_ADDR),
            ZEXT        (CPU_REG_ADDR),
            ADDR_OFFR   (CPU_REG_D, ADDR_OFF_BANK_WRAP),
            MOV_RRW     (CPU_REG_ZERO, CPU_REG_BANK, MOV_RRW_BYTE),
            MOV_L       (MOV_LSB, CPU_REG_ADDR, CPU_REG_BANK, CPU_REG_TEMP),
            SKIPS       (2, CPU_STATUS_FLAG_M),
            ADDR_OFFI   (1, ADDR_OFF_BANK_NEXT),
            MOV_L       (MOV_MSB, CPU_REG_ADDR, CPU_REG_BANK, CPU_REG_TEMP),
            ALU_OP      (ALU_OP_TRB, CPU_STATUS_FLAG_M, CPU_REG_TEMP, CPU_REG_ZERO),
            IO,
            SKIPS       (2, CPU_STATUS_FLAG_M),
            MOV_S       (MOV_MSB, CPU_REG_ADDR, CPU_REG_BANK, CPU_REG_TEMP),
            ADDR_OFFIS  (0xffff, ADDR_OFF_BANK_NEXT, ADDR_OFF_SIGNED),
            MOV_S       (MOV_LSB, CPU_REG_ADDR, CPU_REG_BANK, CPU_REG_TEMP)
        }
    },

    /**************************************************************************************/
    /*                                      TSB                                           */
    /**************************************************************************************/

    [CPU_OPCODE_TSB_ABS] = {
        .uops = {
            MOV_LPC     (MOV_LSB, CPU_REG_ADDR),
            MOV_LPC     (MOV_MSB, CPU_REG_ADDR),
            MOV_RRW     (CPU_REG_DBR, CPU_REG_BANK, MOV_RRW_BYTE),
            MOV_L       (MOV_LSB, CPU_REG_ADDR, CPU_REG_BANK, CPU_REG_TEMP),
            SKIPS       (2, CPU_STATUS_FLAG_M),
            ADDR_OFFI   (1, ADDR_OFF_BANK_NEXT),
            MOV_L       (MOV_MSB, CPU_REG_ADDR, CPU_REG_BANK, CPU_REG_TEMP),
            ALU_OP      (ALU_OP_TSB, CPU_STATUS_FLAG_M, CPU_REG_TEMP, CPU_REG_ZERO),
            IO,
            SKIPS       (2, CPU_STATUS_FLAG_M),
            MOV_S       (MOV_MSB, CPU_REG_ADDR, CPU_REG_BANK, CPU_REG_TEMP),
            ADDR_OFFIS  (0xffff, ADDR_OFF_BANK_NEXT, ADDR_OFF_SIGNED),
            MOV_S       (MOV_LSB, CPU_REG_ADDR, CPU_REG_BANK, CPU_REG_TEMP)
        }
    },

    [CPU_OPCODE_TSB_DIR] = {
        .uops = {
            MOV_LPC     (MOV_LSB, CPU_REG_ADDR),
            ZEXT        (CPU_REG_ADDR),
            ADDR_OFFR   (CPU_REG_D, ADDR_OFF_BANK_WRAP),
            MOV_RRW     (CPU_REG_ZERO, CPU_REG_BANK, MOV_RRW_BYTE),
            MOV_L       (MOV_LSB, CPU_REG_ADDR, CPU_REG_BANK, CPU_REG_TEMP),
            SKIPS       (2, CPU_STATUS_FLAG_M),
            ADDR_OFFI   (1, ADDR_OFF_BANK_NEXT),
            MOV_L       (MOV_MSB, CPU_REG_ADDR, CPU_REG_BANK, CPU_REG_TEMP),
            ALU_OP      (ALU_OP_TSB, CPU_STATUS_FLAG_M, CPU_REG_TEMP, CPU_REG_ZERO),
            IO,
            SKIPS       (2, CPU_STATUS_FLAG_M),
            MOV_S       (MOV_MSB, CPU_REG_ADDR, CPU_REG_BANK, CPU_REG_TEMP),
            ADDR_OFFIS  (0xffff, ADDR_OFF_BANK_NEXT, ADDR_OFF_SIGNED),
            MOV_S       (MOV_LSB, CPU_REG_ADDR, CPU_REG_BANK, CPU_REG_TEMP)
        }
    },

    /**************************************************************************************/
    /*                                      TCS                                           */
    /**************************************************************************************/

    [CPU_OPCODE_TCS_IMP] = {
        .uops = {
            IO,
            MOV_RRW     (CPU_REG_ACCUM, CPU_REG_S, MOV_RRW_WORD),
        }
    },

    /**************************************************************************************/
    /*                                      TDC                                           */
    /**************************************************************************************/

    [CPU_OPCODE_TDC_IMP] = {
        .uops = {
            IO,
            MOV_RRW     (CPU_REG_D, CPU_REG_ACCUM, MOV_RRW_WORD),
            CHK_ZN      (CPU_REG_ACCUM),
        }
    },

    /**************************************************************************************/
    /*                                      TSC                                           */
    /**************************************************************************************/

    [CPU_OPCODE_TSC_ACC] = {
        .uops = {
            IO,
            MOV_RRW     (CPU_REG_S, CPU_REG_ACCUM, MOV_RRW_WORD),
            CHK_ZN      (CPU_REG_ACCUM),
        }
    },

    /**************************************************************************************/
    /*                                      TSX                                           */
    /**************************************************************************************/

    [CPU_OPCODE_TSX_ACC] = {
        .uops = {
            IO,
            MOV_RR      (CPU_REG_S, CPU_REG_X),
            CHK_ZN      (CPU_REG_X),
        }
    },

    /**************************************************************************************/
    /*                                      TXA                                           */
    /**************************************************************************************/

    [CPU_OPCODE_TXA_ACC] = {
        .uops = {
            IO,
            MOV_RR      (CPU_REG_X, CPU_REG_ACCUM),
            CHK_ZN      (CPU_REG_ACCUM),
        }
    },

    /**************************************************************************************/
    /*                                      TXS                                           */
    /**************************************************************************************/

    [CPU_OPCODE_TXS_ACC] = {
        .uops = {
            IO,
            MOV_RRW     (CPU_REG_X, CPU_REG_S, MOV_RRW_WORD),
        }
    },

    /**************************************************************************************/
    /*                                      TXY                                           */
    /**************************************************************************************/

    [CPU_OPCODE_TXY_ACC] = {
        .uops = {
            IO,
            MOV_RR      (CPU_REG_X, CPU_REG_Y),
            CHK_ZN      (CPU_REG_Y),
        }
    },

    /**************************************************************************************/
    /*                                      TYA                                           */
    /**************************************************************************************/

    [CPU_OPCODE_TYA_ACC] = {
        .uops = {
            IO,
            MOV_RR      (CPU_REG_Y, CPU_REG_ACCUM),
            CHK_ZN      (CPU_REG_ACCUM),
        }
    },

    /**************************************************************************************/
    /*                                      TYX                                           */
    /**************************************************************************************/

    [CPU_OPCODE_TYX_ACC] = {
        .uops = {
            IO,
            MOV_RR      (CPU_REG_Y, CPU_REG_X),
            CHK_ZN      (CPU_REG_X),
        }
    },

    /**************************************************************************************/
    /*                                      WAI                                           */
    /**************************************************************************************/

    [CPU_OPCODE_WAI_ACC] = {
        .uops = {
            IO,
            IO,
            WAI
        }
    },

    /**************************************************************************************/
    /*                                      WDM                                           */
    /**************************************************************************************/

    [CPU_OPCODE_WDM_ACC] = {
        .uops = {
            // SKIPC(0, 0)
            // INC_PC(0)
            MOV_LPC(MOV_LSB, CPU_REG_ADDR),
        }
    },

    /**************************************************************************************/
    /*                                      XBA                                           */
    /**************************************************************************************/

    [CPU_OPCODE_XBA_ACC] = {
        .uops = {
            IO,
            IO,
            XBA,
            CHK_ZNW     (CPU_REG_ACCUM, CHK_ZW_BYTE)
        }
    },

    /**************************************************************************************/
    /*                                      XCE                                           */
    /**************************************************************************************/

    [CPU_OPCODE_XCE_ACC] = {
        .uops = {
            IO,
            XCE
        }
    },


    [CPU_OPCODE_FETCH] = {
        .uops = {
            MOV_LPC     (MOV_LSB, CPU_REG_INST),
            DECODE
        }
    },

    [CPU_OPCODE_INT_HW] = {
        .uops = {
            IO,
            SKIPS       (1, CPU_STATUS_FLAG_E),
            IO,
            MOV_S       (MOV_LSB, CPU_REG_S, CPU_REG_ZERO, CPU_REG_PBR),
            DECS,
            MOV_S       (MOV_MSB, CPU_REG_S, CPU_REG_ZERO, CPU_REG_PC),
            DECS,
            MOV_S       (MOV_LSB, CPU_REG_S, CPU_REG_ZERO, CPU_REG_PC),
            DECS,
            MOV_P       (CPU_REG_P, CPU_REG_TEMP),
            MOV_S       (MOV_LSB, CPU_REG_S, CPU_REG_ZERO, CPU_REG_TEMP),
            DECS,
            SET_P       (CPU_STATUS_FLAG_I),
            BRK,
            MOV_RRW     (CPU_REG_ADDR, CPU_REG_PC, MOV_RRW_WORD),
            MOV_RRW     (CPU_REG_ZERO, CPU_REG_PBR, MOV_RRW_BYTE),
            MOV_LPC     (MOV_LSB, CPU_REG_ADDR),
            MOV_LPC     (MOV_MSB, CPU_REG_ADDR),
            MOV_RRW     (CPU_REG_ADDR, CPU_REG_PC, MOV_RRW_WORD),
        }
    },
};

// struct opcode_t opcode_matrix[256] =
// {
//     [0x61] = OPCODE(CPU_OPCODE_ADC, OPCODE_CLASS_ALU, ADDRESS_MODE_DIRECT_INDEXED_INDIRECT),
//     [0x63] = OPCODE(CPU_OPCODE_ADC, OPCODE_CLASS_ALU, ADDRESS_MODE_STACK_RELATIVE),
//     [0x65] = OPCODE(CPU_OPCODE_ADC, OPCODE_CLASS_ALU, ADDRESS_MODE_DIRECT),
//     [0x67] = OPCODE(CPU_OPCODE_ADC, OPCODE_CLASS_ALU, ADDRESS_MODE_DIRECT_INDIRECT_LONG),
//     [0x69] = OPCODE(CPU_OPCODE_ADC, OPCODE_CLASS_ALU, ADDRESS_MODE_IMMEDIATE),
//     [0x6d] = OPCODE(CPU_OPCODE_ADC, OPCODE_CLASS_ALU, ADDRESS_MODE_ABSOLUTE),
//     [0x6f] = OPCODE(CPU_OPCODE_ADC, OPCODE_CLASS_ALU, ADDRESS_MODE_ABSOLUTE_LONG),
//     [0x71] = OPCODE(CPU_OPCODE_ADC, OPCODE_CLASS_ALU, ADDRESS_MODE_DIRECT_INDIRECT_INDEXED),
//     [0x72] = OPCODE(CPU_OPCODE_ADC, OPCODE_CLASS_ALU, ADDRESS_MODE_DIRECT_INDIRECT),
//     [0x73] = OPCODE(CPU_OPCODE_ADC, OPCODE_CLASS_ALU, ADDRESS_MODE_STACK_RELATIVE_INDIRECT_INDEXED),
//     [0x75] = OPCODE(CPU_OPCODE_ADC, OPCODE_CLASS_ALU, ADDRESS_MODE_DIRECT_INDEXED_X),
//     [0x77] = OPCODE(CPU_OPCODE_ADC, OPCODE_CLASS_ALU, ADDRESS_MODE_DIRECT_INDIRECT_LONG_INDEXED),
//     [0x79] = OPCODE(CPU_OPCODE_ADC, OPCODE_CLASS_ALU, ADDRESS_MODE_ABSOLUTE_INDEXED_Y),
//     [0x7d] = OPCODE(CPU_OPCODE_ADC, OPCODE_CLASS_ALU, ADDRESS_MODE_ABSOLUTE_INDEXED_X),
//     [0x7f] = OPCODE(CPU_OPCODE_ADC, OPCODE_CLASS_ALU, ADDRESS_MODE_ABSOLUTE_LONG_INDEXED_X),

//     [0x21] = OPCODE(CPU_OPCODE_AND, OPCODE_CLASS_ALU, ADDRESS_MODE_DIRECT_INDEXED_INDIRECT),
//     [0x23] = OPCODE(CPU_OPCODE_AND, OPCODE_CLASS_ALU, ADDRESS_MODE_STACK_RELATIVE),
//     [0x25] = OPCODE(CPU_OPCODE_AND, OPCODE_CLASS_ALU, ADDRESS_MODE_DIRECT),
//     [0x27] = OPCODE(CPU_OPCODE_AND, OPCODE_CLASS_ALU, ADDRESS_MODE_DIRECT_INDIRECT_LONG),
//     [0x29] = OPCODE(CPU_OPCODE_AND, OPCODE_CLASS_ALU, ADDRESS_MODE_IMMEDIATE),
//     [0x2d] = OPCODE(CPU_OPCODE_AND, OPCODE_CLASS_ALU, ADDRESS_MODE_ABSOLUTE),
//     [0x2f] = OPCODE(CPU_OPCODE_AND, OPCODE_CLASS_ALU, ADDRESS_MODE_ABSOLUTE_LONG),
//     [0x31] = OPCODE(CPU_OPCODE_AND, OPCODE_CLASS_ALU, ADDRESS_MODE_DIRECT_INDIRECT_INDEXED),
//     [0x32] = OPCODE(CPU_OPCODE_AND, OPCODE_CLASS_ALU, ADDRESS_MODE_DIRECT_INDIRECT),
//     [0x33] = OPCODE(CPU_OPCODE_AND, OPCODE_CLASS_ALU, ADDRESS_MODE_STACK_RELATIVE_INDIRECT_INDEXED),
//     [0x35] = OPCODE(CPU_OPCODE_AND, OPCODE_CLASS_ALU, ADDRESS_MODE_DIRECT_INDEXED_X),
//     [0x37] = OPCODE(CPU_OPCODE_AND, OPCODE_CLASS_ALU, ADDRESS_MODE_DIRECT_INDIRECT_LONG_INDEXED),
//     [0x39] = OPCODE(CPU_OPCODE_AND, OPCODE_CLASS_ALU, ADDRESS_MODE_ABSOLUTE_INDEXED_Y),
//     [0x3d] = OPCODE(CPU_OPCODE_AND, OPCODE_CLASS_ALU, ADDRESS_MODE_ABSOLUTE_INDEXED_X),
//     [0x3f] = OPCODE(CPU_OPCODE_AND, OPCODE_CLASS_ALU, ADDRESS_MODE_ABSOLUTE_LONG_INDEXED_X),

//     [0x06] = OPCODE(CPU_OPCODE_ASL, OPCODE_CLASS_ALU, ADDRESS_MODE_DIRECT),
//     [0x0a] = OPCODE(CPU_OPCODE_ASL, OPCODE_CLASS_ALU, ADDRESS_MODE_ACCUMULATOR),
//     [0x0e] = OPCODE(CPU_OPCODE_ASL, OPCODE_CLASS_ALU, ADDRESS_MODE_ABSOLUTE),
//     [0x16] = OPCODE(CPU_OPCODE_ASL, OPCODE_CLASS_ALU, ADDRESS_MODE_DIRECT_INDEXED_X),
//     [0x1e] = OPCODE(CPU_OPCODE_ASL, OPCODE_CLASS_ALU, ADDRESS_MODE_ABSOLUTE_INDEXED_X),

//     [0x10] = OPCODE(CPU_OPCODE_BPL, OPCODE_CLASS_BRANCH, ADDRESS_MODE_PROGRAM_COUNTER_RELATIVE),
//     [0x30] = OPCODE(CPU_OPCODE_BMI, OPCODE_CLASS_BRANCH, ADDRESS_MODE_PROGRAM_COUNTER_RELATIVE),
//     [0x50] = OPCODE(CPU_OPCODE_BVC, OPCODE_CLASS_BRANCH, ADDRESS_MODE_PROGRAM_COUNTER_RELATIVE),
//     [0x70] = OPCODE(CPU_OPCODE_BVS, OPCODE_CLASS_BRANCH, ADDRESS_MODE_PROGRAM_COUNTER_RELATIVE),
//     [0x80] = OPCODE(CPU_OPCODE_BRA, OPCODE_CLASS_BRANCH, ADDRESS_MODE_PROGRAM_COUNTER_RELATIVE),
//     [0x82] = OPCODE(CPU_OPCODE_BRL, OPCODE_CLASS_BRANCH, ADDRESS_MODE_PROGRAM_COUNTER_RELATIVE_LONG),
//     [0x90] = OPCODE(CPU_OPCODE_BCC, OPCODE_CLASS_BRANCH, ADDRESS_MODE_PROGRAM_COUNTER_RELATIVE),
//     [0xb0] = OPCODE(CPU_OPCODE_BCS, OPCODE_CLASS_BRANCH, ADDRESS_MODE_PROGRAM_COUNTER_RELATIVE),
//     [0xd0] = OPCODE(CPU_OPCODE_BNE, OPCODE_CLASS_BRANCH, ADDRESS_MODE_PROGRAM_COUNTER_RELATIVE),
//     [0xf0] = OPCODE(CPU_OPCODE_BEQ, OPCODE_CLASS_BRANCH, ADDRESS_MODE_PROGRAM_COUNTER_RELATIVE),

//     [0x24] = OPCODE(CPU_OPCODE_BIT, OPCODE_CLASS_ALU, ADDRESS_MODE_DIRECT),
//     [0x2c] = OPCODE(CPU_OPCODE_BIT, OPCODE_CLASS_ALU, ADDRESS_MODE_ABSOLUTE),
//     [0x34] = OPCODE(CPU_OPCODE_BIT, OPCODE_CLASS_ALU, ADDRESS_MODE_DIRECT_INDEXED_X),
//     [0x3c] = OPCODE(CPU_OPCODE_BIT, OPCODE_CLASS_ALU, ADDRESS_MODE_ABSOLUTE_INDEXED_X),
//     [0x89] = OPCODE(CPU_OPCODE_BIT, OPCODE_CLASS_ALU, ADDRESS_MODE_IMMEDIATE),

//     [0x00] = OPCODE(CPU_OPCODE_BRK, OPCODE_CLASS_STANDALONE, ADDRESS_MODE_STACK),

//     [0x18] = OPCODE(CPU_OPCODE_CLC, OPCODE_CLASS_STANDALONE, ADDRESS_MODE_IMPLIED),
//     [0x38] = OPCODE(CPU_OPCODE_SEC, OPCODE_CLASS_STANDALONE, ADDRESS_MODE_IMPLIED),
//     [0x58] = OPCODE(CPU_OPCODE_CLI, OPCODE_CLASS_STANDALONE, ADDRESS_MODE_IMPLIED),
//     [0x78] = OPCODE(CPU_OPCODE_SEI, OPCODE_CLASS_STANDALONE, ADDRESS_MODE_IMPLIED),
//     [0xb8] = OPCODE(CPU_OPCODE_CLV, OPCODE_CLASS_STANDALONE, ADDRESS_MODE_IMPLIED),
//     [0xd8] = OPCODE(CPU_OPCODE_CLD, OPCODE_CLASS_STANDALONE, ADDRESS_MODE_IMPLIED),
//     [0xf8] = OPCODE(CPU_OPCODE_SED, OPCODE_CLASS_STANDALONE, ADDRESS_MODE_IMPLIED),

//     [0xc2] = OPCODE(CPU_OPCODE_REP, OPCODE_CLASS_STANDALONE, ADDRESS_MODE_IMMEDIATE),
//     [0xe2] = OPCODE(CPU_OPCODE_SEP, OPCODE_CLASS_STANDALONE, ADDRESS_MODE_IMMEDIATE),

//     [0xc1] = OPCODE(CPU_OPCODE_CMP, OPCODE_CLASS_ALU, ADDRESS_MODE_DIRECT_INDEXED_INDIRECT),
//     [0xc3] = OPCODE(CPU_OPCODE_CMP, OPCODE_CLASS_ALU, ADDRESS_MODE_STACK_RELATIVE),
//     [0xc5] = OPCODE(CPU_OPCODE_CMP, OPCODE_CLASS_ALU, ADDRESS_MODE_DIRECT),
//     [0xc7] = OPCODE(CPU_OPCODE_CMP, OPCODE_CLASS_ALU, ADDRESS_MODE_DIRECT_INDIRECT_LONG),
//     [0xc9] = OPCODE(CPU_OPCODE_CMP, OPCODE_CLASS_ALU, ADDRESS_MODE_IMMEDIATE),
//     [0xcd] = OPCODE(CPU_OPCODE_CMP, OPCODE_CLASS_ALU, ADDRESS_MODE_ABSOLUTE),
//     [0xcf] = OPCODE(CPU_OPCODE_CMP, OPCODE_CLASS_ALU, ADDRESS_MODE_ABSOLUTE_LONG),
//     [0xd1] = OPCODE(CPU_OPCODE_CMP, OPCODE_CLASS_ALU, ADDRESS_MODE_DIRECT_INDIRECT_INDEXED),
//     [0xd2] = OPCODE(CPU_OPCODE_CMP, OPCODE_CLASS_ALU, ADDRESS_MODE_DIRECT_INDIRECT),
//     [0xd3] = OPCODE(CPU_OPCODE_CMP, OPCODE_CLASS_ALU, ADDRESS_MODE_STACK_RELATIVE_INDIRECT_INDEXED),
//     [0xd5] = OPCODE(CPU_OPCODE_CMP, OPCODE_CLASS_ALU, ADDRESS_MODE_DIRECT_INDEXED_X),
//     [0xd7] = OPCODE(CPU_OPCODE_CMP, OPCODE_CLASS_ALU, ADDRESS_MODE_DIRECT_INDIRECT_LONG_INDEXED),
//     [0xd9] = OPCODE(CPU_OPCODE_CMP, OPCODE_CLASS_ALU, ADDRESS_MODE_ABSOLUTE_INDEXED_Y),
//     [0xdd] = OPCODE(CPU_OPCODE_CMP, OPCODE_CLASS_ALU, ADDRESS_MODE_ABSOLUTE_INDEXED_X),
//     [0xdf] = OPCODE(CPU_OPCODE_CMP, OPCODE_CLASS_ALU, ADDRESS_MODE_ABSOLUTE_LONG_INDEXED_X),

//     [0x02] = OPCODE(CPU_OPCODE_COP, OPCODE_CLASS_STANDALONE, ADDRESS_MODE_STACK),

//     [0xe0] = OPCODE(CPU_OPCODE_CPX, OPCODE_CLASS_ALU, ADDRESS_MODE_IMMEDIATE),
//     [0xe4] = OPCODE(CPU_OPCODE_CPX, OPCODE_CLASS_ALU, ADDRESS_MODE_DIRECT),
//     [0xec] = OPCODE(CPU_OPCODE_CPX, OPCODE_CLASS_ALU, ADDRESS_MODE_ABSOLUTE),

//     [0xc0] = OPCODE(CPU_OPCODE_CPY, OPCODE_CLASS_ALU, ADDRESS_MODE_IMMEDIATE),
//     [0xc4] = OPCODE(CPU_OPCODE_CPY, OPCODE_CLASS_ALU, ADDRESS_MODE_DIRECT),
//     [0xcc] = OPCODE(CPU_OPCODE_CPY, OPCODE_CLASS_ALU, ADDRESS_MODE_ABSOLUTE),

//     [0x3a] = OPCODE(CPU_OPCODE_DEC, OPCODE_CLASS_ALU, ADDRESS_MODE_ACCUMULATOR),
//     [0xc6] = OPCODE(CPU_OPCODE_DEC, OPCODE_CLASS_ALU, ADDRESS_MODE_DIRECT),
//     [0xce] = OPCODE(CPU_OPCODE_DEC, OPCODE_CLASS_ALU, ADDRESS_MODE_ABSOLUTE),
//     [0xd6] = OPCODE(CPU_OPCODE_DEC, OPCODE_CLASS_ALU, ADDRESS_MODE_DIRECT_INDEXED_X),
//     [0xde] = OPCODE(CPU_OPCODE_DEC, OPCODE_CLASS_ALU, ADDRESS_MODE_ABSOLUTE_INDEXED_X),

//     [0xca] = OPCODE(CPU_OPCODE_DEX, OPCODE_CLASS_ALU, ADDRESS_MODE_IMPLIED),
//     [0x88] = OPCODE(CPU_OPCODE_DEY, OPCODE_CLASS_ALU, ADDRESS_MODE_IMPLIED),

//     [0x41] = OPCODE(CPU_OPCODE_EOR, OPCODE_CLASS_ALU, ADDRESS_MODE_DIRECT_INDEXED_INDIRECT),
//     [0x43] = OPCODE(CPU_OPCODE_EOR, OPCODE_CLASS_ALU, ADDRESS_MODE_STACK_RELATIVE),
//     [0x45] = OPCODE(CPU_OPCODE_EOR, OPCODE_CLASS_ALU, ADDRESS_MODE_DIRECT),
//     [0x47] = OPCODE(CPU_OPCODE_EOR, OPCODE_CLASS_ALU, ADDRESS_MODE_DIRECT_INDIRECT_LONG),
//     [0x49] = OPCODE(CPU_OPCODE_EOR, OPCODE_CLASS_ALU, ADDRESS_MODE_IMMEDIATE),
//     [0x4d] = OPCODE(CPU_OPCODE_EOR, OPCODE_CLASS_ALU, ADDRESS_MODE_ABSOLUTE),
//     [0x4f] = OPCODE(CPU_OPCODE_EOR, OPCODE_CLASS_ALU, ADDRESS_MODE_ABSOLUTE_LONG),
//     [0x51] = OPCODE(CPU_OPCODE_EOR, OPCODE_CLASS_ALU, ADDRESS_MODE_DIRECT_INDIRECT_INDEXED),
//     [0x52] = OPCODE(CPU_OPCODE_EOR, OPCODE_CLASS_ALU, ADDRESS_MODE_DIRECT_INDIRECT),
//     [0x53] = OPCODE(CPU_OPCODE_EOR, OPCODE_CLASS_ALU, ADDRESS_MODE_STACK_RELATIVE_INDIRECT_INDEXED),
//     [0x55] = OPCODE(CPU_OPCODE_EOR, OPCODE_CLASS_ALU, ADDRESS_MODE_DIRECT_INDEXED_X),
//     [0x57] = OPCODE(CPU_OPCODE_EOR, OPCODE_CLASS_ALU, ADDRESS_MODE_DIRECT_INDIRECT_LONG_INDEXED),
//     [0x59] = OPCODE(CPU_OPCODE_EOR, OPCODE_CLASS_ALU, ADDRESS_MODE_ABSOLUTE_INDEXED_Y),
//     [0x5d] = OPCODE(CPU_OPCODE_EOR, OPCODE_CLASS_ALU, ADDRESS_MODE_ABSOLUTE_INDEXED_X),
//     [0x5f] = OPCODE(CPU_OPCODE_EOR, OPCODE_CLASS_ALU, ADDRESS_MODE_ABSOLUTE_LONG_INDEXED_X),

//     [0x1a] = OPCODE(CPU_OPCODE_INC, OPCODE_CLASS_ALU, ADDRESS_MODE_ACCUMULATOR),
//     [0xe6] = OPCODE(CPU_OPCODE_INC, OPCODE_CLASS_ALU, ADDRESS_MODE_DIRECT),
//     [0xee] = OPCODE(CPU_OPCODE_INC, OPCODE_CLASS_ALU, ADDRESS_MODE_ABSOLUTE),
//     [0xf6] = OPCODE(CPU_OPCODE_INC, OPCODE_CLASS_ALU, ADDRESS_MODE_DIRECT_INDEXED_X),
//     [0xfe] = OPCODE(CPU_OPCODE_INC, OPCODE_CLASS_ALU, ADDRESS_MODE_ABSOLUTE_INDEXED_X),

//     [0xe8] = OPCODE(CPU_OPCODE_INX, OPCODE_CLASS_ALU, ADDRESS_MODE_IMPLIED),
//     [0xc8] = OPCODE(CPU_OPCODE_INY, OPCODE_CLASS_ALU, ADDRESS_MODE_IMPLIED),

//     [0x4c] = OPCODE(CPU_OPCODE_JMP, OPCODE_CLASS_BRANCH, ADDRESS_MODE_ABSOLUTE),
//     [0x5c] = OPCODE(CPU_OPCODE_JMP, OPCODE_CLASS_BRANCH, ADDRESS_MODE_ABSOLUTE_LONG),
//     [0x6c] = OPCODE(CPU_OPCODE_JMP, OPCODE_CLASS_BRANCH, ADDRESS_MODE_ABSOLUTE_INDIRECT),
//     [0x7c] = OPCODE(CPU_OPCODE_JMP, OPCODE_CLASS_BRANCH, ADDRESS_MODE_ABSOLUTE_INDEXED_INDIRECT),
//     [0xdc] = OPCODE(CPU_OPCODE_JML, OPCODE_CLASS_BRANCH, ADDRESS_MODE_ABSOLUTE_INDIRECT),

//     [0x22] = OPCODE(CPU_OPCODE_JSL, OPCODE_CLASS_BRANCH, ADDRESS_MODE_ABSOLUTE_LONG),

//     [0x20] = OPCODE(CPU_OPCODE_JSR, OPCODE_CLASS_BRANCH, ADDRESS_MODE_ABSOLUTE),
//     [0xfc] = OPCODE(CPU_OPCODE_JSR, OPCODE_CLASS_BRANCH, ADDRESS_MODE_ABSOLUTE_INDEXED_INDIRECT),

//     [0xa1] = OPCODE(CPU_OPCODE_LDA, OPCODE_CLASS_LOAD, ADDRESS_MODE_DIRECT_INDEXED_INDIRECT),
//     [0xa3] = OPCODE(CPU_OPCODE_LDA, OPCODE_CLASS_LOAD, ADDRESS_MODE_STACK_RELATIVE),
//     [0xa5] = OPCODE(CPU_OPCODE_LDA, OPCODE_CLASS_LOAD, ADDRESS_MODE_DIRECT),
//     [0xa7] = OPCODE(CPU_OPCODE_LDA, OPCODE_CLASS_LOAD, ADDRESS_MODE_DIRECT_INDIRECT_LONG),
//     [0xa9] = OPCODE(CPU_OPCODE_LDA, OPCODE_CLASS_LOAD, ADDRESS_MODE_IMMEDIATE),
//     [0xad] = OPCODE(CPU_OPCODE_LDA, OPCODE_CLASS_LOAD, ADDRESS_MODE_ABSOLUTE),
//     [0xaf] = OPCODE(CPU_OPCODE_LDA, OPCODE_CLASS_LOAD, ADDRESS_MODE_ABSOLUTE_LONG),
//     [0xb1] = OPCODE(CPU_OPCODE_LDA, OPCODE_CLASS_LOAD, ADDRESS_MODE_DIRECT_INDIRECT_INDEXED),
//     [0xb2] = OPCODE(CPU_OPCODE_LDA, OPCODE_CLASS_LOAD, ADDRESS_MODE_DIRECT_INDIRECT),
//     [0xb3] = OPCODE(CPU_OPCODE_LDA, OPCODE_CLASS_LOAD, ADDRESS_MODE_STACK_RELATIVE_INDIRECT_INDEXED),
//     [0xb5] = OPCODE(CPU_OPCODE_LDA, OPCODE_CLASS_LOAD, ADDRESS_MODE_DIRECT_INDEXED_X),
//     [0xb7] = OPCODE(CPU_OPCODE_LDA, OPCODE_CLASS_LOAD, ADDRESS_MODE_DIRECT_INDIRECT_LONG_INDEXED),
//     [0xb9] = OPCODE(CPU_OPCODE_LDA, OPCODE_CLASS_LOAD, ADDRESS_MODE_ABSOLUTE_INDEXED_Y),
//     [0xbd] = OPCODE(CPU_OPCODE_LDA, OPCODE_CLASS_LOAD, ADDRESS_MODE_ABSOLUTE_INDEXED_X),
//     [0xbf] = OPCODE(CPU_OPCODE_LDA, OPCODE_CLASS_LOAD, ADDRESS_MODE_ABSOLUTE_LONG_INDEXED_X),

//     [0xa2] = OPCODE(CPU_OPCODE_LDX, OPCODE_CLASS_LOAD, ADDRESS_MODE_IMMEDIATE),
//     [0xa6] = OPCODE(CPU_OPCODE_LDX, OPCODE_CLASS_LOAD, ADDRESS_MODE_DIRECT),
//     [0xae] = OPCODE(CPU_OPCODE_LDX, OPCODE_CLASS_LOAD, ADDRESS_MODE_ABSOLUTE),
//     [0xb6] = OPCODE(CPU_OPCODE_LDX, OPCODE_CLASS_LOAD, ADDRESS_MODE_DIRECT_INDEXED_Y),
//     [0xbe] = OPCODE(CPU_OPCODE_LDX, OPCODE_CLASS_LOAD, ADDRESS_MODE_ABSOLUTE_INDEXED_Y),

//     [0xa0] = OPCODE(CPU_OPCODE_LDY, OPCODE_CLASS_LOAD, ADDRESS_MODE_IMMEDIATE),
//     [0xa4] = OPCODE(CPU_OPCODE_LDY, OPCODE_CLASS_LOAD, ADDRESS_MODE_DIRECT),
//     [0xac] = OPCODE(CPU_OPCODE_LDY, OPCODE_CLASS_LOAD, ADDRESS_MODE_ABSOLUTE),
//     [0xb4] = OPCODE(CPU_OPCODE_LDY, OPCODE_CLASS_LOAD, ADDRESS_MODE_DIRECT_INDEXED_X),
//     [0xbc] = OPCODE(CPU_OPCODE_LDY, OPCODE_CLASS_LOAD, ADDRESS_MODE_ABSOLUTE_INDEXED_Y),

//     [0x46] = OPCODE(CPU_OPCODE_LSR, OPCODE_CLASS_ALU, ADDRESS_MODE_DIRECT),
//     [0x4a] = OPCODE(CPU_OPCODE_LSR, OPCODE_CLASS_ALU, ADDRESS_MODE_ACCUMULATOR),
//     [0x4e] = OPCODE(CPU_OPCODE_LSR, OPCODE_CLASS_ALU, ADDRESS_MODE_ABSOLUTE),
//     [0x56] = OPCODE(CPU_OPCODE_LSR, OPCODE_CLASS_ALU, ADDRESS_MODE_DIRECT_INDEXED_X),
//     [0x5e] = OPCODE(CPU_OPCODE_LSR, OPCODE_CLASS_ALU, ADDRESS_MODE_ABSOLUTE_INDEXED_X),

//     [0x54] = OPCODE(CPU_OPCODE_MVN, OPCODE_CLASS_STANDALONE, ADDRESS_MODE_BLOCK_MOVE),
//     [0x44] = OPCODE(CPU_OPCODE_MVP, OPCODE_CLASS_STANDALONE, ADDRESS_MODE_BLOCK_MOVE),

//     [0xea] = OPCODE(CPU_OPCODE_NOP, OPCODE_CLASS_STANDALONE, ADDRESS_MODE_IMPLIED),

//     [0x01] = OPCODE(CPU_OPCODE_ORA, OPCODE_CLASS_ALU, ADDRESS_MODE_DIRECT_INDEXED_INDIRECT),
//     [0x03] = OPCODE(CPU_OPCODE_ORA, OPCODE_CLASS_ALU, ADDRESS_MODE_STACK_RELATIVE),
//     [0x05] = OPCODE(CPU_OPCODE_ORA, OPCODE_CLASS_ALU, ADDRESS_MODE_DIRECT),
//     [0x07] = OPCODE(CPU_OPCODE_ORA, OPCODE_CLASS_ALU, ADDRESS_MODE_DIRECT_INDIRECT_LONG),
//     [0x09] = OPCODE(CPU_OPCODE_ORA, OPCODE_CLASS_ALU, ADDRESS_MODE_IMMEDIATE),
//     [0x0d] = OPCODE(CPU_OPCODE_ORA, OPCODE_CLASS_ALU, ADDRESS_MODE_ABSOLUTE),
//     [0x0f] = OPCODE(CPU_OPCODE_ORA, OPCODE_CLASS_ALU, ADDRESS_MODE_ABSOLUTE_LONG),
//     [0x11] = OPCODE(CPU_OPCODE_ORA, OPCODE_CLASS_ALU, ADDRESS_MODE_DIRECT_INDIRECT_INDEXED),
//     [0x12] = OPCODE(CPU_OPCODE_ORA, OPCODE_CLASS_ALU, ADDRESS_MODE_DIRECT_INDIRECT),
//     [0x13] = OPCODE(CPU_OPCODE_ORA, OPCODE_CLASS_ALU, ADDRESS_MODE_STACK_RELATIVE_INDIRECT_INDEXED),
//     [0x15] = OPCODE(CPU_OPCODE_ORA, OPCODE_CLASS_ALU, ADDRESS_MODE_DIRECT_INDEXED_X),
//     [0x17] = OPCODE(CPU_OPCODE_ORA, OPCODE_CLASS_ALU, ADDRESS_MODE_DIRECT_INDIRECT_LONG_INDEXED),
//     [0x19] = OPCODE(CPU_OPCODE_ORA, OPCODE_CLASS_ALU, ADDRESS_MODE_ABSOLUTE_INDEXED_Y),
//     [0x1d] = OPCODE(CPU_OPCODE_ORA, OPCODE_CLASS_ALU, ADDRESS_MODE_ABSOLUTE_INDEXED_X),
//     [0x1f] = OPCODE(CPU_OPCODE_ORA, OPCODE_CLASS_ALU, ADDRESS_MODE_ABSOLUTE_LONG_INDEXED_X),


//     [0xf4] = OPCODE(CPU_OPCODE_PEA, OPCODE_CLASS_STACK, ADDRESS_MODE_STACK),
//     [0xd4] = OPCODE(CPU_OPCODE_PEI, OPCODE_CLASS_STACK, ADDRESS_MODE_STACK),
//     [0x62] = OPCODE(CPU_OPCODE_PER, OPCODE_CLASS_STACK, ADDRESS_MODE_STACK),
//     [0x48] = OPCODE(CPU_OPCODE_PHA, OPCODE_CLASS_STACK, ADDRESS_MODE_STACK),
//     [0x8b] = OPCODE(CPU_OPCODE_PHB, OPCODE_CLASS_STACK, ADDRESS_MODE_STACK),
//     [0x0b] = OPCODE(CPU_OPCODE_PHD, OPCODE_CLASS_STACK, ADDRESS_MODE_STACK),
//     [0x4b] = OPCODE(CPU_OPCODE_PHK, OPCODE_CLASS_STACK, ADDRESS_MODE_STACK),
//     [0x08] = OPCODE(CPU_OPCODE_PHP, OPCODE_CLASS_STACK, ADDRESS_MODE_STACK),
//     [0xda] = OPCODE(CPU_OPCODE_PHX, OPCODE_CLASS_STACK, ADDRESS_MODE_STACK),
//     [0x5a] = OPCODE(CPU_OPCODE_PHY, OPCODE_CLASS_STACK, ADDRESS_MODE_STACK),
//     [0x68] = OPCODE(CPU_OPCODE_PLA, OPCODE_CLASS_STACK, ADDRESS_MODE_STACK),
//     [0xab] = OPCODE(CPU_OPCODE_PLB, OPCODE_CLASS_STACK, ADDRESS_MODE_STACK),
//     [0x2b] = OPCODE(CPU_OPCODE_PLD, OPCODE_CLASS_STACK, ADDRESS_MODE_STACK),
//     [0x28] = OPCODE(CPU_OPCODE_PLP, OPCODE_CLASS_STACK, ADDRESS_MODE_STACK),
//     [0xfa] = OPCODE(CPU_OPCODE_PLX, OPCODE_CLASS_STACK, ADDRESS_MODE_STACK),
//     [0x7a] = OPCODE(CPU_OPCODE_PLY, OPCODE_CLASS_STACK, ADDRESS_MODE_STACK),

//     [0x26] = OPCODE(CPU_OPCODE_ROL, OPCODE_CLASS_ALU, ADDRESS_MODE_DIRECT),
//     [0x2a] = OPCODE(CPU_OPCODE_ROL, OPCODE_CLASS_ALU, ADDRESS_MODE_ACCUMULATOR),
//     [0x2e] = OPCODE(CPU_OPCODE_ROL, OPCODE_CLASS_ALU, ADDRESS_MODE_ABSOLUTE),
//     [0x36] = OPCODE(CPU_OPCODE_ROL, OPCODE_CLASS_ALU, ADDRESS_MODE_DIRECT_INDEXED_X),
//     [0x3e] = OPCODE(CPU_OPCODE_ROL, OPCODE_CLASS_ALU, ADDRESS_MODE_ABSOLUTE_INDEXED_X),

//     [0x66] = OPCODE(CPU_OPCODE_ROR, OPCODE_CLASS_ALU, ADDRESS_MODE_DIRECT),
//     [0x6a] = OPCODE(CPU_OPCODE_ROR, OPCODE_CLASS_ALU, ADDRESS_MODE_ACCUMULATOR),
//     [0x6e] = OPCODE(CPU_OPCODE_ROR, OPCODE_CLASS_ALU, ADDRESS_MODE_ABSOLUTE),
//     [0x76] = OPCODE(CPU_OPCODE_ROR, OPCODE_CLASS_ALU, ADDRESS_MODE_DIRECT_INDEXED_X),
//     [0x7e] = OPCODE(CPU_OPCODE_ROR, OPCODE_CLASS_ALU, ADDRESS_MODE_ABSOLUTE_INDEXED_X),

//     [0x40] = OPCODE(CPU_OPCODE_RTI, OPCODE_CLASS_BRANCH, ADDRESS_MODE_STACK),
//     [0x6b] = OPCODE(CPU_OPCODE_RTL, OPCODE_CLASS_BRANCH, ADDRESS_MODE_STACK),
//     [0x60] = OPCODE(CPU_OPCODE_RTS, OPCODE_CLASS_BRANCH, ADDRESS_MODE_STACK),

//     [0xe1] = OPCODE(CPU_OPCODE_SBC, OPCODE_CLASS_ALU, ADDRESS_MODE_DIRECT_INDEXED_INDIRECT),
//     [0xe3] = OPCODE(CPU_OPCODE_SBC, OPCODE_CLASS_ALU, ADDRESS_MODE_STACK_RELATIVE),
//     [0xe5] = OPCODE(CPU_OPCODE_SBC, OPCODE_CLASS_ALU, ADDRESS_MODE_DIRECT),
//     [0xe7] = OPCODE(CPU_OPCODE_SBC, OPCODE_CLASS_ALU, ADDRESS_MODE_DIRECT_INDIRECT_LONG),
//     [0xe9] = OPCODE(CPU_OPCODE_SBC, OPCODE_CLASS_ALU, ADDRESS_MODE_IMMEDIATE),
//     [0xed] = OPCODE(CPU_OPCODE_SBC, OPCODE_CLASS_ALU, ADDRESS_MODE_ABSOLUTE),
//     [0xef] = OPCODE(CPU_OPCODE_SBC, OPCODE_CLASS_ALU, ADDRESS_MODE_ABSOLUTE_LONG),
//     [0xf1] = OPCODE(CPU_OPCODE_SBC, OPCODE_CLASS_ALU, ADDRESS_MODE_DIRECT_INDIRECT_INDEXED),
//     [0xf2] = OPCODE(CPU_OPCODE_SBC, OPCODE_CLASS_ALU, ADDRESS_MODE_DIRECT_INDIRECT),
//     [0xf3] = OPCODE(CPU_OPCODE_SBC, OPCODE_CLASS_ALU, ADDRESS_MODE_STACK_RELATIVE_INDIRECT_INDEXED),
//     [0xf5] = OPCODE(CPU_OPCODE_SBC, OPCODE_CLASS_ALU, ADDRESS_MODE_DIRECT_INDEXED_X),
//     [0xf7] = OPCODE(CPU_OPCODE_SBC, OPCODE_CLASS_ALU, ADDRESS_MODE_DIRECT_INDIRECT_LONG_INDEXED),
//     [0xf9] = OPCODE(CPU_OPCODE_SBC, OPCODE_CLASS_ALU, ADDRESS_MODE_ABSOLUTE_INDEXED_Y),
//     [0xfd] = OPCODE(CPU_OPCODE_SBC, OPCODE_CLASS_ALU, ADDRESS_MODE_ABSOLUTE_INDEXED_X),
//     [0xff] = OPCODE(CPU_OPCODE_SBC, OPCODE_CLASS_ALU, ADDRESS_MODE_ABSOLUTE_LONG_INDEXED_X),

//     [0xdb] = OPCODE(CPU_OPCODE_STP, OPCODE_CLASS_STANDALONE, ADDRESS_MODE_IMPLIED),

//     [0x81] = OPCODE(CPU_OPCODE_STA, OPCODE_CLASS_STORE, ADDRESS_MODE_DIRECT_INDEXED_INDIRECT),
//     [0x83] = OPCODE(CPU_OPCODE_STA, OPCODE_CLASS_STORE, ADDRESS_MODE_STACK_RELATIVE),
//     [0x85] = OPCODE(CPU_OPCODE_STA, OPCODE_CLASS_STORE, ADDRESS_MODE_DIRECT),
//     [0x87] = OPCODE(CPU_OPCODE_STA, OPCODE_CLASS_STORE, ADDRESS_MODE_DIRECT_INDIRECT_LONG),
//     [0x8d] = OPCODE(CPU_OPCODE_STA, OPCODE_CLASS_STORE, ADDRESS_MODE_ABSOLUTE),
//     [0x8f] = OPCODE(CPU_OPCODE_STA, OPCODE_CLASS_STORE, ADDRESS_MODE_ABSOLUTE_LONG),
//     [0x91] = OPCODE(CPU_OPCODE_STA, OPCODE_CLASS_STORE, ADDRESS_MODE_DIRECT_INDIRECT_INDEXED),
//     [0x92] = OPCODE(CPU_OPCODE_STA, OPCODE_CLASS_STORE, ADDRESS_MODE_DIRECT_INDIRECT),
//     [0x93] = OPCODE(CPU_OPCODE_STA, OPCODE_CLASS_STORE, ADDRESS_MODE_STACK_RELATIVE_INDIRECT_INDEXED),
//     [0x95] = OPCODE(CPU_OPCODE_STA, OPCODE_CLASS_STORE, ADDRESS_MODE_DIRECT_INDEXED_X),
//     [0x97] = OPCODE(CPU_OPCODE_STA, OPCODE_CLASS_STORE, ADDRESS_MODE_DIRECT_INDIRECT_LONG_INDEXED),
//     [0x99] = OPCODE(CPU_OPCODE_STA, OPCODE_CLASS_STORE, ADDRESS_MODE_ABSOLUTE_INDEXED_Y),
//     [0x9d] = OPCODE(CPU_OPCODE_STA, OPCODE_CLASS_STORE, ADDRESS_MODE_ABSOLUTE_INDEXED_X),
//     [0x9f] = OPCODE(CPU_OPCODE_STA, OPCODE_CLASS_STORE, ADDRESS_MODE_ABSOLUTE_LONG_INDEXED_X),

//     [0x86] = OPCODE(CPU_OPCODE_STX, OPCODE_CLASS_STORE, ADDRESS_MODE_DIRECT),
//     [0x8e] = OPCODE(CPU_OPCODE_STX, OPCODE_CLASS_STORE, ADDRESS_MODE_ABSOLUTE),
//     [0x96] = OPCODE(CPU_OPCODE_STX, OPCODE_CLASS_STORE, ADDRESS_MODE_DIRECT_INDEXED_Y),

//     [0x84] = OPCODE(CPU_OPCODE_STY, OPCODE_CLASS_STORE, ADDRESS_MODE_DIRECT),
//     [0x8c] = OPCODE(CPU_OPCODE_STY, OPCODE_CLASS_STORE, ADDRESS_MODE_ABSOLUTE),
//     [0x94] = OPCODE(CPU_OPCODE_STY, OPCODE_CLASS_STORE, ADDRESS_MODE_DIRECT_INDEXED_X),

//     [0x64] = OPCODE(CPU_OPCODE_STZ, OPCODE_CLASS_STORE, ADDRESS_MODE_DIRECT),
//     [0x74] = OPCODE(CPU_OPCODE_STZ, OPCODE_CLASS_STORE, ADDRESS_MODE_DIRECT_INDEXED_X),
//     [0x9c] = OPCODE(CPU_OPCODE_STZ, OPCODE_CLASS_STORE, ADDRESS_MODE_ABSOLUTE),
//     [0x9e] = OPCODE(CPU_OPCODE_STZ, OPCODE_CLASS_STORE, ADDRESS_MODE_ABSOLUTE_INDEXED_X),

//     [0xaa] = OPCODE(CPU_OPCODE_TAX, OPCODE_CLASS_TRANSFER, ADDRESS_MODE_IMPLIED),
//     [0xa8] = OPCODE(CPU_OPCODE_TAY, OPCODE_CLASS_TRANSFER, ADDRESS_MODE_IMPLIED),
//     [0x5b] = OPCODE(CPU_OPCODE_TCD, OPCODE_CLASS_TRANSFER, ADDRESS_MODE_IMPLIED),
//     [0x1b] = OPCODE(CPU_OPCODE_TCS, OPCODE_CLASS_TRANSFER, ADDRESS_MODE_IMPLIED),
//     [0x7b] = OPCODE(CPU_OPCODE_TDC, OPCODE_CLASS_TRANSFER, ADDRESS_MODE_IMPLIED),

//     [0x14] = OPCODE(CPU_OPCODE_TRB, OPCODE_CLASS_ALU, ADDRESS_MODE_DIRECT),
//     [0x1c] = OPCODE(CPU_OPCODE_TRB, OPCODE_CLASS_ALU, ADDRESS_MODE_ABSOLUTE),

//     [0x04] = OPCODE(CPU_OPCODE_TSB, OPCODE_CLASS_ALU, ADDRESS_MODE_DIRECT),
//     [0x0c] = OPCODE(CPU_OPCODE_TSB, OPCODE_CLASS_ALU, ADDRESS_MODE_ABSOLUTE),

//     [0x3b] = OPCODE(CPU_OPCODE_TSC, OPCODE_CLASS_TRANSFER, ADDRESS_MODE_ACCUMULATOR),
//     [0xba] = OPCODE(CPU_OPCODE_TSX, OPCODE_CLASS_TRANSFER, ADDRESS_MODE_ACCUMULATOR),
//     [0x8a] = OPCODE(CPU_OPCODE_TXA, OPCODE_CLASS_TRANSFER, ADDRESS_MODE_ACCUMULATOR),
//     [0x9a] = OPCODE(CPU_OPCODE_TXS, OPCODE_CLASS_TRANSFER, ADDRESS_MODE_ACCUMULATOR),
//     [0x9b] = OPCODE(CPU_OPCODE_TXY, OPCODE_CLASS_TRANSFER, ADDRESS_MODE_ACCUMULATOR),
//     [0x98] = OPCODE(CPU_OPCODE_TYA, OPCODE_CLASS_TRANSFER, ADDRESS_MODE_ACCUMULATOR),
//     [0xbb] = OPCODE(CPU_OPCODE_TYX, OPCODE_CLASS_TRANSFER, ADDRESS_MODE_ACCUMULATOR),
//     [0xcb] = OPCODE(CPU_OPCODE_WAI, OPCODE_CLASS_STANDALONE, ADDRESS_MODE_ACCUMULATOR),
//     [0x42] = OPCODE(CPU_OPCODE_WDM, OPCODE_CLASS_STANDALONE, ADDRESS_MODE_ACCUMULATOR),
//     [0xeb] = OPCODE(CPU_OPCODE_XBA, OPCODE_CLASS_STANDALONE, ADDRESS_MODE_ACCUMULATOR),
//     [0xfb] = OPCODE(CPU_OPCODE_XCE, OPCODE_CLASS_STANDALONE, ADDRESS_MODE_ACCUMULATOR)
// };

struct opcode_info_t opcode_info[] = {
    [CPU_OPCODE_ADC_IMM]           = {.opcode = CPU_INST_ADC, .addr_mode = ADDRESS_MODE_IMMEDIATE,                       .width[0] = 3, .width[1] = 2, .width_flag = CPU_STATUS_FLAG_M},
    [CPU_OPCODE_ADC_ABS]           = {.opcode = CPU_INST_ADC, .addr_mode = ADDRESS_MODE_ABSOLUTE,                        .width[0] = 3},
    [CPU_OPCODE_ADC_ABSL]          = {.opcode = CPU_INST_ADC, .addr_mode = ADDRESS_MODE_ABSOLUTE_LONG,                   .width[0] = 4},
    [CPU_OPCODE_ADC_DIR]           = {.opcode = CPU_INST_ADC, .addr_mode = ADDRESS_MODE_DIRECT,                          .width[0] = 2},
    [CPU_OPCODE_ADC_DIR_IND]       = {.opcode = CPU_INST_ADC, .addr_mode = ADDRESS_MODE_DIRECT_INDIRECT,                 .width[0] = 2},
    [CPU_OPCODE_ADC_DIR_INDL]      = {.opcode = CPU_INST_ADC, .addr_mode = ADDRESS_MODE_DIRECT_INDIRECT_LONG,            .width[0] = 2},
    [CPU_OPCODE_ADC_ABS_X]         = {.opcode = CPU_INST_ADC, .addr_mode = ADDRESS_MODE_ABSOLUTE_INDEXED_X,              .width[0] = 3},
    [CPU_OPCODE_ADC_ABSL_X]        = {.opcode = CPU_INST_ADC, .addr_mode = ADDRESS_MODE_ABSOLUTE_LONG_INDEXED_X,         .width[0] = 4},
    [CPU_OPCODE_ADC_ABS_Y]         = {.opcode = CPU_INST_ADC, .addr_mode = ADDRESS_MODE_ABSOLUTE_INDEXED_Y,              .width[0] = 3},
    [CPU_OPCODE_ADC_DIR_X]         = {.opcode = CPU_INST_ADC, .addr_mode = ADDRESS_MODE_DIRECT_INDEXED_X,                .width[0] = 2},
    [CPU_OPCODE_ADC_DIR_X_IND]     = {.opcode = CPU_INST_ADC, .addr_mode = ADDRESS_MODE_DIRECT_INDEXED_INDIRECT,         .width[0] = 2},
    [CPU_OPCODE_ADC_DIR_IND_Y]     = {.opcode = CPU_INST_ADC, .addr_mode = ADDRESS_MODE_DIRECT_INDIRECT_INDEXED,         .width[0] = 2},
    [CPU_OPCODE_ADC_DIR_INDL_Y]    = {.opcode = CPU_INST_ADC, .addr_mode = ADDRESS_MODE_DIRECT_INDIRECT_LONG_INDEXED,    .width[0] = 2},
    [CPU_OPCODE_ADC_S_REL]         = {.opcode = CPU_INST_ADC, .addr_mode = ADDRESS_MODE_STACK_RELATIVE,                  .width[0] = 2},
    [CPU_OPCODE_ADC_S_REL_IND_Y]   = {.opcode = CPU_INST_ADC, .addr_mode = ADDRESS_MODE_STACK_RELATIVE_INDIRECT_INDEXED, .width[0] = 2},

    [CPU_OPCODE_AND_IMM]           = {.opcode = CPU_INST_AND, .addr_mode = ADDRESS_MODE_IMMEDIATE,                       .width[0] = 3, .width[1] = 2, .width_flag = CPU_STATUS_FLAG_M},
    [CPU_OPCODE_AND_ABS]           = {.opcode = CPU_INST_AND, .addr_mode = ADDRESS_MODE_ABSOLUTE,                        .width[0] = 3},
    [CPU_OPCODE_AND_ABSL]          = {.opcode = CPU_INST_AND, .addr_mode = ADDRESS_MODE_ABSOLUTE_LONG,                   .width[0] = 4},
    [CPU_OPCODE_AND_DIR]           = {.opcode = CPU_INST_AND, .addr_mode = ADDRESS_MODE_DIRECT,                          .width[0] = 2},
    [CPU_OPCODE_AND_DIR_IND]       = {.opcode = CPU_INST_AND, .addr_mode = ADDRESS_MODE_DIRECT_INDIRECT,                 .width[0] = 2},
    [CPU_OPCODE_AND_DIR_INDL]      = {.opcode = CPU_INST_AND, .addr_mode = ADDRESS_MODE_DIRECT_INDIRECT_LONG,            .width[0] = 2},
    [CPU_OPCODE_AND_ABS_X]         = {.opcode = CPU_INST_AND, .addr_mode = ADDRESS_MODE_ABSOLUTE_INDEXED_X,              .width[0] = 3},
    [CPU_OPCODE_AND_ABSL_X]        = {.opcode = CPU_INST_AND, .addr_mode = ADDRESS_MODE_ABSOLUTE_LONG_INDEXED_X,         .width[0] = 4},
    [CPU_OPCODE_AND_ABS_Y]         = {.opcode = CPU_INST_AND, .addr_mode = ADDRESS_MODE_ABSOLUTE_INDEXED_Y,              .width[0] = 3},
    [CPU_OPCODE_AND_DIR_X]         = {.opcode = CPU_INST_AND, .addr_mode = ADDRESS_MODE_DIRECT_INDEXED_X,                .width[0] = 2},
    [CPU_OPCODE_AND_DIR_X_IND]     = {.opcode = CPU_INST_AND, .addr_mode = ADDRESS_MODE_DIRECT_INDEXED_INDIRECT,         .width[0] = 2},
    [CPU_OPCODE_AND_DIR_IND_Y]     = {.opcode = CPU_INST_AND, .addr_mode = ADDRESS_MODE_DIRECT_INDIRECT_INDEXED,         .width[0] = 2},
    [CPU_OPCODE_AND_DIR_INDL_Y]    = {.opcode = CPU_INST_AND, .addr_mode = ADDRESS_MODE_DIRECT_INDIRECT_LONG_INDEXED,    .width[0] = 2},
    [CPU_OPCODE_AND_S_REL]         = {.opcode = CPU_INST_AND, .addr_mode = ADDRESS_MODE_STACK_RELATIVE,                  .width[0] = 2},
    [CPU_OPCODE_AND_S_REL_IND_Y]   = {.opcode = CPU_INST_AND, .addr_mode = ADDRESS_MODE_STACK_RELATIVE_INDIRECT_INDEXED, .width[0] = 2},

    [CPU_OPCODE_ASL_ACC]           = {.opcode = CPU_INST_ASL, .addr_mode = ADDRESS_MODE_ACCUMULATOR,                     .width[0] = 1},
    [CPU_OPCODE_ASL_ABS]           = {.opcode = CPU_INST_ASL, .addr_mode = ADDRESS_MODE_ABSOLUTE,                        .width[0] = 3},
    [CPU_OPCODE_ASL_DIR]           = {.opcode = CPU_INST_ASL, .addr_mode = ADDRESS_MODE_DIRECT,                          .width[0] = 2},
    [CPU_OPCODE_ASL_ABS_X]         = {.opcode = CPU_INST_ASL, .addr_mode = ADDRESS_MODE_ABSOLUTE_INDEXED_X,              .width[0] = 3},  
    [CPU_OPCODE_ASL_DIR_X]         = {.opcode = CPU_INST_ASL, .addr_mode = ADDRESS_MODE_DIRECT_INDEXED_X,                .width[0] = 2},

    [CPU_OPCODE_BCC_PC_REL]        = {.opcode = CPU_INST_BCC, .addr_mode = ADDRESS_MODE_PROGRAM_COUNTER_RELATIVE,        .width[0] = 2},
    [CPU_OPCODE_BCS_PC_REL]        = {.opcode = CPU_INST_BCS, .addr_mode = ADDRESS_MODE_PROGRAM_COUNTER_RELATIVE,        .width[0] = 2},
    [CPU_OPCODE_BEQ_PC_REL]        = {.opcode = CPU_INST_BEQ, .addr_mode = ADDRESS_MODE_PROGRAM_COUNTER_RELATIVE,        .width[0] = 2},
    [CPU_OPCODE_BMI_PC_REL]        = {.opcode = CPU_INST_BMI, .addr_mode = ADDRESS_MODE_PROGRAM_COUNTER_RELATIVE,        .width[0] = 2},
    [CPU_OPCODE_BNE_PC_REL]        = {.opcode = CPU_INST_BNE, .addr_mode = ADDRESS_MODE_PROGRAM_COUNTER_RELATIVE,        .width[0] = 2},
    [CPU_OPCODE_BPL_PC_REL]        = {.opcode = CPU_INST_BPL, .addr_mode = ADDRESS_MODE_PROGRAM_COUNTER_RELATIVE,        .width[0] = 2},
    [CPU_OPCODE_BRA_PC_REL]        = {.opcode = CPU_INST_BRA, .addr_mode = ADDRESS_MODE_PROGRAM_COUNTER_RELATIVE,        .width[0] = 2},
    [CPU_OPCODE_BRL_PC_RELL]       = {.opcode = CPU_INST_BRL, .addr_mode = ADDRESS_MODE_PROGRAM_COUNTER_RELATIVE_LONG,   .width[0] = 3},
    [CPU_OPCODE_BVC_PC_REL]        = {.opcode = CPU_INST_BVC, .addr_mode = ADDRESS_MODE_PROGRAM_COUNTER_RELATIVE,        .width[0] = 2},
    [CPU_OPCODE_BVS_PC_REL]        = {.opcode = CPU_INST_BVS, .addr_mode = ADDRESS_MODE_PROGRAM_COUNTER_RELATIVE,        .width[0] = 2},

    [CPU_OPCODE_BIT_IMM]           = {.opcode = CPU_INST_BIT, .addr_mode = ADDRESS_MODE_IMMEDIATE,                       .width[0] = 3, .width[1] = 2, .width_flag = CPU_STATUS_FLAG_M},
    [CPU_OPCODE_BIT_ABS]           = {.opcode = CPU_INST_BIT, .addr_mode = ADDRESS_MODE_ABSOLUTE,                        .width[0] = 3},
    [CPU_OPCODE_BIT_DIR]           = {.opcode = CPU_INST_BIT, .addr_mode = ADDRESS_MODE_DIRECT,                          .width[0] = 2},
    [CPU_OPCODE_BIT_ABS_X]         = {.opcode = CPU_INST_BIT, .addr_mode = ADDRESS_MODE_ABSOLUTE_INDEXED_X,              .width[0] = 3},
    [CPU_OPCODE_BIT_DIR_X]         = {.opcode = CPU_INST_BIT, .addr_mode = ADDRESS_MODE_DIRECT_INDEXED_X,                .width[0] = 2},

    [CPU_OPCODE_BRK_S]             = {.opcode = CPU_INST_BRK, .addr_mode = ADDRESS_MODE_STACK,                           .width[0] = 2, .width[1] = 1, .width_flag = CPU_STATUS_FLAG_E},

    [CPU_OPCODE_CLC_IMP]           = {.opcode = CPU_INST_CLC, .addr_mode = ADDRESS_MODE_IMPLIED,                         .width[0] = 1},
    [CPU_OPCODE_CLD_IMP]           = {.opcode = CPU_INST_CLD, .addr_mode = ADDRESS_MODE_IMPLIED,                         .width[0] = 1},
    [CPU_OPCODE_CLI_IMP]           = {.opcode = CPU_INST_CLI, .addr_mode = ADDRESS_MODE_IMPLIED,                         .width[0] = 1},
    [CPU_OPCODE_CLV_IMP]           = {.opcode = CPU_INST_CLV, .addr_mode = ADDRESS_MODE_IMPLIED,                         .width[0] = 1},

    [CPU_OPCODE_CMP_IMM]           = {.opcode = CPU_INST_CMP, .addr_mode = ADDRESS_MODE_IMMEDIATE,                       .width[0] = 3, .width[1] = 2, .width_flag = CPU_STATUS_FLAG_M},
    [CPU_OPCODE_CMP_ABS]           = {.opcode = CPU_INST_CMP, .addr_mode = ADDRESS_MODE_ABSOLUTE,                        .width[0] = 3},
    [CPU_OPCODE_CMP_ABSL]          = {.opcode = CPU_INST_CMP, .addr_mode = ADDRESS_MODE_ABSOLUTE_LONG,                   .width[0] = 4},
    [CPU_OPCODE_CMP_DIR]           = {.opcode = CPU_INST_CMP, .addr_mode = ADDRESS_MODE_DIRECT,                          .width[0] = 2},
    [CPU_OPCODE_CMP_DIR_IND]       = {.opcode = CPU_INST_CMP, .addr_mode = ADDRESS_MODE_DIRECT_INDIRECT,                 .width[0] = 2},
    [CPU_OPCODE_CMP_DIR_INDL]      = {.opcode = CPU_INST_CMP, .addr_mode = ADDRESS_MODE_DIRECT_INDIRECT_LONG,            .width[0] = 2},
    [CPU_OPCODE_CMP_ABS_X]         = {.opcode = CPU_INST_CMP, .addr_mode = ADDRESS_MODE_ABSOLUTE_INDEXED_X,              .width[0] = 3},
    [CPU_OPCODE_CMP_ABSL_X]        = {.opcode = CPU_INST_CMP, .addr_mode = ADDRESS_MODE_ABSOLUTE_LONG_INDEXED_X,         .width[0] = 4},
    [CPU_OPCODE_CMP_ABS_Y]         = {.opcode = CPU_INST_CMP, .addr_mode = ADDRESS_MODE_ABSOLUTE_INDEXED_Y,              .width[0] = 3},
    [CPU_OPCODE_CMP_DIR_X]         = {.opcode = CPU_INST_CMP, .addr_mode = ADDRESS_MODE_DIRECT_INDEXED_X,                .width[0] = 2},
    [CPU_OPCODE_CMP_DIR_X_IND]     = {.opcode = CPU_INST_CMP, .addr_mode = ADDRESS_MODE_DIRECT_INDEXED_INDIRECT,         .width[0] = 2},
    [CPU_OPCODE_CMP_DIR_IND_Y]     = {.opcode = CPU_INST_CMP, .addr_mode = ADDRESS_MODE_DIRECT_INDIRECT_INDEXED,         .width[0] = 2},
    [CPU_OPCODE_CMP_DIR_INDL_Y]    = {.opcode = CPU_INST_CMP, .addr_mode = ADDRESS_MODE_DIRECT_INDIRECT_LONG_INDEXED,    .width[0] = 2},
    [CPU_OPCODE_CMP_S_REL]         = {.opcode = CPU_INST_CMP, .addr_mode = ADDRESS_MODE_STACK_RELATIVE,                  .width[0] = 2},
    [CPU_OPCODE_CMP_S_REL_IND_Y]   = {.opcode = CPU_INST_CMP, .addr_mode = ADDRESS_MODE_STACK_RELATIVE_INDIRECT_INDEXED, .width[0] = 2},

    [CPU_OPCODE_COP_S]             = {.opcode = CPU_INST_COP, .addr_mode = ADDRESS_MODE_STACK,                           .width[0] = 1},

    [CPU_OPCODE_CPX_IMM]           = {.opcode = CPU_INST_CPX, .addr_mode = ADDRESS_MODE_IMMEDIATE,                       .width[0] = 3, .width[1] = 2, .width_flag = CPU_STATUS_FLAG_X},
    [CPU_OPCODE_CPX_ABS]           = {.opcode = CPU_INST_CPX, .addr_mode = ADDRESS_MODE_ABSOLUTE,                        .width[0] = 3},
    [CPU_OPCODE_CPX_DIR]           = {.opcode = CPU_INST_CPX, .addr_mode = ADDRESS_MODE_DIRECT,                          .width[0] = 2},

    [CPU_OPCODE_CPY_IMM]           = {.opcode = CPU_INST_CPY, .addr_mode = ADDRESS_MODE_IMMEDIATE,                       .width[0] = 3, .width[1] = 2, .width_flag = CPU_STATUS_FLAG_X},
    [CPU_OPCODE_CPY_ABS]           = {.opcode = CPU_INST_CPY, .addr_mode = ADDRESS_MODE_ABSOLUTE,                        .width[0] = 3},
    [CPU_OPCODE_CPY_DIR]           = {.opcode = CPU_INST_CPY, .addr_mode = ADDRESS_MODE_DIRECT,                          .width[0] = 2},

    [CPU_OPCODE_DEC_ACC]           = {.opcode = CPU_INST_DEC, .addr_mode = ADDRESS_MODE_ACCUMULATOR,                     .width[0] = 1},
    [CPU_OPCODE_DEC_ABS]           = {.opcode = CPU_INST_DEC, .addr_mode = ADDRESS_MODE_ABSOLUTE,                        .width[0] = 3},
    [CPU_OPCODE_DEC_DIR]           = {.opcode = CPU_INST_DEC, .addr_mode = ADDRESS_MODE_DIRECT,                          .width[0] = 2},
    [CPU_OPCODE_DEC_ABS_X]         = {.opcode = CPU_INST_DEC, .addr_mode = ADDRESS_MODE_ABSOLUTE_INDEXED_X,              .width[0] = 3},
    [CPU_OPCODE_DEC_DIR_X]         = {.opcode = CPU_INST_DEC, .addr_mode = ADDRESS_MODE_DIRECT_INDEXED_X,                .width[0] = 2},

    [CPU_OPCODE_DEX_IMP]           = {.opcode = CPU_INST_DEX, .addr_mode = ADDRESS_MODE_IMPLIED,                         .width[0] = 1},
    [CPU_OPCODE_DEY_IMP]           = {.opcode = CPU_INST_DEY, .addr_mode = ADDRESS_MODE_IMPLIED,                         .width[0] = 1},

    [CPU_OPCODE_EOR_IMM]           = {.opcode = CPU_INST_EOR, .addr_mode = ADDRESS_MODE_IMMEDIATE,                       .width[0] = 3, .width[1] = 2, .width_flag = CPU_STATUS_FLAG_M},
    [CPU_OPCODE_EOR_ABS]           = {.opcode = CPU_INST_EOR, .addr_mode = ADDRESS_MODE_ABSOLUTE,                        .width[0] = 3},
    [CPU_OPCODE_EOR_ABSL]          = {.opcode = CPU_INST_EOR, .addr_mode = ADDRESS_MODE_ABSOLUTE_LONG,                   .width[0] = 4},
    [CPU_OPCODE_EOR_DIR]           = {.opcode = CPU_INST_EOR, .addr_mode = ADDRESS_MODE_DIRECT,                          .width[0] = 2},
    [CPU_OPCODE_EOR_DIR_IND]       = {.opcode = CPU_INST_EOR, .addr_mode = ADDRESS_MODE_DIRECT_INDIRECT,                 .width[0] = 2},
    [CPU_OPCODE_EOR_DIR_INDL]      = {.opcode = CPU_INST_EOR, .addr_mode = ADDRESS_MODE_DIRECT_INDIRECT_LONG,            .width[0] = 2},
    [CPU_OPCODE_EOR_ABS_X]         = {.opcode = CPU_INST_EOR, .addr_mode = ADDRESS_MODE_ABSOLUTE_INDEXED_X,              .width[0] = 3},
    [CPU_OPCODE_EOR_ABSL_X]        = {.opcode = CPU_INST_EOR, .addr_mode = ADDRESS_MODE_ABSOLUTE_LONG_INDEXED_X,         .width[0] = 4},
    [CPU_OPCODE_EOR_ABS_Y]         = {.opcode = CPU_INST_EOR, .addr_mode = ADDRESS_MODE_ABSOLUTE_INDEXED_Y,              .width[0] = 3},
    [CPU_OPCODE_EOR_DIR_X]         = {.opcode = CPU_INST_EOR, .addr_mode = ADDRESS_MODE_DIRECT_INDEXED_X,                .width[0] = 2},
    [CPU_OPCODE_EOR_DIR_X_IND]     = {.opcode = CPU_INST_EOR, .addr_mode = ADDRESS_MODE_DIRECT_INDEXED_INDIRECT,         .width[0] = 2},
    [CPU_OPCODE_EOR_DIR_IND_Y]     = {.opcode = CPU_INST_EOR, .addr_mode = ADDRESS_MODE_DIRECT_INDIRECT_INDEXED,         .width[0] = 2},
    [CPU_OPCODE_EOR_DIR_INDL_Y]    = {.opcode = CPU_INST_EOR, .addr_mode = ADDRESS_MODE_DIRECT_INDIRECT_LONG_INDEXED,    .width[0] = 2},
    [CPU_OPCODE_EOR_S_REL]         = {.opcode = CPU_INST_EOR, .addr_mode = ADDRESS_MODE_STACK_RELATIVE,                  .width[0] = 2},
    [CPU_OPCODE_EOR_S_REL_IND_Y]   = {.opcode = CPU_INST_EOR, .addr_mode = ADDRESS_MODE_STACK_RELATIVE_INDIRECT_INDEXED, .width[0] = 2},

    [CPU_OPCODE_INC_ACC]           = {.opcode = CPU_INST_INC, .addr_mode = ADDRESS_MODE_ACCUMULATOR,                     .width[0] = 1},
    [CPU_OPCODE_INC_ABS]           = {.opcode = CPU_INST_INC, .addr_mode = ADDRESS_MODE_ABSOLUTE,                        .width[0] = 3},
    [CPU_OPCODE_INC_DIR]           = {.opcode = CPU_INST_INC, .addr_mode = ADDRESS_MODE_DIRECT,                          .width[0] = 2},
    [CPU_OPCODE_INC_ABS_X]         = {.opcode = CPU_INST_INC, .addr_mode = ADDRESS_MODE_ABSOLUTE_INDEXED_X,              .width[0] = 3},
    [CPU_OPCODE_INC_DIR_X]         = {.opcode = CPU_INST_INC, .addr_mode = ADDRESS_MODE_DIRECT_INDEXED_X,                .width[0] = 2},

    [CPU_OPCODE_INX_IMP]           = {.opcode = CPU_INST_INX, .addr_mode = ADDRESS_MODE_IMPLIED,                         .width[0] = 1},
    [CPU_OPCODE_INY_IMP]           = {.opcode = CPU_INST_INY, .addr_mode = ADDRESS_MODE_IMPLIED,                         .width[0] = 1},

    [CPU_OPCODE_JMP_ABS]           = {.opcode = CPU_INST_JMP, .addr_mode = ADDRESS_MODE_ABSOLUTE,                        .width[0] = 3},
    [CPU_OPCODE_JMP_ABS_IND]       = {.opcode = CPU_INST_JMP, .addr_mode = ADDRESS_MODE_ABSOLUTE_INDIRECT,               .width[0] = 3},
    [CPU_OPCODE_JMP_ABS_X_IND]     = {.opcode = CPU_INST_JMP, .addr_mode = ADDRESS_MODE_ABSOLUTE_INDEXED_INDIRECT,       .width[0] = 3},
    [CPU_OPCODE_JMP_ABSL]          = {.opcode = CPU_INST_JMP, .addr_mode = ADDRESS_MODE_ABSOLUTE_LONG,                   .width[0] = 4},
    [CPU_OPCODE_JML_ABS_IND]       = {.opcode = CPU_INST_JMP, .addr_mode = ADDRESS_MODE_ABSOLUTE_INDIRECT,               .width[0] = 3},

    [CPU_OPCODE_JSR_ABS]           = {.opcode = CPU_INST_JSR, .addr_mode = ADDRESS_MODE_ABSOLUTE,                        .width[0] = 3},
    [CPU_OPCODE_JSR_ABS_X_IND]     = {.opcode = CPU_INST_JSR, .addr_mode = ADDRESS_MODE_ABSOLUTE_INDEXED_INDIRECT,       .width[0] = 3},
    [CPU_OPCODE_JSL_ABSL]          = {.opcode = CPU_INST_JSL, .addr_mode = ADDRESS_MODE_ABSOLUTE_LONG,                   .width[0] = 4},

    [CPU_OPCODE_LDA_IMM]           = {.opcode = CPU_INST_LDA, .addr_mode = ADDRESS_MODE_IMMEDIATE,                       .width[0] = 3, .width[1] = 2, .width_flag = CPU_STATUS_FLAG_M},
    [CPU_OPCODE_LDA_ABS]           = {.opcode = CPU_INST_LDA, .addr_mode = ADDRESS_MODE_ABSOLUTE,                        .width[0] = 3},
    [CPU_OPCODE_LDA_ABSL]          = {.opcode = CPU_INST_LDA, .addr_mode = ADDRESS_MODE_ABSOLUTE_LONG,                   .width[0] = 4},
    [CPU_OPCODE_LDA_DIR]           = {.opcode = CPU_INST_LDA, .addr_mode = ADDRESS_MODE_DIRECT,                          .width[0] = 2},
    [CPU_OPCODE_LDA_DIR_IND]       = {.opcode = CPU_INST_LDA, .addr_mode = ADDRESS_MODE_DIRECT_INDIRECT,                 .width[0] = 2},
    [CPU_OPCODE_LDA_DIR_INDL]      = {.opcode = CPU_INST_LDA, .addr_mode = ADDRESS_MODE_DIRECT_INDIRECT_LONG,            .width[0] = 2},
    [CPU_OPCODE_LDA_ABS_X]         = {.opcode = CPU_INST_LDA, .addr_mode = ADDRESS_MODE_ABSOLUTE_INDEXED_X,              .width[0] = 3},
    [CPU_OPCODE_LDA_ABSL_X]        = {.opcode = CPU_INST_LDA, .addr_mode = ADDRESS_MODE_ABSOLUTE_LONG_INDEXED_X,         .width[0] = 4},
    [CPU_OPCODE_LDA_ABS_Y]         = {.opcode = CPU_INST_LDA, .addr_mode = ADDRESS_MODE_ABSOLUTE_INDEXED_Y,              .width[0] = 3},
    [CPU_OPCODE_LDA_DIR_X]         = {.opcode = CPU_INST_LDA, .addr_mode = ADDRESS_MODE_DIRECT_INDEXED_X,                .width[0] = 2},
    [CPU_OPCODE_LDA_DIR_X_IND]     = {.opcode = CPU_INST_LDA, .addr_mode = ADDRESS_MODE_DIRECT_INDEXED_INDIRECT,         .width[0] = 2},
    [CPU_OPCODE_LDA_DIR_IND_Y]     = {.opcode = CPU_INST_LDA, .addr_mode = ADDRESS_MODE_DIRECT_INDIRECT_INDEXED,         .width[0] = 2},
    [CPU_OPCODE_LDA_DIR_INDL_Y]    = {.opcode = CPU_INST_LDA, .addr_mode = ADDRESS_MODE_DIRECT_INDIRECT_LONG_INDEXED,    .width[0] = 2},
    [CPU_OPCODE_LDA_S_REL]         = {.opcode = CPU_INST_LDA, .addr_mode = ADDRESS_MODE_STACK_RELATIVE,                  .width[0] = 2},
    [CPU_OPCODE_LDA_S_REL_IND_Y]   = {.opcode = CPU_INST_LDA, .addr_mode = ADDRESS_MODE_STACK_RELATIVE_INDIRECT_INDEXED, .width[0] = 2},

    [CPU_OPCODE_LDX_IMM]           = {.opcode = CPU_INST_LDX, .addr_mode = ADDRESS_MODE_IMMEDIATE,                       .width[0] = 3, .width[1] = 2, .width_flag = CPU_STATUS_FLAG_X},
    [CPU_OPCODE_LDX_ABS]           = {.opcode = CPU_INST_LDX, .addr_mode = ADDRESS_MODE_ABSOLUTE,                        .width[0] = 3},
    [CPU_OPCODE_LDX_DIR]           = {.opcode = CPU_INST_LDX, .addr_mode = ADDRESS_MODE_DIRECT,                          .width[0] = 2},
    [CPU_OPCODE_LDX_ABS_Y]         = {.opcode = CPU_INST_LDX, .addr_mode = ADDRESS_MODE_ABSOLUTE_INDEXED_Y,              .width[0] = 3},
    [CPU_OPCODE_LDX_DIR_Y]         = {.opcode = CPU_INST_LDX, .addr_mode = ADDRESS_MODE_DIRECT_INDEXED_Y,                .width[0] = 2},

    [CPU_OPCODE_LDY_IMM]           = {.opcode = CPU_INST_LDY, .addr_mode = ADDRESS_MODE_IMMEDIATE,                       .width[0] = 3, .width[1] = 2, .width_flag = CPU_STATUS_FLAG_X},
    [CPU_OPCODE_LDY_ABS]           = {.opcode = CPU_INST_LDY, .addr_mode = ADDRESS_MODE_ABSOLUTE,                        .width[0] = 3},
    [CPU_OPCODE_LDY_DIR]           = {.opcode = CPU_INST_LDY, .addr_mode = ADDRESS_MODE_DIRECT,                          .width[0] = 2},
    [CPU_OPCODE_LDY_ABS_X]         = {.opcode = CPU_INST_LDY, .addr_mode = ADDRESS_MODE_ABSOLUTE_INDEXED_X,              .width[0] = 3},
    [CPU_OPCODE_LDY_DIR_X]         = {.opcode = CPU_INST_LDY, .addr_mode = ADDRESS_MODE_DIRECT_INDEXED_X,                .width[0] = 2},

    [CPU_OPCODE_LSR_ACC]           = {.opcode = CPU_INST_LSR, .addr_mode = ADDRESS_MODE_ACCUMULATOR,                     .width[0] = 1},
    [CPU_OPCODE_LSR_ABS]           = {.opcode = CPU_INST_LSR, .addr_mode = ADDRESS_MODE_ABSOLUTE,                        .width[0] = 3},
    [CPU_OPCODE_LSR_DIR]           = {.opcode = CPU_INST_LSR, .addr_mode = ADDRESS_MODE_DIRECT,                          .width[0] = 2},
    [CPU_OPCODE_LSR_ABS_X]         = {.opcode = CPU_INST_LSR, .addr_mode = ADDRESS_MODE_ABSOLUTE_INDEXED_X,              .width[0] = 3},  
    [CPU_OPCODE_LSR_DIR_X]         = {.opcode = CPU_INST_LSR, .addr_mode = ADDRESS_MODE_DIRECT_INDEXED_X,                .width[0] = 2},

    [CPU_OPCODE_MVN_BLK]           = {.opcode = CPU_INST_MVN, .addr_mode = ADDRESS_MODE_BLOCK_MOVE,                      .width[0] = 3},
    [CPU_OPCODE_MVP_BLK]           = {.opcode = CPU_INST_MVP, .addr_mode = ADDRESS_MODE_BLOCK_MOVE,                      .width[0] = 3},

    [CPU_OPCODE_NOP_IMP]           = {.opcode = CPU_INST_NOP, .addr_mode = ADDRESS_MODE_IMPLIED,                         .width[0] = 1},

    [CPU_OPCODE_ORA_IMM]           = {.opcode = CPU_INST_ORA, .addr_mode = ADDRESS_MODE_IMMEDIATE,                       .width[0] = 3, .width[1] = 2, .width_flag = CPU_STATUS_FLAG_M},
    [CPU_OPCODE_ORA_ABS]           = {.opcode = CPU_INST_ORA, .addr_mode = ADDRESS_MODE_ABSOLUTE,                        .width[0] = 3},
    [CPU_OPCODE_ORA_ABSL]          = {.opcode = CPU_INST_ORA, .addr_mode = ADDRESS_MODE_ABSOLUTE_LONG,                   .width[0] = 4},
    [CPU_OPCODE_ORA_DIR]           = {.opcode = CPU_INST_ORA, .addr_mode = ADDRESS_MODE_DIRECT,                          .width[0] = 2},
    [CPU_OPCODE_ORA_DIR_IND]       = {.opcode = CPU_INST_ORA, .addr_mode = ADDRESS_MODE_DIRECT_INDIRECT,                 .width[0] = 2},
    [CPU_OPCODE_ORA_DIR_INDL]      = {.opcode = CPU_INST_ORA, .addr_mode = ADDRESS_MODE_DIRECT_INDIRECT_LONG,            .width[0] = 2},
    [CPU_OPCODE_ORA_ABS_X]         = {.opcode = CPU_INST_ORA, .addr_mode = ADDRESS_MODE_ABSOLUTE_INDEXED_X,              .width[0] = 3},
    [CPU_OPCODE_ORA_ABSL_X]        = {.opcode = CPU_INST_ORA, .addr_mode = ADDRESS_MODE_ABSOLUTE_LONG_INDEXED_X,         .width[0] = 4},
    [CPU_OPCODE_ORA_ABS_Y]         = {.opcode = CPU_INST_ORA, .addr_mode = ADDRESS_MODE_ABSOLUTE_INDEXED_Y,              .width[0] = 3},
    [CPU_OPCODE_ORA_DIR_X]         = {.opcode = CPU_INST_ORA, .addr_mode = ADDRESS_MODE_DIRECT_INDEXED_X,                .width[0] = 2},
    [CPU_OPCODE_ORA_DIR_X_IND]     = {.opcode = CPU_INST_ORA, .addr_mode = ADDRESS_MODE_DIRECT_INDEXED_INDIRECT,         .width[0] = 2},
    [CPU_OPCODE_ORA_DIR_IND_Y]     = {.opcode = CPU_INST_ORA, .addr_mode = ADDRESS_MODE_DIRECT_INDIRECT_INDEXED,         .width[0] = 2},
    [CPU_OPCODE_ORA_DIR_INDL_Y]    = {.opcode = CPU_INST_ORA, .addr_mode = ADDRESS_MODE_DIRECT_INDIRECT_LONG_INDEXED,    .width[0] = 2},
    [CPU_OPCODE_ORA_S_REL]         = {.opcode = CPU_INST_ORA, .addr_mode = ADDRESS_MODE_STACK_RELATIVE,                  .width[0] = 2},
    [CPU_OPCODE_ORA_S_REL_IND_Y]   = {.opcode = CPU_INST_ORA, .addr_mode = ADDRESS_MODE_STACK_RELATIVE_INDIRECT_INDEXED, .width[0] = 2},
    
    [CPU_OPCODE_PEA_S]             = {.opcode = CPU_INST_PEA, .addr_mode = ADDRESS_MODE_STACK,                           .width[0] = 3},
    [CPU_OPCODE_PEI_S]             = {.opcode = CPU_INST_PEI, .addr_mode = ADDRESS_MODE_STACK,                           .width[0] = 2},
    [CPU_OPCODE_PER_S]             = {.opcode = CPU_INST_PER, .addr_mode = ADDRESS_MODE_STACK,                           .width[0] = 3},
    [CPU_OPCODE_PHA_S]             = {.opcode = CPU_INST_PHA, .addr_mode = ADDRESS_MODE_STACK,                           .width[0] = 1},
    [CPU_OPCODE_PHB_S]             = {.opcode = CPU_INST_PHB, .addr_mode = ADDRESS_MODE_STACK,                           .width[0] = 1},
    [CPU_OPCODE_PHD_S]             = {.opcode = CPU_INST_PHD, .addr_mode = ADDRESS_MODE_STACK,                           .width[0] = 1},
    [CPU_OPCODE_PHK_S]             = {.opcode = CPU_INST_PHK, .addr_mode = ADDRESS_MODE_STACK,                           .width[0] = 1},
    [CPU_OPCODE_PHP_S]             = {.opcode = CPU_INST_PHP, .addr_mode = ADDRESS_MODE_STACK,                           .width[0] = 1},
    [CPU_OPCODE_PHX_S]             = {.opcode = CPU_INST_PHX, .addr_mode = ADDRESS_MODE_STACK,                           .width[0] = 1},
    [CPU_OPCODE_PHY_S]             = {.opcode = CPU_INST_PHY, .addr_mode = ADDRESS_MODE_STACK,                           .width[0] = 1},

    [CPU_OPCODE_PLA_S]             = {.opcode = CPU_INST_PLA, .addr_mode = ADDRESS_MODE_STACK,                           .width[0] = 1},
    [CPU_OPCODE_PLB_S]             = {.opcode = CPU_INST_PLB, .addr_mode = ADDRESS_MODE_STACK,                           .width[0] = 1},
    [CPU_OPCODE_PLD_S]             = {.opcode = CPU_INST_PLD, .addr_mode = ADDRESS_MODE_STACK,                           .width[0] = 1},
    [CPU_OPCODE_PLP_S]             = {.opcode = CPU_INST_PLP, .addr_mode = ADDRESS_MODE_STACK,                           .width[0] = 1},
    [CPU_OPCODE_PLX_S]             = {.opcode = CPU_INST_PLX, .addr_mode = ADDRESS_MODE_STACK,                           .width[0] = 1},
    [CPU_OPCODE_PLY_S]             = {.opcode = CPU_INST_PLY, .addr_mode = ADDRESS_MODE_STACK,                           .width[0] = 1},

    [CPU_OPCODE_REP_IMM]           = {.opcode = CPU_INST_REP, .addr_mode = ADDRESS_MODE_IMMEDIATE,                       .width[0] = 2},

    [CPU_OPCODE_ROL_ACC]           = {.opcode = CPU_INST_ROL, .addr_mode = ADDRESS_MODE_ACCUMULATOR,                     .width[0] = 1},
    [CPU_OPCODE_ROL_ABS]           = {.opcode = CPU_INST_ROL, .addr_mode = ADDRESS_MODE_ABSOLUTE,                        .width[0] = 3},
    [CPU_OPCODE_ROL_DIR]           = {.opcode = CPU_INST_ROL, .addr_mode = ADDRESS_MODE_DIRECT,                          .width[0] = 2},
    [CPU_OPCODE_ROL_ABS_X]         = {.opcode = CPU_INST_ROL, .addr_mode = ADDRESS_MODE_ABSOLUTE_INDEXED_X,              .width[0] = 3},
    [CPU_OPCODE_ROL_DIR_X]         = {.opcode = CPU_INST_ROL, .addr_mode = ADDRESS_MODE_DIRECT_INDEXED_X,                .width[0] = 2},

    [CPU_OPCODE_ROR_ACC]           = {.opcode = CPU_INST_ROR, .addr_mode = ADDRESS_MODE_ACCUMULATOR,                     .width[0] = 1},
    [CPU_OPCODE_ROR_ABS]           = {.opcode = CPU_INST_ROR, .addr_mode = ADDRESS_MODE_ABSOLUTE,                        .width[0] = 3},
    [CPU_OPCODE_ROR_DIR]           = {.opcode = CPU_INST_ROR, .addr_mode = ADDRESS_MODE_DIRECT,                          .width[0] = 2},
    [CPU_OPCODE_ROR_ABS_X]         = {.opcode = CPU_INST_ROR, .addr_mode = ADDRESS_MODE_ABSOLUTE_INDEXED_X,              .width[0] = 3},
    [CPU_OPCODE_ROR_DIR_X]         = {.opcode = CPU_INST_ROR, .addr_mode = ADDRESS_MODE_DIRECT_INDEXED_X,                .width[0] = 2},

    [CPU_OPCODE_RTI_S]             = {.opcode = CPU_INST_RTI, .addr_mode = ADDRESS_MODE_STACK,                           .width[0] = 1},
    [CPU_OPCODE_RTL_S]             = {.opcode = CPU_INST_RTL, .addr_mode = ADDRESS_MODE_STACK,                           .width[0] = 1},
    [CPU_OPCODE_RTS_S]             = {.opcode = CPU_INST_RTS, .addr_mode = ADDRESS_MODE_STACK,                           .width[0] = 1},

    [CPU_OPCODE_SBC_IMM]           = {.opcode = CPU_INST_SBC, .addr_mode = ADDRESS_MODE_IMMEDIATE,                       .width[0] = 3, .width[1] = 2, .width_flag = CPU_STATUS_FLAG_M},
    [CPU_OPCODE_SBC_ABS]           = {.opcode = CPU_INST_SBC, .addr_mode = ADDRESS_MODE_ABSOLUTE,                        .width[0] = 3},
    [CPU_OPCODE_SBC_ABSL]          = {.opcode = CPU_INST_SBC, .addr_mode = ADDRESS_MODE_ABSOLUTE_LONG,                   .width[0] = 4},
    [CPU_OPCODE_SBC_DIR]           = {.opcode = CPU_INST_SBC, .addr_mode = ADDRESS_MODE_DIRECT,                          .width[0] = 2},
    [CPU_OPCODE_SBC_DIR_IND]       = {.opcode = CPU_INST_SBC, .addr_mode = ADDRESS_MODE_DIRECT_INDIRECT,                 .width[0] = 2},
    [CPU_OPCODE_SBC_DIR_INDL]      = {.opcode = CPU_INST_SBC, .addr_mode = ADDRESS_MODE_DIRECT_INDIRECT_LONG,            .width[0] = 2},
    [CPU_OPCODE_SBC_ABS_X]         = {.opcode = CPU_INST_SBC, .addr_mode = ADDRESS_MODE_ABSOLUTE_INDEXED_X,              .width[0] = 3},
    [CPU_OPCODE_SBC_ABSL_X]        = {.opcode = CPU_INST_SBC, .addr_mode = ADDRESS_MODE_ABSOLUTE_LONG_INDEXED_X,         .width[0] = 4},
    [CPU_OPCODE_SBC_ABS_Y]         = {.opcode = CPU_INST_SBC, .addr_mode = ADDRESS_MODE_ABSOLUTE_INDEXED_Y,              .width[0] = 3},
    [CPU_OPCODE_SBC_DIR_X]         = {.opcode = CPU_INST_SBC, .addr_mode = ADDRESS_MODE_DIRECT_INDEXED_X,                .width[0] = 2},
    [CPU_OPCODE_SBC_DIR_X_IND]     = {.opcode = CPU_INST_SBC, .addr_mode = ADDRESS_MODE_DIRECT_INDEXED_INDIRECT,         .width[0] = 2},
    [CPU_OPCODE_SBC_DIR_IND_Y]     = {.opcode = CPU_INST_SBC, .addr_mode = ADDRESS_MODE_DIRECT_INDIRECT_INDEXED,         .width[0] = 2},
    [CPU_OPCODE_SBC_DIR_INDL_Y]    = {.opcode = CPU_INST_SBC, .addr_mode = ADDRESS_MODE_DIRECT_INDIRECT_LONG_INDEXED,    .width[0] = 2},
    [CPU_OPCODE_SBC_S_REL]         = {.opcode = CPU_INST_SBC, .addr_mode = ADDRESS_MODE_STACK_RELATIVE,                  .width[0] = 2},
    [CPU_OPCODE_SBC_S_REL_IND_Y]   = {.opcode = CPU_INST_SBC, .addr_mode = ADDRESS_MODE_STACK_RELATIVE_INDIRECT_INDEXED, .width[0] = 2},

    [CPU_OPCODE_SEC_IMP]           = {.opcode = CPU_INST_SEC, .addr_mode = ADDRESS_MODE_IMPLIED,                         .width[0] = 1},
    [CPU_OPCODE_SED_IMP]           = {.opcode = CPU_INST_SED, .addr_mode = ADDRESS_MODE_IMPLIED,                         .width[0] = 1},
    [CPU_OPCODE_SEI_IMP]           = {.opcode = CPU_INST_SEI, .addr_mode = ADDRESS_MODE_IMPLIED,                         .width[0] = 1},
    [CPU_OPCODE_SEP_IMM]           = {.opcode = CPU_INST_SEP, .addr_mode = ADDRESS_MODE_IMPLIED,                         .width[0] = 2},

    [CPU_OPCODE_STA_ABS]           = {.opcode = CPU_INST_STA, .addr_mode = ADDRESS_MODE_ABSOLUTE,                        .width[0] = 3},
    [CPU_OPCODE_STA_ABSL]          = {.opcode = CPU_INST_STA, .addr_mode = ADDRESS_MODE_ABSOLUTE_LONG,                   .width[0] = 4},
    [CPU_OPCODE_STA_DIR]           = {.opcode = CPU_INST_STA, .addr_mode = ADDRESS_MODE_DIRECT,                          .width[0] = 2},
    [CPU_OPCODE_STA_DIR_IND]       = {.opcode = CPU_INST_STA, .addr_mode = ADDRESS_MODE_DIRECT_INDIRECT,                 .width[0] = 2},
    [CPU_OPCODE_STA_DIR_INDL]      = {.opcode = CPU_INST_STA, .addr_mode = ADDRESS_MODE_DIRECT_INDIRECT_LONG,            .width[0] = 2},
    [CPU_OPCODE_STA_ABS_X]         = {.opcode = CPU_INST_STA, .addr_mode = ADDRESS_MODE_ABSOLUTE_INDEXED_X,              .width[0] = 3},
    [CPU_OPCODE_STA_ABSL_X]        = {.opcode = CPU_INST_STA, .addr_mode = ADDRESS_MODE_ABSOLUTE_LONG_INDEXED_X,         .width[0] = 4},
    [CPU_OPCODE_STA_ABS_Y]         = {.opcode = CPU_INST_STA, .addr_mode = ADDRESS_MODE_ABSOLUTE_INDEXED_Y,              .width[0] = 3},
    [CPU_OPCODE_STA_DIR_X]         = {.opcode = CPU_INST_STA, .addr_mode = ADDRESS_MODE_DIRECT_INDEXED_X,                .width[0] = 2},
    [CPU_OPCODE_STA_DIR_X_IND]     = {.opcode = CPU_INST_STA, .addr_mode = ADDRESS_MODE_DIRECT_INDEXED_INDIRECT,         .width[0] = 2},
    [CPU_OPCODE_STA_DIR_IND_Y]     = {.opcode = CPU_INST_STA, .addr_mode = ADDRESS_MODE_DIRECT_INDIRECT_INDEXED,         .width[0] = 2},
    [CPU_OPCODE_STA_DIR_INDL_Y]    = {.opcode = CPU_INST_STA, .addr_mode = ADDRESS_MODE_DIRECT_INDIRECT_LONG_INDEXED,    .width[0] = 2},
    [CPU_OPCODE_STA_S_REL]         = {.opcode = CPU_INST_STA, .addr_mode = ADDRESS_MODE_STACK_RELATIVE,                  .width[0] = 2},
    [CPU_OPCODE_STA_S_REL_IND_Y]   = {.opcode = CPU_INST_STA, .addr_mode = ADDRESS_MODE_STACK_RELATIVE_INDIRECT_INDEXED, .width[0] = 2},

    [CPU_OPCODE_STP_IMP]           = {.opcode = CPU_INST_STP, .addr_mode = ADDRESS_MODE_IMPLIED,                         .width[0] = 1},

    [CPU_OPCODE_STX_ABS]           = {.opcode = CPU_INST_STX, .addr_mode = ADDRESS_MODE_ABSOLUTE,                        .width[0] = 3},
    [CPU_OPCODE_STX_DIR]           = {.opcode = CPU_INST_STX, .addr_mode = ADDRESS_MODE_DIRECT,                          .width[0] = 2},
    [CPU_OPCODE_STX_DIR_Y]         = {.opcode = CPU_INST_STX, .addr_mode = ADDRESS_MODE_DIRECT_INDEXED_Y,                .width[0] = 2},

    [CPU_OPCODE_STY_ABS]           = {.opcode = CPU_INST_STY, .addr_mode = ADDRESS_MODE_ABSOLUTE,                        .width[0] = 3},
    [CPU_OPCODE_STY_DIR]           = {.opcode = CPU_INST_STY, .addr_mode = ADDRESS_MODE_DIRECT,                          .width[0] = 2},
    [CPU_OPCODE_STY_DIR_X]         = {.opcode = CPU_INST_STY, .addr_mode = ADDRESS_MODE_DIRECT_INDEXED_X,                .width[0] = 2},

    [CPU_OPCODE_STZ_ABS]           = {.opcode = CPU_INST_STZ, .addr_mode = ADDRESS_MODE_ABSOLUTE,                        .width[0] = 3},
    [CPU_OPCODE_STZ_DIR]           = {.opcode = CPU_INST_STZ, .addr_mode = ADDRESS_MODE_DIRECT,                          .width[0] = 2},
    [CPU_OPCODE_STZ_ABS_X]         = {.opcode = CPU_INST_STZ, .addr_mode = ADDRESS_MODE_ABSOLUTE_INDEXED_X,              .width[0] = 3},
    [CPU_OPCODE_STZ_DIR_X]         = {.opcode = CPU_INST_STZ, .addr_mode = ADDRESS_MODE_DIRECT_INDEXED_X,                .width[0] = 2},

    [CPU_OPCODE_TAX_IMP]           = {.opcode = CPU_INST_TAX, .addr_mode = ADDRESS_MODE_IMPLIED,                         .width[0] = 1},
    [CPU_OPCODE_TAY_IMP]           = {.opcode = CPU_INST_TAY, .addr_mode = ADDRESS_MODE_IMPLIED,                         .width[0] = 1},
    [CPU_OPCODE_TCD_IMP]           = {.opcode = CPU_INST_TCD, .addr_mode = ADDRESS_MODE_IMPLIED,                         .width[0] = 1},
    [CPU_OPCODE_TCS_IMP]           = {.opcode = CPU_INST_TCS, .addr_mode = ADDRESS_MODE_IMPLIED,                         .width[0] = 1},
    [CPU_OPCODE_TDC_IMP]           = {.opcode = CPU_INST_TDC, .addr_mode = ADDRESS_MODE_IMPLIED,                         .width[0] = 1},
    [CPU_OPCODE_TSC_ACC]           = {.opcode = CPU_INST_TSC, .addr_mode = ADDRESS_MODE_ACCUMULATOR,                     .width[0] = 1},
    [CPU_OPCODE_TSX_ACC]           = {.opcode = CPU_INST_TSX, .addr_mode = ADDRESS_MODE_ACCUMULATOR,                     .width[0] = 1},
    [CPU_OPCODE_TXA_ACC]           = {.opcode = CPU_INST_TXA, .addr_mode = ADDRESS_MODE_ACCUMULATOR,                     .width[0] = 1},
    [CPU_OPCODE_TXS_ACC]           = {.opcode = CPU_INST_TXS, .addr_mode = ADDRESS_MODE_ACCUMULATOR,                     .width[0] = 1},
    [CPU_OPCODE_TXY_ACC]           = {.opcode = CPU_INST_TXY, .addr_mode = ADDRESS_MODE_ACCUMULATOR,                     .width[0] = 1},
    [CPU_OPCODE_TYA_ACC]           = {.opcode = CPU_INST_TYA, .addr_mode = ADDRESS_MODE_ACCUMULATOR,                     .width[0] = 1},
    [CPU_OPCODE_TYX_ACC]           = {.opcode = CPU_INST_TYX, .addr_mode = ADDRESS_MODE_ACCUMULATOR,                     .width[0] = 1},

    [CPU_OPCODE_TRB_ABS]           = {.opcode = CPU_INST_TRB, .addr_mode = ADDRESS_MODE_ABSOLUTE,                        .width[0] = 3},
    [CPU_OPCODE_TRB_DIR]           = {.opcode = CPU_INST_TRB, .addr_mode = ADDRESS_MODE_DIRECT,                          .width[0] = 2},

    [CPU_OPCODE_TSB_ABS]           = {.opcode = CPU_INST_TSB, .addr_mode = ADDRESS_MODE_ABSOLUTE,                        .width[0] = 3},
    [CPU_OPCODE_TSB_DIR]           = {.opcode = CPU_INST_TSB, .addr_mode = ADDRESS_MODE_DIRECT,                          .width[0] = 2},

    [CPU_OPCODE_WAI_ACC]           = {.opcode = CPU_INST_WAI, .addr_mode = ADDRESS_MODE_ACCUMULATOR,                     .width[0] = 1},

    [CPU_OPCODE_WDM_ACC]           = {.opcode = CPU_INST_WDM, .addr_mode = ADDRESS_MODE_ACCUMULATOR,                     .width[0] = 2},

    [CPU_OPCODE_XBA_ACC]           = {.opcode = CPU_INST_XBA, .addr_mode = ADDRESS_MODE_ACCUMULATOR,                     .width[0] = 1},
    [CPU_OPCODE_XCE_ACC]           = {.opcode = CPU_INST_XCE, .addr_mode = ADDRESS_MODE_ACCUMULATOR,                     .width[0] = 1},
};

const char *opcode_strs[] = {
    [CPU_INST_BRK] = "BRK",
    [CPU_INST_BIT] = "BIT",
    [CPU_INST_BCC] = "BCC",
    [CPU_INST_BCS] = "BCS",
    [CPU_INST_BNE] = "BNE",
    [CPU_INST_BEQ] = "BEQ",
    [CPU_INST_BPL] = "BPL",
    [CPU_INST_BMI] = "BMI",
    [CPU_INST_BVC] = "BVC",
    [CPU_INST_BVS] = "BVS",
    [CPU_INST_BRA] = "BRA",
    [CPU_INST_BRL] = "BRL",
    [CPU_INST_CLC] = "CLC",
    [CPU_INST_CLD] = "CLD",
    [CPU_INST_CLV] = "CLV",
    [CPU_INST_CLI] = "CLI",
    [CPU_INST_CMP] = "CMP",
    [CPU_INST_CPX] = "CPX",
    [CPU_INST_CPY] = "CPY",
    [CPU_INST_ADC] = "ADC",
    [CPU_INST_AND] = "AND",
    [CPU_INST_SBC] = "SBC",
    [CPU_INST_EOR] = "EOR",
    [CPU_INST_ORA] = "ORA",
    [CPU_INST_ROL] = "ROL",
    [CPU_INST_ROR] = "ROR",
    [CPU_INST_DEC] = "DEC",
    [CPU_INST_INC] = "INC",
    [CPU_INST_ASL] = "ASL",
    [CPU_INST_LSR] = "LSR",
    [CPU_INST_DEX] = "DEX",
    [CPU_INST_INX] = "INX",
    [CPU_INST_DEY] = "DEY",
    [CPU_INST_INY] = "INY",
    [CPU_INST_TRB] = "TRB",
    [CPU_INST_TSB] = "TSB",
    [CPU_INST_JMP] = "JMP",
    [CPU_INST_JML] = "JML",
    [CPU_INST_JSL] = "JSL",
    [CPU_INST_JSR] = "JSR",
    [CPU_INST_LDA] = "LDA",
    [CPU_INST_LDX] = "LDX",
    [CPU_INST_LDY] = "LDY",
    [CPU_INST_STA] = "STA",
    [CPU_INST_STX] = "STX",
    [CPU_INST_STY] = "STY",
    [CPU_INST_STZ] = "STZ",
    [CPU_INST_MVN] = "MVN",
    [CPU_INST_MVP] = "MVP",
    [CPU_INST_NOP] = "NOP",
    [CPU_INST_PEA] = "PEA",
    [CPU_INST_PEI] = "PEI",
    [CPU_INST_PER] = "PER",
    [CPU_INST_PHA] = "PHA",
    [CPU_INST_PHB] = "PHB",
    [CPU_INST_PHD] = "PHD",
    [CPU_INST_PHK] = "PHK",
    [CPU_INST_PHP] = "PHP",
    [CPU_INST_PHX] = "PHX",
    [CPU_INST_PHY] = "PHY",
    [CPU_INST_PLA] = "PLA",
    [CPU_INST_PLB] = "PLB",
    [CPU_INST_PLD] = "PLD",
    [CPU_INST_PLP] = "PLP",
    [CPU_INST_PLX] = "PLX",
    [CPU_INST_PLY] = "PLY",
    [CPU_INST_REP] = "REP",
    [CPU_INST_RTI] = "RTI",
    [CPU_INST_RTL] = "RTL",
    [CPU_INST_RTS] = "RTS",
    [CPU_INST_SEP] = "SEP",
    [CPU_INST_SEC] = "SEC",
    [CPU_INST_SED] = "SED",
    [CPU_INST_SEI] = "SEI",
    [CPU_INST_TAX] = "TAX",
    [CPU_INST_TAY] = "TAY",
    [CPU_INST_TCD] = "TCD",
    [CPU_INST_TCS] = "TCS",
    [CPU_INST_TDC] = "TDC",
    [CPU_INST_TSC] = "TSC",
    [CPU_INST_TSX] = "TSX",
    [CPU_INST_TXA] = "TXA",
    [CPU_INST_TXS] = "TXS",
    [CPU_INST_TXY] = "TXY",
    [CPU_INST_TYA] = "TYA",
    [CPU_INST_TYX] = "TYX",
    [CPU_INST_WAI] = "WAI",
    [CPU_INST_WDM] = "WDM",
    [CPU_INST_XBA] = "XBA",
    [CPU_INST_STP] = "STP",
    [CPU_INST_COP] = "COP",
    [CPU_INST_XCE] = "XCE",
    [CPU_INST_UNKNOWN] = "UNKNOWN",
};

const char *addr_mode_strs[] = {
    [ADDRESS_MODE_ABSOLUTE]                         = "a",    
    [ADDRESS_MODE_ABSOLUTE_INDEXED_INDIRECT]        = "(a,x)",
    [ADDRESS_MODE_ABSOLUTE_INDEXED_X]               = "a,x",  
    [ADDRESS_MODE_ABSOLUTE_INDEXED_Y]               = "a,y",  
    [ADDRESS_MODE_ABSOLUTE_INDIRECT]                = "(a)",  
    [ADDRESS_MODE_ABSOLUTE_LONG_INDEXED_X]          = "al, x",
    [ADDRESS_MODE_ABSOLUTE_LONG]                    = "al",   
    [ADDRESS_MODE_ACCUMULATOR]                      = "ACC",  
    [ADDRESS_MODE_BLOCK_MOVE]                       = "xyc",  
    [ADDRESS_MODE_DIRECT_INDEXED_INDIRECT]          = "(d, x)",
    [ADDRESS_MODE_DIRECT_INDEXED_X]                 = "d,x",  
    [ADDRESS_MODE_DIRECT_INDEXED_Y]                 = "d,y",  
    [ADDRESS_MODE_DIRECT_INDIRECT_INDEXED]          = "(d),y",
    [ADDRESS_MODE_DIRECT_INDIRECT_LONG_INDEXED]     = "[d],y",
    [ADDRESS_MODE_DIRECT_INDIRECT_LONG]             = "[d]",  
    [ADDRESS_MODE_DIRECT_INDIRECT]                  = "(d)",  
    [ADDRESS_MODE_DIRECT]                           = "d",    
    [ADDRESS_MODE_IMMEDIATE]                        = "#",    
    [ADDRESS_MODE_IMPLIED]                          = "",     
    [ADDRESS_MODE_PROGRAM_COUNTER_RELATIVE_LONG]    = "rl",   
    [ADDRESS_MODE_PROGRAM_COUNTER_RELATIVE]         = "r",    
    [ADDRESS_MODE_STACK]                            = "s",    
    [ADDRESS_MODE_STACK_RELATIVE]                   = "d,s",  
    [ADDRESS_MODE_STACK_RELATIVE_INDIRECT_INDEXED]  = "(d,x),y"
};

// const char *cpu_reg_strs[] = {
//     // [CPU_REG_]
// };

// struct branch_cond_t branch_conds[5] =
// {
//     [0] = {CPU_STATUS_FLAG_CARRY},
//     [1] = {CPU_STATUS_FLAG_ZERO},
//     [2] = {CPU_STATUS_FLAG_NEGATIVE},
//     [3] = {CPU_STATUS_FLAG_OVERFLOW},
//     [4] = {0xff}
// };

//struct transfer_params_t transfer_params[] =
//{
//    [TPARM_INDEX(OPCODE_TAX)] = {.src_reg = &cpu_state.reg_accum.reg_accumC, .dst_reg = &cpu_state.reg_x.reg_x,          .flag = CPU_STATUS_FLAG_INDEX},
//    [TPARM_INDEX(OPCODE_TAY)] = {.src_reg = &cpu_state.reg_accum.reg_accumC, .dst_reg = &cpu_state.reg_y.reg_y,          .flag = CPU_STATUS_FLAG_INDEX},
//    [TPARM_INDEX(OPCODE_TCD)] = {.src_reg = &cpu_state.reg_accum.reg_accumC, .dst_reg = &cpu_state.reg_d},
//    [TPARM_INDEX(OPCODE_TCS)] = {.src_reg = &cpu_state.reg_accum.reg_accumC, .dst_reg = &cpu_state.reg_s},
//    [TPARM_INDEX(OPCODE_TDC)] = {.src_reg = &cpu_state.reg_d,                .dst_reg = &cpu_state.reg_accum.reg_accumC, .flag = CPU_STATUS_FLAG_MEMORY},
//    [TPARM_INDEX(OPCODE_TSC)] = {.src_reg = &cpu_state.reg_s,                .dst_reg = &cpu_state.reg_accum.reg_accumC, .flag = CPU_STATUS_FLAG_MEMORY},
//    [TPARM_INDEX(OPCODE_TSX)] = {.src_reg = &cpu_state.reg_s,                .dst_reg = &cpu_state.reg_x.reg_x,          .flag = CPU_STATUS_FLAG_INDEX},
//    [TPARM_INDEX(OPCODE_TXA)] = {.src_reg = &cpu_state.reg_x.reg_x,          .dst_reg = &cpu_state.reg_accum.reg_accumC, .flag = CPU_STATUS_FLAG_MEMORY},
//    [TPARM_INDEX(OPCODE_TXS)] = {.src_reg = &cpu_state.reg_x.reg_x,          .dst_reg = &cpu_state.reg_s},
//    [TPARM_INDEX(OPCODE_TXY)] = {.src_reg = &cpu_state.reg_x.reg_x,          .dst_reg = &cpu_state.reg_y.reg_y,          .flag = CPU_STATUS_FLAG_INDEX},
//    [TPARM_INDEX(OPCODE_TYA)] = {.src_reg = &cpu_state.reg_y.reg_y,          .dst_reg = &cpu_state.reg_accum.reg_accumC, .flag = CPU_STATUS_FLAG_MEMORY},
//    [TPARM_INDEX(OPCODE_TYX)] = {.src_reg = &cpu_state.reg_y.reg_y,          .dst_reg = &cpu_state.reg_x.reg_x,          .flag = CPU_STATUS_FLAG_INDEX},
//};

//struct transfer_t transfers[] = {
//    [OPCODE_TAX - OPCODE_TAX] = {.src_reg = &cpu_state.reg_accum.reg_accumC, .dst_reg = &cpu_state.reg_x.reg_x,          .flag = CPU_STATUS_FLAG_INDEX},
//    [OPCODE_TAY - OPCODE_TAX] = {.src_reg = &cpu_state.reg_accum.reg_accumC, .dst_reg = &cpu_state.reg_y.reg_y,          .flag = CPU_STATUS_FLAG_INDEX},
//    [OPCODE_TCD - OPCODE_TAX] = {.src_reg = &cpu_state.reg_accum.reg_accumC, .dst_reg = &cpu_state.reg_d},
//    [OPCODE_TCS - OPCODE_TAX] = {.src_reg = &cpu_state.reg_accum.reg_accumC, .dst_reg = &cpu_state.reg_s},
//    [OPCODE_TDC - OPCODE_TAX] = {.src_reg = &cpu_state.reg_d,                .dst_reg = &cpu_state.reg_accum.reg_accumC, .flag = CPU_STATUS_FLAG_MEMORY},
//    [OPCODE_TSC - OPCODE_TAX] = {.src_reg = &cpu_state.reg_s,                .dst_reg = &cpu_state.reg_accum.reg_accumC, .flag = CPU_STATUS_FLAG_MEMORY},
//    [OPCODE_TSX - OPCODE_TAX] = {.src_reg = &cpu_state.reg_s,                .dst_reg = &cpu_state.reg_x.reg_x,          .flag = CPU_STATUS_FLAG_INDEX},
//    [OPCODE_TXA - OPCODE_TAX] = {.src_reg = &cpu_state.reg_x.reg_x,          .dst_reg = &cpu_state.reg_accum.reg_accumC, .flag = CPU_STATUS_FLAG_MEMORY},
//    [OPCODE_TXS - OPCODE_TAX] = {.src_reg = &cpu_state.reg_x.reg_x,          .dst_reg = &cpu_state.reg_s},
//    [OPCODE_TXY - OPCODE_TAX] = {.src_reg = &cpu_state.reg_x.reg_x,          .dst_reg = &cpu_state.reg_y.reg_y,          .flag = CPU_STATUS_FLAG_INDEX},
//    [OPCODE_TYA - OPCODE_TAX] = {.src_reg = &cpu_state.reg_y.reg_y,          .dst_reg = &cpu_state.reg_accum.reg_accumC, .flag = CPU_STATUS_FLAG_MEMORY},
//    [OPCODE_TYX - OPCODE_TAX] = {.src_reg = &cpu_state.reg_y.reg_y,          .dst_reg = &cpu_state.reg_x.reg_x,          .flag = CPU_STATUS_FLAG_INDEX},
//};

// uint32_t zero_reg = 0;

//struct store_t stores[] = {
//    [OPCODE_STA - OPCODE_STA] = {.src_reg = &cpu_state.reg_accum.reg_accumC, .flag = CPU_STATUS_FLAG_MEMORY},
//    [OPCODE_STX - OPCODE_STA] = {.src_reg = &cpu_state.reg_x.reg_x,          .flag = CPU_STATUS_FLAG_INDEX},
//    [OPCODE_STY - OPCODE_STA] = {.src_reg = &cpu_state.reg_y.reg_y,          .flag = CPU_STATUS_FLAG_INDEX},
//    [OPCODE_STZ - OPCODE_STA] = {.src_reg = &zero_reg,                       .flag = CPU_STATUS_FLAG_MEMORY}
//};
//
//struct load_t loads[] = {
//    [OPCODE_LDA - OPCODE_LDA] = {.dst_reg = &cpu_state.reg_accum.reg_accumC, .flag = CPU_STATUS_FLAG_MEMORY},
//    [OPCODE_LDX - OPCODE_LDA] = {.dst_reg = &cpu_state.reg_x.reg_x,          .flag = CPU_STATUS_FLAG_INDEX},
//    [OPCODE_LDY - OPCODE_LDA] = {.dst_reg = &cpu_state.reg_y.reg_y,          .flag = CPU_STATUS_FLAG_INDEX},
//};

//  uint8_t carry_flag_alu_op[ALU_OP_LAST] =
//  {
//      [ALU_OP_ADD] = 1,
//      [ALU_OP_SUB] = 1,
//      [ALU_OP_SHL] = 1,
//      [ALU_OP_SHR] = 1,
//      [ALU_OP_ROR] = 1,
//      [ALU_OP_ROL] = 1,
//      [ALU_OP_CMP] = 1
//  };

//struct push_t pushes[] = {
//    [OPCODE_PEA - OPCODE_PEA] = {NULL},
//    [OPCODE_PEI - OPCODE_PEA] = {NULL},
//    [OPCODE_PER - OPCODE_PEA] = {NULL},
//    [OPCODE_PHA - OPCODE_PEA] = {.src_reg = &cpu_state.reg_accum.reg_accumC,    .flag = CPU_STATUS_FLAG_MEMORY},
//    [OPCODE_PHB - OPCODE_PEA] = {.src_reg = &cpu_state.reg_dbrw.reg_dbr},
//    [OPCODE_PHD - OPCODE_PEA] = {.src_reg = &cpu_state.reg_d,                   .flag = 0xffff},
//    [OPCODE_PHK - OPCODE_PEA] = {.src_reg = &cpu_state.reg_pbrw.reg_pbr},
//    [OPCODE_PHP - OPCODE_PEA] = {.src_reg = cpu_state.reg_p},
//    [OPCODE_PHX - OPCODE_PEA] = {.src_reg = &cpu_state.reg_x.reg_x,             .flag = CPU_STATUS_FLAG_INDEX},
//    [OPCODE_PHY - OPCODE_PEA] = {.src_reg = &cpu_state.reg_y.reg_y,             .flag = CPU_STATUS_FLAG_INDEX},
//};

//struct pop_t pops[] = {
//    [OPCODE_PLA - OPCODE_PLA] = {.dst_reg = &cpu_state.reg_accum.reg_accumC,    .flag = CPU_STATUS_FLAG_MEMORY},
//    [OPCODE_PLB - OPCODE_PLA] = {.dst_reg = &cpu_state.reg_dbrw.reg_dbr},
//    [OPCODE_PLD - OPCODE_PLA] = {.dst_reg = &cpu_state.reg_d,                   .flag = 0xffff},
//    [OPCODE_PLP - OPCODE_PLA] = {.dst_reg = &cpu_state.reg_p},
//    [OPCODE_PLX - OPCODE_PLA] = {.dst_reg = &cpu_state.reg_x.reg_x,             .flag = CPU_STATUS_FLAG_INDEX},
//    [OPCODE_PLY - OPCODE_PLA] = {.dst_reg = &cpu_state.reg_y.reg_y,             .flag = CPU_STATUS_FLAG_INDEX},
//};


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


// uint32_t opcode_width(struct opcode_t *opcode)
// {
//     uint32_t width = 0;
//     switch(opcode->address_mode)
//     {
//         case ADDRESS_MODE_ABSOLUTE:
//         case ADDRESS_MODE_ABSOLUTE_INDEXED_INDIRECT:
//         case ADDRESS_MODE_ABSOLUTE_INDEXED_X:
//         case ADDRESS_MODE_ABSOLUTE_INDEXED_Y:
//         case ADDRESS_MODE_BLOCK_MOVE:
//         case ADDRESS_MODE_PROGRAM_COUNTER_RELATIVE_LONG:
//             width = 3;
//         break;

//         case ADDRESS_MODE_ABSOLUTE_INDIRECT:
//             if(opcode->opcode == CPU_OPCODE_JML)
//             {
//                 width = 4;
//             }
//             else
//             {
//                 width = 3;
//             }
//         break;

//         case ADDRESS_MODE_ABSOLUTE_LONG:
//             width = 4;
//         break;

//         case ADDRESS_MODE_ACCUMULATOR:
//         case ADDRESS_MODE_IMPLIED:
//         case ADDRESS_MODE_STACK:
//             width = 1;
//         break;

//         case ADDRESS_MODE_DIRECT_INDEXED_INDIRECT:
//         case ADDRESS_MODE_DIRECT_INDEXED_X:
//         case ADDRESS_MODE_DIRECT_INDEXED_Y:
//         case ADDRESS_MODE_DIRECT_INDIRECT_INDEXED:
//         case ADDRESS_MODE_DIRECT_INDIRECT_LONG_INDEXED:
//         case ADDRESS_MODE_DIRECT_INDIRECT_LONG:
//         case ADDRESS_MODE_DIRECT_INDIRECT:
//         case ADDRESS_MODE_DIRECT:
//         case ADDRESS_MODE_PROGRAM_COUNTER_RELATIVE:
//         case ADDRESS_MODE_STACK_RELATIVE:
//         case ADDRESS_MODE_STACK_RELATIVE_INDIRECT_INDEXED:
//             width = 2;
//         break;

//         case ADDRESS_MODE_IMMEDIATE:
//             switch(opcode->opcode)
//             {
//                 case CPU_OPCODE_LDA:
//                 case CPU_OPCODE_CMP:
//                 case CPU_OPCODE_AND:
//                 case CPU_OPCODE_EOR:
//                 case CPU_OPCODE_ORA:
//                 case CPU_OPCODE_ADC:
//                 case CPU_OPCODE_BIT:
//                 case CPU_OPCODE_SBC:
//                 case CPU_OPCODE_ROR:
//                 case CPU_OPCODE_ROL:
//                 case CPU_OPCODE_LSR:
//                 case CPU_OPCODE_ASL:
//                     if(cpu_state.reg_p.m)
//                     {
//                         width = 2;
//                     }
//                     else
//                     {
//                         width = 3;
//                     }
//                 break;

//                 case CPU_OPCODE_LDX:
//                 case CPU_OPCODE_LDY:
//                 case CPU_OPCODE_CPX:
//                 case CPU_OPCODE_CPY:
//                     if(cpu_state.reg_p.x)
//                     {
//                         width = 2;
//                     }
//                     else
//                     {
//                         width = 3;
//                     }
//                 break;

//                 default:
//                     width = 2;
//                 break;
//             }
//         break;
//     }

//     return width;
// }

// char instruction_str_buffer[512];

// char *instruction_str(uint32_t effective_address)
// {
// //    char *opcode_str;
//     const char *opcode_str;
//     char addr_mode_str[128];
// //    char flags_str[32];
//     char temp_str[64];
//     int32_t width = 0;
//     struct opcode_t opcode;

//     opcode = opcode_matrix[peek_byte(effective_address)];
//     width = opcode_width(&opcode);

//     switch(opcode.address_mode)
//     {
//         case ADDRESS_MODE_ABSOLUTE:
// //            strcpy(addr_mode_str, "absolute addr (");
//             sprintf(addr_mode_str, "DBR(%02x):addr(%04x)", cpu_state.regs[REG_DBR].word, peek_word(effective_address + 1));
// //            strcat(addr_mode_str, temp_str);
//             // for(int32_t index = width - 1; index > 0; index--)
//             // {
// //            sprintf(temp_str, "%04x", peek_word(effective_address + 1));
// //            strcat(addr_mode_str, temp_str);
//             // }
// //            strcat(addr_mode_str, ")");
//         break;

//         case ADDRESS_MODE_ABSOLUTE_INDEXED_INDIRECT:
//         {
// //            strcpy(addr_mode_str, "absolute addr ( pointer (");
// //            strcat(addr_mode_str, "00:");
// //            uint32_t pointer = peek_word(effective_address + 1);
// //            sprintf(temp_str, "%04x", pointer);
// //            strcat(addr_mode_str, temp_str);
// //            strcat(addr_mode_str, ") + ");
// //            sprintf(temp_str, "X(%04x) ) = ", cpu_state.regs[REG_X].word);
// //            strcat(addr_mode_str, temp_str);
// //            sprintf(temp_str, "(%04x)", peek_word(pointer));
// //            strcat(addr_mode_str, temp_str);
//             uint16_t address = peek_word(effective_address + 1);
//             uint16_t target = peek_word(address + cpu_state.regs[REG_X].word);
//             if(opcode.opcode == CPU_OPCODE_JMP || opcode.opcode == CPU_OPCODE_JSR)
//             {
//                 sprintf(addr_mode_str, "[addr(%04x) + X(%04x)] => PBR(%02x):%04x", address, cpu_state.regs[REG_X].word,
//                                                                             cpu_state.regs[REG_PBR].byte[0], target);
//             }
//         }
//         break;

//         case ADDRESS_MODE_ABSOLUTE_INDEXED_X:
//         {
//             uint16_t address = peek_word(effective_address + 1);
//             sprintf(addr_mode_str, "addr(%04x) + X(%04x) => DBR(%02x):%04x", address, cpu_state.regs[REG_X].word,
//                                                         cpu_state.regs[REG_DBR].byte[0], cpu_state.regs[REG_X].word + address);
//         }
// //            strcpy(addr_mode_str, "absolute addr (");
// //            sprintf(temp_str, "DBR(%02x):", cpu_state.regs[REG_DBR].word);
// //            strcat(addr_mode_str, temp_str);
// //            sprintf(temp_str, "%04x", peek_word(effective_address + 1));
// //            strcat(addr_mode_str, temp_str);
// //            strcat(addr_mode_str, ") + ");
// //            sprintf(temp_str, "X(%04x) )", cpu_state.regs[REG_X].word);
// //            strcat(addr_mode_str, temp_str);
//         break;

//         case ADDRESS_MODE_ABSOLUTE_INDEXED_Y:
//         {
//             uint16_t address = peek_word(effective_address + 1);
//             sprintf(addr_mode_str, "addr(%04x) + Y(%04x) => DBR(%02x):%04x", address, cpu_state.regs[REG_Y].word,
//                                                         cpu_state.regs[REG_DBR].byte[0], cpu_state.regs[REG_Y].word + address);
//         }
// //            strcpy(addr_mode_str, "absolute addr (");
// //            sprintf(temp_str, "DBR(%02x):", cpu_state.regs[REG_DBR].word);
// //            strcat(addr_mode_str, temp_str);
// //            sprintf(temp_str, "%04x", peek_word(effective_address + 1));
// //            strcat(addr_mode_str, temp_str);
// //            strcat(addr_mode_str, ") + ");
// //            sprintf(temp_str, "Y(%04x)", cpu_state.regs[REG_Y].word);
// //            strcat(addr_mode_str, temp_str);
//         break;

//         case ADDRESS_MODE_ABSOLUTE_INDIRECT:
//         {
//             strcpy(addr_mode_str, "absolute addr ( pointer (");
//             uint32_t pointer = peek_word(effective_address + 1);
//             // for(int32_t index = width - 1; index > 0; index--)
//             // {
//             sprintf(temp_str, "%04x", pointer);
//             strcat(addr_mode_str, temp_str);
//             // }
//             strcat(addr_mode_str, ") ) = ");
//             sprintf(temp_str, "(%04x)", peek_word(pointer));
//             strcat(addr_mode_str, temp_str);
//         }
//         break;

//         case ADDRESS_MODE_ABSOLUTE_LONG_INDEXED_X:
//         {
//             strcpy(addr_mode_str, "absolute long addr (");
//             uint32_t pointer = peek_word(effective_address + 1);
//             pointer |= (uint32_t)peek_byte(effective_address + 3) << 16;
//             // for(int32_t index = width - 1; index > 0; index--)
//             // {
//             sprintf(temp_str, "%06x", pointer);
//             strcat(addr_mode_str, temp_str);
//             // }

//             strcat(addr_mode_str, ") + ");
//             sprintf(temp_str, "X(%04x)", cpu_state.regs[REG_X].word);
//             strcat(addr_mode_str, temp_str);
//         }
//         break;

//         case ADDRESS_MODE_ABSOLUTE_LONG:
//         {
//             uint32_t address = peek_word(effective_address + 1);
//             address |= (uint32_t)peek_byte(effective_address + 3) << 16;
//             sprintf(addr_mode_str, "addrl(%06x)", address);
// //            strcpy(addr_mode_str, "absolute long addr (");
// //            uint32_t pointer = peek_word(effective_address + 1);
// //            pointer |= (uint32_t)peek_byte(effective_address + 3) << 16;
// //            sprintf(temp_str, "%06x", pointer);
// //            strcat(addr_mode_str, temp_str);
// //            strcat(addr_mode_str, ")");
//         }
//         break;

//         case ADDRESS_MODE_ACCUMULATOR:
//             addr_mode_str[0] = '\0';
//             if(opcode.opcode != CPU_OPCODE_XCE && opcode.opcode != CPU_OPCODE_TXS && opcode.opcode != CPU_OPCODE_WDM && opcode.opcode != CPU_OPCODE_WAI)
//             {
//                 sprintf(addr_mode_str, "accumulator(%04x)", cpu_state.regs[REG_ACCUM].word);
//             }
//         break;

//         case ADDRESS_MODE_BLOCK_MOVE:
//             sprintf(addr_mode_str, "dst addr(%02x:%04x), src addr(%02x:%04x)", peek_byte(effective_address + 1), cpu_state.regs[REG_Y].word,
//                                                                                peek_byte(effective_address + 2), cpu_state.regs[REG_X].word);
//         break;

//         case ADDRESS_MODE_DIRECT_INDEXED_INDIRECT:
//             strcpy(addr_mode_str, "direct indexed indirect - (d,x)");
//         break;

//         case ADDRESS_MODE_DIRECT_INDEXED_X:
//             sprintf(addr_mode_str, "D(%04x) + X(%04x) => 00:%04x", cpu_state.regs[REG_D].word, cpu_state.regs[REG_X].word,
//                                                                    cpu_state.regs[REG_D].word + cpu_state.regs[REG_X].word);
//         break;

//         case ADDRESS_MODE_DIRECT_INDEXED_Y:
//             sprintf(addr_mode_str, "D(%04x) + Y(%04x) => 00:%04x", cpu_state.regs[REG_D].word, cpu_state.regs[REG_Y].word,
//                                                                    cpu_state.regs[REG_D].word + cpu_state.regs[REG_Y].word);
//         break;

//         case ADDRESS_MODE_DIRECT_INDIRECT_INDEXED:
//         {
//             uint8_t offset = peek_byte(effective_address + 1);
//             uint16_t address = peek_word(cpu_state.regs[REG_D].word + offset);
//             sprintf(addr_mode_str, "DBR:(%02x):[D(%04x) + offset(%02x)]=(%04x) + Y(%04x)",  cpu_state.regs[REG_DBR].byte[0],
//                                                                                             cpu_state.regs[REG_D].word, offset,
//                                                                                             address, cpu_state.regs[REG_Y].word);
//         }
//         break;

//         case ADDRESS_MODE_DIRECT_INDIRECT_LONG_INDEXED:
//             strcpy(addr_mode_str, "direct indirect long indexed - [d],y");
//         break;

//         case ADDRESS_MODE_DIRECT_INDIRECT_LONG:
//             strcpy(addr_mode_str, "direct indirect long - [d]");
//         break;

//         case ADDRESS_MODE_DIRECT_INDIRECT:
//         {
//             uint8_t offset = peek_byte(effective_address + 1);
//             uint16_t address = peek_word(cpu_state.regs[REG_D].word + offset);
//             sprintf(addr_mode_str, "[D(%04x) + offset(%02x)](%04x) => DBR(%02x):%04x", address, cpu_state.regs[REG_D].word, offset,
//                                                                                         cpu_state.regs[REG_DBR].word, address);
//         }
//         break;

//         case ADDRESS_MODE_DIRECT:
//         {
//             uint8_t offset = peek_byte(effective_address + 1);
//             sprintf(addr_mode_str, "D(%04x) + offset(%02x) => 00:%04x", cpu_state.regs[REG_D].word, offset, cpu_state.regs[REG_D].word + offset);
//         }
//         break;

//         case ADDRESS_MODE_IMMEDIATE:
//             strcpy(addr_mode_str, "immediate (");

//             for(int32_t index = width - 1; index > 0; index--)
//             {
//                 sprintf(temp_str, "%02x", peek_byte(effective_address + index));
//                 strcat(addr_mode_str, temp_str);
//             }
//             strcat(addr_mode_str, ")");
//         break;

//         case ADDRESS_MODE_IMPLIED:
//             strcpy(addr_mode_str, "");
//         break;

//         case ADDRESS_MODE_PROGRAM_COUNTER_RELATIVE_LONG:
//             strcpy(addr_mode_str, "program counter relative long - rl");
//         break;

//         case ADDRESS_MODE_PROGRAM_COUNTER_RELATIVE:
//         {
//             uint16_t offset = peek_byte(effective_address + 1);
//             if(offset & 0x80)
//             {
//                 offset |= 0xff00;
//             }
//             sprintf(addr_mode_str, "PC(%04x) + offset(%02x) => PBR(%02x):%04x", cpu_state.regs[REG_PC].word + 2, (uint8_t)offset, cpu_state.regs[REG_PBR].byte[0],
//                                                                                 (uint16_t)((cpu_state.regs[REG_PC].word + 2) + offset));
//         }
//         break;

//         case ADDRESS_MODE_STACK:
//             addr_mode_str[0] = '\0';
//             if(opcode.opcode == CPU_OPCODE_PEA)
//             {
//                 sprintf(temp_str, "immediate (%04x)", peek_word(effective_address + 1));
//                 strcat(addr_mode_str, temp_str);
//             }
//         break;

//         case ADDRESS_MODE_STACK_RELATIVE:
//             sprintf(addr_mode_str, "S(%04x) + offset(%02x)", cpu_state.regs[REG_S].word, peek_byte(effective_address + 1));
//         break;

//         case ADDRESS_MODE_STACK_RELATIVE_INDIRECT_INDEXED:
//         {
//             uint8_t offset = peek_byte(effective_address + 1);
//             uint16_t pointer = peek_word(cpu_state.regs[REG_S].word + offset) + cpu_state.regs[REG_Y].word;
//             sprintf(addr_mode_str, "[S(%04x) + offset(%02x)]=(%04x) + Y(%04x)", cpu_state.regs[REG_S].word, offset, pointer, cpu_state.regs[REG_Y].word);
//         }
// //            strcpy(addr_mode_str, "stack relative indirect indexed - (d,s),y");
//         break;

//         default:
//         case ADDRESS_MODE_UNKNOWN:
//             strcpy(addr_mode_str, "unknown");
//         break;
//     }

//     opcode_str = opcode_strs[opcode.opcode];

//     sprintf(instruction_str_buffer, "[%02x:%04x]: ", (effective_address >> 16) & 0xff, effective_address & 0xffff);
//     uint32_t index;
//     for(index = 0; index < width; index++)
//     {
//         sprintf(temp_str, "%02x ", peek_byte(effective_address + index));
//         strcat(instruction_str_buffer, temp_str);
//     }

//     for(; index < 4; index++)
//     {
//         strcat(instruction_str_buffer, "   ");
//     }
// //    uint32_t instruction_bytes = 0;
// //    for(int32_t i = 0; i < width; i++)
// //    {
// //        instruction_bytes <<= 8;
// //        instruction_bytes |= peek_byte(effective_address + i);
// //    }
// //    sprintf(temp_str, "%-8x", instruction_bytes);
// //    strcat(instruction_str_buffer, temp_str);

//     strcat(instruction_str_buffer, "| ");
//     strcat(instruction_str_buffer, opcode_str);

//     if(addr_mode_str[0])
//     {
//         strcat(instruction_str_buffer, " ");
//         strcat(instruction_str_buffer, addr_mode_str);
//     }

//     return instruction_str_buffer;
// }

// char *instruction_str2(uint32_t effective_address)
// {
//     //    char *opcode_str;
// //     char opcode_str[16];
// //     char addr_mode_str[32] = "";
// //     char flags_str[32] = "";
// //     char temp_str[32] = "";
// //     char regs_str[64] = "";
// //     int32_t width = 0;
// //     struct opcode_t opcode;
// //     uint32_t show_computed_address = 0;
// //     uint32_t computed_effective_address = 0;

// //     opcode = opcode_matrix[peek_byte(effective_address)];
// //     width = opcode_width(&opcode);

// //     switch(opcode.address_mode)
// //     {
// //         case ADDRESS_MODE_ABSOLUTE:
// //         {
// //             uint16_t address = peek_word(effective_address + 1);
// //             sprintf(addr_mode_str, "$%04x", address);
// //             computed_effective_address = ((uint32_t)cpu_state.regs[REG_DBR].byte[0] << 16) | address;
// //             show_computed_address = 1;
// //         }
// //         break;

// //         case ADDRESS_MODE_ABSOLUTE_INDEXED_INDIRECT:
// //         {
// // //            uint16_t address = peek_word(effective_address + 1);
// // //            uint16_t target = peek_word(address + cpu_state.regs[REG_X].word);
// // //            if(opcode.opcode == OPCODE_JMP)
// // //            {
// // //
// // //                sprintf(addr_mode_str, "[addr(%04x) + X(%04x)] => PBR(%02x):%04x", address, cpu_state.regs[REG_X].word,
// // //                                                                            cpu_state.regs[REG_PBR].byte[0], target);
// // //            }
// //         }
// //         break;

// //         case ADDRESS_MODE_ABSOLUTE_INDEXED_X:
// //         {
// //             show_computed_address = 1;
// //             uint16_t address = peek_word(effective_address + 1);
// //             sprintf(addr_mode_str, "$%04x,x", address);
// //             computed_effective_address = (((uint32_t)cpu_state.regs[REG_DBR].byte[0] << 16) | address) + cpu_state.regs[REG_X].word;
// //         }
// //         break;

// //         case ADDRESS_MODE_ABSOLUTE_INDEXED_Y:
// //         {
// //             show_computed_address = 1;
// //             uint16_t address = peek_word(effective_address + 1);
// //             sprintf(addr_mode_str, "$%04x,y", address);
// //             computed_effective_address = (((uint32_t)cpu_state.regs[REG_DBR].byte[0] << 16) | address) + cpu_state.regs[REG_Y].word;
// //         }
// //         break;

// //         case ADDRESS_MODE_ABSOLUTE_INDIRECT:
// //         {
// // //            strcpy(addr_mode_str, "absolute addr ( pointer (");
// // //            uint32_t pointer = peek_word(effective_address + 1);
// // //            // for(int32_t index = width - 1; index > 0; index--)
// // //            // {
// // //            sprintf(temp_str, "%04x", pointer);
// // //            strcat(addr_mode_str, temp_str);
// // //            // }
// // //            strcat(addr_mode_str, ") ) = ");
// // //            sprintf(temp_str, "(%04x)", peek_word(pointer));
// // //            strcat(addr_mode_str, temp_str);
// //         }
// //         break;

// //         case ADDRESS_MODE_ABSOLUTE_LONG_INDEXED_X:
// //         {
// //             show_computed_address = 1;
// //             uint32_t address = peek_word(effective_address + 1);
// //             address |= (uint32_t)peek_byte(effective_address + 3) << 16;
// //             sprintf(addr_mode_str, "$%06x,x", address);
// //             computed_effective_address = (((uint32_t)cpu_state.regs[REG_DBR].byte[0] << 16) | address) + cpu_state.regs[REG_X].word;
// //         }
// //         break;

// //         case ADDRESS_MODE_ABSOLUTE_LONG:
// //         {
// //             show_computed_address = 1;
// //             uint32_t address = peek_word(effective_address + 1);
// //             address |= (uint32_t)peek_byte(effective_address + 3) << 16;
// //             sprintf(addr_mode_str, "$%06x", address);
// //             computed_effective_address = ((uint32_t)cpu_state.regs[REG_DBR].byte[0] << 16) | address;
// //         }
// //         break;

// //         case ADDRESS_MODE_ACCUMULATOR:
// //             // addr_mode_str[0] = '\0';
// //             if(opcode.opcode != OPCODE_XCE && opcode.opcode != OPCODE_TXS && opcode.opcode != OPCODE_WDM && opcode.opcode != OPCODE_WAI)
// //             {
// //                 // sprintf(addr_mode_str, "accumulator(%04x)", cpu_state.regs[REG_ACCUM].word);
// //                 strcpy(addr_mode_str, "a");
// //             }
// //         break;

// //         case ADDRESS_MODE_BLOCK_MOVE:
// //             // sprintf(addr_mode_str, "dst addr(%02x:%04x), src addr(%02x:%04x)", peek_byte(effective_address + 1), cpu_state.regs[REG_Y].word,
// //             //                                                                    peek_byte(effective_address + 2), cpu_state.regs[REG_X].word);
// //         break;

// //         case ADDRESS_MODE_DIRECT_INDEXED_INDIRECT:
// //             // strcpy(addr_mode_str, "direct indexed indirect - (d,x)");
// //         break;

// //         case ADDRESS_MODE_DIRECT_INDEXED_X:
// //         {
// //             uint8_t offset = peek_byte(effective_address + 1);
// //             sprintf(addr_mode_str, "$%02x,x", offset);
// //             computed_effective_address = (cpu_state.regs[REG_D].word + offset) + cpu_state.regs[REG_X].word;
// //             show_computed_address = 1;
// //         }
// //         break;

// //         case ADDRESS_MODE_DIRECT_INDEXED_Y:
// //         {
// //             uint8_t offset = peek_byte(effective_address + 1);
// //             sprintf(addr_mode_str, "$%02x,y", offset);
// //             computed_effective_address = (cpu_state.regs[REG_D].word + offset) + cpu_state.regs[REG_Y].word;
// //             show_computed_address = 1;
// //         }
// //         break;

// //         case ADDRESS_MODE_DIRECT_INDIRECT_INDEXED:
// //         // {
// //         //     uint8_t offset = peek_byte(effective_address + 1);
// //         //     uint16_t address = peek_word(cpu_state.regs[REG_D].word + offset);
// //         //     sprintf(addr_mode_str, "DBR:(%02x):[D(%04x) + offset(%02x)]=(%04x) + Y(%04x)",  cpu_state.regs[REG_DBR].byte[0],
// //         //                                                                                     cpu_state.regs[REG_D].word, offset,
// //         //                                                                                     address, cpu_state.regs[REG_Y].word);
// //         // }
// //         break;

// //         case ADDRESS_MODE_DIRECT_INDIRECT_LONG_INDEXED:
// //             // strcpy(addr_mode_str, "direct indirect long indexed - [d],y");
// //         break;

// //         case ADDRESS_MODE_DIRECT_INDIRECT_LONG:
// //             // strcpy(addr_mode_str, "direct indirect long - [d]");
// //         break;

// //         case ADDRESS_MODE_DIRECT_INDIRECT:
// //         // {
// //         //     uint8_t offset = peek_byte(effective_address + 1);
// //         //     uint16_t address = peek_word(cpu_state.regs[REG_D].word + offset);
// //         //     sprintf(addr_mode_str, "[D(%04x) + offset(%02x)](%04x) => DBR(%02x):%04x", address, cpu_state.regs[REG_D].word, offset,
// //         //                                                                                 cpu_state.regs[REG_DBR].word, address);
// //         // }
// //         break;

// //         case ADDRESS_MODE_DIRECT:
// //         {
// //             uint8_t offset = peek_byte(effective_address + 1);
// //             sprintf(addr_mode_str, "$%02x", offset);
// //             computed_effective_address = cpu_state.regs[REG_D].word + offset;
// //             show_computed_address = 1;
// //         }
// //         // {
// //         //     uint8_t offset = peek_byte(effective_address + 1);
// //         //     sprintf(addr_mode_str, "D(%04x) + offset(%02x) => 00:%04x", cpu_state.regs[REG_D].word, offset, cpu_state.regs[REG_D].word + offset);
// //         // }
// //         break;

// //         case ADDRESS_MODE_IMMEDIATE:
// //         {
// //             strcpy(addr_mode_str, "#$");
// // //            uint32_t immediate = 0;
// //             for(int32_t index = width - 1; index > 0; index--)
// //             {
// //                 sprintf(temp_str, "%02x", peek_byte(effective_address + index));
// //                 strcat(addr_mode_str, temp_str);
// // //                immediate <<= 8;
// // //                immediate |= peek_byte(effective_address + index);
// //             }

// // //            sprintf(addr_mode_str, "#$%04x", immediate);
// //         }
// //         break;

// //         case ADDRESS_MODE_IMPLIED:
// //             // strcpy(addr_mode_str, "");
// //         break;

// //         case ADDRESS_MODE_PROGRAM_COUNTER_RELATIVE_LONG:
// //             // strcpy(addr_mode_str, "program counter relative long - rl");
// //         break;

// //         case ADDRESS_MODE_PROGRAM_COUNTER_RELATIVE:
// //         {
// //             uint16_t offset = peek_byte(effective_address + 1);
// //             if(offset & 0x80)
// //             {
// //                 offset |= 0xff00;
// //             }

// //             computed_effective_address = ((uint32_t)cpu_state.regs[REG_PBR].word << 16) | ((cpu_state.regs[REG_PC].word + offset + 2) & 0xffff);
// //             show_computed_address = 1;
// //             sprintf(addr_mode_str, "$%04x", computed_effective_address & 0xffff);
// //         //     sprintf(addr_mode_str, "PC(%04x) + offset(%02x) => PBR(%02x):%04x", cpu_state.regs[REG_PC].word + 2, (uint8_t)offset, cpu_state.regs[REG_PBR].byte[0],
// //         //                                                                         (uint16_t)((cpu_state.regs[REG_PC].word + 2) + offset));
// //         }
// //         break;

// //         case ADDRESS_MODE_STACK:
// //             // addr_mode_str[0] = '\0';
// //             // if(opcode.opcode == OPCODE_PEA)
// //             // {
// //             //     sprintf(temp_str, "immediate (%04x)", peek_word(effective_address + 1));
// //             //     strcat(addr_mode_str, temp_str);
// //             // }
// //         break;

// //         case ADDRESS_MODE_STACK_RELATIVE:
// //         {
// //             uint16_t offset = peek_byte(effective_address + 1);
// //             if(offset & 0x80)
// //             {
// //                 offset |= 0xff00;
// //             }
// //             computed_effective_address = cpu_state.regs[REG_S].word + offset;
// //             show_computed_address = 1;
// //             sprintf(addr_mode_str, "$%02x,s", offset);
// //         }
// //             // sprintf(addr_mode_str, "S(%04x) + offset(%02x)", cpu_state.regs[REG_S].word, peek_byte(effective_address + 1));
// //         break;

// //         case ADDRESS_MODE_STACK_RELATIVE_INDIRECT_INDEXED:
// //         // {
// //         //     uint8_t offset = peek_byte(effective_address + 1);
// //         //     uint16_t pointer = peek_word(cpu_state.regs[REG_S].word + offset) + cpu_state.regs[REG_Y].word;
// //         //     sprintf(addr_mode_str, "[S(%04x) + offset(%02x)]=(%04x) + Y(%04x)", cpu_state.regs[REG_S].word, offset, pointer, cpu_state.regs[REG_Y].word);
// //         // }
// // //            strcpy(addr_mode_str, "stack relative indirect indexed - (d,s),y");
// //         break;

// //         default:
// //         // case ADDRESS_MODE_UNKNOWN:
// //         //     strcpy(addr_mode_str, "unknown");
// //         // break;
// //     }

// //     uint32_t addr_mode_str_len = strlen(addr_mode_str);
// //     while(addr_mode_str_len < 18)
// //     {
// //         addr_mode_str[addr_mode_str_len] = ' ';
// //         addr_mode_str_len++;
// //     }

// //     addr_mode_str[addr_mode_str_len] = '\0';

// //     // opcode_str = opcode_strs[opcode.opcode];

// //     uint32_t index = 0;
// //     while(opcode_strs[opcode.opcode][index])
// //     {
// //         opcode_str[index] = tolower(opcode_strs[opcode.opcode][index]);
// //         index++;
// //     }
// //     opcode_str[index] = '\0';

// //     if(show_computed_address)
// //     {
// //         sprintf(addr_mode_str + 10, "[%06x]", computed_effective_address);
// //     }

// //     if(cpu_state.reg_p.e)
// //     {
// //         sprintf(flags_str, "%c%c1%c%c%c%c%c", cpu_state.reg_p.n ? 'N' : 'n',
// //                                                cpu_state.reg_p.v ? 'V' : 'v',
// //                                                cpu_state.reg_p.b ? 'B' : 'b',
// //                                                cpu_state.reg_p.d ? 'D' : 'd',
// //                                                cpu_state.reg_p.i ? 'I' : 'i',
// //                                                cpu_state.reg_p.z ? 'Z' : 'z',
// //                                                cpu_state.reg_p.c ? 'C' : 'c');
// //     }
// //     else
// //     {
// //         sprintf(flags_str, "%c%c%c%c%c%c%c%c", cpu_state.reg_p.n ? 'N' : 'n',
// //                                                cpu_state.reg_p.v ? 'V' : 'v',
// //                                                cpu_state.reg_p.m ? 'M' : 'm',
// //                                                cpu_state.reg_p.x ? 'X' : 'x',
// //                                                cpu_state.reg_p.d ? 'D' : 'd',
// //                                                cpu_state.reg_p.i ? 'I' : 'i',
// //                                                cpu_state.reg_p.z ? 'Z' : 'z',
// //                                                cpu_state.reg_p.c ? 'C' : 'c');
// //     }

// //     sprintf(regs_str, "A:%04x X:%04x Y:%04x S:%04x D:%04x DB:%02x %s", cpu_state.regs[REG_ACCUM].word,
// //                                                                         cpu_state.regs[REG_X].word,
// //                                                                         cpu_state.regs[REG_Y].word,
// //                                                                         cpu_state.regs[REG_S].word,
// //                                                                         cpu_state.regs[REG_D].word,
// //                                                                         cpu_state.regs[REG_DBR].byte[0], flags_str);

// //     sprintf(instruction_str_buffer, "%06x %s %s %s V:%3d H:%4d", effective_address, opcode_str, addr_mode_str, regs_str, vcounter, scanline_cycles);

//     return instruction_str_buffer;
// }

// int dump_cpu(int show_registers)
// {
//     uint32_t address;
//     int opcode_offset;
//     struct opcode_t opcode;
//     char *op_str;
//     unsigned char *opcode_address;
//     address = cpu_state.instruction_address;

// //    op_str = instruction_str(address);
//     op_str = instruction_str(address);

//     printf("===================== CPU ========================\n");

//     if(show_registers)
//     {
//         // printf("=========== REGISTERS ===========\n");
//         printf("[A: %04x] | ", cpu_state.regs[REG_ACCUM].word);
//         printf("[X: %04x] | ", cpu_state.regs[REG_X].word);
//         printf("[Y: %04x] | ", cpu_state.regs[REG_Y].word);
//         printf("[S: %04x] | ", cpu_state.regs[REG_S].word);
//         printf("[D: %04x]\n", cpu_state.regs[REG_D].word);
//         printf("[DBR: %02x] | ", cpu_state.regs[REG_DBR].byte[0]);
//         printf("[PBR: %02x] | ", cpu_state.regs[REG_PBR].byte[0]);

//         printf("[P: N:%d V:%d ", cpu_state.reg_p.n, cpu_state.reg_p.v);

//         if(cpu_state.reg_p.e)
//         {
//             printf("B:%d ", cpu_state.reg_p.b);
//         }
//         else
//         {
//             printf("M:%d X:%d ", cpu_state.reg_p.m, cpu_state.reg_p.x);
//         }

//         printf("D:%d Z:%d I:%d C:%d]\n", cpu_state.reg_p.d, cpu_state.reg_p.z, cpu_state.reg_p.i, cpu_state.reg_p.c);
//         printf("  [E: %02x] | [PC: %04x]", cpu_state.reg_p.e, cpu_state.regs[REG_PC].word);

//         if(cpu_state.regs[REG_INST].word == CPU_BRK_S || cpu_state.regs[REG_INST].word == CPU_INT_HW)
//         {
//             char *interrupt_str = "";
//             switch(cpu_state.cur_interrupt)
//             {
//                 case CPU_INT_BRK:
//                     interrupt_str = "BRK";
//                 break;

//                 case CPU_INT_IRQ:
//                     interrupt_str = "IRQ";
//                 break;

//                 case CPU_INT_NMI:
//                     interrupt_str = "NMI";
//                 break;

//                 case CPU_INT_COP:
//                     interrupt_str = "COP";
//                 break;

//                 case CPU_INT_RES:
//                     interrupt_str = "RES";
//                 break;
//             }

//             printf(" | handling %s", interrupt_str);
//         }
//         printf("\n");

//         // printf("=========== REGISTERS ===========\n");
//         printf("-------------------------------------\n");
//     }

//     printf("%s\n", op_str);

//     printf("\n");

//     return 0;
// }

int view_hardware_registers()
{
    // int i;

    // for(i = 0; i < sizeof(hardware_regs.hardware_regs); i++)
    // {
    //     printf("<0x%04x>: 0x%02x\n", CPU_REGS_START_ADDRESS + i, hardware_regs.hardware_regs[i]);
    // }
}

uint32_t cpu_mem_cycles(uint32_t effective_address)
{
    uint32_t offset = effective_address & 0xffff;
    uint32_t bank = (effective_address >> 16) & 0xff;

    uint32_t bank_region1 = bank >= 0x00 && bank <= 0x3f;
    uint32_t bank_region2 = bank >= 0x80 && bank <= 0xbf;

    if(bank_region1 || bank_region2)
    {
        uint32_t index;
        index =  offset < 0x8000;
        index += offset < 0x6000;
        index += offset < 0x4200;
        index += offset < 0x4000;
        index += offset < 0x2000;

        return mem_speed_map[index][bank_region2 && (ram1_regs[CPU_MEM_REG_MEMSEL] & 1)];
    }
    else if(ram1_regs[CPU_MEM_REG_MEMSEL] & 1)
    {
        return MEM_SPEED_FAST_CYCLES;
    }


    return MEM_SPEED_MED_CYCLES;
}

void cpu_write_byte(uint32_t effective_address, uint8_t data)
{
//    cpu_cycle_count += ACCESS_CYCLES(effective_address);
    // cpu_cycle_count += cpu_mem_cycles(effective_address);
    write_byte(effective_address, data);
}

void cpu_write_word(uint32_t effective_address, uint16_t data)
{
    cpu_write_byte(effective_address, data);
    cpu_write_byte(effective_address + 1, (data >> 8) & 0xff);
}

uint8_t cpu_read_byte(uint32_t effective_address)
{
//    cpu_cycle_count += ACCESS_CYCLES(effective_address);
    // cpu_cycle_count += cpu_mem_cycles(effective_address);
    return read_byte(effective_address);
}

uint16_t cpu_read_word(uint32_t effective_address)
{
    return (uint16_t)cpu_read_byte(effective_address) | ((uint16_t)cpu_read_byte(effective_address + 1) << 8);
}

uint32_t cpu_read_wordbyte(uint32_t effective_address)
{
    return (uint32_t)cpu_read_word(effective_address) | ((uint32_t)cpu_read_byte(effective_address + 2) << 16);
}

void assert_nmi(uint8_t bit)
{
    cpu_state.nmi1 = cpu_state.nmi0;

    if(!cpu_state.nmi1)
    {
        cpu_state.interrupts[CPU_INT_NMI] = 1;
    }

    cpu_state.nmi0 |= 1 << bit;
}

void deassert_nmi(uint8_t bit)
{
    cpu_state.nmi0 = cpu_state.nmi1;
    cpu_state.nmi1 &= ~(1 << bit);
}

void assert_irq(uint8_t bit)
{
    cpu_state.irq |= 1 << bit;
}

void deassert_irq(uint8_t bit)
{
    cpu_state.irq &= ~(1 << bit);
}

void assert_rdy(uint8_t bit)
{
    if(!cpu_state.wai && !cpu_state.stp)
    {
        cpu_state.rdy |= 1 << bit;
    }
}

void deassert_rdy(uint8_t bit)
{
    cpu_state.rdy &= ~(1 << bit);
}

void reset_cpu()
{
    reset_core();
    /* elusive CPU delay at startup. Ripped off from bsnes. Wish I
    knew why this is needed... */
    cpu_state.uop_cycles = -22 * CPU_MASTER_CYCLES;

    ram1_regs[CPU_MEM_REG_MEMSEL] = 0;
    ram1_regs[CPU_MEM_REG_TIMEUP] = 0;
    ram1_regs[CPU_MEM_REG_HTIMEL] = 0xff;
    ram1_regs[CPU_MEM_REG_HTIMEH] = 0x01;
    ram1_regs[CPU_MEM_REG_VTIMEL] = 0xff;
    ram1_regs[CPU_MEM_REG_VTIMEH] = 0x01;
    ram1_regs[CPU_MEM_REG_MDMAEN] = 0;
    ram1_regs[CPU_MEM_REG_HDMAEN] = 0;
}

void reset_core()
{
    cpu_state.regs[CPU_REG_PC].word = 0xfffc;
    cpu_state.regs[CPU_REG_DBR].byte[0] = 0;
    cpu_state.regs[CPU_REG_PBR].byte[0] = 0;
    cpu_state.regs[CPU_REG_D].word = 0;
    cpu_state.regs[CPU_REG_X].byte[1] = 0;
    cpu_state.regs[CPU_REG_Y].byte[1] = 0;
    cpu_state.regs[CPU_REG_S].byte[1] = 0x01;
    cpu_state.regs[CPU_REG_ZERO].word = 0;
    cpu_state.reg_p.e = 1;
    cpu_state.reg_p.i = 1;
    cpu_state.reg_p.d = 0;
    cpu_state.reg_p.x = 1;
    cpu_state.reg_p.m = 1;

    cpu_state.interrupts[CPU_INT_BRK] = 0;
    cpu_state.interrupts[CPU_INT_IRQ] = 0;
    cpu_state.interrupts[CPU_INT_NMI] = 0;
    cpu_state.nmi0 = 0;
    cpu_state.nmi1 = 0;
    cpu_state.irq = 0;
    cpu_state.rdy = 1;
    cpu_state.res = 0;
    cpu_state.stp = 0;

    cpu_state.cur_interrupt = CPU_INT_RES;
    cpu_state.interrupts[cpu_state.cur_interrupt] = 1;
    cpu_state.instruction_address = EFFECTIVE_ADDRESS(cpu_state.regs[CPU_REG_PBR].byte[0], cpu_state.regs[CPU_REG_PC].word);
    cpu_state.regs[CPU_REG_INST].word = CPU_OPCODE_INT_HW;
    load_instruction();
}

// void assert_pin(uint32_t pin)
// {
//     cpu_state.pins[pin] = 0;

//     switch(pin)
//     {
// //        case CPU_PIN_IRQ:
// //            cpu_state.interrupts[CPU_INT_IRQ] = 1;
// //        break;

//         case CPU_PIN_NMI:
//             cpu_state.interrupts[CPU_INT_NMI] = 1;
//         break;
//     }
// }

// void deassert_pin(uint32_t pin)
// {
//     if(pin == CPU_PIN_RDY && cpu_state.wai)
//     {
//         return;
//     }

//     cpu_state.pins[pin] = 1;
// }

void load_instruction()
{
    cpu_state.instruction = instructions + cpu_state.regs[CPU_REG_INST].word;
    cpu_state.uop_index = 0;
    load_uop();

    if(!cpu_state.instruction->uops[0].func)
    {
        unimplemented(0);
    }
}

void load_uop()
{
    cpu_state.uop = cpu_state.instruction->uops + cpu_state.uop_index;
    cpu_state.last_uop = cpu_state.instruction->uops[cpu_state.uop_index + 1].func == NULL;
}

void next_uop()
{
    cpu_state.uop_index++;
    load_uop();
}

uint32_t step_cpu(int32_t *cycle_count)
{
    uint32_t end_of_instruction = 0;
    uint32_t run_step = cpu_state.rdy & (!cpu_state.stp);
    if(run_step)
    {
        cpu_state.uop_cycles += *cycle_count;
        cpu_state.instruction_cycles += *cycle_count;

        while(cpu_state.uop->func)
        {
            int32_t prev_uop_cycles = cpu_state.uop_cycles;

            cpu_state.run_mul = cpu_state.mul_cycles > 0;
            cpu_state.run_div = cpu_state.div_cycles > 0;

            if(cpu_state.last_uop)
            {
                if(cpu_state.irq && !cpu_state.reg_p.i && !cpu_state.interrupts[CPU_INT_NMI])
                {
                    cpu_state.interrupts[CPU_INT_IRQ] = 1;
                }
            }

            if(!cpu_state.uop->func(cpu_state.uop->arg))
            {
                break;
            }

            uint32_t spent_cycle = (cpu_state.uop_cycles - prev_uop_cycles) != 0;

            if(spent_cycle)
            {
                if(cpu_state.run_mul)
                {
                    cpu_state.mul_cycles--;
                    uint16_t current_product = (uint16_t)ram1_regs[CPU_MEM_REG_RDMPYL] | ((uint16_t)ram1_regs[CPU_MEM_REG_RDMPYH] << 8);
                    uint32_t shift = CPU_MUL_MACHINE_CYCLES - cpu_state.mul_cycles;
                    uint16_t quotient = (uint16_t)ram1_regs[CPU_MEM_REG_RDDIVL] | ((uint16_t)ram1_regs[CPU_MEM_REG_RDDIVH] << 8);

                    if(quotient & 0x01)
                    {
                        current_product += cpu_state.shifter;
                    }

                    quotient >>= 1;
                    cpu_state.shifter <<= 1;

                    ram1_regs[CPU_MEM_REG_RDMPYL] = current_product & 0xff;
                    ram1_regs[CPU_MEM_REG_RDMPYH] = (current_product >> 8) & 0xff;
                    ram1_regs[CPU_MEM_REG_RDDIVL] = quotient & 0xff;
                    ram1_regs[CPU_MEM_REG_RDDIVH] = (quotient >> 8) & 0xff;
                }

                if(cpu_state.run_div)
                {
                    cpu_state.div_cycles--;
                    uint16_t current_quotient = (uint16_t)ram1_regs[CPU_MEM_REG_RDDIVL] | ((uint16_t)ram1_regs[CPU_MEM_REG_RDDIVH] << 8);
                    uint16_t product = (uint16_t)ram1_regs[CPU_MEM_REG_RDMPYL] | ((uint16_t)ram1_regs[CPU_MEM_REG_RDMPYH] << 8);
                    current_quotient <<= 1;
                    cpu_state.shifter >>= 1;
                    if(product >= cpu_state.shifter)
                    {
                        product -= cpu_state.shifter;
                        current_quotient |= 1;
                    }

                    ram1_regs[CPU_MEM_REG_RDDIVL] = current_quotient & 0xff;
                    ram1_regs[CPU_MEM_REG_RDDIVH] = (current_quotient >> 8) & 0xff;
                    ram1_regs[CPU_MEM_REG_RDMPYL] = product & 0xff;
                    ram1_regs[CPU_MEM_REG_RDMPYH] = (product >> 8) & 0xff;
                }
            }

            cpu_state.reg_p.dl = cpu_state.regs[CPU_REG_D].byte[0] != 0;
            cpu_state.reg_p.am = cpu_state.regs[CPU_REG_ACCUM].word == 0xffff;
            next_uop();
        }
    }
    else if(cpu_state.wai && cpu_state.irq && !cpu_state.interrupts[CPU_INT_NMI])
    {
        cpu_state.interrupts[CPU_INT_IRQ] = 1;
    }

    if(!cpu_state.uop->func)
    {
        if(cpu_state.interrupts[CPU_INT_NMI])
        {
            cpu_state.rdy = 1;
            cpu_state.wai = 0;
            cpu_state.regs[CPU_REG_INST].word = CPU_OPCODE_INT_HW;
            cpu_state.cur_interrupt = CPU_INT_NMI;
            cpu_state.interrupts[CPU_INT_NMI] = 0;
        }
        else if(cpu_state.interrupts[CPU_INT_IRQ])
        {
            if(cpu_state.reg_p.i)
            {
                cpu_state.regs[CPU_REG_INST].word = CPU_OPCODE_FETCH;
                cpu_state.cur_interrupt = CPU_INT_BRK;
            }
            else
            {
                cpu_state.regs[CPU_REG_INST].word = CPU_OPCODE_INT_HW;
                cpu_state.cur_interrupt = CPU_INT_IRQ;
            }

            cpu_state.rdy = 1;
            cpu_state.wai = 0;
            cpu_state.interrupts[CPU_INT_IRQ] = 0;
        }
        else
        {
            cpu_state.regs[CPU_REG_INST].word = CPU_OPCODE_FETCH;
            cpu_state.cur_interrupt = CPU_INT_BRK;
        }

        if(!cpu_state.wai)
        {
            *cycle_count -= cpu_state.uop_cycles;
            cpu_state.uop_cycles = 0;
            // cpu_state.interrupts[cpu_state.cur_interrupt] = 0;
            cpu_state.instruction_cycles = cpu_state.uop_cycles;
            cpu_state.instruction_address = EFFECTIVE_ADDRESS(cpu_state.regs[CPU_REG_PBR].byte[0], cpu_state.regs[CPU_REG_PC].word);
            cpu_state.uop_index = 0;
            load_instruction();
        }

        end_of_instruction = 1;
    }

    return end_of_instruction;
}

void init_disasm(struct disasm_state_t *disasm_state, struct cpu_state_t *cpu_state)
{
    // disasm_state->reg_pc = cpu_state->regs[REG_PC].word;
    // disasm_state->reg_pbr = cpu_state->regs[REG_PBR].byte[0];
    disasm_state->reg_pc = cpu_state->instruction_address & 0xffff;
    disasm_state->reg_pbr = (cpu_state->instruction_address >> 16) & 0xff;
    disasm_state->reg_p = (((uint32_t)cpu_state->reg_p.m) << CPU_STATUS_FLAG_M) | 
                          (((uint32_t)cpu_state->reg_p.x) << CPU_STATUS_FLAG_X) | 
                          (((uint32_t)cpu_state->reg_p.e) << CPU_STATUS_FLAG_E);
}

uint32_t disasm(struct disasm_state_t *disasm_state, struct disasm_inst_t *instruction)
{
    uint32_t effective_address = EFFECTIVE_ADDRESS(disasm_state->reg_pbr, disasm_state->reg_pc);
    uint8_t opcode = peek_byte(effective_address);
    struct opcode_info_t *info = opcode_info + opcode;

    instruction->bytes[0] = opcode;
    instruction->width = info->width[(disasm_state->reg_p & (1 << info->width_flag)) && 1];
    instruction->opcode_str = opcode_strs[info->opcode];
    instruction->addr_mode_str = addr_mode_strs[info->addr_mode];

    for(uint32_t index = 1; index < instruction->width; index++)
    {
        effective_address = EFFECTIVE_ADDRESS(disasm_state->reg_pbr, disasm_state->reg_pc + index);
        instruction->bytes[index] = peek_byte(effective_address);;
    }

    disasm_state->reg_pc += instruction->width;
    return 1;
}

// void disasm(struct disasm_state_t *disasm_state, uint32_t instruction_count)
// {
//     while(instruction_count)
//     {
//         uint32_t effective_address = EFFECTIVE_ADDRESS(disasm_state->reg_pbr, disasm_state->reg_pc);
//         uint8_t opcode = peek_byte(effective_address);
//         struct opcode_info_t *info = opcode_info + opcode;
//         printf("[%02x:%04x]: %s\n", disasm_state->reg_pbr, disasm_state->reg_pc, opcode_strs[info->opcode]);
//         disasm_state->reg_pc += info->width[(disasm_state->reg_p & info->width_flag) && 1];
//         instruction_count--;
//     }
// }

uint8_t io_read(uint32_t effective_address)
{
    // printf("io read\n");
}

void io_write(uint32_t effective_address, uint8_t value)
{
    // printf("io write\n");
}

void nmitimen_write(uint32_t effective_address, uint8_t value)
{
    ram1_regs[CPU_MEM_REG_NMITIMEN] = value;

    if(value & (CPU_NMITIMEN_FLAG_HTIMER_EN | CPU_NMITIMEN_FLAG_VTIMER_EN))
    {
        update_irq_counter();
    }
    else
    {
        ram1_regs[CPU_MEM_REG_TIMEUP] = 0;
    }
}

uint8_t timeup_read(uint32_t effective_address)
{
    uint8_t value = ram1_regs[CPU_MEM_REG_TIMEUP];
    ram1_regs[CPU_MEM_REG_TIMEUP] = 0;
    /* bits 6-0 are open bus, so we put the last thing that was on the data bus in them */
    return value | (last_bus_value & 0x7f);
}

uint8_t rdnmi_read(uint32_t effective_address)
{
    uint8_t value = ram1_regs[CPU_MEM_REG_RDNMI];
    ram1_regs[CPU_MEM_REG_RDNMI] &= ~CPU_RDNMI_BLANK_NMI;
    /* bits 6-4 are open bus, so we put the last thing that was on the data bus in them */
    return value | (last_bus_value & 0x70);
}

uint8_t hvbjoy_read(uint32_t effective_address)
{
    uint8_t value = ram1_regs[CPU_MEM_REG_HVBJOY];
    /* bits 5-1 are open bus, so we put the last thing that was on the data bus in them */
    return value | (last_bus_value & 0x3e);
}

void wrmpyb_write(uint32_t effective_address, uint8_t value)
{
    if(cpu_state.mul_cycles == 0)
    {
        ram1_regs[CPU_MEM_REG_WRMPYB] = value;
        cpu_state.shifter = ram1_regs[CPU_MEM_REG_WRMPYB];
        ram1_regs[CPU_MEM_REG_RDDIVL] = ram1_regs[CPU_MEM_REG_WRMPYA];
        ram1_regs[CPU_MEM_REG_RDDIVH] = ram1_regs[CPU_MEM_REG_WRMPYB];
        cpu_state.mul_cycles = CPU_MUL_MACHINE_CYCLES;
        /* setting this to zero here guarantees there will be a delay of a single
        machine cycle before the multiplication begins, which is necessary for timing */
        cpu_state.run_mul = 0;
    }

    ram1_regs[CPU_MEM_REG_RDMPYL] = 0;
    ram1_regs[CPU_MEM_REG_RDMPYH] = 0;
}

void wrdivb_write(uint32_t effective_address, uint8_t value)
{
    if(cpu_state.div_cycles == 0)
    {
        ram1_regs[CPU_MEM_REG_WRDIVB] = value;
        cpu_state.shifter = (uint32_t)ram1_regs[CPU_MEM_REG_WRDIVB] << 16;
        cpu_state.div_cycles = CPU_DIV_MACHINE_CYCLES;
        /* setting this to zero here guarantees there will be a delay of a single
        machine cycle before the division begins, which is necessary for timing */
        cpu_state.run_div = 0;
//        cpu_state.latched_dividend = (uint16_t)ram1_regs[CPU_MEM_REG_WRDIVL] | ((uint16_t)ram1_regs[CPU_MEM_REG_WRDIVH] << 8);
    }

    ram1_regs[CPU_MEM_REG_RDMPYL] = ram1_regs[CPU_MEM_REG_WRDIVL];
    ram1_regs[CPU_MEM_REG_RDMPYH] = ram1_regs[CPU_MEM_REG_WRDIVH];
//    ram1_regs[CPU_MEM_REG_RDMPYL] = cpu_state.latched_dividend & 0xff;
//    ram1_regs[CPU_MEM_REG_RDMPYH] = (cpu_state.latched_dividend >> 8) & 0xff;
}



uint32_t inc_pc(uint32_t arg)
{
    cpu_state.regs[CPU_REG_PC].word += arg;
    return 1;
}

uint32_t decode(uint32_t arg)
{
    cpu_state.regs[CPU_REG_INST].byte[1] = 0;
    load_instruction();
    cpu_state.uop_index--;
    return 1;
}

uint32_t mov_lpc(uint32_t arg)
{
    arg |= ((uint32_t)CPU_REG_PBR << 16) | ((uint32_t)CPU_REG_PC << 8);
    uint32_t done = mov_l(arg);
    cpu_state.regs[CPU_REG_PC].word += done;
    return done;
}

uint32_t mov_l(uint32_t arg)
{
    uint32_t dst_reg  = (arg >> 24) & 0xff;
    uint32_t bank_reg = (arg >> 16) & 0xff;
    uint32_t addr_reg = (arg >> 8) & 0xff;
    uint32_t byte =      arg & 0xff;

    uint32_t address = EFFECTIVE_ADDRESS(cpu_state.regs[bank_reg].byte[0], cpu_state.regs[addr_reg].word);
    int32_t read_cycles = cpu_mem_cycles(address);

    if(cpu_state.uop_cycles >= read_cycles)
    {
        cpu_state.uop_cycles -= read_cycles;
        cpu_state.regs[dst_reg].byte[byte] = read_byte(address);
        return 1;
    }

    return 0;
}

uint32_t mov_s(uint32_t arg)
{
    uint32_t src_reg  = (arg >> 24) & 0xff;
    uint32_t bank_reg = (arg >> 16) & 0xff;
    uint32_t addr_reg = (arg >> 8) & 0xff;
    uint32_t byte =      arg & 0x01;

    uint32_t address = EFFECTIVE_ADDRESS(cpu_state.regs[bank_reg].byte[0], cpu_state.regs[addr_reg].word);
    int32_t read_cycles = cpu_mem_cycles(address);

    if(cpu_state.uop_cycles >= read_cycles)
    {
        cpu_state.uop_cycles -= read_cycles;
        write_byte(address, cpu_state.regs[src_reg].byte[byte]);
        return 1;
    }

    return 0;
}

uint32_t mov_rrw(uint32_t arg)
{
    uint32_t width = (arg >> 16) & 0xff;
    uint32_t src = (arg >> 8) & 0xff;
    uint32_t dst = arg & 0xff;
    struct reg_t *dst_reg = cpu_state.regs + dst;
    struct reg_t *src_reg = cpu_state.regs + src;

    if(width)
    {
        dst_reg->byte[0] = src_reg->byte[0];
    }
    else
    {
        dst_reg->word = src_reg->word;
    }

    return 1;
}

uint32_t mov_rr(uint32_t arg)
{
    uint32_t dst = arg & 0xff;
    struct reg_t *dst_reg = cpu_state.regs + dst;
    uint32_t width = cpu_state.reg_p.flags[dst_reg->flag];
    return mov_rrw(arg | width << 16);
}

uint32_t mov_p(uint32_t arg)
{
    uint32_t src = (arg >> 8) & 0xff;
    uint32_t dst = arg & 0xff;

    if(src == CPU_REG_P)
    {
        struct reg_t *dst_reg = cpu_state.regs + dst;
        dst_reg->byte[0]  = cpu_state.reg_p.c << CPU_STATUS_FLAG_C;
        dst_reg->byte[0] |= cpu_state.reg_p.z << CPU_STATUS_FLAG_Z;
        dst_reg->byte[0] |= cpu_state.reg_p.i << CPU_STATUS_FLAG_I;
        dst_reg->byte[0] |= cpu_state.reg_p.d << CPU_STATUS_FLAG_D;
        dst_reg->byte[0] |= cpu_state.reg_p.v << CPU_STATUS_FLAG_V;
        dst_reg->byte[0] |= cpu_state.reg_p.n << CPU_STATUS_FLAG_N;

        if(cpu_state.reg_p.e)
        {
            dst_reg->byte[0] |= cpu_state.reg_p.b << CPU_STATUS_FLAG_X;
            dst_reg->byte[0] |= 1 << CPU_STATUS_FLAG_M;
        }
        else
        {
            dst_reg->byte[0] |= cpu_state.reg_p.x << CPU_STATUS_FLAG_X;
            dst_reg->byte[0] |= cpu_state.reg_p.m << CPU_STATUS_FLAG_M;
        }
    }
    else
    {
        struct reg_t *src_reg = cpu_state.regs + src;
        cpu_state.reg_p.c = (src_reg->byte[0] >> CPU_STATUS_FLAG_C) & 1;
        cpu_state.reg_p.z = (src_reg->byte[0] >> CPU_STATUS_FLAG_Z) & 1;
        cpu_state.reg_p.i = (src_reg->byte[0] >> CPU_STATUS_FLAG_I) & 1;
        cpu_state.reg_p.d = (src_reg->byte[0] >> CPU_STATUS_FLAG_D) & 1;
        cpu_state.reg_p.v = (src_reg->byte[0] >> CPU_STATUS_FLAG_V) & 1;
        cpu_state.reg_p.n = (src_reg->byte[0] >> CPU_STATUS_FLAG_N) & 1;

        if(cpu_state.reg_p.e)
        {
            cpu_state.reg_p.b = (src_reg->byte[0] >> CPU_STATUS_FLAG_X) & 1;
        }
        else
        {
            cpu_state.reg_p.x = (src_reg->byte[0] >> CPU_STATUS_FLAG_X) & 1;
            cpu_state.reg_p.m = (src_reg->byte[0] >> CPU_STATUS_FLAG_M) & 1;
        }
    }

    return 1;
}

uint32_t decs(uint32_t arg)
{

    if(cpu_state.reg_p.e)
    {
        cpu_state.regs[CPU_REG_S].byte[0]--;
    }
    else
    {
        cpu_state.regs[CPU_REG_S].word--;
    }

    return 1;
}

uint32_t dec_rw(uint32_t arg)
{
    cpu_state.regs[arg].word--;
    return 1;
}

uint32_t dec_rb(uint32_t arg)
{
    cpu_state.regs[arg].byte[0]--;
    return 1;
}

uint32_t incs(uint32_t arg)
{
    if(cpu_state.reg_p.e)
    {
        cpu_state.regs[CPU_REG_S].byte[0]++;
    }
    else
    {
        cpu_state.regs[CPU_REG_S].word++;
    }

    return 1;
}

uint32_t inc_rw(uint32_t arg)
{
    cpu_state.regs[arg].word++;
    return 1;
}

uint32_t inc_rb(uint32_t arg)
{
    cpu_state.regs[arg].byte[0]++;
    return 1;
}

uint32_t zext(uint32_t arg)
{
    struct reg_t *reg = cpu_state.regs + arg;
    reg->byte[1] = 0;
    return 1;
}

uint32_t sext(uint32_t arg)
{
    struct reg_t *reg = cpu_state.regs + arg;
    reg->byte[1] = (reg->byte[0] & 0x80) ? 0xff : 0x00;
    return 1;
}

uint32_t set_p(uint32_t arg)
{
    if(arg < CPU_STATUS_FLAG_LAST)
    {
        cpu_state.reg_p.flags[arg] = 1;
    }
    else
    {
        uint32_t flags = cpu_state.regs[CPU_REG_TEMP].byte[0];

        cpu_state.reg_p.c = cpu_state.reg_p.c || (flags & (1 << CPU_STATUS_FLAG_C));
        cpu_state.reg_p.z = cpu_state.reg_p.z || (flags & (1 << CPU_STATUS_FLAG_Z));
        cpu_state.reg_p.i = cpu_state.reg_p.i || (flags & (1 << CPU_STATUS_FLAG_I));
        cpu_state.reg_p.d = cpu_state.reg_p.d || (flags & (1 << CPU_STATUS_FLAG_D));
        cpu_state.reg_p.v = cpu_state.reg_p.v || (flags & (1 << CPU_STATUS_FLAG_V));
        cpu_state.reg_p.n = cpu_state.reg_p.n || (flags & (1 << CPU_STATUS_FLAG_N));
        cpu_state.reg_p.x = cpu_state.reg_p.x || (flags & (1 << CPU_STATUS_FLAG_X));
        cpu_state.reg_p.m = cpu_state.reg_p.m || (flags & (1 << CPU_STATUS_FLAG_M));
    }

    if(cpu_state.reg_p.x)
    {
        cpu_state.regs[CPU_REG_X].byte[1] = 0;
        cpu_state.regs[CPU_REG_Y].byte[1] = 0;
    }

    return 1;
}

uint32_t clr_p(uint32_t arg)
{
    if(arg < CPU_STATUS_FLAG_LAST)
    {
        cpu_state.reg_p.flags[arg] = 0;
    }
    else
    {
        uint32_t flags = ~cpu_state.regs[CPU_REG_TEMP].byte[0];

        cpu_state.reg_p.c = cpu_state.reg_p.c && (flags & (1 << CPU_STATUS_FLAG_C));
        cpu_state.reg_p.z = cpu_state.reg_p.z && (flags & (1 << CPU_STATUS_FLAG_Z));
        cpu_state.reg_p.i = cpu_state.reg_p.i && (flags & (1 << CPU_STATUS_FLAG_I));
        cpu_state.reg_p.d = cpu_state.reg_p.d && (flags & (1 << CPU_STATUS_FLAG_D));
        cpu_state.reg_p.x = cpu_state.reg_p.x && (flags & (1 << CPU_STATUS_FLAG_X));
        cpu_state.reg_p.m = cpu_state.reg_p.m && (flags & (1 << CPU_STATUS_FLAG_M));
        cpu_state.reg_p.v = cpu_state.reg_p.v && (flags & (1 << CPU_STATUS_FLAG_V));
        cpu_state.reg_p.n = cpu_state.reg_p.n && (flags & (1 << CPU_STATUS_FLAG_N));
    }

    if(cpu_state.reg_p.e)
    {
        cpu_state.reg_p.x = 1;
        cpu_state.reg_p.m = 1;
    }

    return 1;
} 

uint32_t xce(uint32_t arg)
{
    uint32_t carry = cpu_state.reg_p.c;

    if(carry != cpu_state.reg_p.e)
    {
        if(carry)
        {
            /* M and X flags are forced to 1 when in emulation mode */
            cpu_state.reg_p.m = 1;
            cpu_state.reg_p.x = 1;
            /* the upper byte of the stack pointer is forced to 1 when in emulation mode */
            cpu_state.regs[CPU_REG_S].byte[1] = 0x01;
            /* the upper byte of the index registers are forced to 0 when in emulation mode */
            cpu_state.regs[CPU_REG_X].byte[1] = 0x00;
            cpu_state.regs[CPU_REG_Y].byte[1] = 0x00;
        }

        cpu_state.reg_p.c = cpu_state.reg_p.e;
        cpu_state.reg_p.e = carry;
    }

    return 1;
}

uint32_t xba(uint32_t arg)
{
    uint8_t msb = cpu_state.regs[CPU_REG_ACCUM].byte[1];
    cpu_state.regs[CPU_REG_ACCUM].byte[1] = cpu_state.regs[CPU_REG_ACCUM].byte[0];
    cpu_state.regs[CPU_REG_ACCUM].byte[0] = msb;
    return 1;
}

uint32_t wai(uint32_t arg)
{
    cpu_state.wai = 1;
    cpu_state.rdy = 0;
//    cpu_state.pins[CPU_PIN_RDY] = 0;
    return 1;
}

uint32_t io(uint32_t arg)
{
    if(cpu_state.uop_cycles >= 6)
    {
        cpu_state.uop_cycles -= 6;
        return 1;
    }
    return 0;
}

uint32_t addr_offr(uint32_t arg)
{
    uint32_t signed_offset = (arg >> 16) & 0xff;
    uint32_t bank_wrap = (arg >> 8) & 0xff;
    uint32_t addr_reg = arg & 0xff;
    uint32_t offset = (uint32_t)cpu_state.regs[addr_reg].word;
    uint32_t prev_addr = EFFECTIVE_ADDRESS(cpu_state.regs[CPU_REG_BANK].byte[0], cpu_state.regs[CPU_REG_ADDR].word);

    if(signed_offset && (offset & 0x8000))
    {
        offset |= 0xffff0000;
    }

    uint32_t addr = prev_addr + offset;
    cpu_state.regs[CPU_REG_ADDR].word = addr & 0xffff;

    if(!bank_wrap)
    {
        cpu_state.regs[CPU_REG_BANK].byte[0] = (addr >> 16) & 0xff;
    }

    /* crossed page boundary */
    cpu_state.reg_p.page = ((prev_addr ^ addr) & 0x00ff00) && 1;
    /* crossed bank boundary */
    cpu_state.reg_p.bank = ((prev_addr ^ addr) & 0xff0000) && bank_wrap;
    return 1;
}

uint32_t addr_offi(uint32_t arg)
{
    uint32_t signed_offset = (arg >> 24) & 0xff;
    uint32_t bank_wrap = (arg >> 16) & 0xff;
    uint16_t offset = arg & 0xffff;
    uint32_t prev_addr = EFFECTIVE_ADDRESS(cpu_state.regs[CPU_REG_BANK].byte[0], cpu_state.regs[CPU_REG_ADDR].word);

    if(signed_offset && (offset & 0x8000))
    {
        offset |= 0xffff0000;
    }

    uint32_t addr = prev_addr + (uint32_t)offset;
    cpu_state.regs[CPU_REG_ADDR].word = addr & 0xffff;

    if(!bank_wrap)
    {
        cpu_state.regs[CPU_REG_BANK].byte[0] = (addr >> 16) & 0xff;
    }

    /* crossed page boundary */
    cpu_state.reg_p.page = ((prev_addr ^ addr) & 0x00ff00) && 1;
    /* crossed bank boundary */
    cpu_state.reg_p.bank = ((prev_addr ^ addr) & 0xff0000) && bank_wrap;
    return 1;
}

uint32_t skips(uint32_t arg)
{
    uint32_t count = arg & 0xff;
    uint32_t flag = (arg >> 8) & 0xff;

    if(cpu_state.reg_p.flags[flag])
    {
        cpu_state.uop_index += count;
        load_uop();
    }

    return 1;
}

uint32_t skipc(uint32_t arg)
{
    uint32_t count = arg & 0xff;
    uint32_t flag = (arg >> 8) & 0xff;

    if(!cpu_state.reg_p.flags[flag])
    {
        cpu_state.uop_index += count;
        load_uop();
    }

    return 1;
}

uint32_t chk_znw(uint32_t arg)
{
    uint32_t reg_index = arg & 0xff;
    uint32_t width = (arg >> 8) & 0xff;
    struct reg_t *reg = cpu_state.regs + reg_index;
    cpu_state.reg_p.z = !(reg->word & alu_zero_masks[width]);
    cpu_state.reg_p.n = (reg->word & alu_sign_masks[width]) && 1;
    return 1;
}

uint32_t chk_zn(uint32_t arg)
{
    uint32_t reg_index = arg & 0xff;
    struct reg_t *reg = cpu_state.regs + reg_index;
    uint32_t width = cpu_state.reg_p.flags[reg->flag];
    arg |= width << 8;
    return chk_znw(arg);
}

uint32_t alu_op(uint32_t arg)
{
    uint32_t operand_b_reg = (arg >> 24) & 0xff;
    uint32_t operand_a_reg = (arg >> 16) & 0xff;
    uint32_t operation = (arg >> 8) & 0xff;
    uint32_t width_flag = arg & 0xff;
    uint32_t width = cpu_state.reg_p.flags[width_flag];
//    uint8_t flags = cpu_state.regs[CPU_REG_P].byte[cpu_state.reg_e];

    uint32_t sign_mask = alu_sign_masks[width];
    uint32_t carry_mask = alu_carry_masks[width];
    uint32_t zero_mask = alu_zero_masks[width];
    uint32_t carry = cpu_state.reg_p.c;
    uint32_t carry_shift = alu_carry_shifts[width];

    uint32_t operand0 = cpu_state.regs[operand_a_reg].word & zero_mask;
    uint32_t operand1 = cpu_state.regs[operand_b_reg].word & zero_mask;
    uint32_t result;

    switch(operation)
    {
        case ALU_OP_CMP:
            carry = 1;
            operand1 ^= zero_mask;
            result = operand0 + operand1 + carry;
        break;

        case ALU_OP_SUB:
            operand1 ^= zero_mask;
        case ALU_OP_ADD:
            if(cpu_state.reg_p.d)
            {

            }
            else
            {
                result = operand0 + operand1 + carry;
            }

            /* if operand0 and operand1 have the same sign, and the result have a different
            sign than the operands, then overflow ocurred */
            cpu_state.reg_p.v = (((~(operand0 ^ operand1)) & (operand1 ^ result)) & sign_mask) && 1;
        break;

        case ALU_OP_AND:
            result = (operand0 & operand1) /* | (carry << carry_shift) */;
        break;

        case ALU_OP_OR:
            result = (operand0 | operand1) /* | (carry << carry_shift) */;
        break;

        case ALU_OP_XOR:
            result = (operand0 ^ operand1) /* | (carry << carry_shift) */;
        break;

        case ALU_OP_INC:
            result = ((operand0 + 1) & zero_mask) | (carry << carry_shift);
        break;

        case ALU_OP_DEC:
            result = ((operand0 - 1) & zero_mask) | (carry << carry_shift);
        break;

        case ALU_OP_SHL:
            /* shift left shifts msb into the carry */
            carry = 0;
        case ALU_OP_ROL:
            /* rotate left shifts the carry into the lsb and the
            msb into the carry */
            result = (operand0 << 1) | carry;
        break;


        case ALU_OP_ROR:
            /* rotate right shifts the carry into the msb and the lsb
            into the carry */
            operand0 |= carry << carry_shift;
        case ALU_OP_SHR:
            carry = (operand0 & 1) << carry_shift;
            /* shift right shifts lsb into the carry */
            result = (operand0 >> 1) | carry;
        break;

        case ALU_OP_TRB:
            cpu_state.regs[CPU_REG_TEMP].word = (uint32_t)((~cpu_state.regs[CPU_REG_ACCUM].word) & zero_mask) & operand0;
            cpu_state.reg_p.z = !((cpu_state.regs[CPU_REG_ACCUM].word & operand0) & zero_mask);
            return 1;
        break;

        case ALU_OP_TSB:
            cpu_state.regs[CPU_REG_TEMP].word = (uint32_t)(cpu_state.regs[CPU_REG_ACCUM].word & zero_mask) | operand0;
            cpu_state.reg_p.z = !((cpu_state.regs[CPU_REG_ACCUM].word & operand0) & zero_mask);
            return 1;
        break;

        case ALU_OP_BIT:
            cpu_state.reg_p.n = (operand0 & sign_mask) && 1;
            cpu_state.reg_p.v = (operand0 & (sign_mask >> 1)) && 1;
        case ALU_OP_BIT_IMM:
            cpu_state.reg_p.z = !((cpu_state.regs[CPU_REG_ACCUM].word & operand0) & zero_mask);
            return 1;
        break;
    }

    // if(alu_op_carry_flag[operation])
    // {
    //     cpu_state.reg_p.c = (result & carry_mask) && 1;
    // }

//    cpu_state.reg_p.c = (result & carry_mask) && 1;
    cpu_state.reg_p.c = result > zero_mask;
    cpu_state.regs[CPU_REG_TEMP].word = result & zero_mask;
    chk_znw((width << 8) | CPU_REG_TEMP);
//    CHK_ZNW(CPU_REG_TEMP, width);

    return 1;
}

uint32_t brk(uint32_t arg) 
{
    cpu_state.regs[CPU_REG_ADDR].word = interrupt_vectors[cpu_state.cur_interrupt][cpu_state.reg_p.e];
    /* adjust PC back one byte in case this is a hardware interrupt */
    cpu_state.regs[CPU_REG_PC].word -= cpu_state.cur_interrupt == CPU_INT_IRQ || cpu_state.cur_interrupt == CPU_INT_NMI;
    cpu_state.reg_p.b = cpu_state.reg_p.e && cpu_state.cur_interrupt == CPU_INT_BRK;
    return 1;
}

uint32_t cop(uint32_t arg)
{
    cpu_state.regs[CPU_REG_INST].word = CPU_OPCODE_BRK_S;
    cpu_state.cur_interrupt = CPU_INT_COP;
    cpu_state.interrupts[CPU_INT_COP] = 1;
    decode(0);
    return 1;
}

uint32_t stp(uint32_t arg)
{
    cpu_state.stp = 1;
    cpu_state.rdy = 0;
    cpu_state.cur_interrupt = CPU_INT_RES;
    cpu_state.interrupts[CPU_INT_RES] = 1;
    return 1;
}

uint32_t unimplemented(uint32_t arg)
{
    printf("Unimplemented opcode %x at %02x:%04x\n", cpu_state.regs[CPU_REG_INST].byte[0], cpu_state.regs[CPU_REG_PBR].byte[0], cpu_state.regs[CPU_REG_PC].word);
    while(1);
    return 0;
}
