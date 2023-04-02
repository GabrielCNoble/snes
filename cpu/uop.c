#include "uop.h"
#include "../mem.h"
#include <stdio.h>

extern struct cpu_state_t   cpu_state;
extern uint64_t             master_cycles;
extern struct opcode_t      opcode_matrix[];
extern struct inst_t        instructions[];
extern uint16_t             interrupt_vectors[][2];

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

uint32_t inc_pc(uint32_t arg)
{
    cpu_state.regs[REG_PC].word += arg;
    return 1;
}

uint32_t decode(uint32_t arg)
{
    cpu_state.regs[REG_INST].byte[1] = 0;
    load_instruction();
    cpu_state.uop_index--;
    return 1;
}

uint32_t mov_lpc(uint32_t arg)
{
    arg |= ((uint32_t)REG_PBR << 16) | ((uint32_t)REG_PC << 8);
    uint32_t done = mov_l(arg);
    cpu_state.regs[REG_PC].word += done;
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

    if(src == REG_P)
    {
        struct reg_t *dst_reg = cpu_state.regs + dst;
        dst_reg->byte[0]  = cpu_state.reg_p.c << STATUS_FLAG_C;
        dst_reg->byte[0] |= cpu_state.reg_p.z << STATUS_FLAG_Z;
        dst_reg->byte[0] |= cpu_state.reg_p.i << STATUS_FLAG_I;
        dst_reg->byte[0] |= cpu_state.reg_p.d << STATUS_FLAG_D;
        dst_reg->byte[0] |= cpu_state.reg_p.v << STATUS_FLAG_V;
        dst_reg->byte[0] |= cpu_state.reg_p.n << STATUS_FLAG_N;

        if(cpu_state.reg_p.e)
        {
            dst_reg->byte[0] |= cpu_state.reg_p.b << STATUS_FLAG_X;
            dst_reg->byte[0] |= 1 << STATUS_FLAG_M;
        }
        else
        {
            dst_reg->byte[0] |= cpu_state.reg_p.x << STATUS_FLAG_X;
            dst_reg->byte[0] |= cpu_state.reg_p.m << STATUS_FLAG_M;
        }
    }
    else
    {
        struct reg_t *src_reg = cpu_state.regs + src;
        cpu_state.reg_p.c = (src_reg->byte[0] >> STATUS_FLAG_C) & 1;
        cpu_state.reg_p.z = (src_reg->byte[0] >> STATUS_FLAG_Z) & 1;
        cpu_state.reg_p.i = (src_reg->byte[0] >> STATUS_FLAG_I) & 1;
        cpu_state.reg_p.d = (src_reg->byte[0] >> STATUS_FLAG_D) & 1;
        cpu_state.reg_p.v = (src_reg->byte[0] >> STATUS_FLAG_V) & 1;
        cpu_state.reg_p.n = (src_reg->byte[0] >> STATUS_FLAG_N) & 1;

        if(cpu_state.reg_p.e)
        {
            cpu_state.reg_p.b = (src_reg->byte[0] >> STATUS_FLAG_X) & 1;
        }
        else
        {
            cpu_state.reg_p.x = (src_reg->byte[0] >> STATUS_FLAG_X) & 1;
            cpu_state.reg_p.m = (src_reg->byte[0] >> STATUS_FLAG_M) & 1;
        }
    }

    return 1;
}

uint32_t decs(uint32_t arg)
{

    if(cpu_state.reg_p.e)
    {
        cpu_state.regs[REG_S].byte[0]--;
    }
    else
    {
        cpu_state.regs[REG_S].word--;
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
        cpu_state.regs[REG_S].byte[0]++;
    }
    else
    {
        cpu_state.regs[REG_S].word++;
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
    if(arg < STATUS_FLAG_LAST)
    {
        cpu_state.reg_p.flags[arg] = 1;
    }
    else
    {
        uint32_t flags = cpu_state.regs[REG_TEMP].byte[0];

        cpu_state.reg_p.c = cpu_state.reg_p.c || (flags & (1 << STATUS_FLAG_C));
        cpu_state.reg_p.z = cpu_state.reg_p.z || (flags & (1 << STATUS_FLAG_Z));
        cpu_state.reg_p.i = cpu_state.reg_p.i || (flags & (1 << STATUS_FLAG_I));
        cpu_state.reg_p.d = cpu_state.reg_p.d || (flags & (1 << STATUS_FLAG_D));
        cpu_state.reg_p.v = cpu_state.reg_p.v || (flags & (1 << STATUS_FLAG_V));
        cpu_state.reg_p.n = cpu_state.reg_p.n || (flags & (1 << STATUS_FLAG_N));
        cpu_state.reg_p.x = cpu_state.reg_p.x || (flags & (1 << STATUS_FLAG_X));
        cpu_state.reg_p.m = cpu_state.reg_p.m || (flags & (1 << STATUS_FLAG_M));
    }

    if(cpu_state.reg_p.x)
    {
        cpu_state.regs[REG_X].byte[1] = 0;
        cpu_state.regs[REG_Y].byte[1] = 0;
    }

    return 1;
}

uint32_t clr_p(uint32_t arg)
{
    if(arg < STATUS_FLAG_LAST)
    {
        cpu_state.reg_p.flags[arg] = 0;
    }
    else
    {
        uint32_t flags = ~cpu_state.regs[REG_TEMP].byte[0];

        cpu_state.reg_p.c = cpu_state.reg_p.c && (flags & (1 << STATUS_FLAG_C));
        cpu_state.reg_p.z = cpu_state.reg_p.z && (flags & (1 << STATUS_FLAG_Z));
        cpu_state.reg_p.i = cpu_state.reg_p.i && (flags & (1 << STATUS_FLAG_I));
        cpu_state.reg_p.d = cpu_state.reg_p.d && (flags & (1 << STATUS_FLAG_D));
        cpu_state.reg_p.x = cpu_state.reg_p.x && (flags & (1 << STATUS_FLAG_X));
        cpu_state.reg_p.m = cpu_state.reg_p.m && (flags & (1 << STATUS_FLAG_M));
        cpu_state.reg_p.v = cpu_state.reg_p.v && (flags & (1 << STATUS_FLAG_V));
        cpu_state.reg_p.n = cpu_state.reg_p.n && (flags & (1 << STATUS_FLAG_N));
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
            cpu_state.regs[REG_S].byte[1] = 0x01;
            /* the upper byte of the index registers are forced to 0 when in emulation mode */
            cpu_state.regs[REG_X].byte[1] = 0x00;
            cpu_state.regs[REG_Y].byte[1] = 0x00;
        }

        cpu_state.reg_p.c = cpu_state.reg_p.e;
        cpu_state.reg_p.e = carry;
    }

    return 1;
}

