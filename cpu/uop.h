#ifndef UOP_H
#define UOP_H

#include "cpu.h"

//uint32_t op_fetch(uint32_t arg);

// uint32_t d_fetch(uint32_t arg);

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


#endif
