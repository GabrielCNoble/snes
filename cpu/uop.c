#include "uop.h"
#include "../mem.h"

extern struct cpu_state_t   cpu_state;
extern uint64_t             master_cycles;
extern struct opcode_t      opcode_matrix[];
extern struct inst_t        instructions[];

uint8_t alu_op_carry_flag[ALU_OP_LAST] =
{
    [ALU_OP_ADD] = 1,
    [ALU_OP_SUB] = 1,
    [ALU_OP_SHL] = 1,
    [ALU_OP_SHR] = 1,
    [ALU_OP_ROR] = 1,
    [ALU_OP_ROL] = 1,
    [ALU_OP_CMP] = 1
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

uint32_t inc_pc(uint32_t arg)
{
    cpu_state.regs[REG_PC].word += arg;
    return 1;
}

uint32_t decode(uint32_t arg)
{
    cpu_state.instruction = instructions + cpu_state.regs[REG_INST].byte[0];
    cpu_state.cur_uop = 0;
    return 0;
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
    // if(arg)
    // {
    //     cpu_state.regs[REG_P].byte[cpu_state.reg_e] |= (uint8_t)arg;
    // }
    // else
    // {
    //     cpu_state.regs[REG_P].byte[cpu_state.reg_e] |= cpu_state.regs[REG_DATA_LATCH].byte[0];
    // }

    uint32_t flags;

    if(arg)
    {
        flags = (uint8_t)arg;
    }
    else
    {
        flags = cpu_state.regs[REG_TEMP].byte[0];
    }

    cpu_state.reg_p.c = cpu_state.reg_p.c || (flags & (1 << STATUS_FLAG_C));
    cpu_state.reg_p.z = cpu_state.reg_p.z || (flags & (1 << STATUS_FLAG_Z));
    cpu_state.reg_p.i = cpu_state.reg_p.i || (flags & (1 << STATUS_FLAG_I));
    cpu_state.reg_p.d = cpu_state.reg_p.d || (flags & (1 << STATUS_FLAG_D));
    cpu_state.reg_p.x = cpu_state.reg_p.x || (flags & (1 << STATUS_FLAG_X));
    cpu_state.reg_p.m = cpu_state.reg_p.m || (flags & (1 << STATUS_FLAG_M));
    cpu_state.reg_p.v = cpu_state.reg_p.v || (flags & (1 << STATUS_FLAG_V));
    cpu_state.reg_p.n = cpu_state.reg_p.n || (flags & (1 << STATUS_FLAG_N));

    return 1;
}

uint32_t clr_p(uint32_t arg)
{
    uint32_t flags;

    if(arg)
    {
        flags = ~(uint8_t)arg;
    }
    else
    {
        flags = ~cpu_state.regs[REG_TEMP].byte[0];
    }

    cpu_state.reg_p.c = cpu_state.reg_p.c && (flags & (1 << STATUS_FLAG_C));
    cpu_state.reg_p.z = cpu_state.reg_p.z && (flags & (1 << STATUS_FLAG_Z));
    cpu_state.reg_p.i = cpu_state.reg_p.i && (flags & (1 << STATUS_FLAG_I));
    cpu_state.reg_p.d = cpu_state.reg_p.d && (flags & (1 << STATUS_FLAG_D));
    cpu_state.reg_p.x = cpu_state.reg_p.x && (flags & (1 << STATUS_FLAG_X));
    cpu_state.reg_p.m = cpu_state.reg_p.m && (flags & (1 << STATUS_FLAG_M));
    cpu_state.reg_p.v = cpu_state.reg_p.v && (flags & (1 << STATUS_FLAG_V));
    cpu_state.reg_p.n = cpu_state.reg_p.n && (flags & (1 << STATUS_FLAG_N));

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
    uint32_t bank_wrap = (arg >> 8) & 0xff;
    uint32_t addr_reg = arg & 0xff;
    uint32_t addr_offset = cpu_state.regs[addr_reg].word;
    uint32_t prev_addr = EFFECTIVE_ADDRESS(cpu_state.regs[REG_BANK].byte[0], cpu_state.regs[REG_ADDR].word);
    uint32_t addr = prev_addr + addr_offset;
    cpu_state.regs[REG_ADDR].word = addr & 0xffff;
    cpu_state.regs[REG_BANK].byte[0] = (addr >> 16) & 0xff;
    cpu_state.reg_p.page = ((prev_addr ^ addr) & 0x00ff00) && 1;
    cpu_state.reg_p.bank = ((prev_addr ^ addr) & 0xff0000) && 1;
    return 1;
}

uint32_t addr_offi(uint32_t arg)
{
    uint32_t bank_wrap = (arg >> 16) & 0xff;
    uint16_t offset = arg & 0xffff;
    uint32_t prev_addr = EFFECTIVE_ADDRESS(cpu_state.regs[REG_BANK].byte[0], cpu_state.regs[REG_ADDR].word);
    uint32_t addr = prev_addr + (uint32_t)offset;
    cpu_state.regs[REG_ADDR].word = addr & 0xffff;
    cpu_state.regs[REG_BANK].byte[0] = (addr >> 16) & 0xff;
    cpu_state.reg_p.page = ((prev_addr ^ addr) & 0x00ff00) && 1;
    cpu_state.reg_p.bank = ((prev_addr ^ addr) & 0xff0000) && 1;
    return 1;
}

uint32_t skips(uint32_t arg)
{
    uint32_t count = arg & 0xff;
    uint32_t flag = (arg >> 8) & 0xff;

    if(cpu_state.reg_p.flags[flag])
    {
        cpu_state.cur_uop += count;
    }

    return 1;
}

uint32_t skipc(uint32_t arg)
{
    uint32_t count = arg & 0xff;
    uint32_t flag = (arg >> 8) & 0xff;

    if(!cpu_state.reg_p.flags[flag])
    {
        cpu_state.cur_uop += count;
    }

    return 1;
}

uint32_t chk_zn(uint32_t arg)
{
    uint32_t reg_index = arg & 0xff;
    struct reg_t *reg = cpu_state.regs + reg_index;
    uint32_t width = cpu_state.reg_p.flags[reg->flag];
    cpu_state.reg_p.z = !(reg->word & alu_zero_masks[width]);
    cpu_state.reg_p.n = (reg->word & alu_sign_masks[width]) && 1;
    // uint32_t zero = !(cpu_state.idata_bus.word & alu_zero_masks[width]);
    // uint32_t sign = cpu_state.idata_bus.word & alu_sign_masks[width];

    // uint8_t flags = cpu_state.regs[REG_P].byte[cpu_state.reg_e] & ~(CPU_STATUS_FLAG_ZERO | CPU_STATUS_FLAG_NEGATIVE);
    // flags = zero ? (flags | CPU_STATUS_FLAG_ZERO) : (flags & ~CPU_STATUS_FLAG_ZERO);
    // flags = sign ? (flags | CPU_STATUS_FLAG_NEGATIVE) : (flags & ~CPU_STATUS_FLAG_NEGATIVE);
    // cpu_state.regs[REG_P].byte[cpu_state.reg_e] = flags;

    return 1;
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

    uint32_t operand0 = cpu_state.regs[operand_a_reg].word;
    uint32_t operand1 = cpu_state.regs[operand_b_reg].word;
    uint32_t result;

    // operand0 &= zero_mask;
    // operand1 &= zero_mask;

    switch(operation)
    {
        case ALU_OP_CMP:
            carry = 1;
        case ALU_OP_SUB:
            operand1 = ((~operand1) & zero_mask) + 1;
            operand1 -= (!carry);
            result = operand0 + operand1;
        break;

        case ALU_OP_ADD:
            result = operand0 + operand1 + carry;
            /* if operand0 and operand1 have the same sign, and the result have a different
            sign than the operands, then overflow ocurred */
            // flags = (((~(operand0 ^ operand1)) & (operand1 ^ result)) & sign_mask) ?
            //         (flags | CPU_STATUS_FLAG_OVERFLOW) : (flags & ~CPU_STATUS_FLAG_OVERFLOW);

            cpu_state.reg_p.v = (((~(operand0 ^ operand1)) & (operand1 ^ result)) & sign_mask) && 1;
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
            cpu_state.reg_p.c = (operand0 & sign_mask) && 1;
            /* rotate left shifts the carry into the lsb */
            result = (operand0 << 1) | (operation == ALU_OP_ROL && carry);
        break;


        case ALU_OP_ROR:
            /* rotate right shifts the carry into the msb */
            operand0 = carry ? (operand0 | carry_mask) : operand0;
        case ALU_OP_SHR:
            /* shift right shifts lsb into the carry */
            cpu_state.reg_p.c = operand0 & 1;
            result = operand0 >> 1;
        break;

        case ALU_OP_TSB:

        break;

        case ALU_OP_TRB:

        break;
    }

    /* every alu op affects negative and zero */
    // flags = (result & sign_mask) ? (flags | CPU_STATUS_FLAG_NEGATIVE) : (flags & ~CPU_STATUS_FLAG_NEGATIVE);
    // flags = (result & zero_mask) ? (flags | CPU_STATUS_FLAG_ZERO) : (flags & ~CPU_STATUS_FLAG_ZERO);

    if(alu_op_carry_flag[operation])
    {
        // flags = (result & carry_mask) ? (flags | CPU_STATUS_FLAG_CARRY) : (flags & ~CPU_STATUS_FLAG_CARRY);
        cpu_state.reg_p.c = (result & carry_mask) && 1;
    }

    // cpu_state.reg_p[cpu_state.reg_e] = flags;
    cpu_state.regs[REG_TEMP].word = result;

    return 1;
}

uint32_t abs_fetch(uint32_t arg)
{
//    uint32_t read_cycles = cpu_mem_cycles(cpu_state.cur_effective_address);
//
//    if(cpu_state.step_cycles >= read_cycles)
//    {
//        switch(arg)
//        {
//            case ABS_FETCH_LSB:
//                cpu_state.data_latchl = read_byte(cpu_state.cur_effective_address);
//            break;
//
//            case ABS_FETCH_MSB:
//                cpu_state.data_latchh |= (uint16_t)read_byte(cpu_state.cur_effective_address) << 8;
//            break;
//        }
//        cpu_state.reg_pc++;
//        cpu_state.cur_effective_address = EFFECTIVE_ADDRESS(cpu_state.reg_pbrw.reg_pbr, cpu_state.reg_pc);
//        return 1;
//    }
//
//    return 0;
}