uint32_t xba(uint32_t arg)
{
    uint8_t msb = cpu_state.regs[REG_ACCUM].byte[1];
    cpu_state.regs[REG_ACCUM].byte[1] = cpu_state.regs[REG_ACCUM].byte[0];
    cpu_state.regs[REG_ACCUM].byte[0] = msb;
    return 1;
}

uint32_t wai(uint32_t)
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
    uint32_t prev_addr = EFFECTIVE_ADDRESS(cpu_state.regs[REG_BANK].byte[0], cpu_state.regs[REG_ADDR].word);

    if(signed_offset && (offset & 0x8000))
    {
        offset |= 0xffff0000;
    }

    uint32_t addr = prev_addr + offset;
    cpu_state.regs[REG_ADDR].word = addr & 0xffff;

    if(!bank_wrap)
    {
        cpu_state.regs[REG_BANK].byte[0] = (addr >> 16) & 0xff;
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
    uint32_t prev_addr = EFFECTIVE_ADDRESS(cpu_state.regs[REG_BANK].byte[0], cpu_state.regs[REG_ADDR].word);

    if(signed_offset && (offset & 0x8000))
    {
        offset |= 0xffff0000;
    }

    uint32_t addr = prev_addr + (uint32_t)offset;
    cpu_state.regs[REG_ADDR].word = addr & 0xffff;

    if(!bank_wrap)
    {
        cpu_state.regs[REG_BANK].byte[0] = (addr >> 16) & 0xff;
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
//    uint8_t flags = cpu_state.regs[REG_P].byte[cpu_state.reg_e];

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
            cpu_state.regs[REG_TEMP].word = (uint32_t)((~cpu_state.regs[REG_ACCUM].word) & zero_mask) & operand0;
            cpu_state.reg_p.z = !((cpu_state.regs[REG_ACCUM].word & operand0) & zero_mask);
            return 1;
        break;

        case ALU_OP_TSB:
            cpu_state.regs[REG_TEMP].word = (uint32_t)(cpu_state.regs[REG_ACCUM].word & zero_mask) | operand0;
            cpu_state.reg_p.z = !((cpu_state.regs[REG_ACCUM].word & operand0) & zero_mask);
            return 1;
        break;

        case ALU_OP_BIT:
            cpu_state.reg_p.n = (operand0 & sign_mask) && 1;
            cpu_state.reg_p.v = (operand0 & (sign_mask >> 1)) && 1;
        case ALU_OP_BIT_IMM:
            cpu_state.reg_p.z = !((cpu_state.regs[REG_ACCUM].word & operand0) & zero_mask);
            return 1;
        break;
    }

    // if(alu_op_carry_flag[operation])
    // {
    //     cpu_state.reg_p.c = (result & carry_mask) && 1;
    // }

//    cpu_state.reg_p.c = (result & carry_mask) && 1;
    cpu_state.reg_p.c = result > zero_mask;
    cpu_state.regs[REG_TEMP].word = result & zero_mask;
    chk_znw((width << 8) | REG_TEMP);
//    CHK_ZNW(REG_TEMP, width);

    return 1;
}

uint32_t brk(uint32_t arg)
{
    cpu_state.regs[REG_ADDR].word = interrupt_vectors[cpu_state.cur_interrupt][cpu_state.reg_p.e];
    /* adjust PC back one byte in case this is a hardware interrupt */
    cpu_state.regs[REG_PC].word -= cpu_state.cur_interrupt == CPU_INT_IRQ || cpu_state.cur_interrupt == CPU_INT_NMI;
    cpu_state.reg_p.b = cpu_state.reg_p.e && cpu_state.cur_interrupt == CPU_INT_BRK;
    return 1;
}

uint32_t cop(uint32_t arg)
{
    cpu_state.regs[REG_INST].word = BRK_S;
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
    printf("Unimplemented opcode %x at %02x:%04x\n", cpu_state.regs[REG_INST].byte[0], cpu_state.regs[REG_PBR].byte[0], cpu_state.regs[REG_PC].word);
    while(1);
    return 0;
}
