#ifndef CPU_H
#define CPU_H

#include "addr_common.h"
#include <stdint.h>

enum OP_CODE_CATEGORIES
{
    OP_CODE_CATEGORY_LOAD = 0,
    OP_CODE_CATEGORY_STORE,
    OP_CODE_CATEGORY_BRANCH,
    OP_CODE_CATEGORY_STACK,
    OP_CODE_CATEGORY_ARITHMETIC,
    OP_CODE_CATEGORY_COMPARISON,
    OP_CODE_CATEGORY_TRANSFER,
    OP_CODE_CATEGORY_STANDALONE,
};

enum ALU_OP
{
    ALU_OP_ADD = 0,
    ALU_OP_SUB,
    ALU_OP_AND,
    ALU_OP_OR,
    ALU_OP_EOR,
    ALU_OP_CMP,
    ALU_OP_INC,
    ALU_OP_DEC,
    ALU_OP_SHR,
    ALU_OP_SHL,
    ALU_OP_ROR,
    ALU_OP_ROL,
};

enum OP_CODES
{
    OP_CODE_ADC,                            /* Add memory to accumulator with carry */
    OP_CODE_AND,                            /* AND memory with accumulator */
    OP_CODE_ASL,                            /* Shift one bit left, memory or accumulator */

    OP_CODE_BCC,                            /* Branch on carry clear */
    OP_CODE_BCS,                            /* Branch on carry set */
    OP_CODE_BEQ,                            /* Branch if equal (Z == 1) */
    OP_CODE_BMI,                            /* Branch if result minus (N == 1) */
    OP_CODE_BNE,                            /* Branch if not equal (Z == 0) */
    OP_CODE_BPL,                            /* Branch if result plus (N == 0) */
    OP_CODE_BRA,                            /* Branch always */
    OP_CODE_BRK,                            /* Force break */
    OP_CODE_BRL,                            /* Branch always long */
    OP_CODE_BVC,                            /* Branch on overflow clear (V == 0) */
    OP_CODE_BVS,                            /* Branch on overflow set (V == 1) */
    OP_CODE_BIT,                            /* Bit test */


    OP_CODE_CLC,                            /* Clear carry flag */
    OP_CODE_CLD,                            /* Clear decimal mode */
    OP_CODE_CLV,                            /* Clear overflow flag */
    OP_CODE_CLI,                            /* Clear interrupt disable bit */


    OP_CODE_CMP,                            /* Compare memory and accumulator */
    OP_CODE_CPX,                            /* Compare memory and index X */
    OP_CODE_CPY,                            /* Compare memory and index Y */


    OP_CODE_DEC,                            /* Decrement memory or accumulator */
    OP_CODE_INC,                            /* Increment memory or accumulator */
    OP_CODE_DEX,                            /* Decrement index X */
    OP_CODE_INX,                            /* Increment index X */
    OP_CODE_DEY,                            /* Decrement index Y */
    OP_CODE_INY,                            /* Increment index Y */

    OP_CODE_JMP,                            /* Jump */
    OP_CODE_JML,                            /* Jump long */
    OP_CODE_JSL,                            /* Jump subroutine long */
    OP_CODE_JSR,                            /* Jump and save return */


    OP_CODE_LDA,                            /* Load accumulator with memory */
    OP_CODE_STA,                            /* Store accumulator in memory */
    OP_CODE_LDX,                            /* Load index X with memory */
    OP_CODE_STX,                            /* Store index X in memory */
    OP_CODE_LDY,                            /* Load index Y with memory */
    OP_CODE_STY,                            /* Store index Y in memory */
    OP_CODE_STZ,                            /* Store zero in memory */


    OP_CODE_LSR,                            /* 1 bit left shift memory or accumulator */


    OP_CODE_MVN,                            /* Block move negative */
    OP_CODE_MVP,                            /* Block move positive */
    OP_CODE_NOP,                            /* No operation */


    OP_CODE_PEA,                            /* Push effective absolute address on stack (or push immediate data on stack) */
    OP_CODE_PEI,                            /* Push effective absolute address on stack (or push direct data on stack) */
    OP_CODE_PER,                            /* Push effective program counter relative address on stack */
    OP_CODE_PHA,                            /* Push accumulator on stack */
    OP_CODE_PHB,                            /* Push data bank register on stack */
    OP_CODE_PHD,                            /* Push direct register on stack */
    OP_CODE_PHK,                            /* Push program bank register on stack */
    OP_CODE_PHP,                            /* Push processor status on stack */
    OP_CODE_PHX,                            /* Push index X on stack */
    OP_CODE_PHY,                            /* Push index Y on stack */
    OP_CODE_PLA,                            /* Pull accumulator from stack */
    OP_CODE_PLB,                            /* Pull data bank register from stack */
    OP_CODE_PLD,                            /* Pull direct register from stack */
    OP_CODE_PLP,                            /* Pull processor status from stack */
    OP_CODE_PLX,                            /* Pull index X from stack */
    OP_CODE_PLY,                            /* Pull index Y from stack */


