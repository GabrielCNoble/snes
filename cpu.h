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
    // OPCODE_CLASS_COMPARISON,
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


    OPCODE_LDA,                            /* Load accumulator with memory */
    OPCODE_STA,                            /* Store accumulator in memory */
    OPCODE_LDX,                            /* Load index X with memory */
    OPCODE_STX,                            /* Store index X in memory */
    OPCODE_LDY,                            /* Load index Y with memory */
    OPCODE_STY,                            /* Store index Y in memory */
    OPCODE_STZ,                            /* Store zero in memory */



    OPCODE_MVN,                            /* Block move negative */
    OPCODE_MVP,                            /* Block move positive */
    OPCODE_NOP,                            /* No operation */


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
    OPCODE_TCD,                            /* Transfer C accumulator? to direct register */
    OPCODE_TCS,                            /* Transfer C accumulator? to stack pointer */
    OPCODE_TDC,                            /* Transfer direct register to C accumulator? */
    OPCODE_TSC,                            /* Transfer stack pointer to C accumulator? */
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


enum CPU_STATUS_FLAGS
{
    CPU_STATUS_FLAG_CARRY = 1,              /* (C) */
    CPU_STATUS_FLAG_IRQ_DISABLE = 1 << 1,   /* (I) */
    CPU_STATUS_FLAG_ZERO = 1 << 2,          /* (Z) */
    CPU_STATUS_FLAG_DECIMAL = 1 << 3,       /* (D) */
    CPU_STATUS_FLAG_INDEX = 1 << 4,         /* (X) */
    CPU_STATUS_FLAG_MEMORY = 1 << 5,        /* (M) */
    CPU_STATUS_FLAG_OVERFLOW = 1 << 6,      /* (V) */
    CPU_STATUS_FLAG_NEGATIVE = 1 << 7,      /* (N) */
};

enum CPU_STORE_RESULT
{   
    CPU_STORE_RESULT_NO_STORE = 0,          /* result gets used to set some status flags, and then is thrown away */
    CPU_STORE_RESULT_REGISTER,              /* result is kept in the register */
    CPU_STORE_RESULT_MEMORY                 /* result gets written back to memory */
};

enum CPU_INTERRUPT
{
    CPU_INTERRUPT_RESET = 0,
    CPU_INTERRUPT_IRQB,
    CPU_INTERRUPT_NMIB,
    CPU_INTERRUPT_ABORTB
};

struct opcode_t
{
    unsigned char opcode;
    unsigned address_mode : 5;
    unsigned opcode_class : 3;
};

struct branch_cond_t
{
    uint8_t condition;
};

#define TPARM_INDEX(opcode) (opcode - OPCODE_TAX)
struct transfer_params_t
{
    void *src_reg;
    void *dst_reg;
    uint8_t flag;
};

struct cpu_state_t
{
    union
    {
        unsigned short reg_accumC;

        struct
        {
            unsigned char reg_accumA;
            unsigned char reg_accumB;
        };

    }reg_accum;

    uint32_t reg_temp0;
    uint32_t reg_temp1;


    union
    {
        unsigned short reg_x;

        struct
        {
            unsigned char reg_xL;
            unsigned char reg_xH;
        };

    }reg_x;


    union
    {
        unsigned short reg_y;

        struct
        {
            unsigned char reg_yL;
            unsigned char reg_yH;
        };

    }reg_y;
    
    uint8_t reg_e;         /* emulation flag */
    uint16_t reg_d;                                     /* direct register */
    uint16_t reg_s;                                     /* stack pointer register */

    uint16_t reg_pc;                                    /* program counter register */
    struct {uint8_t reg_dbr; uint8_t unused;} reg_dbrw; /* data bank register */
    struct {uint8_t reg_pbr; uint8_t unused;} reg_pbrw; /* program bank register */     
    uint8_t reg_p;                                      /* status register */
    uint8_t in_irqb;
    uint8_t in_rdy;
    uint8_t in_resb;
    uint8_t in_prev_resb;
    uint8_t in_abortb;
    uint8_t in_nmib;
    uint8_t s_wai;
};



char *instruction_str(unsigned int effective_address);

void disassemble(int start, int byte_count);

int dump_cpu(int show_registers);

void poke(uint32_t effective_address, uint32_t *value);

int view_hardware_registers();

//void exec_interrupt();

void *cpu_pointer(uint32_t effective_address, uint32_t access_location);



void cpu_write_byte(uint32_t effective_address, uint8_t data);

void cpu_write_word(uint32_t effective_address, uint16_t data);

void cpu_regs_write(uint32_t effective_address, uint32_t data, uint32_t byte_write);

void cpu_wram1_write(uint32_t effective_address, uint32_t data, uint32_t byte_write);

void cpu_wram2_write(uint32_t effective_address, uint32_t data, uint32_t byte_write);



uint8_t cpu_read_byte(uint32_t effective_address);

uint16_t cpu_read_word(uint32_t effective_address);

uint32_t cpu_read_wordbyte(uint32_t effective_address);

uint32_t cpu_regs_read(uint32_t effective_address);

uint32_t cpu_wram1_read(uint32_t effective_address);

uint32_t cpu_wram2_read(uint32_t effective_address);

uint16_t alu(uint32_t operand0, uint32_t operand1, uint32_t op, uint32_t width);

uint32_t check_int();

void reset_cpu();

uint32_t step_cpu();




#endif // CPU_H






