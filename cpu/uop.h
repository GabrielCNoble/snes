#ifndef UOP_H
#define UOP_H

#include "cpu.h"

//uint32_t op_fetch(uint32_t arg);

// uint32_t d_fetch(uint32_t arg);

#define UOP(fn, arg) ((struct uop_t){(fn), (arg)})


uint32_t inc_pc(uint32_t arg);
#define INC_PC(inc) UOP(inc_pc, inc)

uint32_t decode(uint32_t arg);
#define DECODE UOP(decode, 0)

enum MOV_BYTES
{
    MOV_LSB = 0,
    MOV_MSB
};

uint32_t mov_lpc(uint32_t arg);
#define MOV_LPC(mov, dst_reg) UOP(mov_lpc, ((uint32_t)dst_reg << 24) | ((uint32_t)mov))

uint32_t mov_l(uint32_t arg);
#define MOV_L(mov, addr_reg, bank_reg, dst_reg) UOP(mov_l, (((uint32_t)dst_reg << 24)) | ((uint32_t)bank_reg << 16) | ((uint32_t)addr_reg << 8) | ((uint32_t)mov))

uint32_t mov_s(uint32_t arg);
#define MOV_S(mov, addr_reg, bank_reg, src_reg) UOP(mov_s, ((uint32_t)src_reg << 24) | ((uint32_t)bank_reg << 16) | ((uint32_t)addr_reg << 8) | ((uint32_t)mov))

uint32_t mov_rrw(uint32_t arg);
#define MOV_RRW(src, dst, width) UOP(mov_rrw, ((uint32_t)(width) << 16) | ((uint32_t)(src) << 8) | ((uint32_t)(dst)))

uint32_t mov_rr(uint32_t arg);
#define MOV_RR(src, dst) UOP(mov_rr, ((uint32_t)(src) << 8) | ((uint32_t)(dst)))

uint32_t decs(uint32_t arg);
#define DECS UOP(decs, 0)

uint32_t incs(uint32_t arg);
#define INCS UOP(incs, 0)

uint32_t zext(uint32_t arg);
#define ZEXT(reg) UOP(zext, reg)

uint32_t sext(uint32_t arg);
#define SEXT(reg) UOP(sext, reg)

uint32_t set_p(uint32_t arg);
#define SET_P(flags) UOP(set_p, flags)

uint32_t clr_p(uint32_t arg);
#define CLR_P(flags) UOP(clr_p, flags)

uint32_t xce(uint32_t arg);
#define XCE UOP(xce, 0)

uint32_t io(uint32_t arg);
#define IO UOP(io, 0)

uint32_t addr_offr(uint32_t arg);
#define ADDR_OFFR(addr_reg, bank_wrap) UOP(addr_offr, ((uint32_t)bank_wrap << 8) | ((uint32_t)addr_reg))

uint32_t addr_offi(uint32_t arg);
#define ADDR_OFFI(offset, bank_wrap) UOP(addr_offi, ((uint32_t)bank_wrap << 16) | ((uint32_t)offset))

uint32_t skips(uint32_t arg);
#define SKIPS(count, flag) UOP(skips, ((uint32_t)flag << 8) | ((uint32_t)count))

uint32_t skipc(uint32_t arg);
#define SKIPC(count, flag) UOP(skipc, ((uint32_t)flag << 8) | ((uint32_t)count))

uint32_t chk_zn(uint32_t arg);
#define CHK_ZN(reg) UOP(chk_zn, reg)

uint32_t alu_op(uint32_t arg);
#define ALU_OP(op, width_flag, operand_a, operand_b) UOP(alu_op, ((uint32_t)operand_b << 24) | ((uint32_t)operand_a << 16) | ((uint32_t)op << 8) | ((uint32_t)width_flag))


enum ABS_FETCH_ARGS
{
    ABS_FETCH_LSB = 0,
    ABS_FETCH_MSB,
};

uint32_t abs_fetch(uint32_t arg);



// void adc(int32_t cycle_count);

// void and(int32_t cycle_count);

// void bit(int32_t cycle_count);

// void cmp(int32_t cycle_count);

// void cpx(int32_t cycle_count);

void implied_addr_mode_cycle(int32_t cycle_count);

void clc_cycle(int32_t cycle_count);

void cld_cycle(int32_t cycle_count);


#endif