    OP_CODE_REP,                            /* Reset status bits */


    OP_CODE_ROL,                            /* Rotate left one bit (memory or accumulator) */
    OP_CODE_ROR,                            /* Rotate right one bit (memory or accumulator) */


    OP_CODE_RTI,                            /* Return from interrupt */
    OP_CODE_RTL,                            /* Return from subroutine long */
    OP_CODE_RTS,                            /* Return from subroutine */


    OP_CODE_SBC,                            /* Subtract memory from accumulator with borrow */


    OP_CODE_SEP,                            /* Set processor status bit */
    OP_CODE_SEC,                            /* Set carry flag */
    OP_CODE_SED,                            /* Set decimal mode */
    OP_CODE_SEI,                            /* Set interrupt disable status */


    OP_CODE_TAX,                            /* Transfer accumulator to index X */
    OP_CODE_TAY,                            /* Transfer accumulator to index Y */
    OP_CODE_TCD,                            /* Transfer C accumulator? to direct register */
    OP_CODE_TCS,                            /* Transfer C accumulator? to stack pointer */
    OP_CODE_TDC,                            /* Transfer direct register to C accumulator? */
    OP_CODE_TSC,                            /* Transfer stack pointer to C accumulator? */
    OP_CODE_TSX,                            /* Transfer stack pointer to index X */
    OP_CODE_TXA,                            /* Transfer index X to accumulator */
    OP_CODE_TXS,                            /* Transfer index X to stack pointer */
    OP_CODE_TXY,                            /* Transfer index X to index Y */
    OP_CODE_TYA,                            /* Transfer index Y to accumulator */
    OP_CODE_TYX,                            /* Transfer index Y to index X */


    OP_CODE_WAI,                            /* Wait for interrupt */


    OP_CODE_WDM,                            /* Reserved */


    OP_CODE_XBA,                            /* Exchange B and A accumulator??? */


    OP_CODE_TRB,                            /* Test and reset bit */
    OP_CODE_TSB,                            /* Test and set bit */



    OP_CODE_STP,                            /* Stop the clock */


    OP_CODE_COP,                            /* Coprocessor */


    OP_CODE_EOR,                            /* Exclusive OR memory with accumulator */
    OP_CODE_ORA,                             /* OR memory with accumulator */


    OP_CODE_XCE,                            /* Exchange carry and emulation bits */

    OP_CODE_UNKNOWN,
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


enum CPU_STATUS_FLAGS
{
    CPU_STATUS_FLAG_CARRY = 1,              /* (C) */
    CPU_STATUS_FLAG_IRQ_DISABLE = 1 << 1,   /* (I) */
    CPU_STATUS_FLAG_ZERO = 1 << 1,          /* (Z) */
    CPU_STATUS_FLAG_DECIMAL = 1 << 3,       /* (D) */
    CPU_STATUS_FLAG_INDEX = 1 << 4,         /* (X) */
    CPU_STATUS_FLAG_MEMORY = 1 << 5,        /* (M) */
    CPU_STATUS_FLAG_OVERFLOW = 1 << 6,      /* (V) */
    CPU_STATUS_FLAG_NEGATIVE = 1 << 7,      /* (N) */
};

struct op_code_t
{
    unsigned char opcode;
    unsigned address_mode : 5;
    unsigned opcode_category : 3;
    // unsigned short opcode;
    // unsigned char address_mode;
    // unsigned char opcode_category;
};

#define OPCODE(opcode, category, address_mode) (struct op_code_t){opcode, address_mode, category}



enum CPU_ACCESSS
{
    CPU_ACCESS_PPU = 0,
    CPU_ACCESS_APU,
    CPU_ACCESS_WRAM1,
    CPU_ACCESS_WRAM2,
    CPU_ACCESS_REGS,
    CPU_ACCESS_ROM,
    CPU_ACCESS_CONTROLLER,
    CPU_ACCESS_DMA,
};


void reset_cpu();

char *opcode_str(unsigned int effective_address);

void disassemble(int start, int byte_count);

int disassemble_current(int show_registers);

void poke(int effective_address, int value);

int view_hardware_registers();

/*__fastcall */ int cpu_access_location(unsigned int effective_address);

/*__fastcall */ void *memory_pointer(unsigned int effective_address);

/*__fastcall */ void exec_interrupt();


void update_reg_p(uint32_t operand0, uint32_t operand1, uint32_t result, uint32_t flags);

uint32_t alu_op(uint32_t operand0, uint32_t operand1, uint32_t op, uint32_t width);

void step_cpu();




#endif // CPU_H






