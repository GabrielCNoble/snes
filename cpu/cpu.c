#include "cpu.h"
#include "../ppu.h"
#include "../cart.h"
#include "../mem.h"
#include "uop.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <stdint.h>

unsigned long step_count = 0;

struct cpu_state_t cpu_state = {
//    .in_irqb = 1,
//    .in_rdy = 1,
//    .in_resb = 1,
//    .in_abortb = 1,
//    .in_nmib = 1,
    .regs = (struct reg_t []) {
        [REG_ACCUM]         = { .flag = STATUS_FLAG_M },
        [REG_X]             = { .flag = STATUS_FLAG_X },
        [REG_Y]             = { .flag = STATUS_FLAG_X },
        [REG_D]             = { },
        [REG_S]             = { },
        [REG_PBR]           = { },
        [REG_DBR]           = { },
        [REG_INST]          = { },
        [REG_PC]            = { },
        // [REG_P]             = { .flag = 0x0000},
        [REG_ADDR]          = { },
        [REG_BANK]          = { },
        [REG_ZERO]          = { },
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
    [CPU_INT_COP] = {0xffe4, 0xfff4}
};

uint32_t            cpu_cycle_count = 0;
extern uint64_t     master_cycles;
extern uint8_t *    ram1_regs;
extern uint8_t      last_bus_value;

#define ALU_WIDTH_WORD 0
#define ALU_WIDTH_BYTE 1

#define OPCODE(op, cat, addr_mod) {.opcode = op, .address_mode = addr_mod, .opcode_class = cat}

struct inst_t fetch_inst = {
    .uops = {
        MOV_LPC         (MOV_LSB, REG_INST),
        DECODE
    }
};

struct inst_t instructions[] = {

    /**************************************************************************************/
    /*                                      ADC                                           */
    /**************************************************************************************/

    [ADC_ABS] = {
        .uops = {
            MOV_LPC     (MOV_LSB, REG_ADDR),
            MOV_LPC     (MOV_MSB, REG_ADDR),
            MOV_L       (MOV_LSB, REG_ADDR, REG_DBR, REG_TEMP),
            SKIPS       (2, STATUS_FLAG_M),
            ADDR_OFFI   (1, ADDR_OFF_BANK_WRAP),
            MOV_L       (MOV_MSB, REG_ADDR, REG_DBR, REG_TEMP),
            ALU_OP      (ALU_OP_ADD, STATUS_FLAG_M, REG_ACCUM, REG_TEMP),
            MOV_RR      (REG_TEMP, REG_ACCUM),
            // CHK_ZN      (REG_ACCUM),
        }
    },
    [ADC_ABS_X] = {
        .uops = {
            MOV_LPC     (MOV_LSB, REG_ADDR),
            MOV_LPC     (MOV_MSB, REG_ADDR),
            MOV_RRW     (REG_DBR, REG_BANK, MOV_RRW_BYTE),
            ADDR_OFFR   (REG_X, ADDR_OFF_BANK_NEXT),
            SKIPC       (2, STATUS_FLAG_PAGE),
            SKIPS       (1, STATUS_FLAG_X),
            IO          /* X=0 or page boundary */,
            MOV_L       (MOV_LSB, REG_ADDR, REG_BANK, REG_TEMP),
            SKIPS       (2, STATUS_FLAG_M),
            ADDR_OFFI   (1, ADDR_OFF_BANK_NEXT),
            MOV_L       (MOV_MSB, REG_ADDR, REG_BANK, REG_TEMP),
            ALU_OP      (ALU_OP_ADD, STATUS_FLAG_M, REG_ACCUM, REG_TEMP),
            MOV_RR      (REG_TEMP, REG_ACCUM),
            // CHK_ZN      (REG_ACCUM),
        }
    },
    [ADC_ABS_Y] = {
        .uops = {
            MOV_LPC     (MOV_LSB, REG_ADDR),
            MOV_LPC     (MOV_MSB, REG_ADDR),
            MOV_RRW     (REG_DBR, REG_BANK, MOV_RRW_BYTE),
            ADDR_OFFR   (REG_Y, ADDR_OFF_BANK_NEXT),
            SKIPC       (2, STATUS_FLAG_PAGE),
            SKIPS       (1, STATUS_FLAG_X),
            IO          /* X=0 or page boundary */,
            MOV_L       (MOV_LSB, REG_ADDR, REG_BANK, REG_TEMP),
            SKIPS       (2, STATUS_FLAG_M),
            ADDR_OFFI   (1, ADDR_OFF_BANK_NEXT),
            MOV_L       (MOV_MSB, REG_ADDR, REG_BANK, REG_TEMP),
            ALU_OP      (ALU_OP_ADD, STATUS_FLAG_M, REG_ACCUM, REG_TEMP),
            MOV_RR      (REG_TEMP, REG_ACCUM),
            // CHK_ZN      (REG_ACCUM),
        }
    },
    [ADC_ABSL] = {
        .uops = {
            MOV_LPC     (MOV_LSB, REG_ADDR),
            MOV_LPC     (MOV_MSB, REG_ADDR),
            MOV_LPC     (MOV_LSB, REG_BANK),
            MOV_L       (MOV_LSB, REG_ADDR, REG_BANK, REG_TEMP),
            SKIPS       (2, STATUS_FLAG_M),
            ADDR_OFFI   (1, ADDR_OFF_BANK_WRAP),
            MOV_L       (MOV_MSB, REG_ADDR, REG_BANK, REG_TEMP),
            ALU_OP      (ALU_OP_ADD, STATUS_FLAG_M, REG_ACCUM, REG_TEMP),
            MOV_RR      (REG_TEMP, REG_ACCUM),
            // CHK_ZN      (REG_ACCUM),
        }
    },
    [ADC_ABSL_X] = {
        .uops = {
            MOV_LPC     (MOV_LSB, REG_ADDR),
            MOV_LPC     (MOV_MSB, REG_ADDR),
            MOV_LPC     (MOV_LSB, REG_BANK),
            ADDR_OFFR   (REG_X, ADDR_OFF_BANK_NEXT),
            MOV_L       (MOV_LSB, REG_ADDR, REG_BANK, REG_TEMP),
            SKIPS       (2, STATUS_FLAG_M),
            ADDR_OFFI   (1, ADDR_OFF_BANK_NEXT),
            MOV_L       (MOV_MSB, REG_ADDR, REG_BANK, REG_TEMP),
            ALU_OP      (ALU_OP_ADD, STATUS_FLAG_M, REG_ACCUM, REG_TEMP),
            MOV_RR      (REG_TEMP, REG_ACCUM),
            // CHK_ZN      (REG_ACCUM),
        }
    },
    [ADC_DIR] = {
        .uops = {
            MOV_LPC     (MOV_LSB, REG_ADDR),
            ZEXT        (REG_ADDR),
            ADDR_OFFR   (REG_D, ADDR_OFF_BANK_WRAP),
            SKIPC       (1, STATUS_FLAG_DL),
            IO          /* DL != 0 */,
            MOV_L       (MOV_LSB, REG_ADDR, REG_ZERO, REG_TEMP),
            SKIPS       (2, STATUS_FLAG_M),
            ADDR_OFFI   (1, ADDR_OFF_BANK_WRAP),
            MOV_L       (MOV_MSB, REG_ADDR, REG_ZERO, REG_TEMP),
            ALU_OP      (ALU_OP_ADD, STATUS_FLAG_M, REG_ACCUM, REG_TEMP),
            MOV_RR      (REG_TEMP, REG_ACCUM),
            // CHK_ZN      (REG_ACCUM),
        }
    },
    [ADC_S_REL] = {
        .uops = {
            MOV_LPC     (MOV_LSB, REG_ADDR),
            ZEXT        (REG_ADDR),
            ADDR_OFFR   (REG_S, ADDR_OFF_BANK_WRAP),
            IO,
            MOV_L       (MOV_LSB, REG_ADDR, REG_ZERO, REG_TEMP),
            SKIPS       (2, STATUS_FLAG_M),
            ADDR_OFFI   (1, ADDR_OFF_BANK_WRAP),
            MOV_L       (MOV_MSB, REG_ADDR, REG_ZERO, REG_TEMP),
            ALU_OP      (ALU_OP_ADD, STATUS_FLAG_M, REG_ACCUM, REG_TEMP),
            MOV_RR      (REG_TEMP, REG_ACCUM),
            // CHK_ZN      (REG_ACCUM),
        },
    },
    [ADC_DIR_X] = {
        .uops = {
            MOV_LPC     (MOV_LSB, REG_ADDR),
            ZEXT        (REG_ADDR),
            SKIPC       (1, STATUS_FLAG_DL),
            IO          /* DL != 0 */,
            ADDR_OFFR   (REG_D, ADDR_OFF_BANK_WRAP),
            ADDR_OFFR   (REG_X, ADDR_OFF_BANK_WRAP),
            IO,
            MOV_L       (MOV_LSB, REG_ADDR, REG_ZERO, REG_TEMP),
            SKIPS       (2, STATUS_FLAG_M),
            ADDR_OFFI   (1, ADDR_OFF_BANK_WRAP),
            MOV_L       (MOV_MSB, REG_ADDR, REG_ZERO, REG_TEMP),
            ALU_OP      (ALU_OP_ADD, STATUS_FLAG_M, REG_ACCUM, REG_TEMP),
            MOV_RR      (REG_TEMP, REG_ACCUM),
            // CHK_ZN      (REG_ACCUM),
        }
    },

    [ADC_DIR_IND] = {
        .uops = {
            MOV_LPC     (MOV_LSB, REG_ADDR),
            ZEXT        (REG_ADDR),
            SKIPC       (1, STATUS_FLAG_DL),
            IO          /* DL != 0 */,
            ADDR_OFFR   (REG_D, ADDR_OFF_BANK_WRAP),
            MOV_L       (MOV_LSB, REG_ADDR, REG_ZERO, REG_TEMP),
            ADDR_OFFI   (1, ADDR_OFF_BANK_WRAP),
            MOV_L       (MOV_MSB, REG_ADDR, REG_ZERO, REG_TEMP),
            MOV_RRW     (REG_TEMP, REG_ADDR, MOV_RRW_WORD),
            MOV_L       (MOV_LSB, REG_ADDR, REG_DBR, REG_TEMP),
            SKIPS       (2, STATUS_FLAG_M),
            ADDR_OFFI   (1, ADDR_OFF_BANK_WRAP),
            MOV_L       (MOV_MSB, REG_ADDR, REG_DBR, REG_TEMP),
            ALU_OP      (ALU_OP_ADD, STATUS_FLAG_M, REG_ACCUM, REG_TEMP),
            MOV_RR      (REG_TEMP, REG_ACCUM),
            // CHK_ZN      (REG_ACCUM),
        }
    },

    [ADC_DIR_INDL] = {
        .uops = {
            MOV_LPC     (MOV_LSB, REG_ADDR),
            ZEXT        (REG_ADDR),
            SKIPC       (1, STATUS_FLAG_DL),
            IO          /* DL != 0 */,
            ADDR_OFFR   (REG_D, ADDR_OFF_BANK_WRAP),
            MOV_L       (MOV_LSB, REG_ADDR, REG_ZERO, REG_TEMP),
            ADDR_OFFI   (1, ADDR_OFF_BANK_WRAP),
            MOV_L       (MOV_MSB, REG_ADDR, REG_ZERO, REG_TEMP),
            ADDR_OFFI   (1, ADDR_OFF_BANK_WRAP),
            MOV_L       (MOV_LSB, REG_ADDR, REG_ZERO, REG_BANK),
            MOV_RRW     (REG_TEMP, REG_ADDR, MOV_RRW_WORD),
            MOV_L       (MOV_LSB, REG_ADDR, REG_BANK, REG_TEMP),
            SKIPS       (2, STATUS_FLAG_M),
            ADDR_OFFI   (1, ADDR_OFF_BANK_WRAP),
            MOV_L       (MOV_MSB, REG_ADDR, REG_BANK, REG_TEMP),
            ALU_OP      (ALU_OP_ADD, STATUS_FLAG_M, REG_ACCUM, REG_TEMP),
            MOV_RR      (REG_TEMP, REG_ACCUM),
            // CHK_ZN      (REG_ACCUM),
        }
    },

    [ADC_S_REL_IND_Y] = {
        .uops = {
            MOV_LPC     (MOV_LSB, REG_ADDR),
            ZEXT        (REG_ADDR),
            IO,
            ADDR_OFFR   (REG_S, ADDR_OFF_BANK_WRAP),
            MOV_L       (MOV_LSB, REG_ADDR, REG_ZERO, REG_TEMP),
            ADDR_OFFI   (1, ADDR_OFF_BANK_WRAP),
            MOV_L       (MOV_MSB, REG_ADDR, REG_ZERO, REG_TEMP),
            MOV_RRW     (REG_TEMP, REG_ADDR, MOV_RRW_WORD),
            MOV_RRW     (REG_DBR, REG_BANK, MOV_RRW_BYTE),
            IO,
            ADDR_OFFR   (REG_Y, ADDR_OFF_BANK_NEXT),
            MOV_L       (MOV_LSB, REG_ADDR, REG_BANK, REG_TEMP),
            SKIPS       (2, STATUS_FLAG_M),
            ADDR_OFFI   (1, ADDR_OFF_BANK_NEXT),
            MOV_L       (MOV_MSB, REG_ADDR, REG_BANK, REG_TEMP),
            ALU_OP      (ALU_OP_ADD, STATUS_FLAG_M, REG_ACCUM, REG_TEMP),
            MOV_RR      (REG_TEMP, REG_ACCUM),
            // CHK_ZN      (REG_ACCUM),

        }
    },

    [ADC_DIR_X_IND] = {
        .uops = {
            MOV_LPC     (MOV_LSB, REG_ADDR),
            ZEXT        (REG_ADDR),
            SKIPC       (1, STATUS_FLAG_DL),
            IO          /* DL != 0 */,
            ADDR_OFFR   (REG_D, ADDR_OFF_BANK_WRAP),
            ADDR_OFFR   (REG_X, ADDR_OFF_BANK_WRAP),
            IO,
            MOV_L       (MOV_LSB, REG_ADDR, REG_ZERO, REG_TEMP),
            ADDR_OFFI   (1, ADDR_OFF_BANK_WRAP),
            MOV_L       (MOV_MSB, REG_ADDR, REG_ZERO, REG_TEMP),
            MOV_RRW     (REG_TEMP, REG_ADDR, MOV_RRW_WORD),
            MOV_L       (MOV_LSB, REG_ADDR, REG_DBR, REG_TEMP),
            SKIPS       (2, STATUS_FLAG_M),
            ADDR_OFFI   (1, ADDR_OFF_BANK_WRAP),
            MOV_L       (MOV_MSB, REG_ADDR, REG_DBR, REG_TEMP),
            ALU_OP      (ALU_OP_ADD, STATUS_FLAG_M, REG_ACCUM, REG_TEMP),
            MOV_RR      (REG_TEMP, REG_ACCUM),
            // CHK_ZN      (REG_ACCUM),
        }
    },

    [ADC_DIR_IND_Y] = {
        .uops = {
            MOV_LPC     (MOV_LSB, REG_ADDR),
            ZEXT        (REG_ADDR),
            SKIPC       (1, STATUS_FLAG_DL),
            IO          /* DL != 0 */,
            ADDR_OFFR   (REG_D, ADDR_OFF_BANK_WRAP),
            MOV_L       (MOV_LSB, REG_ADDR, REG_ZERO, REG_TEMP),
            ADDR_OFFI   (1, ADDR_OFF_BANK_WRAP),
            MOV_L       (MOV_MSB, REG_ADDR, REG_ZERO, REG_TEMP),
            MOV_RRW     (REG_TEMP, REG_ADDR, MOV_RRW_WORD),
            MOV_RRW     (REG_DBR, REG_BANK, MOV_RRW_BYTE),
            ADDR_OFFR   (REG_Y, ADDR_OFF_BANK_NEXT),
            SKIPS       (2, STATUS_FLAG_X),
            SKIPC       (1, STATUS_FLAG_PAGE),
            IO          /* X = 0 or page crossed */,
            MOV_L       (MOV_LSB, REG_ADDR, REG_BANK, REG_TEMP),
            SKIPS       (2, STATUS_FLAG_M),
            ADDR_OFFI   (1, ADDR_OFF_BANK_NEXT),
            MOV_L       (MOV_MSB, REG_ADDR, REG_BANK, REG_TEMP),
            ALU_OP      (ALU_OP_ADD, STATUS_FLAG_M, REG_ACCUM, REG_TEMP),
            MOV_RR      (REG_TEMP, REG_ACCUM),
            // CHK_ZN      (REG_ACCUM),
        }
    },

    [ADC_DIR_INDL_Y] = {
        .uops = {
            MOV_LPC     (MOV_LSB, REG_ADDR),
            ZEXT        (REG_ADDR),
            ADDR_OFFR   (REG_D, 0),
            SKIPC       (1, STATUS_FLAG_DL),
            IO          /* DL != 0 */,
            MOV_L       (MOV_LSB, REG_ADDR, REG_ZERO, REG_TEMP),
            ADDR_OFFI   (1, ADDR_OFF_BANK_WRAP),
            MOV_L       (MOV_MSB, REG_ADDR, REG_ZERO, REG_TEMP),
            ADDR_OFFI   (1, ADDR_OFF_BANK_WRAP),
            MOV_L       (MOV_LSB, REG_ADDR, REG_ZERO, REG_BANK),
            MOV_RRW     (REG_TEMP, REG_ADDR, MOV_RRW_WORD),
            // MOV_RRW     (REG_DBR, REG_BANK, MOV_RRW_BYTE),
            ADDR_OFFR   (REG_Y, ADDR_OFF_BANK_NEXT),
            MOV_L       (MOV_LSB, REG_ADDR, REG_BANK, REG_TEMP),
            SKIPS       (2, STATUS_FLAG_M),
            ADDR_OFFI   (1, ADDR_OFF_BANK_NEXT),
            MOV_L       (MOV_MSB, REG_ADDR, REG_BANK, REG_TEMP),
            ALU_OP      (ALU_OP_ADD, STATUS_FLAG_M, REG_ACCUM, REG_TEMP),
            MOV_RR      (REG_TEMP, REG_ACCUM),
            // CHK_ZN      (REG_ACCUM),
        }
    },

    [ADC_IMM] = {
        .uops = {
            MOV_LPC     (MOV_LSB, REG_TEMP),
            SKIPS       (1, STATUS_FLAG_M),
            MOV_LPC     (MOV_MSB, REG_TEMP),
            ALU_OP      (ALU_OP_ADD, STATUS_FLAG_M, REG_ACCUM, REG_TEMP),
            MOV_RR      (REG_TEMP, REG_ACCUM),
            // CHK_ZN      (REG_ACCUM),
        }
    },

    /**************************************************************************************/
    /*                                      AND                                           */
    /**************************************************************************************/

    [AND_ABS] = {
        .uops = {
            MOV_LPC     (MOV_LSB, REG_ADDR),
            MOV_LPC     (MOV_MSB, REG_ADDR),
            MOV_L       (MOV_LSB, REG_ADDR, REG_DBR, REG_TEMP),
            SKIPS       (2, STATUS_FLAG_M),
            ADDR_OFFI   (1, ADDR_OFF_BANK_WRAP),
            MOV_L       (MOV_MSB, REG_ADDR, REG_DBR, REG_TEMP),
            ALU_OP      (ALU_OP_AND, STATUS_FLAG_M, REG_ACCUM, REG_TEMP),
            MOV_RR      (REG_TEMP, REG_ACCUM),
            // CHK_ZN      (REG_ACCUM),
        }
    },
    [AND_ABS_X] = {
        .uops = {
            MOV_LPC     (MOV_LSB, REG_ADDR),
            MOV_LPC     (MOV_MSB, REG_ADDR),
            MOV_RRW     (REG_DBR, REG_BANK, MOV_RRW_BYTE),
            ADDR_OFFR   (REG_X, ADDR_OFF_BANK_NEXT),
            SKIPC       (2, STATUS_FLAG_PAGE),
            SKIPS       (1, STATUS_FLAG_X),
            IO          /* X=0 or page boundary */,
            MOV_L       (MOV_LSB, REG_ADDR, REG_BANK, REG_TEMP),
            SKIPS       (2, STATUS_FLAG_M),
            ADDR_OFFI   (1, ADDR_OFF_BANK_NEXT),
            MOV_L       (MOV_MSB, REG_ADDR, REG_BANK, REG_TEMP),
            ALU_OP      (ALU_OP_AND, STATUS_FLAG_M, REG_ACCUM, REG_TEMP),
            MOV_RR      (REG_TEMP, REG_ACCUM),
            // CHK_ZN      (REG_ACCUM),
        }
    },
    [AND_ABS_Y] = {
        .uops = {
            MOV_LPC     (MOV_LSB, REG_ADDR),
            MOV_LPC     (MOV_MSB, REG_ADDR),
            MOV_RRW     (REG_DBR, REG_BANK, MOV_RRW_BYTE),
            ADDR_OFFR   (REG_Y, ADDR_OFF_BANK_NEXT),
            SKIPC       (2, STATUS_FLAG_PAGE),
            SKIPS       (1, STATUS_FLAG_X),
            IO          /* X=0 or page boundary */,
            MOV_L       (MOV_LSB, REG_ADDR, REG_BANK, REG_TEMP),
            SKIPS       (2, STATUS_FLAG_M),
            ADDR_OFFI   (1, ADDR_OFF_BANK_NEXT),
            MOV_L       (MOV_MSB, REG_ADDR, REG_BANK, REG_TEMP),
            ALU_OP      (ALU_OP_AND, STATUS_FLAG_M, REG_ACCUM, REG_TEMP),
            MOV_RR      (REG_TEMP, REG_ACCUM),
            // CHK_ZN      (REG_ACCUM),
        }
    },
    [AND_ABSL] = {
        .uops = {
            MOV_LPC     (MOV_LSB, REG_ADDR),
            MOV_LPC     (MOV_MSB, REG_ADDR),
            MOV_LPC     (MOV_LSB, REG_BANK),
            MOV_L       (MOV_LSB, REG_ADDR, REG_BANK, REG_TEMP),
            SKIPS       (2, STATUS_FLAG_M),
            ADDR_OFFI   (1, ADDR_OFF_BANK_WRAP),
            MOV_L       (MOV_MSB, REG_ADDR, REG_BANK, REG_TEMP),
            ALU_OP      (ALU_OP_AND, STATUS_FLAG_M, REG_ACCUM, REG_TEMP),
            MOV_RR      (REG_TEMP, REG_ACCUM),
            // CHK_ZN      (REG_ACCUM),
        }
    },
    [AND_ABSL_X] = {
        .uops = {
            MOV_LPC     (MOV_LSB, REG_ADDR),
            MOV_LPC     (MOV_MSB, REG_ADDR),
            MOV_LPC     (MOV_LSB, REG_BANK),
            ADDR_OFFR   (REG_X, ADDR_OFF_BANK_NEXT),
            MOV_L       (MOV_LSB, REG_ADDR, REG_BANK, REG_TEMP),
            SKIPS       (2, STATUS_FLAG_M),
            ADDR_OFFI   (1, ADDR_OFF_BANK_NEXT),
            MOV_L       (MOV_MSB, REG_ADDR, REG_BANK, REG_TEMP),
            ALU_OP      (ALU_OP_AND, STATUS_FLAG_M, REG_ACCUM, REG_TEMP),
            MOV_RR      (REG_TEMP, REG_ACCUM),
            // CHK_ZN      (REG_ACCUM),
        }
    },
    [AND_DIR] = {
        .uops = {
            MOV_LPC     (MOV_LSB, REG_ADDR),
            ZEXT        (REG_ADDR),
            ADDR_OFFR   (REG_D, ADDR_OFF_BANK_WRAP),
            SKIPC       (1, STATUS_FLAG_DL),
            IO          /* DL != 0 */,
            MOV_L       (MOV_LSB, REG_ADDR, REG_ZERO, REG_TEMP),
            SKIPS       (2, STATUS_FLAG_M),
            ADDR_OFFI   (1, ADDR_OFF_BANK_WRAP),
            MOV_L       (MOV_MSB, REG_ADDR, REG_ZERO, REG_TEMP),
            ALU_OP      (ALU_OP_AND, STATUS_FLAG_M, REG_ACCUM, REG_TEMP),
            MOV_RR      (REG_TEMP, REG_ACCUM),
            // CHK_ZN      (REG_ACCUM),
        }
    },
    [AND_S_REL] = {
        .uops = {
            MOV_LPC     (MOV_LSB, REG_ADDR),
            ZEXT        (REG_ADDR),
            ADDR_OFFR   (REG_S, ADDR_OFF_BANK_WRAP),
            IO,
            MOV_L       (MOV_LSB, REG_ADDR, REG_ZERO, REG_TEMP),
            SKIPS       (2, STATUS_FLAG_M),
            ADDR_OFFI   (1, ADDR_OFF_BANK_WRAP),
            MOV_L       (MOV_MSB, REG_ADDR, REG_ZERO, REG_TEMP),
            ALU_OP      (ALU_OP_AND, STATUS_FLAG_M, REG_ACCUM, REG_TEMP),
            MOV_RR      (REG_TEMP, REG_ACCUM),
            // CHK_ZN      (REG_ACCUM),
        },
    },
    [AND_DIR_X] = {
        .uops = {
            MOV_LPC     (MOV_LSB, REG_ADDR),
            ZEXT        (REG_ADDR),
            SKIPC       (1, STATUS_FLAG_DL),
            IO          /* DL != 0 */,
            ADDR_OFFR   (REG_D, ADDR_OFF_BANK_WRAP),
            ADDR_OFFR   (REG_X, ADDR_OFF_BANK_WRAP),
            IO,
            MOV_L       (MOV_LSB, REG_ADDR, REG_ZERO, REG_TEMP),
            SKIPS       (2, STATUS_FLAG_M),
            ADDR_OFFI   (1, ADDR_OFF_BANK_WRAP),
            MOV_L       (MOV_MSB, REG_ADDR, REG_ZERO, REG_TEMP),
            ALU_OP      (ALU_OP_AND, STATUS_FLAG_M, REG_ACCUM, REG_TEMP),
            MOV_RR      (REG_TEMP, REG_ACCUM),
            // CHK_ZN      (REG_ACCUM),
        }
    },

    [AND_DIR_IND] = {
        .uops = {
            MOV_LPC     (MOV_LSB, REG_ADDR),
            ZEXT        (REG_ADDR),
            SKIPC       (1, STATUS_FLAG_DL),
            IO          /* DL != 0 */,
            ADDR_OFFR   (REG_D, ADDR_OFF_BANK_WRAP),
            MOV_L       (MOV_LSB, REG_ADDR, REG_ZERO, REG_TEMP),
            ADDR_OFFI   (1, ADDR_OFF_BANK_WRAP),
            MOV_L       (MOV_MSB, REG_ADDR, REG_ZERO, REG_TEMP),
            MOV_RRW     (REG_TEMP, REG_ADDR, MOV_RRW_WORD),
            MOV_L       (MOV_LSB, REG_ADDR, REG_DBR, REG_TEMP),
            SKIPS       (2, STATUS_FLAG_M),
            ADDR_OFFI   (1, ADDR_OFF_BANK_WRAP),
            MOV_L       (MOV_MSB, REG_ADDR, REG_DBR, REG_TEMP),
            ALU_OP      (ALU_OP_AND, STATUS_FLAG_M, REG_ACCUM, REG_TEMP),
            MOV_RR      (REG_TEMP, REG_ACCUM),
            // CHK_ZN      (REG_ACCUM),
        }
    },

    [AND_DIR_INDL] = {
        .uops = {
            MOV_LPC     (MOV_LSB, REG_ADDR),
            ZEXT        (REG_ADDR),
            SKIPC       (1, STATUS_FLAG_DL),
            IO          /* DL != 0 */,
        ADDR_OFFR   (REG_D, ADDR_OFF_BANK_WRAP),
            MOV_L       (MOV_LSB, REG_ADDR, REG_ZERO, REG_TEMP),
            ADDR_OFFI   (1, ADDR_OFF_BANK_WRAP),
            MOV_L       (MOV_MSB, REG_ADDR, REG_ZERO, REG_TEMP),
            ADDR_OFFI   (1, ADDR_OFF_BANK_WRAP),
            MOV_L       (MOV_LSB, REG_ADDR, REG_ZERO, REG_BANK),
            MOV_RRW     (REG_TEMP, REG_ADDR, MOV_RRW_WORD),
            MOV_L       (MOV_LSB, REG_ADDR, REG_BANK, REG_TEMP),
            SKIPS       (2, STATUS_FLAG_M),
            ADDR_OFFI   (1, ADDR_OFF_BANK_WRAP),
            MOV_L       (MOV_MSB, REG_ADDR, REG_BANK, REG_TEMP),
            ALU_OP      (ALU_OP_AND, STATUS_FLAG_M, REG_ACCUM, REG_TEMP),
            MOV_RR      (REG_TEMP, REG_ACCUM),
            // CHK_ZN      (REG_ACCUM),
        }
    },

    [AND_S_REL_IND_Y] = {
        .uops = {
            MOV_LPC     (MOV_LSB, REG_ADDR),
            ZEXT        (REG_ADDR),
            IO,
            ADDR_OFFR   (REG_S, ADDR_OFF_BANK_WRAP),
            MOV_L       (MOV_LSB, REG_ADDR, REG_ZERO, REG_TEMP),
            ADDR_OFFI   (1, ADDR_OFF_BANK_WRAP),
            MOV_L       (MOV_MSB, REG_ADDR, REG_ZERO, REG_TEMP),
            MOV_RRW     (REG_TEMP, REG_ADDR, MOV_RRW_WORD),
            MOV_RRW     (REG_DBR, REG_BANK, MOV_RRW_BYTE),
            IO,
            ADDR_OFFR   (REG_Y, ADDR_OFF_BANK_NEXT),
            MOV_L       (MOV_LSB, REG_ADDR, REG_BANK, REG_TEMP),
            SKIPS       (2, STATUS_FLAG_M),
            ADDR_OFFI   (1, ADDR_OFF_BANK_NEXT),
            MOV_L       (MOV_MSB, REG_ADDR, REG_BANK, REG_TEMP),
            ALU_OP      (ALU_OP_AND, STATUS_FLAG_M, REG_ACCUM, REG_TEMP),
            MOV_RR      (REG_TEMP, REG_ACCUM),
            // CHK_ZN      (REG_ACCUM),

        }
    },

    [AND_DIR_X_IND] = {
        .uops = {
            MOV_LPC     (MOV_LSB, REG_ADDR),
            ZEXT        (REG_ADDR),
            SKIPC       (1, STATUS_FLAG_DL),
            IO          /* DL != 0 */,
            ADDR_OFFR   (REG_D, ADDR_OFF_BANK_WRAP),
            ADDR_OFFR   (REG_X, ADDR_OFF_BANK_WRAP),
            IO,
            MOV_L       (MOV_LSB, REG_ADDR, REG_ZERO, REG_TEMP),
            ADDR_OFFI   (1, ADDR_OFF_BANK_WRAP),
            MOV_L       (MOV_MSB, REG_ADDR, REG_ZERO, REG_TEMP),
            MOV_RRW     (REG_TEMP, REG_ADDR, MOV_RRW_WORD),
            MOV_L       (MOV_LSB, REG_ADDR, REG_DBR, REG_TEMP),
            SKIPS       (2, STATUS_FLAG_M),
            ADDR_OFFI   (1, ADDR_OFF_BANK_WRAP),
            MOV_L       (MOV_MSB, REG_ADDR, REG_DBR, REG_TEMP),
            ALU_OP      (ALU_OP_AND, STATUS_FLAG_M, REG_ACCUM, REG_TEMP),
            MOV_RR      (REG_TEMP, REG_ACCUM),
            // CHK_ZN      (REG_ACCUM),
        }
    },

    [AND_DIR_IND_Y] = {
        .uops = {
            MOV_LPC     (MOV_LSB, REG_ADDR),
            ZEXT        (REG_ADDR),
            SKIPC       (1, STATUS_FLAG_DL),
            IO          /* DL != 0 */,
            ADDR_OFFR   (REG_D, 0),
            MOV_L       (MOV_LSB, REG_ADDR, REG_ZERO, REG_TEMP),
            ADDR_OFFI   (1, ADDR_OFF_BANK_WRAP),
            MOV_L       (MOV_MSB, REG_ADDR, REG_ZERO, REG_TEMP),
            MOV_RRW     (REG_TEMP, REG_ADDR, MOV_RRW_WORD),
            MOV_RRW     (REG_DBR, REG_BANK, MOV_RRW_BYTE),
            ADDR_OFFR   (REG_Y, ADDR_OFF_BANK_NEXT),
            SKIPS       (2, STATUS_FLAG_X),
            SKIPC       (1, STATUS_FLAG_PAGE),
            IO          /* X = 0 or page crossed */,
            MOV_L       (MOV_LSB, REG_ADDR, REG_BANK, REG_TEMP),
            SKIPS       (2, STATUS_FLAG_M),
            ADDR_OFFI   (1, ADDR_OFF_BANK_NEXT),
            MOV_L       (MOV_MSB, REG_ADDR, REG_BANK, REG_TEMP),
            ALU_OP      (ALU_OP_AND, STATUS_FLAG_M, REG_ACCUM, REG_TEMP),
            MOV_RR      (REG_TEMP, REG_ACCUM),
            // CHK_ZN      (REG_ACCUM),
        }
    },

    [AND_DIR_INDL_Y] = {
        .uops = {
            MOV_LPC     (MOV_LSB, REG_ADDR),
            ZEXT        (REG_ADDR),
            ADDR_OFFR   (REG_D, 0),
            SKIPC       (1, STATUS_FLAG_DL),
            IO          /* DL != 0 */,
            MOV_L       (MOV_LSB, REG_ADDR, REG_ZERO, REG_TEMP),
            ADDR_OFFI   (1, ADDR_OFF_BANK_WRAP),
            MOV_L       (MOV_MSB, REG_ADDR, REG_ZERO, REG_TEMP),
            ADDR_OFFI   (1, ADDR_OFF_BANK_WRAP),
            MOV_L       (MOV_LSB, REG_ADDR, REG_ZERO, REG_BANK),
            MOV_RRW     (REG_TEMP, REG_ADDR, MOV_RRW_WORD),
            // MOV_RRW     (REG_DBR, REG_BANK, MOV_RRW_BYTE),
            ADDR_OFFR   (REG_Y, ADDR_OFF_BANK_NEXT),
            MOV_L       (MOV_LSB, REG_ADDR, REG_BANK, REG_TEMP),
            SKIPS       (2, STATUS_FLAG_M),
            ADDR_OFFI   (1, ADDR_OFF_BANK_NEXT),
            MOV_L       (MOV_MSB, REG_ADDR, REG_BANK, REG_TEMP),
            ALU_OP      (ALU_OP_AND, STATUS_FLAG_M, REG_ACCUM, REG_TEMP),
            MOV_RR      (REG_TEMP, REG_ACCUM),
            // CHK_ZN      (REG_ACCUM),
        }
    },

    [AND_IMM] = {
        .uops = {
            MOV_LPC     (MOV_LSB, REG_TEMP),
            SKIPS       (1, STATUS_FLAG_M),
            MOV_LPC     (MOV_MSB, REG_TEMP),
            ALU_OP      (ALU_OP_AND, STATUS_FLAG_M, REG_ACCUM, REG_TEMP),
            MOV_RR      (REG_TEMP, REG_ACCUM),
            // CHK_ZN      (REG_ACCUM),
        }
    },

    /**************************************************************************************/
    /*                                      ASL                                           */
    /**************************************************************************************/

    [ASL_ABS] = {
        .uops = {
            MOV_LPC     (MOV_LSB, REG_ADDR),
            MOV_LPC     (MOV_MSB, REG_ADDR),
            MOV_L       (MOV_LSB, REG_ADDR, REG_DBR, REG_TEMP),
            SKIPS       (2, STATUS_FLAG_M),
            ADDR_OFFI   (1, ADDR_OFF_BANK_WRAP),
            MOV_L       (MOV_MSB, REG_ADDR, REG_DBR, REG_TEMP),
            IO,
            ALU_OP      (ALU_OP_SHL, STATUS_FLAG_M, REG_TEMP, REG_ZERO),
            // CHK_ZNW     (REG_TEMP, 0),
            SKIPS       (2, STATUS_FLAG_M),
            MOV_S       (MOV_MSB, REG_ADDR, REG_DBR, REG_TEMP),
            ADDR_OFFIS  (0xffff, ADDR_OFF_BANK_WRAP, ADDR_OFF_SIGNED),
            MOV_S       (MOV_LSB, REG_ADDR, REG_DBR, REG_TEMP),
        }
    },

    [ASL_ACC] = {
        .uops = {
            IO,
            ALU_OP      (ALU_OP_SHL, STATUS_FLAG_M, REG_ACCUM, REG_ZERO),
            MOV_RR      (REG_TEMP, REG_ACCUM),
            // CHK_ZN      (REG_ACCUM),
        }
    },

    [ASL_ABS_X] = {
        .uops = {
            MOV_LPC     (MOV_LSB, REG_ADDR),
            MOV_LPC     (MOV_MSB, REG_ADDR),
            MOV_RRW     (REG_DBR, REG_BANK, MOV_RRW_BYTE),
            IO,
            ADDR_OFFR   (REG_X, ADDR_OFF_BANK_NEXT),
            MOV_L       (MOV_LSB, REG_ADDR, REG_BANK, REG_TEMP),
            SKIPS       (2, STATUS_FLAG_M),
            ADDR_OFFI   (1, ADDR_OFF_BANK_NEXT),
            MOV_L       (MOV_MSB, REG_ADDR, REG_BANK, REG_TEMP),
            ALU_OP      (ALU_OP_SHL, STATUS_FLAG_M, REG_TEMP, REG_ZERO),
            // CHK_ZNW     (REG_TEMP, 0),
            SKIPS       (2, STATUS_FLAG_M),
            MOV_S       (MOV_MSB, REG_ADDR, REG_BANK, REG_TEMP),
            ADDR_OFFIS  (0xffff, ADDR_OFF_BANK_WRAP, ADDR_OFF_SIGNED),
            MOV_S       (MOV_LSB, REG_ADDR, REG_BANK, REG_TEMP),
        }
    },

    [ASL_DIR] = {
        .uops = {
            MOV_LPC     (MOV_LSB, REG_ADDR),
            ZEXT        (REG_ADDR),
            SKIPC       (1, STATUS_FLAG_DL),
            IO          /* DL != 0 */,
            ADDR_OFFR   (REG_D, ADDR_OFF_BANK_WRAP),
            MOV_L       (MOV_LSB, REG_ADDR, REG_ZERO, REG_TEMP),
            SKIPS       (2, STATUS_FLAG_M),
            ADDR_OFFI   (1, ADDR_OFF_BANK_WRAP),
            MOV_L       (MOV_MSB, REG_ADDR, REG_ZERO, REG_TEMP),
            IO,
            ALU_OP      (ALU_OP_SHL, STATUS_FLAG_M, REG_TEMP, REG_ZERO),
            // CHK_ZNW     (REG_TEMP, 0),
            SKIPS       (2, STATUS_FLAG_M),
            MOV_S       (MOV_MSB, REG_ADDR, REG_ZERO, REG_TEMP),
            ADDR_OFFIS  (0xffff, ADDR_OFF_BANK_WRAP, ADDR_OFF_SIGNED),
            MOV_S       (MOV_LSB, REG_ADDR, REG_ZERO, REG_TEMP),
        }
    },

    [ASL_DIR_X] = {
        .uops = {
            MOV_LPC     (MOV_LSB, REG_ADDR),
            ZEXT        (REG_ADDR),
            SKIPC       (1, STATUS_FLAG_DL),
            IO          /* DL != 0 */,
            IO,
            ADDR_OFFR   (REG_D, ADDR_OFF_BANK_WRAP),
            ADDR_OFFR   (REG_X, ADDR_OFF_BANK_WRAP),
            MOV_L       (MOV_LSB, REG_ADDR, REG_ZERO, REG_TEMP),
            SKIPS       (2, STATUS_FLAG_M),
            ADDR_OFFI   (1, ADDR_OFF_BANK_WRAP),
            MOV_L       (MOV_MSB, REG_ADDR, REG_ZERO, REG_TEMP),
            IO,
            ALU_OP      (ALU_OP_SHL, STATUS_FLAG_M, REG_TEMP, REG_ZERO),
            // CHK_ZNW     (REG_TEMP, 0),
            SKIPS       (2, STATUS_FLAG_M),
            MOV_S       (MOV_MSB, REG_ADDR, REG_ZERO, REG_TEMP),
            ADDR_OFFIS  (0xffff, ADDR_OFF_BANK_WRAP, ADDR_OFF_SIGNED),
            MOV_S       (MOV_LSB, REG_ADDR, REG_ZERO, REG_TEMP),
        }
    },

    /**************************************************************************************/
    /*                                      BCC                                           */
    /**************************************************************************************/

    [BCC_PC_REL] = {
        .uops = {
            MOV_LPC     (MOV_LSB, REG_ADDR),
            SEXT        (REG_ADDR),
            ADDR_OFFRS  (REG_PC, ADDR_OFF_BANK_WRAP, ADDR_OFF_SIGNED),
            SKIPS       (2, STATUS_FLAG_C),
            IO          /* branch taken */,
            MOV_RRW     (REG_ADDR, REG_PC, MOV_RRW_WORD),
        }
    },

    /**************************************************************************************/
    /*                                      BCS                                           */
    /**************************************************************************************/

    [BCS_PC_REL] = {
        .uops = {
            MOV_LPC     (MOV_LSB, REG_ADDR),
            SEXT        (REG_ADDR),
            ADDR_OFFRS  (REG_PC, ADDR_OFF_BANK_WRAP, ADDR_OFF_SIGNED),
            SKIPC       (2, STATUS_FLAG_C),
            IO          /* branch taken */,
            MOV_RRW     (REG_ADDR, REG_PC, MOV_RRW_WORD),
        }
    },

    /**************************************************************************************/
    /*                                      BEQ                                           */
    /**************************************************************************************/

    [BEQ_PC_REL] = {
        .uops = {
            MOV_LPC     (MOV_LSB, REG_ADDR),
            SEXT        (REG_ADDR),
            ADDR_OFFRS  (REG_PC, ADDR_OFF_BANK_WRAP, ADDR_OFF_SIGNED),
            SKIPC       (2, STATUS_FLAG_Z),
            IO          /* branch taken */,
            MOV_RRW     (REG_ADDR, REG_PC, 0),
        }
    },

    /**************************************************************************************/
    /*                                      BIT                                           */
    /**************************************************************************************/

    [BIT_ABS] = {
        .uops = {
            MOV_LPC     (MOV_LSB, REG_ADDR),
            MOV_LPC     (MOV_MSB, REG_ADDR),
            MOV_L       (MOV_LSB, REG_ADDR, REG_DBR, REG_TEMP),
            SKIPS       (2, STATUS_FLAG_M),
            ADDR_OFFI   (1, ADDR_OFF_BANK_WRAP),
            MOV_L       (MOV_MSB, REG_ADDR, REG_DBR, REG_TEMP),
            ALU_OP      (ALU_OP_BIT, STATUS_FLAG_M, REG_TEMP, REG_ZERO),
        }
    },

    [BIT_ABS_X] = {
        .uops = {
            MOV_LPC     (MOV_LSB, REG_ADDR),
            MOV_LPC     (MOV_MSB, REG_ADDR),
            MOV_RRW     (REG_DBR, REG_BANK, MOV_RRW_BYTE),
            ADDR_OFFR   (REG_X, ADDR_OFF_BANK_NEXT),
            SKIPC       (2, STATUS_FLAG_PAGE),
            SKIPS       (1, STATUS_FLAG_X),
            IO          /* X=0 or page boundary */,
            MOV_L       (MOV_LSB, REG_ADDR, REG_BANK, REG_TEMP),
            SKIPS       (2, STATUS_FLAG_M),
            ADDR_OFFI   (1, ADDR_OFF_BANK_NEXT),
            MOV_L       (MOV_MSB, REG_ADDR, REG_BANK, REG_TEMP),
            ALU_OP      (ALU_OP_BIT, STATUS_FLAG_M, REG_TEMP, REG_ZERO),
        }
    },

    [BIT_DIR] = {
        .uops = {
            MOV_LPC     (MOV_LSB, REG_ADDR),
            ZEXT        (REG_ADDR),
            ADDR_OFFR   (REG_D, ADDR_OFF_BANK_WRAP),
            SKIPC       (1, STATUS_FLAG_DL),
            IO          /* DL != 0 */,
            MOV_L       (MOV_LSB, REG_ADDR, REG_ZERO, REG_TEMP),
            SKIPS       (2, STATUS_FLAG_M),
            ADDR_OFFI   (1, ADDR_OFF_BANK_WRAP),
            MOV_L       (MOV_MSB, REG_ADDR, REG_ZERO, REG_TEMP),
            ALU_OP      (ALU_OP_BIT, STATUS_FLAG_M, REG_TEMP, REG_ZERO),
        }
    },

    [BIT_DIR_X] = {
        .uops = {
            MOV_LPC     (MOV_LSB, REG_ADDR),
            ZEXT        (REG_ADDR),
            SKIPC       (1, STATUS_FLAG_DL),
            IO          /* DL != 0 */,
            ADDR_OFFR   (REG_D, ADDR_OFF_BANK_WRAP),
            ADDR_OFFR   (REG_X, ADDR_OFF_BANK_WRAP),
            IO,
            MOV_L       (MOV_LSB, REG_ADDR, REG_ZERO, REG_TEMP),
            SKIPS       (2, STATUS_FLAG_M),
            ADDR_OFFI   (1, ADDR_OFF_BANK_WRAP),
            MOV_L       (MOV_MSB, REG_ADDR, REG_ZERO, REG_TEMP),
            ALU_OP      (ALU_OP_BIT, STATUS_FLAG_M, REG_TEMP, REG_ZERO),
        }
    },

    [BIT_IMM] = {
        .uops = {
            MOV_LPC     (MOV_LSB, REG_TEMP),
            SKIPS       (1, STATUS_FLAG_M),
            MOV_LPC     (MOV_MSB, REG_TEMP),
            ALU_OP      (ALU_OP_BIT_IMM, STATUS_FLAG_M, REG_TEMP, REG_ZERO),
        }
    },

    /**************************************************************************************/
    /*                                      BMI                                           */
    /**************************************************************************************/

    [BMI_PC_REL] = {
        .uops = {
            MOV_LPC     (MOV_LSB, REG_ADDR),
            SEXT        (REG_ADDR),
            ADDR_OFFRS  (REG_PC, ADDR_OFF_BANK_WRAP, ADDR_OFF_SIGNED),
            SKIPC       (2, STATUS_FLAG_N),
            IO          /* branch taken */,
            MOV_RRW     (REG_ADDR, REG_PC, MOV_RRW_WORD),
        }
    },

    /**************************************************************************************/
    /*                                      BNE                                           */
    /**************************************************************************************/

    [BNE_PC_REL] = {
        .uops = {
            MOV_LPC     (MOV_LSB, REG_ADDR),
            SEXT        (REG_ADDR),
            ADDR_OFFRS  (REG_PC, ADDR_OFF_BANK_WRAP, ADDR_OFF_SIGNED),
            SKIPS       (2, STATUS_FLAG_Z),
            IO          /* branch taken */,
            MOV_RRW     (REG_ADDR, REG_PC, MOV_RRW_WORD),
        }
    },

    /**************************************************************************************/
    /*                                      BPL                                           */
    /**************************************************************************************/

    [BPL_PC_REL] = {
        .uops = {
            MOV_LPC     (MOV_LSB, REG_ADDR),
            SEXT        (REG_ADDR),
            ADDR_OFFRS  (REG_PC, ADDR_OFF_BANK_WRAP, ADDR_OFF_SIGNED),
            SKIPS       (2, STATUS_FLAG_N),
            IO          /* branch taken */,
            MOV_RRW     (REG_ADDR, REG_PC, MOV_RRW_WORD),
        }
    },

    /**************************************************************************************/
    /*                                      BRA                                           */
    /**************************************************************************************/

    [BRA_PC_REL] = {
        .uops = {
            MOV_LPC     (MOV_LSB, REG_ADDR),
            SEXT        (REG_ADDR),
            ADDR_OFFRS  (REG_PC, ADDR_OFF_BANK_WRAP, ADDR_OFF_SIGNED),
            IO          /* branch taken */,
            MOV_RRW     (REG_ADDR, REG_PC, MOV_RRW_WORD),
        }
    },

    /**************************************************************************************/
    /*                                      BRK                                           */
    /**************************************************************************************/

    [BRK_S] = {
        .uops = {
            MOV_LPC     (MOV_LSB, REG_TEMP),
            /* this sets the B flag in the status register (when in emulation
            mode) and also loads the appropriate interrupt vector into REG_ADDR */
            BRK,

            /* when in native mode... */
            SKIPS       (2, STATUS_FLAG_E),
            /* the program bank register gets pushed onto the stack */
            MOV_S       (MOV_LSB, REG_S, REG_ZERO, REG_PBR),
            DECS,

            MOV_S       (MOV_MSB, REG_S, REG_ZERO, REG_PC),
            DECS,
            MOV_S       (MOV_LSB, REG_S, REG_ZERO, REG_PC),
            DECS,
            MOV_P       (REG_P, REG_TEMP),
            MOV_S       (MOV_LSB, REG_S, REG_ZERO, REG_TEMP),
            DECS,

            SET_P       (STATUS_FLAG_I),

            /* when in native mode... */
            SKIPS       (1, STATUS_FLAG_E),
            /* the D flag gets cleared */
            CLR_P       (STATUS_FLAG_D),

            MOV_L       (MOV_LSB, REG_ADDR, REG_ZERO, REG_TEMP),
            ADDR_OFFI   (1, ADDR_OFF_BANK_WRAP),
            MOV_L       (MOV_MSB, REG_ADDR, REG_ZERO, REG_TEMP),
            MOV_RRW     (REG_TEMP, REG_PC, MOV_RRW_WORD),
            MOV_RRW     (REG_ZERO, REG_PBR, MOV_RRW_BYTE),
            // MOV_LPC     (MOV_LSB, REG_INST),
            // DECODE
        }
    },

    /**************************************************************************************/
    /*                                      BRL                                           */
    /**************************************************************************************/

    [BRL_PC_RELL] = {
        .uops = {
            MOV_LPC     (MOV_LSB, REG_ADDR),
            MOV_LPC     (MOV_MSB, REG_ADDR),
            ADDR_OFFR   (REG_PC, ADDR_OFF_BANK_WRAP),
            IO          /* branch taken */,
            MOV_RRW     (REG_ADDR, REG_PC, MOV_RRW_WORD),
        }
    },

    /**************************************************************************************/
    /*                                      BVC                                           */
    /**************************************************************************************/

    [BVC_PC_REL] = {
        .uops = {
            MOV_LPC     (MOV_LSB, REG_ADDR),
            SEXT        (REG_ADDR),
            ADDR_OFFRS  (REG_PC, ADDR_OFF_BANK_WRAP, ADDR_OFF_SIGNED),
            SKIPS       (2, STATUS_FLAG_V),
            IO          /* branch taken */,
            MOV_RRW     (REG_ADDR, REG_PC, MOV_RRW_WORD),
        }
    },

    /**************************************************************************************/
    /*                                      BVS                                           */
    /**************************************************************************************/

    [BVS_PC_REL] = {
        .uops = {
            MOV_LPC     (MOV_LSB, REG_ADDR),
            SEXT        (REG_ADDR),
            ADDR_OFFRS  (REG_PC, ADDR_OFF_BANK_WRAP, ADDR_OFF_SIGNED),
            SKIPC       (2, STATUS_FLAG_V),
            IO          /* branch taken */,
            MOV_RRW     (REG_ADDR, REG_PC, MOV_RRW_WORD),
        }
    },

    /**************************************************************************************/
    /*                                      CLC                                           */
    /**************************************************************************************/

    [CLC_IMP] = {
        .uops = {
            IO,
            CLR_P       (STATUS_FLAG_C)
        }
    },

    /**************************************************************************************/
    /*                                      CLD                                           */
    /**************************************************************************************/

    [CLD_IMP] = {
        .uops = {
            IO,
            CLR_P       (STATUS_FLAG_D)
        }
    },

    /**************************************************************************************/
    /*                                      CLI                                           */
    /**************************************************************************************/

    [CLI_IMP] = {
        .uops = {
            IO,
            CLR_P       (STATUS_FLAG_I)
        }
    },

    /**************************************************************************************/
    /*                                      CLV                                           */
    /**************************************************************************************/

    [CLV_IMP] = {
        .uops = {
            IO,
            CLR_P       (STATUS_FLAG_V)
        }
    },

    /**************************************************************************************/
    /*                                      CMP                                           */
    /**************************************************************************************/

    [CMP_ABS] = {
        .uops = {
            MOV_LPC     (MOV_LSB, REG_ADDR),
            MOV_LPC     (MOV_MSB, REG_ADDR),
            MOV_L       (MOV_LSB, REG_ADDR, REG_DBR, REG_TEMP),
            SKIPS       (2, STATUS_FLAG_M),
            ADDR_OFFI   (1, ADDR_OFF_BANK_WRAP),
            MOV_L       (MOV_MSB, REG_ADDR, REG_DBR, REG_TEMP),
            ALU_OP      (ALU_OP_CMP, STATUS_FLAG_M, REG_ACCUM, REG_TEMP),
            // CHK_ZNW     (REG_TEMP, 0),
        }
    },
    [CMP_ABS_X] = {
        .uops = {
            MOV_LPC     (MOV_LSB, REG_ADDR),
            MOV_LPC     (MOV_MSB, REG_ADDR),
            MOV_RRW     (REG_DBR, REG_BANK, MOV_RRW_BYTE),
            ADDR_OFFR   (REG_X, ADDR_OFF_BANK_NEXT),
            SKIPC       (2, STATUS_FLAG_PAGE),
            SKIPS       (1, STATUS_FLAG_X),
            IO          /* X=0 or page boundary */,
            MOV_L       (MOV_LSB, REG_ADDR, REG_BANK, REG_TEMP),
            SKIPS       (2, STATUS_FLAG_M),
            ADDR_OFFI   (1, ADDR_OFF_BANK_NEXT),
            MOV_L       (MOV_MSB, REG_ADDR, REG_BANK, REG_TEMP),
            ALU_OP      (ALU_OP_CMP, STATUS_FLAG_M, REG_ACCUM, REG_TEMP),
            // CHK_ZNW     (REG_TEMP, 0),
        }
    },
    [CMP_ABS_Y] = {
        .uops = {
            MOV_LPC     (MOV_LSB, REG_ADDR),
            MOV_LPC     (MOV_MSB, REG_ADDR),
            MOV_RRW     (REG_DBR, REG_BANK, MOV_RRW_BYTE),
            ADDR_OFFR   (REG_Y, ADDR_OFF_BANK_NEXT),
            SKIPC       (2, STATUS_FLAG_PAGE),
            SKIPS       (1, STATUS_FLAG_X),
            IO          /* X=0 or page boundary */,
            MOV_L       (MOV_LSB, REG_ADDR, REG_BANK, REG_TEMP),
            SKIPS       (2, STATUS_FLAG_M),
            ADDR_OFFI   (1, ADDR_OFF_BANK_NEXT),
            MOV_L       (MOV_MSB, REG_ADDR, REG_BANK, REG_TEMP),
            ALU_OP      (ALU_OP_CMP, STATUS_FLAG_M, REG_ACCUM, REG_TEMP),
            // CHK_ZNW     (REG_TEMP, 0),
        }
    },
    [CMP_ABSL] = {
        .uops = {
            MOV_LPC     (MOV_LSB, REG_ADDR),
            MOV_LPC     (MOV_MSB, REG_ADDR),
            MOV_LPC     (MOV_LSB, REG_BANK),
            MOV_L       (MOV_LSB, REG_ADDR, REG_BANK, REG_TEMP),
            SKIPS       (2, STATUS_FLAG_M),
            ADDR_OFFI   (1, ADDR_OFF_BANK_WRAP),
            MOV_L       (MOV_MSB, REG_ADDR, REG_BANK, REG_TEMP),
            ALU_OP      (ALU_OP_CMP, STATUS_FLAG_M, REG_ACCUM, REG_TEMP),
            // CHK_ZNW     (REG_TEMP, 0),
        }
    },
    [CMP_ABSL_X] = {
        .uops = {
            MOV_LPC     (MOV_LSB, REG_ADDR),
            MOV_LPC     (MOV_MSB, REG_ADDR),
            MOV_LPC     (MOV_LSB, REG_BANK),
            ADDR_OFFR   (REG_X, ADDR_OFF_BANK_NEXT),
            MOV_L       (MOV_LSB, REG_ADDR, REG_BANK, REG_TEMP),
            SKIPS       (2, STATUS_FLAG_M),
            ADDR_OFFI   (1, ADDR_OFF_BANK_NEXT),
            MOV_L       (MOV_MSB, REG_ADDR, REG_BANK, REG_TEMP),
            ALU_OP      (ALU_OP_CMP, STATUS_FLAG_M, REG_ACCUM, REG_TEMP),
            // CHK_ZNW     (REG_TEMP, 0),
        }
    },
    [CMP_DIR] = {
        .uops = {
            MOV_LPC     (MOV_LSB, REG_ADDR),
            ZEXT        (REG_ADDR),
            ADDR_OFFR   (REG_D, ADDR_OFF_BANK_WRAP),
            SKIPC       (1, STATUS_FLAG_DL),
            IO          /* DL != 0 */,
            MOV_L       (MOV_LSB, REG_ADDR, REG_ZERO, REG_TEMP),
            SKIPS       (2, STATUS_FLAG_M),
            ADDR_OFFI   (1, ADDR_OFF_BANK_WRAP),
            MOV_L       (MOV_MSB, REG_ADDR, REG_ZERO, REG_TEMP),
            ALU_OP      (ALU_OP_CMP, STATUS_FLAG_M, REG_ACCUM, REG_TEMP),
            // CHK_ZNW     (REG_TEMP, 0),
        }
    },
    [CMP_S_REL] = {
        .uops = {
            MOV_LPC     (MOV_LSB, REG_ADDR),
            ZEXT        (REG_ADDR),
            ADDR_OFFR   (REG_S, ADDR_OFF_BANK_WRAP),
            IO,
            MOV_L       (MOV_LSB, REG_ADDR, REG_ZERO, REG_TEMP),
            SKIPS       (2, STATUS_FLAG_M),
            ADDR_OFFI   (1, ADDR_OFF_BANK_WRAP),
            MOV_L       (MOV_MSB, REG_ADDR, REG_ZERO, REG_TEMP),
            ALU_OP      (ALU_OP_CMP, STATUS_FLAG_M, REG_ACCUM, REG_TEMP),
            // CHK_ZNW     (REG_TEMP, 0),
        },
    },
    [CMP_DIR_X] = {
        .uops = {
            MOV_LPC     (MOV_LSB, REG_ADDR),
            ZEXT        (REG_ADDR),
            SKIPC       (1, STATUS_FLAG_DL),
            IO          /* DL != 0 */,
            ADDR_OFFR   (REG_D, ADDR_OFF_BANK_WRAP),
            ADDR_OFFR   (REG_X, ADDR_OFF_BANK_WRAP),
            IO,
            MOV_L       (MOV_LSB, REG_ADDR, REG_ZERO, REG_TEMP),
            SKIPS       (2, STATUS_FLAG_M),
            ADDR_OFFI   (1, ADDR_OFF_BANK_WRAP),
            MOV_L       (MOV_MSB, REG_ADDR, REG_ZERO, REG_TEMP),
            ALU_OP      (ALU_OP_CMP, STATUS_FLAG_M, REG_ACCUM, REG_TEMP),
            // CHK_ZNW     (REG_TEMP, 0),
        }
    },

    [CMP_DIR_IND] = {
        .uops = {
            MOV_LPC     (MOV_LSB, REG_ADDR),
            ZEXT        (REG_ADDR),
            SKIPC       (1, STATUS_FLAG_DL),
            IO          /* DL != 0 */,
            ADDR_OFFR   (REG_D, ADDR_OFF_BANK_WRAP),
            MOV_L       (MOV_LSB, REG_ADDR, REG_ZERO, REG_TEMP),
            ADDR_OFFI   (1, ADDR_OFF_BANK_WRAP),
            MOV_L       (MOV_MSB, REG_ADDR, REG_ZERO, REG_TEMP),
            MOV_RRW     (REG_TEMP, REG_ADDR, MOV_RRW_WORD),
            MOV_L       (MOV_LSB, REG_ADDR, REG_DBR, REG_TEMP),
            SKIPS       (2, STATUS_FLAG_M),
            ADDR_OFFI   (1, ADDR_OFF_BANK_WRAP),
            MOV_L       (MOV_MSB, REG_ADDR, REG_DBR, REG_TEMP),
            ALU_OP      (ALU_OP_CMP, STATUS_FLAG_M, REG_ACCUM, REG_TEMP),
            // CHK_ZNW     (REG_TEMP, 0),
        }
    },

    [CMP_DIR_INDL] = {
        .uops = {
            MOV_LPC     (MOV_LSB, REG_ADDR),
            ZEXT        (REG_ADDR),
            SKIPC       (1, STATUS_FLAG_DL),
            IO          /* DL != 0 */,
            ADDR_OFFR   (REG_D, ADDR_OFF_BANK_WRAP),
            MOV_L       (MOV_LSB, REG_ADDR, REG_ZERO, REG_TEMP),
            ADDR_OFFI   (1, ADDR_OFF_BANK_WRAP),
            MOV_L       (MOV_MSB, REG_ADDR, REG_ZERO, REG_TEMP),
            ADDR_OFFI   (1, ADDR_OFF_BANK_WRAP),
            MOV_L       (MOV_LSB, REG_ADDR, REG_ZERO, REG_BANK),
            MOV_RRW     (REG_TEMP, REG_ADDR, MOV_RRW_WORD),
            MOV_L       (MOV_LSB, REG_ADDR, REG_BANK, REG_TEMP),
            SKIPS       (2, STATUS_FLAG_M),
            ADDR_OFFI   (1, ADDR_OFF_BANK_WRAP),
            MOV_L       (MOV_MSB, REG_ADDR, REG_BANK, REG_TEMP),
            ALU_OP      (ALU_OP_CMP, STATUS_FLAG_M, REG_ACCUM, REG_TEMP),
            // CHK_ZNW     (REG_TEMP, 0),
        }
    },

    [CMP_S_REL_IND_Y] = {
        .uops = {
            MOV_LPC     (MOV_LSB, REG_ADDR),
            ZEXT        (REG_ADDR),
            IO,
            ADDR_OFFR   (REG_S, ADDR_OFF_BANK_WRAP),
            MOV_L       (MOV_LSB, REG_ADDR, REG_ZERO, REG_TEMP),
            ADDR_OFFI   (1, ADDR_OFF_BANK_WRAP),
            MOV_L       (MOV_MSB, REG_ADDR, REG_ZERO, REG_TEMP),
            MOV_RRW     (REG_TEMP, REG_ADDR, MOV_RRW_WORD),
            MOV_RRW     (REG_DBR, REG_BANK, MOV_RRW_BYTE),
            IO,
            ADDR_OFFR   (REG_Y, ADDR_OFF_BANK_NEXT),
            MOV_L       (MOV_LSB, REG_ADDR, REG_BANK, REG_TEMP),
            SKIPS       (2, STATUS_FLAG_M),
            ADDR_OFFI   (1, ADDR_OFF_BANK_NEXT),
            MOV_L       (MOV_MSB, REG_ADDR, REG_BANK, REG_TEMP),
            ALU_OP      (ALU_OP_CMP, STATUS_FLAG_M, REG_ACCUM, REG_TEMP),
            // CHK_ZNW     (REG_TEMP, 0),
        }
    },

    [CMP_DIR_X_IND] = {
        .uops = {
            MOV_LPC     (MOV_LSB, REG_ADDR),
            ZEXT        (REG_ADDR),
            SKIPC       (1, STATUS_FLAG_DL),
            IO          /* DL != 0 */,
            ADDR_OFFR   (REG_D, ADDR_OFF_BANK_WRAP),
            ADDR_OFFR   (REG_X, ADDR_OFF_BANK_WRAP),
            IO,
            MOV_L       (MOV_LSB, REG_ADDR, REG_ZERO, REG_TEMP),
            ADDR_OFFI   (1, ADDR_OFF_BANK_WRAP),
            MOV_L       (MOV_MSB, REG_ADDR, REG_ZERO, REG_TEMP),
            MOV_RRW     (REG_TEMP, REG_ADDR, MOV_RRW_WORD),
            MOV_L       (MOV_LSB, REG_ADDR, REG_DBR, REG_TEMP),
            SKIPS       (2, STATUS_FLAG_M),
            ADDR_OFFI   (1, ADDR_OFF_BANK_WRAP),
            MOV_L       (MOV_MSB, REG_ADDR, REG_DBR, REG_TEMP),
            ALU_OP      (ALU_OP_CMP, STATUS_FLAG_M, REG_ACCUM, REG_TEMP),
            // CHK_ZNW     (REG_TEMP, 0),
        }
    },

    [CMP_DIR_IND_Y] = {
        .uops = {
            MOV_LPC     (MOV_LSB, REG_ADDR),
            ZEXT        (REG_ADDR),
            SKIPC       (1, STATUS_FLAG_DL),
            IO          /* DL != 0 */,
            ADDR_OFFR   (REG_D, ADDR_OFF_BANK_WRAP),
            MOV_L       (MOV_LSB, REG_ADDR, REG_ZERO, REG_TEMP),
            ADDR_OFFI   (1, ADDR_OFF_BANK_WRAP),
            MOV_L       (MOV_MSB, REG_ADDR, REG_ZERO, REG_TEMP),
            MOV_RRW     (REG_TEMP, REG_ADDR, MOV_RRW_WORD),
            MOV_RRW     (REG_DBR, REG_BANK, MOV_RRW_BYTE),
            ADDR_OFFR   (REG_Y, ADDR_OFF_BANK_NEXT),
            SKIPS       (2, STATUS_FLAG_X),
            SKIPC       (1, STATUS_FLAG_PAGE),
            IO          /* X = 0 or page crossed */,
            MOV_L       (MOV_LSB, REG_ADDR, REG_BANK, REG_TEMP),
            SKIPS       (2, STATUS_FLAG_M),
            ADDR_OFFI   (1, ADDR_OFF_BANK_NEXT),
            MOV_L       (MOV_MSB, REG_ADDR, REG_BANK, REG_TEMP),
            ALU_OP      (ALU_OP_CMP, STATUS_FLAG_M, REG_ACCUM, REG_TEMP),
            // CHK_ZNW     (REG_TEMP, 0),
        }
    },

    [CMP_DIR_INDL_Y] = {
        .uops = {
            MOV_LPC     (MOV_LSB, REG_ADDR),
            ZEXT        (REG_ADDR),
            ADDR_OFFR   (REG_D, 0),
            SKIPC       (1, STATUS_FLAG_DL),
            IO          /* DL != 0 */,
            MOV_L       (MOV_LSB, REG_ADDR, REG_ZERO, REG_TEMP),
            ADDR_OFFI   (1, ADDR_OFF_BANK_WRAP),
            MOV_L       (MOV_MSB, REG_ADDR, REG_ZERO, REG_TEMP),
            ADDR_OFFI   (1, ADDR_OFF_BANK_WRAP),
            MOV_L       (MOV_LSB, REG_ADDR, REG_ZERO, REG_BANK),
            MOV_RRW     (REG_TEMP, REG_ADDR, MOV_RRW_WORD),
            // MOV_RRW     (REG_DBR, REG_BANK, MOV_RRW_BYTE),
            ADDR_OFFR   (REG_Y, ADDR_OFF_BANK_NEXT),
            MOV_L       (MOV_LSB, REG_ADDR, REG_BANK, REG_TEMP),
            SKIPS       (2, STATUS_FLAG_M),
            ADDR_OFFI   (1, ADDR_OFF_BANK_NEXT),
            MOV_L       (MOV_MSB, REG_ADDR, REG_BANK, REG_TEMP),
            ALU_OP      (ALU_OP_CMP, STATUS_FLAG_M, REG_ACCUM, REG_TEMP),
            // CHK_ZNW     (REG_TEMP, 0),
        }
    },

    [CMP_IMM] = {
        .uops = {
            MOV_LPC     (MOV_LSB, REG_TEMP),
            SKIPS       (1, STATUS_FLAG_M),
            MOV_LPC     (MOV_MSB, REG_TEMP),
            ALU_OP      (ALU_OP_CMP, STATUS_FLAG_M, REG_ACCUM, REG_TEMP),
            // CHK_ZNW     (REG_TEMP, 0),
        }
    },

    /**************************************************************************************/
    /*                                      CPX                                           */
    /**************************************************************************************/

    [CPX_ABS] = {
        .uops = {
            MOV_LPC     (MOV_LSB, REG_ADDR),
            MOV_LPC     (MOV_MSB, REG_ADDR),
            MOV_L       (MOV_LSB, REG_ADDR, REG_DBR, REG_TEMP),
            SKIPS       (2, STATUS_FLAG_X),
            ADDR_OFFI   (1, ADDR_OFF_BANK_WRAP),
            MOV_L       (MOV_MSB, REG_ADDR, REG_DBR, REG_TEMP),
            ALU_OP      (ALU_OP_CMP, STATUS_FLAG_X, REG_X, REG_TEMP),
            // CHK_ZNW     (REG_TEMP, 0),
        }
    },

    [CPX_DIR] = {
        .uops = {
            MOV_LPC     (MOV_LSB, REG_ADDR),
            ZEXT        (REG_ADDR),
            ADDR_OFFR   (REG_D, ADDR_OFF_BANK_WRAP),
            SKIPC       (1, STATUS_FLAG_DL),
            IO          /* DL != 0 */,
            MOV_L       (MOV_LSB, REG_ADDR, REG_ZERO, REG_TEMP),
            SKIPS       (2, STATUS_FLAG_X),
            ADDR_OFFI   (1, ADDR_OFF_BANK_WRAP),
            MOV_L       (MOV_MSB, REG_ADDR, REG_ZERO, REG_TEMP),
            ALU_OP      (ALU_OP_CMP, STATUS_FLAG_X, REG_X, REG_TEMP),
            // CHK_ZN      (REG_TEMP),
        }
    },

    [CPX_IMM] = {
        .uops = {
            MOV_LPC     (MOV_LSB, REG_TEMP),
            SKIPS       (1, STATUS_FLAG_X),
            MOV_LPC     (MOV_MSB, REG_TEMP),
            ALU_OP      (ALU_OP_CMP, STATUS_FLAG_X, REG_X, REG_TEMP),
            // CHK_ZNW     (REG_TEMP, 0),
        }
    },

    /**************************************************************************************/
    /*                                      CPY                                           */
    /**************************************************************************************/

    [CPY_ABS] = {
        .uops = {
            MOV_LPC     (MOV_LSB, REG_ADDR),
            MOV_LPC     (MOV_MSB, REG_ADDR),
            MOV_L       (MOV_LSB, REG_ADDR, REG_DBR, REG_TEMP),
            SKIPS       (2, STATUS_FLAG_X),
            ADDR_OFFI   (1, ADDR_OFF_BANK_WRAP),
            MOV_L       (MOV_MSB, REG_ADDR, REG_DBR, REG_TEMP),
            ALU_OP      (ALU_OP_CMP, STATUS_FLAG_X, REG_Y, REG_TEMP),
            // CHK_ZNW     (REG_TEMP, 0),
        }
    },

    [CPY_DIR] = {
        .uops = {
            MOV_LPC     (MOV_LSB, REG_ADDR),
            ZEXT        (REG_ADDR),
            ADDR_OFFR   (REG_D, ADDR_OFF_BANK_WRAP),
            SKIPC       (1, STATUS_FLAG_DL),
            IO          /* DL != 0 */,
            MOV_L       (MOV_LSB, REG_ADDR, REG_ZERO, REG_TEMP),
            SKIPS       (2, STATUS_FLAG_X),
            ADDR_OFFI   (1, ADDR_OFF_BANK_WRAP),
            MOV_L       (MOV_MSB, REG_ADDR, REG_ZERO, REG_TEMP),
            ALU_OP      (ALU_OP_CMP, STATUS_FLAG_X, REG_Y, REG_TEMP),
            // CHK_ZN      (REG_TEMP),
        }
    },

    [CPY_IMM] = {
        .uops = {
            MOV_LPC     (MOV_LSB, REG_TEMP),
            SKIPS       (1, STATUS_FLAG_X),
            MOV_LPC     (MOV_MSB, REG_TEMP),
            ALU_OP      (ALU_OP_CMP, STATUS_FLAG_X, REG_Y, REG_TEMP),
            // CHK_ZNW     (REG_TEMP, 0),
        }
    },

    /**************************************************************************************/
    /*                                      DEC                                           */
    /**************************************************************************************/

    [DEC_ABS] = {
        .uops = {
            MOV_LPC     (MOV_LSB, REG_ADDR),
            MOV_LPC     (MOV_MSB, REG_ADDR),
            MOV_L       (MOV_LSB, REG_ADDR, REG_DBR, REG_TEMP),
            SKIPS       (2, STATUS_FLAG_M),
            ADDR_OFFI   (1, ADDR_OFF_BANK_WRAP),
            MOV_L       (MOV_MSB, REG_ADDR, REG_DBR, REG_TEMP),
            IO,
            ALU_OP      (ALU_OP_DEC, STATUS_FLAG_M, REG_TEMP, REG_ZERO),
            // CHK_ZNW     (REG_TEMP, 0),
            SKIPS       (2, STATUS_FLAG_M),
            MOV_S       (MOV_MSB, REG_ADDR, REG_DBR, REG_TEMP),
            ADDR_OFFIS  (0xffff, ADDR_OFF_BANK_WRAP, ADDR_OFF_SIGNED),
            MOV_S       (MOV_LSB, REG_ADDR, REG_DBR, REG_TEMP),
        }
    },

    [DEC_ACC] = {
        .uops = {
            IO,
            ALU_OP      (ALU_OP_DEC, STATUS_FLAG_M, REG_ACCUM, REG_ZERO),
            MOV_RR      (REG_TEMP, REG_ACCUM),
            // CHK_ZN      (REG_ACCUM),
        }
    },

    [DEC_ABS_X] = {
        .uops = {
            MOV_LPC     (MOV_LSB, REG_ADDR),
            MOV_LPC     (MOV_MSB, REG_ADDR),
            MOV_RRW     (REG_DBR, REG_BANK, MOV_RRW_BYTE),
            IO,
            ADDR_OFFR   (REG_X, ADDR_OFF_BANK_NEXT),
            MOV_L       (MOV_LSB, REG_ADDR, REG_BANK, REG_TEMP),
            SKIPS       (2, STATUS_FLAG_M),
            ADDR_OFFI   (1, ADDR_OFF_BANK_NEXT),
            MOV_L       (MOV_MSB, REG_ADDR, REG_BANK, REG_TEMP),
            ALU_OP      (ALU_OP_DEC, STATUS_FLAG_M, REG_TEMP, REG_ZERO),
            // CHK_ZNW     (REG_TEMP, 0),
            SKIPS       (2, STATUS_FLAG_M),
            MOV_S       (MOV_MSB, REG_ADDR, REG_BANK, REG_TEMP),
            ADDR_OFFIS  (0xffff, ADDR_OFF_BANK_WRAP, ADDR_OFF_SIGNED),
            MOV_S       (MOV_LSB, REG_ADDR, REG_BANK, REG_TEMP),
        }
    },

    [DEC_DIR] = {
        .uops = {
            MOV_LPC     (MOV_LSB, REG_ADDR),
            ZEXT        (REG_ADDR),
            SKIPC       (1, STATUS_FLAG_DL),
            IO          /* DL != 0 */,
            ADDR_OFFR   (REG_D, ADDR_OFF_BANK_WRAP),
            MOV_L       (MOV_LSB, REG_ADDR, REG_ZERO, REG_TEMP),
            SKIPS       (2, STATUS_FLAG_M),
            ADDR_OFFI   (1, ADDR_OFF_BANK_WRAP),
            MOV_L       (MOV_MSB, REG_ADDR, REG_ZERO, REG_TEMP),
            IO,
            ALU_OP      (ALU_OP_DEC, STATUS_FLAG_M, REG_TEMP, REG_ZERO),
            // CHK_ZNW     (REG_TEMP, 0),
            SKIPS       (2, STATUS_FLAG_M),
            MOV_S       (MOV_MSB, REG_ADDR, REG_ZERO, REG_TEMP),
            ADDR_OFFIS  (0xffff, ADDR_OFF_BANK_WRAP, ADDR_OFF_SIGNED),
            MOV_S       (MOV_LSB, REG_ADDR, REG_ZERO, REG_TEMP),
        }
    },

    [DEC_DIR_X] = {
        .uops = {
            MOV_LPC     (MOV_LSB, REG_ADDR),
            ZEXT        (REG_ADDR),
            SKIPC       (1, STATUS_FLAG_DL),
            IO          /* DL != 0 */,
            IO,
            ADDR_OFFR   (REG_D, ADDR_OFF_BANK_WRAP),
            ADDR_OFFR   (REG_X, ADDR_OFF_BANK_WRAP),
            MOV_L       (MOV_LSB, REG_ADDR, REG_ZERO, REG_TEMP),
            SKIPS       (2, STATUS_FLAG_M),
            ADDR_OFFI   (1, ADDR_OFF_BANK_WRAP),
            MOV_L       (MOV_MSB, REG_ADDR, REG_ZERO, REG_TEMP),
            IO,
            ALU_OP      (ALU_OP_DEC, STATUS_FLAG_M, REG_TEMP, REG_ZERO),
            // CHK_ZNW     (REG_TEMP, 0),
            SKIPS       (2, STATUS_FLAG_M),
            MOV_S       (MOV_MSB, REG_ADDR, REG_ZERO, REG_TEMP),
            ADDR_OFFIS  (0xffff, ADDR_OFF_BANK_WRAP, ADDR_OFF_SIGNED),
            MOV_S       (MOV_LSB, REG_ADDR, REG_ZERO, REG_TEMP),
        }
    },

    /**************************************************************************************/
    /*                                      DEX                                           */
    /**************************************************************************************/

    [DEX_IMP] = {
        .uops = {
            IO,
            ALU_OP      (ALU_OP_DEC, STATUS_FLAG_X, REG_X, REG_ZERO),
            MOV_RR      (REG_TEMP, REG_X),
            // CHK_ZN      (REG_X),
        }
    },

    /**************************************************************************************/
    /*                                      DEY                                           */
    /**************************************************************************************/

    [DEY_IMP] = {
        .uops = {
            IO,
            ALU_OP      (ALU_OP_DEC, STATUS_FLAG_X, REG_Y, REG_ZERO),
            MOV_RR      (REG_TEMP, REG_Y),
            // CHK_ZN      (REG_Y),
        }
    },

    /**************************************************************************************/
    /*                                      EOR                                           */
    /**************************************************************************************/

    [EOR_ABS] = {
        .uops = {
            MOV_LPC     (MOV_LSB, REG_ADDR),
            MOV_LPC     (MOV_MSB, REG_ADDR),
            MOV_L       (MOV_LSB, REG_ADDR, REG_DBR, REG_TEMP),
            SKIPS       (2, STATUS_FLAG_M),
            ADDR_OFFI   (1, ADDR_OFF_BANK_WRAP),
            MOV_L       (MOV_MSB, REG_ADDR, REG_DBR, REG_TEMP),
            ALU_OP      (ALU_OP_XOR, STATUS_FLAG_M, REG_ACCUM, REG_TEMP),
            MOV_RR      (REG_TEMP, REG_ACCUM),
            // CHK_ZN      (REG_ACCUM),
        }
    },
    [EOR_ABS_X] = {
        .uops = {
            MOV_LPC     (MOV_LSB, REG_ADDR),
            MOV_LPC     (MOV_MSB, REG_ADDR),
            MOV_RRW     (REG_DBR, REG_BANK, MOV_RRW_BYTE),
            ADDR_OFFR   (REG_X, ADDR_OFF_BANK_NEXT),
            SKIPC       (2, STATUS_FLAG_PAGE),
            SKIPS       (1, STATUS_FLAG_X),
            IO          /* X=0 or page boundary */,
            MOV_L       (MOV_LSB, REG_ADDR, REG_BANK, REG_TEMP),
            SKIPS       (2, STATUS_FLAG_M),
            ADDR_OFFI   (1, ADDR_OFF_BANK_NEXT),
            MOV_L       (MOV_MSB, REG_ADDR, REG_BANK, REG_TEMP),
            ALU_OP      (ALU_OP_XOR, STATUS_FLAG_M, REG_ACCUM, REG_TEMP),
            MOV_RR      (REG_TEMP, REG_ACCUM),
            // CHK_ZN      (REG_ACCUM),
        }
    },
    [EOR_ABS_Y] = {
        .uops = {
            MOV_LPC     (MOV_LSB, REG_ADDR),
            MOV_LPC     (MOV_MSB, REG_ADDR),
            MOV_RRW     (REG_DBR, REG_BANK, MOV_RRW_BYTE),
            ADDR_OFFR   (REG_Y, ADDR_OFF_BANK_NEXT),
            SKIPC       (2, STATUS_FLAG_PAGE),
            SKIPS       (1, STATUS_FLAG_X),
            IO          /* X=0 or page boundary */,
            MOV_L       (MOV_LSB, REG_ADDR, REG_BANK, REG_TEMP),
            SKIPS       (2, STATUS_FLAG_M),
            ADDR_OFFI   (1, ADDR_OFF_BANK_NEXT),
            MOV_L       (MOV_MSB, REG_ADDR, REG_BANK, REG_TEMP),
            ALU_OP      (ALU_OP_XOR, STATUS_FLAG_M, REG_ACCUM, REG_TEMP),
            MOV_RR      (REG_TEMP, REG_ACCUM),
            // CHK_ZN      (REG_ACCUM),
        }
    },
    [EOR_ABSL] = {
        .uops = {
            MOV_LPC     (MOV_LSB, REG_ADDR),
            MOV_LPC     (MOV_MSB, REG_ADDR),
            MOV_LPC     (MOV_LSB, REG_BANK),
            MOV_L       (MOV_LSB, REG_ADDR, REG_BANK, REG_TEMP),
            SKIPS       (2, STATUS_FLAG_M),
            ADDR_OFFI   (1, ADDR_OFF_BANK_WRAP),
            MOV_L       (MOV_MSB, REG_ADDR, REG_BANK, REG_TEMP),
            ALU_OP      (ALU_OP_XOR, STATUS_FLAG_M, REG_ACCUM, REG_TEMP),
            MOV_RR      (REG_TEMP, REG_ACCUM),
            // CHK_ZN      (REG_ACCUM),
        }
    },
    [EOR_ABSL_X] = {
        .uops = {
            MOV_LPC     (MOV_LSB, REG_ADDR),
            MOV_LPC     (MOV_MSB, REG_ADDR),
            MOV_LPC     (MOV_LSB, REG_BANK),
            ADDR_OFFR   (REG_X, ADDR_OFF_BANK_NEXT),
            MOV_L       (MOV_LSB, REG_ADDR, REG_BANK, REG_TEMP),
            SKIPS       (2, STATUS_FLAG_M),
            ADDR_OFFI   (1, ADDR_OFF_BANK_NEXT),
            MOV_L       (MOV_MSB, REG_ADDR, REG_BANK, REG_TEMP),
            ALU_OP      (ALU_OP_XOR, STATUS_FLAG_M, REG_ACCUM, REG_TEMP),
            MOV_RR      (REG_TEMP, REG_ACCUM),
            // CHK_ZN      (REG_ACCUM),
        }
    },

    [EOR_DIR] = {
        .uops = {
            MOV_LPC     (MOV_LSB, REG_ADDR),
            ZEXT        (REG_ADDR),
            ADDR_OFFR   (REG_D, ADDR_OFF_BANK_WRAP),
            SKIPC       (1, STATUS_FLAG_DL),
            IO          /* DL != 0 */,
            MOV_L       (MOV_LSB, REG_ADDR, REG_ZERO, REG_TEMP),
            SKIPS       (2, STATUS_FLAG_M),
            ADDR_OFFI   (1, ADDR_OFF_BANK_WRAP),
            MOV_L       (MOV_MSB, REG_ADDR, REG_ZERO, REG_TEMP),
            ALU_OP      (ALU_OP_XOR, STATUS_FLAG_M, REG_ACCUM, REG_TEMP),
            MOV_RR      (REG_TEMP, REG_ACCUM),
            // CHK_ZN      (REG_ACCUM),
        }
    },
    [EOR_S_REL] = {
        .uops = {
            MOV_LPC     (MOV_LSB, REG_ADDR),
            ZEXT        (REG_ADDR),
            ADDR_OFFR   (REG_S, ADDR_OFF_BANK_WRAP),
            IO,
            MOV_L       (MOV_LSB, REG_ADDR, REG_ZERO, REG_TEMP),
            SKIPS       (2, STATUS_FLAG_M),
            ADDR_OFFI   (1, ADDR_OFF_BANK_WRAP),
            MOV_L       (MOV_MSB, REG_ADDR, REG_ZERO, REG_TEMP),
            ALU_OP      (ALU_OP_XOR, STATUS_FLAG_M, REG_ACCUM, REG_TEMP),
            MOV_RR      (REG_TEMP, REG_ACCUM),
            // CHK_ZN      (REG_ACCUM),
        },
    },
    [EOR_DIR_X] = {
        .uops = {
            MOV_LPC     (MOV_LSB, REG_ADDR),
            ZEXT        (REG_ADDR),
            SKIPC       (1, STATUS_FLAG_DL),
            IO          /* DL != 0 */,
            ADDR_OFFR   (REG_D, ADDR_OFF_BANK_WRAP),
            ADDR_OFFR   (REG_X, ADDR_OFF_BANK_WRAP),
            IO,
            MOV_L       (MOV_LSB, REG_ADDR, REG_ZERO, REG_TEMP),
            SKIPS       (2, STATUS_FLAG_M),
            ADDR_OFFI   (1, ADDR_OFF_BANK_WRAP),
            MOV_L       (MOV_MSB, REG_ADDR, REG_ZERO, REG_TEMP),
            ALU_OP      (ALU_OP_XOR, STATUS_FLAG_M, REG_ACCUM, REG_TEMP),
            MOV_RR      (REG_TEMP, REG_ACCUM),
            // CHK_ZN      (REG_ACCUM),
        }
    },

    [EOR_DIR_IND] = {
        .uops = {
            MOV_LPC     (MOV_LSB, REG_ADDR),
            ZEXT        (REG_ADDR),
            SKIPC       (1, STATUS_FLAG_DL),
            IO          /* DL != 0 */,
            ADDR_OFFR   (REG_D, ADDR_OFF_BANK_WRAP),
            MOV_L       (MOV_LSB, REG_ADDR, REG_ZERO, REG_TEMP),
            ADDR_OFFI   (1, ADDR_OFF_BANK_WRAP),
            MOV_L       (MOV_MSB, REG_ADDR, REG_ZERO, REG_TEMP),
            MOV_RRW     (REG_TEMP, REG_ADDR, MOV_RRW_WORD),
            MOV_L       (MOV_LSB, REG_ADDR, REG_DBR, REG_TEMP),
            SKIPS       (2, STATUS_FLAG_M),
            ADDR_OFFI   (1, ADDR_OFF_BANK_WRAP),
            MOV_L       (MOV_MSB, REG_ADDR, REG_DBR, REG_TEMP),
            ALU_OP      (ALU_OP_XOR, STATUS_FLAG_M, REG_ACCUM, REG_TEMP),
            MOV_RR      (REG_TEMP, REG_ACCUM),
            // CHK_ZN      (REG_ACCUM),
        }
    },

    [EOR_DIR_INDL] = {
        .uops = {
            MOV_LPC     (MOV_LSB, REG_ADDR),
            ZEXT        (REG_ADDR),
            SKIPC       (1, STATUS_FLAG_DL),
            IO          /* DL != 0 */,
            ADDR_OFFR   (REG_D, ADDR_OFF_BANK_WRAP),
            MOV_L       (MOV_LSB, REG_ADDR, REG_ZERO, REG_TEMP),
            ADDR_OFFI   (1, ADDR_OFF_BANK_WRAP),
            MOV_L       (MOV_MSB, REG_ADDR, REG_ZERO, REG_TEMP),
            ADDR_OFFI   (1, ADDR_OFF_BANK_WRAP),
            MOV_L       (MOV_LSB, REG_ADDR, REG_ZERO, REG_BANK),
            MOV_RRW     (REG_TEMP, REG_ADDR, MOV_RRW_WORD),
            MOV_L       (MOV_LSB, REG_ADDR, REG_BANK, REG_TEMP),
            SKIPS       (2, STATUS_FLAG_M),
            ADDR_OFFI   (1, ADDR_OFF_BANK_WRAP),
            MOV_L       (MOV_MSB, REG_ADDR, REG_BANK, REG_TEMP),
            ALU_OP      (ALU_OP_XOR, STATUS_FLAG_M, REG_ACCUM, REG_TEMP),
            MOV_RR      (REG_TEMP, REG_ACCUM),
            // CHK_ZN      (REG_ACCUM),
        }
    },

    [EOR_S_REL_IND_Y] = {
        .uops = {
            MOV_LPC     (MOV_LSB, REG_ADDR),
            ZEXT        (REG_ADDR),
            IO,
            ADDR_OFFR   (REG_S, ADDR_OFF_BANK_WRAP),
            MOV_L       (MOV_LSB, REG_ADDR, REG_ZERO, REG_TEMP),
            ADDR_OFFI   (1, ADDR_OFF_BANK_WRAP),
            MOV_L       (MOV_MSB, REG_ADDR, REG_ZERO, REG_TEMP),
            MOV_RRW     (REG_TEMP, REG_ADDR, MOV_RRW_WORD),
            MOV_RRW     (REG_DBR, REG_BANK, MOV_RRW_BYTE),
            IO,
            ADDR_OFFR   (REG_Y, ADDR_OFF_BANK_NEXT),
            MOV_L       (MOV_LSB, REG_ADDR, REG_BANK, REG_TEMP),
            SKIPS       (2, STATUS_FLAG_M),
            ADDR_OFFI   (1, ADDR_OFF_BANK_NEXT),
            MOV_L       (MOV_MSB, REG_ADDR, REG_BANK, REG_TEMP),
            ALU_OP      (ALU_OP_XOR, STATUS_FLAG_M, REG_ACCUM, REG_TEMP),
            MOV_RR      (REG_TEMP, REG_ACCUM),
            // CHK_ZN      (REG_ACCUM),

        }
    },

    [EOR_DIR_X_IND] = {
        .uops = {
            MOV_LPC     (MOV_LSB, REG_ADDR),
            ZEXT        (REG_ADDR),
            SKIPC       (1, STATUS_FLAG_DL),
            IO          /* DL != 0 */,
            ADDR_OFFR   (REG_D, ADDR_OFF_BANK_WRAP),
            ADDR_OFFR   (REG_X, ADDR_OFF_BANK_WRAP),
            IO,
            MOV_L       (MOV_LSB, REG_ADDR, REG_ZERO, REG_TEMP),
            ADDR_OFFI   (1, ADDR_OFF_BANK_WRAP),
            MOV_L       (MOV_MSB, REG_ADDR, REG_ZERO, REG_TEMP),
            MOV_RRW     (REG_TEMP, REG_ADDR, MOV_RRW_WORD),
            MOV_L       (MOV_LSB, REG_ADDR, REG_DBR, REG_TEMP),
            SKIPS       (2, STATUS_FLAG_M),
            ADDR_OFFI   (1, ADDR_OFF_BANK_WRAP),
            MOV_L       (MOV_MSB, REG_ADDR, REG_DBR, REG_TEMP),
            ALU_OP      (ALU_OP_XOR, STATUS_FLAG_M, REG_ACCUM, REG_TEMP),
            MOV_RR      (REG_TEMP, REG_ACCUM),
            // CHK_ZN      (REG_ACCUM),
        }
    },

    [EOR_DIR_IND_Y] = {
        .uops = {
            MOV_LPC     (MOV_LSB, REG_ADDR),
            ZEXT        (REG_ADDR),
            SKIPC       (1, STATUS_FLAG_DL),
            IO          /* DL != 0 */,
            ADDR_OFFR   (REG_D, ADDR_OFF_BANK_WRAP),
            MOV_L       (MOV_LSB, REG_ADDR, REG_ZERO, REG_TEMP),
            ADDR_OFFI   (1, ADDR_OFF_BANK_WRAP),
            MOV_L       (MOV_MSB, REG_ADDR, REG_ZERO, REG_TEMP),
            MOV_RRW     (REG_TEMP, REG_ADDR, MOV_RRW_WORD),
            MOV_RRW     (REG_DBR, REG_BANK, MOV_RRW_BYTE),
            ADDR_OFFR   (REG_Y, ADDR_OFF_BANK_NEXT),
            SKIPS       (2, STATUS_FLAG_X),
            SKIPC       (1, STATUS_FLAG_PAGE),
            IO          /* X = 0 or page crossed */,
            MOV_L       (MOV_LSB, REG_ADDR, REG_BANK, REG_TEMP),
            SKIPS       (2, STATUS_FLAG_M),
            ADDR_OFFI   (1, ADDR_OFF_BANK_NEXT),
            MOV_L       (MOV_MSB, REG_ADDR, REG_BANK, REG_TEMP),
            ALU_OP      (ALU_OP_XOR, STATUS_FLAG_M, REG_ACCUM, REG_TEMP),
            MOV_RR      (REG_TEMP, REG_ACCUM),
            // CHK_ZN      (REG_ACCUM),
        }
    },

    [EOR_DIR_INDL_Y] = {
        .uops = {
            MOV_LPC     (MOV_LSB, REG_ADDR),
            ZEXT        (REG_ADDR),
            ADDR_OFFR   (REG_D, ADDR_OFF_BANK_WRAP),
            SKIPC       (1, STATUS_FLAG_DL),
            IO          /* DL != 0 */,
            MOV_L       (MOV_LSB, REG_ADDR, REG_ZERO, REG_TEMP),
            ADDR_OFFI   (1, ADDR_OFF_BANK_WRAP),
            MOV_L       (MOV_MSB, REG_ADDR, REG_ZERO, REG_TEMP),
            ADDR_OFFI   (1, ADDR_OFF_BANK_WRAP),
            MOV_L       (MOV_LSB, REG_ADDR, REG_ZERO, REG_BANK),
            MOV_RRW     (REG_TEMP, REG_ADDR, MOV_RRW_WORD),
            // MOV_RRW     (REG_DBR, REG_BANK, MOV_RRW_BYTE),
            ADDR_OFFR   (REG_Y, ADDR_OFF_BANK_NEXT),
            MOV_L       (MOV_LSB, REG_ADDR, REG_BANK, REG_TEMP),
            SKIPS       (2, STATUS_FLAG_M),
            ADDR_OFFI   (1, ADDR_OFF_BANK_NEXT),
            MOV_L       (MOV_MSB, REG_ADDR, REG_BANK, REG_TEMP),
            ALU_OP      (ALU_OP_XOR, STATUS_FLAG_M, REG_ACCUM, REG_TEMP),
            MOV_RR      (REG_TEMP, REG_ACCUM),
            // CHK_ZN      (REG_ACCUM),
        }
    },

    [EOR_IMM] = {
        .uops = {
            MOV_LPC     (MOV_LSB, REG_TEMP),
            SKIPS       (1, STATUS_FLAG_M),
            MOV_LPC     (MOV_MSB, REG_TEMP),
            ALU_OP      (ALU_OP_XOR, STATUS_FLAG_M, REG_ACCUM, REG_TEMP),
            MOV_RR      (REG_TEMP, REG_ACCUM),
            // CHK_ZN      (REG_ACCUM),
        }
    },

    /**************************************************************************************/
    /*                                      INC                                           */
    /**************************************************************************************/

    [INC_ABS] = {
        .uops = {
            MOV_LPC     (MOV_LSB, REG_ADDR),
            MOV_LPC     (MOV_MSB, REG_ADDR),
            MOV_L       (MOV_LSB, REG_ADDR, REG_DBR, REG_TEMP),
            SKIPS       (2, STATUS_FLAG_M),
            ADDR_OFFI   (1, ADDR_OFF_BANK_WRAP),
            MOV_L       (MOV_MSB, REG_ADDR, REG_DBR, REG_TEMP),
            IO,
            ALU_OP      (ALU_OP_INC, STATUS_FLAG_M, REG_TEMP, REG_ZERO),
            // CHK_ZNW     (REG_TEMP, 0),
            SKIPS       (2, STATUS_FLAG_M),
            MOV_S       (MOV_MSB, REG_ADDR, REG_DBR, REG_TEMP),
            ADDR_OFFIS  (0xffff, ADDR_OFF_BANK_WRAP, ADDR_OFF_SIGNED),
            MOV_S       (MOV_LSB, REG_ADDR, REG_DBR, REG_TEMP),
        }
    },

    [INC_ACC] = {
        .uops = {
            IO,
            ALU_OP      (ALU_OP_INC, STATUS_FLAG_M, REG_ACCUM, REG_ZERO),
            MOV_RR      (REG_TEMP, REG_ACCUM),
            // CHK_ZN      (REG_ACCUM),
        }
    },

    [INC_ABS_X] = {
        .uops = {
            MOV_LPC     (MOV_LSB, REG_ADDR),
            MOV_LPC     (MOV_MSB, REG_ADDR),
            MOV_RRW     (REG_DBR, REG_BANK, MOV_RRW_BYTE),
            IO,
            ADDR_OFFR   (REG_X, ADDR_OFF_BANK_NEXT),
            MOV_L       (MOV_LSB, REG_ADDR, REG_BANK, REG_TEMP),
            SKIPS       (2, STATUS_FLAG_M),
            ADDR_OFFI   (1, ADDR_OFF_BANK_NEXT),
            MOV_L       (MOV_MSB, REG_ADDR, REG_BANK, REG_TEMP),
            ALU_OP      (ALU_OP_INC, STATUS_FLAG_M, REG_TEMP, REG_ZERO),
            // CHK_ZNW     (REG_TEMP, 0),
            SKIPS       (2, STATUS_FLAG_M),
            MOV_S       (MOV_MSB, REG_ADDR, REG_BANK, REG_TEMP),
            ADDR_OFFIS  (0xffff, ADDR_OFF_BANK_WRAP, ADDR_OFF_SIGNED),
            MOV_S       (MOV_LSB, REG_ADDR, REG_BANK, REG_TEMP),
        }
    },

    [INC_DIR] = {
        .uops = {
            MOV_LPC     (MOV_LSB, REG_ADDR),
            ZEXT        (REG_ADDR),
            SKIPC       (1, STATUS_FLAG_DL),
            IO          /* DL != 0 */,
            ADDR_OFFR   (REG_D, ADDR_OFF_BANK_WRAP),
            MOV_L       (MOV_LSB, REG_ADDR, REG_ZERO, REG_TEMP),
            SKIPS       (2, STATUS_FLAG_M),
            ADDR_OFFI   (1, ADDR_OFF_BANK_WRAP),
            MOV_L       (MOV_MSB, REG_ADDR, REG_ZERO, REG_TEMP),
            IO,
            ALU_OP      (ALU_OP_INC, STATUS_FLAG_M, REG_TEMP, REG_ZERO),
            // CHK_ZNW     (REG_TEMP, 0),
            SKIPS       (2, STATUS_FLAG_M),
            MOV_S       (MOV_MSB, REG_ADDR, REG_ZERO, REG_TEMP),
            ADDR_OFFIS  (0xffff, ADDR_OFF_BANK_WRAP, ADDR_OFF_SIGNED),
            MOV_S       (MOV_LSB, REG_ADDR, REG_ZERO, REG_TEMP),
        }
    },

    [INC_DIR_X] = {
        .uops = {
            MOV_LPC     (MOV_LSB, REG_ADDR),
            ZEXT        (REG_ADDR),
            SKIPC       (1, STATUS_FLAG_DL),
            IO          /* DL != 0 */,
            IO,
            ADDR_OFFR   (REG_D, ADDR_OFF_BANK_WRAP),
            ADDR_OFFR   (REG_X, ADDR_OFF_BANK_WRAP),
            MOV_L       (MOV_LSB, REG_ADDR, REG_ZERO, REG_TEMP),
            SKIPS       (2, STATUS_FLAG_M),
            ADDR_OFFI   (1, ADDR_OFF_BANK_WRAP),
            MOV_L       (MOV_MSB, REG_ADDR, REG_ZERO, REG_TEMP),
            IO,
            ALU_OP      (ALU_OP_INC, STATUS_FLAG_M, REG_TEMP, REG_ZERO),
            // CHK_ZNW     (REG_TEMP, 0),
            SKIPS       (2, STATUS_FLAG_M),
            MOV_S       (MOV_MSB, REG_ADDR, REG_ZERO, REG_TEMP),
            ADDR_OFFIS  (0xffff, ADDR_OFF_BANK_WRAP, ADDR_OFF_SIGNED),
            MOV_S       (MOV_LSB, REG_ADDR, REG_ZERO, REG_TEMP),
        }
    },

    /**************************************************************************************/
    /*                                      INX                                           */
    /**************************************************************************************/

    [INX_IMP] = {
        .uops = {
            IO,
            ALU_OP      (ALU_OP_INC, STATUS_FLAG_X, REG_X, REG_ZERO),
            MOV_RR      (REG_TEMP, REG_X),
            // CHK_ZN      (REG_X),
        }
    },

    /**************************************************************************************/
    /*                                      INY                                           */
    /**************************************************************************************/

    [INY_IMP] = {
        .uops = {
            IO,
            ALU_OP      (ALU_OP_INC, STATUS_FLAG_X, REG_Y, REG_ZERO),
            MOV_RR      (REG_TEMP, REG_Y),
            // CHK_ZN      (REG_Y),
        }
    },

    /**************************************************************************************/
    /*                                      JML                                           */
    /**************************************************************************************/

    [JML_ABS_IND] = {
        .uops = {
            MOV_LPC     (MOV_LSB, REG_ADDR),
            MOV_LPC     (MOV_MSB, REG_ADDR),
            MOV_L       (MOV_LSB, REG_ADDR, REG_ZERO, REG_TEMP),
            ADDR_OFFI   (1, ADDR_OFF_BANK_WRAP),
            MOV_L       (MOV_MSB, REG_ADDR, REG_ZERO, REG_TEMP),
            ADDR_OFFI   (1, ADDR_OFF_BANK_WRAP),
            MOV_L       (MOV_LSB, REG_ADDR, REG_ZERO, REG_PBR),
            MOV_RRW     (REG_TEMP, REG_PC, MOV_RRW_WORD),
        }
    },

    /**************************************************************************************/
    /*                                      JMP                                           */
    /**************************************************************************************/

    [JMP_ABS] = {
        .uops = {
            MOV_LPC     (MOV_LSB, REG_ADDR),
            MOV_LPC     (MOV_MSB, REG_ADDR),
            MOV_RRW     (REG_ADDR, REG_PC, MOV_RRW_WORD),
        }
    },

    [JMP_ABSL] = {
        .uops = {
            MOV_LPC     (MOV_LSB, REG_ADDR),
            MOV_LPC     (MOV_MSB, REG_ADDR),
            MOV_LPC     (MOV_LSB, REG_BANK),
            MOV_RRW     (REG_ADDR, REG_PC, MOV_RRW_WORD),
            MOV_RRW     (REG_BANK, REG_PBR, MOV_RRW_BYTE),
        }
    },

    [JMP_ABS_IND] = {
        .uops = {
            MOV_LPC     (MOV_LSB, REG_ADDR),
            MOV_LPC     (MOV_MSB, REG_ADDR),
            MOV_L       (MOV_LSB, REG_ADDR, REG_ZERO, REG_TEMP),
            ADDR_OFFI   (1, ADDR_OFF_BANK_WRAP),
            MOV_L       (MOV_MSB, REG_ADDR, REG_ZERO, REG_TEMP),
            MOV_RRW      (REG_TEMP, REG_PC, MOV_RRW_WORD),
        }
    },

    [JMP_ABS_X_IND] = {
        .uops = {
            MOV_LPC     (MOV_LSB, REG_ADDR),
            MOV_LPC     (MOV_MSB, REG_ADDR),
            IO,
            ADDR_OFFR   (REG_X, ADDR_OFF_BANK_NEXT),
            MOV_L       (MOV_LSB, REG_ADDR, REG_PBR, REG_TEMP),
            ADDR_OFFI   (1, ADDR_OFF_BANK_WRAP),
            MOV_L       (MOV_MSB, REG_ADDR, REG_PBR, REG_TEMP),
            MOV_RRW     (REG_TEMP, REG_PC, MOV_RRW_WORD),
        }
    },

    /**************************************************************************************/
    /*                                      JSL                                           */
    /**************************************************************************************/

    [JSL_ABSL] = {
        .uops = {
            MOV_LPC     (MOV_LSB, REG_ADDR),
            MOV_LPC     (MOV_MSB, REG_ADDR),
            MOV_S       (MOV_LSB, REG_S, REG_ZERO, REG_PBR),
            DECS,
            IO,
            MOV_L       (MOV_LSB, REG_PC, REG_PBR, REG_BANK),
            MOV_S       (MOV_MSB, REG_S, REG_ZERO, REG_PC),
            DECS,
            MOV_S       (MOV_LSB, REG_S, REG_ZERO, REG_PC),
            DECS,
            MOV_RRW     (REG_ADDR, REG_PC, MOV_RRW_WORD),
            MOV_RRW     (REG_BANK, REG_PBR, MOV_RRW_BYTE),
        }
    },

    /**************************************************************************************/
    /*                                      JSR                                           */
    /**************************************************************************************/

    [JSR_ABS] = {
        .uops = {
            MOV_LPC     (MOV_LSB, REG_ADDR),
            MOV_L       (MOV_MSB, REG_PC, REG_PBR, REG_ADDR),
            IO,
            MOV_S       (MOV_MSB, REG_S, REG_ZERO, REG_PC),
            DECS,
            MOV_S       (MOV_LSB, REG_S, REG_ZERO, REG_PC),
            DECS,
            MOV_RRW     (REG_ADDR, REG_PC, MOV_RRW_WORD),
        }
    },

    [JSR_ABS_X_IND] = {
        .uops = {
            MOV_LPC     (MOV_LSB, REG_ADDR),
            MOV_S       (MOV_MSB, REG_S, REG_ZERO, REG_PC),
            DECS,
            MOV_S       (MOV_LSB, REG_S, REG_ZERO, REG_PC),
            DECS,
            MOV_L       (MOV_MSB, REG_PC, REG_PBR, REG_ADDR),
            IO,
            ADDR_OFFR   (REG_X, ADDR_OFF_BANK_NEXT),
            MOV_L       (MOV_LSB, REG_ADDR, REG_PBR, REG_TEMP),
            ADDR_OFFI   (1, ADDR_OFF_BANK_NEXT),
            MOV_L       (MOV_MSB, REG_ADDR, REG_PBR, REG_TEMP),
            MOV_RRW     (REG_TEMP, REG_PC, MOV_RRW_WORD),
        }
    },

    /**************************************************************************************/
    /*                                      LDA                                           */
    /**************************************************************************************/


    [LDA_ABS] = {
        .uops = {
            MOV_LPC     (MOV_LSB, REG_ADDR),
            MOV_LPC     (MOV_MSB, REG_ADDR),
            MOV_L       (MOV_LSB, REG_ADDR, REG_DBR, REG_TEMP),
            SKIPS       (2, STATUS_FLAG_M),
            ADDR_OFFI   (1, ADDR_OFF_BANK_WRAP),
            MOV_L       (MOV_MSB, REG_ADDR, REG_DBR, REG_TEMP),
            MOV_RR      (REG_TEMP, REG_ACCUM),
            CHK_ZN      (REG_ACCUM),
        }
    },

    [LDA_ABS_X] = {
        .uops = {
            MOV_LPC     (MOV_LSB, REG_ADDR),
            MOV_LPC     (MOV_MSB, REG_ADDR),
            MOV_RRW     (REG_DBR, REG_BANK, MOV_RRW_BYTE),
            ADDR_OFFR   (REG_X, ADDR_OFF_BANK_NEXT),
            SKIPC       (2, STATUS_FLAG_PAGE),
            SKIPS       (1, STATUS_FLAG_X),
            IO          /* X=0 or page boundary */,
            MOV_L       (MOV_LSB, REG_ADDR, REG_BANK, REG_TEMP),
            SKIPS       (2, STATUS_FLAG_M),
            ADDR_OFFI   (1, ADDR_OFF_BANK_NEXT),
            MOV_L       (MOV_MSB, REG_ADDR, REG_BANK, REG_TEMP),
            MOV_RR      (REG_TEMP, REG_ACCUM),
            CHK_ZN      (REG_ACCUM),
        }
    },
    [LDA_ABS_Y] = {
        .uops = {
            MOV_LPC     (MOV_LSB, REG_ADDR),
            MOV_LPC     (MOV_MSB, REG_ADDR),
            MOV_RRW     (REG_DBR, REG_BANK, MOV_RRW_BYTE),
            ADDR_OFFR   (REG_Y, ADDR_OFF_BANK_NEXT),
            SKIPC       (2, STATUS_FLAG_PAGE),
            SKIPS       (1, STATUS_FLAG_X),
            IO          /* X=0 or page boundary */,
            MOV_L       (MOV_LSB, REG_ADDR, REG_BANK, REG_TEMP),
            SKIPS       (2, STATUS_FLAG_M),
            ADDR_OFFI   (1, ADDR_OFF_BANK_NEXT),
            MOV_L       (MOV_MSB, REG_ADDR, REG_BANK, REG_TEMP),
            MOV_RR      (REG_TEMP, REG_ACCUM),
            CHK_ZN      (REG_ACCUM),
        }
    },

    [LDA_ABSL] = {
        .uops = {
            MOV_LPC     (MOV_LSB, REG_ADDR),
            MOV_LPC     (MOV_MSB, REG_ADDR),
            MOV_LPC     (MOV_LSB, REG_BANK),
            MOV_L       (MOV_LSB, REG_ADDR, REG_BANK, REG_TEMP),
            SKIPS       (2, STATUS_FLAG_M),
            ADDR_OFFI   (1, ADDR_OFF_BANK_WRAP),
            MOV_L       (MOV_MSB, REG_ADDR, REG_BANK, REG_TEMP),
            MOV_RR      (REG_TEMP, REG_ACCUM),
            CHK_ZN      (REG_ACCUM),
        }
    },

    [LDA_ABSL_X] = {
        .uops = {
            MOV_LPC     (MOV_LSB, REG_ADDR),
            MOV_LPC     (MOV_MSB, REG_ADDR),
            MOV_LPC     (MOV_LSB, REG_BANK),
            ADDR_OFFR   (REG_X, ADDR_OFF_BANK_NEXT),
            MOV_L       (MOV_LSB, REG_ADDR, REG_BANK, REG_TEMP),
            SKIPS       (2, STATUS_FLAG_M),
            ADDR_OFFI   (1, ADDR_OFF_BANK_NEXT),
            MOV_L       (MOV_MSB, REG_ADDR, REG_BANK, REG_TEMP),
            MOV_RR      (REG_TEMP, REG_ACCUM),
            CHK_ZN      (REG_ACCUM),
        }
    },

    [LDA_DIR] = {
        .uops = {
            MOV_LPC     (MOV_LSB, REG_ADDR),
            ZEXT        (REG_ADDR),
            ADDR_OFFR   (REG_D, ADDR_OFF_BANK_WRAP),
            SKIPC       (1, STATUS_FLAG_DL),
            IO          /* DL != 0 */,
            MOV_L       (MOV_LSB, REG_ADDR, REG_ZERO, REG_TEMP),
            SKIPS       (2, STATUS_FLAG_M),
            ADDR_OFFI   (1, ADDR_OFF_BANK_WRAP),
            MOV_L       (MOV_MSB, REG_ADDR, REG_ZERO, REG_TEMP),
            MOV_RR      (REG_TEMP, REG_ACCUM),
            CHK_ZN      (REG_ACCUM),
        }
    },

    [LDA_S_REL] = {
        .uops = {
            MOV_LPC     (MOV_LSB, REG_ADDR),
            ZEXT        (REG_ADDR),
            ADDR_OFFR   (REG_S, ADDR_OFF_BANK_WRAP),
            IO,
            MOV_L       (MOV_LSB, REG_ADDR, REG_ZERO, REG_TEMP),
            SKIPS       (2, STATUS_FLAG_M),
            ADDR_OFFI   (1, ADDR_OFF_BANK_WRAP),
            MOV_L       (MOV_MSB, REG_ADDR, REG_ZERO, REG_TEMP),
            MOV_RR      (REG_TEMP, REG_ACCUM),
            CHK_ZN      (REG_ACCUM),
        },
    },

    [LDA_DIR_X] = {
        .uops = {
            MOV_LPC     (MOV_LSB, REG_ADDR),
            ZEXT        (REG_ADDR),
            SKIPC       (1, STATUS_FLAG_DL),
            IO          /* DL != 0 */,
            ADDR_OFFR   (REG_D, ADDR_OFF_BANK_WRAP),
            ADDR_OFFR   (REG_X, ADDR_OFF_BANK_WRAP),
            IO,
            MOV_L       (MOV_LSB, REG_ADDR, REG_ZERO, REG_TEMP),
            SKIPS       (2, STATUS_FLAG_M),
            ADDR_OFFI   (1, ADDR_OFF_BANK_WRAP),
            MOV_L       (MOV_MSB, REG_ADDR, REG_ZERO, REG_TEMP),
            MOV_RR      (REG_TEMP, REG_ACCUM),
            CHK_ZN      (REG_ACCUM),
        }
    },

    [LDA_DIR_IND] = {
        .uops = {
            MOV_LPC     (MOV_LSB, REG_ADDR),
            ZEXT        (REG_ADDR),
            SKIPC       (1, STATUS_FLAG_DL),
            IO          /* DL != 0 */,
            ADDR_OFFR   (REG_D, ADDR_OFF_BANK_WRAP),
            MOV_L       (MOV_LSB, REG_ADDR, REG_ZERO, REG_TEMP),
            ADDR_OFFI   (1, ADDR_OFF_BANK_WRAP),
            MOV_L       (MOV_MSB, REG_ADDR, REG_ZERO, REG_TEMP),
            MOV_RRW     (REG_TEMP, REG_ADDR, MOV_RRW_WORD),
            MOV_L       (MOV_LSB, REG_ADDR, REG_DBR, REG_TEMP),
            SKIPS       (2, STATUS_FLAG_M),
            ADDR_OFFI   (1, ADDR_OFF_BANK_WRAP),
            MOV_L       (MOV_MSB, REG_ADDR, REG_DBR, REG_TEMP),
            MOV_RR      (REG_TEMP, REG_ACCUM),
            CHK_ZN      (REG_ACCUM),
        }
    },

    [LDA_DIR_INDL] = {
        .uops = {
            MOV_LPC     (MOV_LSB, REG_ADDR),
            ZEXT        (REG_ADDR),
            SKIPC       (1, STATUS_FLAG_DL),
            IO          /* DL != 0 */,
            ADDR_OFFR   (REG_D, ADDR_OFF_BANK_WRAP),
            MOV_L       (MOV_LSB, REG_ADDR, REG_ZERO, REG_TEMP),
            ADDR_OFFI   (1, ADDR_OFF_BANK_WRAP),
            MOV_L       (MOV_MSB, REG_ADDR, REG_ZERO, REG_TEMP),
            ADDR_OFFI   (1, ADDR_OFF_BANK_WRAP),
            MOV_L       (MOV_LSB, REG_ADDR, REG_ZERO, REG_BANK),
            MOV_RRW     (REG_TEMP, REG_ADDR, MOV_RRW_WORD),
            MOV_L       (MOV_LSB, REG_ADDR, REG_BANK, REG_TEMP),
            SKIPS       (2, STATUS_FLAG_M),
            ADDR_OFFI   (1, ADDR_OFF_BANK_WRAP),
            MOV_L       (MOV_MSB, REG_ADDR, REG_BANK, REG_TEMP),
            MOV_RR      (REG_TEMP, REG_ACCUM),
            CHK_ZN      (REG_ACCUM),
        }
    },

    [LDA_S_REL_IND_Y] = {
        .uops = {
            MOV_LPC     (MOV_LSB, REG_ADDR),
            ZEXT        (REG_ADDR),
            IO,
            ADDR_OFFR   (REG_S, ADDR_OFF_BANK_WRAP),
            MOV_L       (MOV_LSB, REG_ADDR, REG_ZERO, REG_TEMP),
            ADDR_OFFI   (1, ADDR_OFF_BANK_WRAP),
            MOV_L       (MOV_MSB, REG_ADDR, REG_ZERO, REG_TEMP),
            MOV_RRW     (REG_TEMP, REG_ADDR, MOV_RRW_WORD),
            MOV_RRW     (REG_DBR, REG_BANK, MOV_RRW_BYTE),
            IO,
            ADDR_OFFR   (REG_Y, ADDR_OFF_BANK_NEXT),
            MOV_L       (MOV_LSB, REG_ADDR, REG_BANK, REG_TEMP),
            SKIPS       (2, STATUS_FLAG_M),
            ADDR_OFFI   (1, ADDR_OFF_BANK_NEXT),
            MOV_L       (MOV_MSB, REG_ADDR, REG_BANK, REG_TEMP),
            MOV_RR      (REG_TEMP, REG_ACCUM),
            CHK_ZN      (REG_ACCUM),

        }
    },

    [LDA_DIR_X_IND] = {
        .uops = {
            MOV_LPC     (MOV_LSB, REG_ADDR),
            ZEXT        (REG_ADDR),
            SKIPC       (1, STATUS_FLAG_DL),
            IO          /* DL != 0 */,
            ADDR_OFFR   (REG_D, ADDR_OFF_BANK_WRAP),
            ADDR_OFFR   (REG_X, ADDR_OFF_BANK_WRAP),
            IO,
            MOV_L       (MOV_LSB, REG_ADDR, REG_ZERO, REG_TEMP),
            ADDR_OFFI   (1, ADDR_OFF_BANK_WRAP),
            MOV_L       (MOV_MSB, REG_ADDR, REG_ZERO, REG_TEMP),
            MOV_RRW     (REG_TEMP, REG_ADDR, MOV_RRW_WORD),
            MOV_L       (MOV_LSB, REG_ADDR, REG_DBR, REG_TEMP),
            SKIPS       (2, STATUS_FLAG_M),
            ADDR_OFFI   (1, ADDR_OFF_BANK_WRAP),
            MOV_L       (MOV_MSB, REG_ADDR, REG_DBR, REG_TEMP),
            MOV_RR      (REG_TEMP, REG_ACCUM),
            CHK_ZN      (REG_ACCUM),
        }
    },

    [LDA_DIR_IND_Y] = {
        .uops = {
            MOV_LPC     (MOV_LSB, REG_ADDR),
            ZEXT        (REG_ADDR),
            SKIPC       (1, STATUS_FLAG_DL),
            IO          /* DL != 0 */,
            ADDR_OFFR   (REG_D, 0),
            MOV_L       (MOV_LSB, REG_ADDR, REG_ZERO, REG_TEMP),
            ADDR_OFFI   (1, ADDR_OFF_BANK_WRAP),
            MOV_L       (MOV_MSB, REG_ADDR, REG_ZERO, REG_TEMP),
            MOV_RRW     (REG_TEMP, REG_ADDR, MOV_RRW_WORD),
            MOV_RRW     (REG_DBR, REG_BANK, MOV_RRW_BYTE),
            ADDR_OFFR   (REG_Y, ADDR_OFF_BANK_NEXT),
            SKIPS       (2, STATUS_FLAG_X),
            SKIPC       (1, STATUS_FLAG_PAGE),
            IO          /* X = 0 or page crossed */,
            MOV_L       (MOV_LSB, REG_ADDR, REG_BANK, REG_TEMP),
            SKIPS       (2, STATUS_FLAG_M),
            ADDR_OFFI   (1, ADDR_OFF_BANK_NEXT),
            MOV_L       (MOV_MSB, REG_ADDR, REG_BANK, REG_TEMP),
            MOV_RR      (REG_TEMP, REG_ACCUM),
            CHK_ZN      (REG_ACCUM),
        }
    },

    [LDA_DIR_INDL_Y] = {
        .uops = {
            MOV_LPC     (MOV_LSB, REG_ADDR),
            ZEXT        (REG_ADDR),
            ADDR_OFFR   (REG_D, 0),
            SKIPC       (1, STATUS_FLAG_DL),
            IO          /* DL != 0 */,
            MOV_L       (MOV_LSB, REG_ADDR, REG_ZERO, REG_TEMP),
            ADDR_OFFI   (1, ADDR_OFF_BANK_WRAP),
            MOV_L       (MOV_MSB, REG_ADDR, REG_ZERO, REG_TEMP),
            ADDR_OFFI   (1, ADDR_OFF_BANK_WRAP),
            MOV_L       (MOV_LSB, REG_ADDR, REG_ZERO, REG_BANK),
            MOV_RRW     (REG_TEMP, REG_ADDR, MOV_RRW_WORD),
            // MOV_RRW     (REG_DBR, REG_BANK, MOV_RRW_BYTE),
            ADDR_OFFR   (REG_Y, ADDR_OFF_BANK_NEXT),
            MOV_L       (MOV_LSB, REG_ADDR, REG_BANK, REG_TEMP),
            SKIPS       (2, STATUS_FLAG_M),
            ADDR_OFFI   (1, ADDR_OFF_BANK_NEXT),
            MOV_L       (MOV_MSB, REG_ADDR, REG_BANK, REG_TEMP),
            MOV_RR      (REG_TEMP, REG_ACCUM),
            CHK_ZN      (REG_ACCUM),
        }
    },

    [LDA_IMM] = {
        .uops = {
            MOV_LPC     (MOV_LSB, REG_TEMP),
            SKIPS       (1, STATUS_FLAG_M),
            MOV_LPC     (MOV_MSB, REG_TEMP),
            MOV_RR      (REG_TEMP, REG_ACCUM),
            CHK_ZN      (REG_ACCUM)
        }
    },

    /**************************************************************************************/
    /*                                      LDX                                           */
    /**************************************************************************************/

    [LDX_ABS] = {
        .uops = {
            MOV_LPC     (MOV_LSB, REG_ADDR),
            MOV_LPC     (MOV_MSB, REG_ADDR),
            MOV_L       (MOV_LSB, REG_ADDR, REG_DBR, REG_TEMP),
            SKIPS       (2, STATUS_FLAG_X),
            ADDR_OFFI   (1, ADDR_OFF_BANK_WRAP),
            MOV_L       (MOV_MSB, REG_ADDR, REG_DBR, REG_TEMP),
            MOV_RR      (REG_TEMP, REG_X),
            CHK_ZN      (REG_X),
        }
    },

    [LDX_ABS_Y] = {
        .uops = {
            MOV_LPC     (MOV_LSB, REG_ADDR),
            MOV_LPC     (MOV_MSB, REG_ADDR),
            MOV_RRW     (REG_DBR, REG_BANK, MOV_RRW_BYTE),
            ADDR_OFFR   (REG_Y, ADDR_OFF_BANK_NEXT),
            SKIPC       (2, STATUS_FLAG_PAGE),
            SKIPS       (1, STATUS_FLAG_X),
            IO          /* X=0 or page boundary */,
            MOV_L       (MOV_LSB, REG_ADDR, REG_BANK, REG_TEMP),
            SKIPS       (2, STATUS_FLAG_X),
            ADDR_OFFI   (1, ADDR_OFF_BANK_NEXT),
            MOV_L       (MOV_MSB, REG_ADDR, REG_BANK, REG_TEMP),
            MOV_RR      (REG_TEMP, REG_X),
            CHK_ZN      (REG_X),
        }
    },

    [LDX_DIR] = {
        .uops = {
            MOV_LPC     (MOV_LSB, REG_ADDR),
            ZEXT        (REG_ADDR),
            ADDR_OFFR   (REG_D, ADDR_OFF_BANK_WRAP),
            SKIPC       (1, STATUS_FLAG_DL),
            IO          /* DL != 0 */,
            MOV_L       (MOV_LSB, REG_ADDR, REG_ZERO, REG_TEMP),
            SKIPS       (2, STATUS_FLAG_X),
            ADDR_OFFI   (1, ADDR_OFF_BANK_WRAP),
            MOV_L       (MOV_MSB, REG_ADDR, REG_ZERO, REG_TEMP),
            MOV_RR      (REG_TEMP, REG_X),
            CHK_ZN      (REG_X),
        }
    },

    [LDX_DIR_Y] = {
        .uops = {
            MOV_LPC     (MOV_LSB, REG_ADDR),
            ZEXT        (REG_ADDR),
            SKIPC       (1, STATUS_FLAG_DL),
            IO          /* DL != 0 */,
            ADDR_OFFR   (REG_D, ADDR_OFF_BANK_WRAP),
            ADDR_OFFR   (REG_Y, ADDR_OFF_BANK_WRAP),
            IO,
            MOV_L       (MOV_LSB, REG_ADDR, REG_ZERO, REG_TEMP),
            SKIPS       (2, STATUS_FLAG_X),
            ADDR_OFFI   (1, ADDR_OFF_BANK_WRAP),
            MOV_L       (MOV_MSB, REG_ADDR, REG_ZERO, REG_TEMP),
            MOV_RR      (REG_TEMP, REG_X),
            CHK_ZN      (REG_X),
        }
    },

    [LDX_IMM] = {
        .uops = {
            MOV_LPC     (MOV_LSB, REG_TEMP),
            SKIPS       (1, STATUS_FLAG_X),
            MOV_LPC     (MOV_MSB, REG_TEMP),
            MOV_RR      (REG_TEMP, REG_X),
            CHK_ZN      (REG_X)
        }
    },

    /**************************************************************************************/
    /*                                      LDY                                           */
    /**************************************************************************************/

    [LDY_ABS] = {
        .uops = {
            MOV_LPC     (MOV_LSB, REG_ADDR),
            MOV_LPC     (MOV_MSB, REG_ADDR),
            MOV_L       (MOV_LSB, REG_ADDR, REG_DBR, REG_TEMP),
            SKIPS       (2, STATUS_FLAG_X),
            ADDR_OFFI   (1, 0),
            MOV_L       (MOV_MSB, REG_ADDR, REG_DBR, REG_TEMP),
            MOV_RR      (REG_TEMP, REG_Y),
            CHK_ZN      (REG_Y),
        }
    },

    [LDY_ABS_X] = {
        .uops = {
            MOV_LPC     (MOV_LSB, REG_ADDR),
            MOV_LPC     (MOV_MSB, REG_ADDR),
            MOV_RRW     (REG_DBR, REG_BANK, MOV_RRW_BYTE),
            ADDR_OFFR   (REG_X, ADDR_OFF_BANK_NEXT),
            SKIPC       (2, STATUS_FLAG_PAGE),
            SKIPS       (1, STATUS_FLAG_X),
            IO          /* X=0 or page boundary */,
            MOV_L       (MOV_LSB, REG_ADDR, REG_BANK, REG_TEMP),
            SKIPS       (2, STATUS_FLAG_X),
            ADDR_OFFI   (1, ADDR_OFF_BANK_NEXT),
            MOV_L       (MOV_MSB, REG_ADDR, REG_BANK, REG_TEMP),
            MOV_RR      (REG_TEMP, REG_Y),
            CHK_ZN      (REG_Y),
        }
    },

    [LDY_DIR] = {
        .uops = {
            MOV_LPC     (MOV_LSB, REG_ADDR),
            ZEXT        (REG_ADDR),
            ADDR_OFFR   (REG_D, ADDR_OFF_BANK_WRAP),
            SKIPC       (1, STATUS_FLAG_DL),
            IO          /* DL != 0 */,
            MOV_L       (MOV_LSB, REG_ADDR, REG_ZERO, REG_TEMP),
            SKIPS       (2, STATUS_FLAG_X),
            ADDR_OFFI   (1, ADDR_OFF_BANK_WRAP),
            MOV_L       (MOV_MSB, REG_ADDR, REG_ZERO, REG_TEMP),
            MOV_RR      (REG_TEMP, REG_Y),
            CHK_ZN      (REG_Y),
        }
    },

    [LDY_DIR_X] = {
        .uops = {
            MOV_LPC     (MOV_LSB, REG_ADDR),
            ZEXT        (REG_ADDR),
            SKIPC       (1, STATUS_FLAG_DL),
            IO          /* DL != 0 */,
            ADDR_OFFR   (REG_D, ADDR_OFF_BANK_WRAP),
            ADDR_OFFR   (REG_X, ADDR_OFF_BANK_WRAP),
            IO,
            MOV_L       (MOV_LSB, REG_ADDR, REG_ZERO, REG_TEMP),
            SKIPS       (2, STATUS_FLAG_X),
            ADDR_OFFI   (1, ADDR_OFF_BANK_WRAP),
            MOV_L       (MOV_MSB, REG_ADDR, REG_ZERO, REG_TEMP),
            MOV_RR      (REG_TEMP, REG_Y),
            CHK_ZN      (REG_Y),
        }
    },

    [LDY_IMM] = {
        .uops = {
            MOV_LPC     (MOV_LSB, REG_TEMP),
            SKIPS       (1, STATUS_FLAG_X),
            MOV_LPC     (MOV_MSB, REG_TEMP),
            MOV_RR      (REG_TEMP, REG_Y),
            CHK_ZN      (REG_Y)
        }
    },

    /**************************************************************************************/
    /*                                      LSR                                           */
    /**************************************************************************************/

    [LSR_ABS] = {
        .uops = {
            MOV_LPC     (MOV_LSB, REG_ADDR),
            MOV_LPC     (MOV_MSB, REG_ADDR),
            MOV_L       (MOV_LSB, REG_ADDR, REG_DBR, REG_TEMP),
            SKIPS       (2, STATUS_FLAG_M),
            ADDR_OFFI   (1, ADDR_OFF_BANK_WRAP),
            MOV_L       (MOV_MSB, REG_ADDR, REG_DBR, REG_TEMP),
            IO,
            ALU_OP      (ALU_OP_SHR, STATUS_FLAG_M, REG_TEMP, REG_ZERO),
            // CHK_ZNW     (REG_TEMP, 0),
            SKIPS       (2, STATUS_FLAG_M),
            MOV_S       (MOV_MSB, REG_ADDR, REG_DBR, REG_TEMP),
            ADDR_OFFIS  (0xffff, ADDR_OFF_BANK_WRAP, ADDR_OFF_SIGNED),
            MOV_S       (MOV_LSB, REG_ADDR, REG_DBR, REG_TEMP),
        }
    },

    [LSR_ACC] = {
        .uops = {
            IO,
            ALU_OP      (ALU_OP_SHR, STATUS_FLAG_M, REG_ACCUM, REG_ZERO),
            MOV_RR      (REG_TEMP, REG_ACCUM),
            // CHK_ZN      (REG_ACCUM),
        }
    },

    [LSR_ABS_X] = {
        .uops = {
            MOV_LPC     (MOV_LSB, REG_ADDR),
            MOV_LPC     (MOV_MSB, REG_ADDR),
            MOV_RRW     (REG_DBR, REG_BANK, MOV_RRW_BYTE),
            IO,
            ADDR_OFFR   (REG_X, ADDR_OFF_BANK_NEXT),
            MOV_L       (MOV_LSB, REG_ADDR, REG_BANK, REG_TEMP),
            SKIPS       (2, STATUS_FLAG_M),
            ADDR_OFFI   (1, ADDR_OFF_BANK_NEXT),
            MOV_L       (MOV_MSB, REG_ADDR, REG_BANK, REG_TEMP),
            ALU_OP      (ALU_OP_SHR, STATUS_FLAG_M, REG_TEMP, REG_ZERO),
            // CHK_ZNW     (REG_TEMP, 0),
            SKIPS       (2, STATUS_FLAG_M),
            MOV_S       (MOV_MSB, REG_ADDR, REG_BANK, REG_TEMP),
            ADDR_OFFIS  (0xffff, ADDR_OFF_BANK_WRAP, ADDR_OFF_SIGNED),
            MOV_S       (MOV_LSB, REG_ADDR, REG_BANK, REG_TEMP),
        }
    },

    [LSR_DIR] = {
        .uops = {
            MOV_LPC     (MOV_LSB, REG_ADDR),
            ZEXT        (REG_ADDR),
            SKIPC       (1, STATUS_FLAG_DL),
            IO          /* DL != 0 */,
            ADDR_OFFR   (REG_D, ADDR_OFF_BANK_WRAP),
            MOV_L       (MOV_LSB, REG_ADDR, REG_ZERO, REG_TEMP),
            SKIPS       (2, STATUS_FLAG_M),
            ADDR_OFFI   (1, ADDR_OFF_BANK_WRAP),
            MOV_L       (MOV_MSB, REG_ADDR, REG_ZERO, REG_TEMP),
            IO,
            ALU_OP      (ALU_OP_SHR, STATUS_FLAG_M, REG_TEMP, REG_ZERO),
            // CHK_ZNW     (REG_TEMP, 0),
            SKIPS       (2, STATUS_FLAG_M),
            MOV_S       (MOV_MSB, REG_ADDR, REG_ZERO, REG_TEMP),
            ADDR_OFFIS  (0xffff, ADDR_OFF_BANK_WRAP, ADDR_OFF_SIGNED),
            MOV_S       (MOV_LSB, REG_ADDR, REG_ZERO, REG_TEMP),
        }
    },

    [LSR_DIR_X] = {
        .uops = {
            MOV_LPC     (MOV_LSB, REG_ADDR),
            ZEXT        (REG_ADDR),
            SKIPC       (1, STATUS_FLAG_DL),
            IO          /* DL != 0 */,
            IO,
            ADDR_OFFR   (REG_D, ADDR_OFF_BANK_WRAP),
            ADDR_OFFR   (REG_X, ADDR_OFF_BANK_WRAP),
            MOV_L       (MOV_LSB, REG_ADDR, REG_ZERO, REG_TEMP),
            SKIPS       (2, STATUS_FLAG_M),
            ADDR_OFFI   (1, ADDR_OFF_BANK_WRAP),
            MOV_L       (MOV_MSB, REG_ADDR, REG_ZERO, REG_TEMP),
            IO,
            ALU_OP      (ALU_OP_SHR, STATUS_FLAG_M, REG_TEMP, REG_ZERO),
            // CHK_ZNW     (REG_TEMP, 0),
            SKIPS       (2, STATUS_FLAG_M),
            MOV_S       (MOV_MSB, REG_ADDR, REG_ZERO, REG_TEMP),
            ADDR_OFFIS  (0xffff, ADDR_OFF_BANK_WRAP, ADDR_OFF_SIGNED),
            MOV_S       (MOV_LSB, REG_ADDR, REG_ZERO, REG_TEMP),
        }
    },

    /**************************************************************************************/
    /*                                      MVN                                           */
    /**************************************************************************************/

    [MVN_BLK] = {
        .uops = {
            MOV_LPC     (MOV_LSB, REG_BANK),
            MOV_LPC     (MOV_LSB, REG_ADDR),
            MOV_L       (MOV_LSB, REG_X, REG_ADDR, REG_TEMP),
            MOV_S       (MOV_LSB, REG_Y, REG_BANK, REG_TEMP),
            IO,
            INC_RW      (REG_X),
            IO,
            INC_RW      (REG_Y),
            DEC_RW      (REG_ACCUM),
            SKIPS       (3, STATUS_FLAG_AM),
            /* we're not done copying stuff yet, so reset PC
            to the start of the instruction */
            DEC_RW      (REG_PC),
            DEC_RW      (REG_PC),
            DEC_RW      (REG_PC),
        }
    },

    /**************************************************************************************/
    /*                                      MVP                                           */
    /**************************************************************************************/
    [MVP_BLK] = {
        .uops = {
            MOV_LPC     (MOV_LSB, REG_BANK),
            MOV_LPC     (MOV_LSB, REG_ADDR),
            MOV_L       (MOV_LSB, REG_X, REG_ADDR, REG_TEMP),
            MOV_S       (MOV_LSB, REG_Y, REG_BANK, REG_TEMP),
            IO,
            DEC_RW      (REG_X),
            IO,
            DEC_RW      (REG_Y),
            DEC_RW      (REG_ACCUM),
            SKIPS       (3, STATUS_FLAG_AM),
            /* we're not done copying stuff yet, so reset PC
            to the start of the instruction */
            DEC_RW      (REG_PC),
            DEC_RW      (REG_PC),
            DEC_RW      (REG_PC),
        }
    },
    /**************************************************************************************/
    /*                                      NOP                                           */
    /**************************************************************************************/

    [NOP_IMP] = {
        .uops = {
            SKIPC   (0, 0)
        }
    },

    /**************************************************************************************/
    /*                                      ORA                                           */
    /**************************************************************************************/

    [ORA_ABS] = {
        .uops = {
            MOV_LPC     (MOV_LSB, REG_ADDR),
            MOV_LPC     (MOV_MSB, REG_ADDR),
            MOV_L       (MOV_LSB, REG_ADDR, REG_DBR, REG_TEMP),
            SKIPS       (2, STATUS_FLAG_M),
            ADDR_OFFI   (1, ADDR_OFF_BANK_WRAP),
            MOV_L       (MOV_MSB, REG_ADDR, REG_DBR, REG_TEMP),
            ALU_OP      (ALU_OP_OR, STATUS_FLAG_M, REG_ACCUM, REG_TEMP),
            MOV_RR      (REG_TEMP, REG_ACCUM),
            // CHK_ZN      (REG_ACCUM),
        }
    },
    [ORA_ABS_X] = {
        .uops = {
            MOV_LPC     (MOV_LSB, REG_ADDR),
            MOV_LPC     (MOV_MSB, REG_ADDR),
            MOV_RRW     (REG_DBR, REG_BANK, MOV_RRW_BYTE),
            ADDR_OFFR   (REG_X, ADDR_OFF_BANK_NEXT),
            SKIPC       (2, STATUS_FLAG_PAGE),
            SKIPS       (1, STATUS_FLAG_X),
            IO          /* X=0 or page boundary */,
            MOV_L       (MOV_LSB, REG_ADDR, REG_BANK, REG_TEMP),
            SKIPS       (2, STATUS_FLAG_M),
            ADDR_OFFI   (1, ADDR_OFF_BANK_NEXT),
            MOV_L       (MOV_MSB, REG_ADDR, REG_BANK, REG_TEMP),
            ALU_OP      (ALU_OP_OR, STATUS_FLAG_M, REG_ACCUM, REG_TEMP),
            MOV_RR      (REG_TEMP, REG_ACCUM),
            // CHK_ZN      (REG_ACCUM),
        }
    },
    [ORA_ABS_Y] = {
        .uops = {
            MOV_LPC     (MOV_LSB, REG_ADDR),
            MOV_LPC     (MOV_MSB, REG_ADDR),
            MOV_RRW     (REG_DBR, REG_BANK, MOV_RRW_BYTE),
            ADDR_OFFR   (REG_Y, ADDR_OFF_BANK_NEXT),
            SKIPC       (2, STATUS_FLAG_PAGE),
            SKIPS       (1, STATUS_FLAG_X),
            IO          /* X=0 or page boundary */,
            MOV_L       (MOV_LSB, REG_ADDR, REG_BANK, REG_TEMP),
            SKIPS       (2, STATUS_FLAG_M),
            ADDR_OFFI   (1, ADDR_OFF_BANK_NEXT),
            MOV_L       (MOV_MSB, REG_ADDR, REG_BANK, REG_TEMP),
            ALU_OP      (ALU_OP_OR, STATUS_FLAG_M, REG_ACCUM, REG_TEMP),
            MOV_RR      (REG_TEMP, REG_ACCUM),
            // CHK_ZN      (REG_ACCUM),
        }
    },
    [ORA_ABSL] = {
        .uops = {
            MOV_LPC     (MOV_LSB, REG_ADDR),
            MOV_LPC     (MOV_MSB, REG_ADDR),
            MOV_LPC     (MOV_LSB, REG_BANK),
            MOV_L       (MOV_LSB, REG_ADDR, REG_BANK, REG_TEMP),
            SKIPS       (2, STATUS_FLAG_M),
            ADDR_OFFI   (1, ADDR_OFF_BANK_WRAP),
            MOV_L       (MOV_MSB, REG_ADDR, REG_BANK, REG_TEMP),
            ALU_OP      (ALU_OP_OR, STATUS_FLAG_M, REG_ACCUM, REG_TEMP),
            MOV_RR      (REG_TEMP, REG_ACCUM),
            // CHK_ZN      (REG_ACCUM),
        }
    },
    [ORA_ABSL_X] = {
        .uops = {
            MOV_LPC     (MOV_LSB, REG_ADDR),
            MOV_LPC     (MOV_MSB, REG_ADDR),
            MOV_LPC     (MOV_LSB, REG_BANK),
            ADDR_OFFR   (REG_X, ADDR_OFF_BANK_NEXT),
            MOV_L       (MOV_LSB, REG_ADDR, REG_BANK, REG_TEMP),
            SKIPS       (2, STATUS_FLAG_M),
            ADDR_OFFI   (1, ADDR_OFF_BANK_NEXT),
            MOV_L       (MOV_MSB, REG_ADDR, REG_BANK, REG_TEMP),
            ALU_OP      (ALU_OP_OR, STATUS_FLAG_M, REG_ACCUM, REG_TEMP),
            MOV_RR      (REG_TEMP, REG_ACCUM),
            // CHK_ZN      (REG_ACCUM),
        }
    },

    [ORA_DIR] = {
        .uops = {
            MOV_LPC     (MOV_LSB, REG_ADDR),
            ZEXT        (REG_ADDR),
            ADDR_OFFR   (REG_D, ADDR_OFF_BANK_WRAP),
            SKIPC       (1, STATUS_FLAG_DL),
            IO          /* DL != 0 */,
            MOV_L       (MOV_LSB, REG_ADDR, REG_ZERO, REG_TEMP),
            SKIPS       (2, STATUS_FLAG_M),
            ADDR_OFFI   (1, ADDR_OFF_BANK_WRAP),
            MOV_L       (MOV_MSB, REG_ADDR, REG_ZERO, REG_TEMP),
            ALU_OP      (ALU_OP_OR, STATUS_FLAG_M, REG_ACCUM, REG_TEMP),
            MOV_RR      (REG_TEMP, REG_ACCUM),
            // CHK_ZN      (REG_ACCUM),
        }
    },
    [ORA_S_REL] = {
        .uops = {
            MOV_LPC     (MOV_LSB, REG_ADDR),
            ZEXT        (REG_ADDR),
            ADDR_OFFR   (REG_S, ADDR_OFF_BANK_WRAP),
            IO,
            MOV_L       (MOV_LSB, REG_ADDR, REG_ZERO, REG_TEMP),
            SKIPS       (2, STATUS_FLAG_M),
            ADDR_OFFI   (1, ADDR_OFF_BANK_WRAP),
            MOV_L       (MOV_MSB, REG_ADDR, REG_ZERO, REG_TEMP),
            ALU_OP      (ALU_OP_OR, STATUS_FLAG_M, REG_ACCUM, REG_TEMP),
            MOV_RR      (REG_TEMP, REG_ACCUM),
            // CHK_ZN      (REG_ACCUM),
        },
    },
    [ORA_DIR_X] = {
        .uops = {
            MOV_LPC     (MOV_LSB, REG_ADDR),
            ZEXT        (REG_ADDR),
            SKIPC       (1, STATUS_FLAG_DL),
            IO          /* DL != 0 */,
            ADDR_OFFR   (REG_D, ADDR_OFF_BANK_WRAP),
            ADDR_OFFR   (REG_X, ADDR_OFF_BANK_WRAP),
            IO,
            MOV_L       (MOV_LSB, REG_ADDR, REG_ZERO, REG_TEMP),
            SKIPS       (2, STATUS_FLAG_M),
            ADDR_OFFI   (1, ADDR_OFF_BANK_WRAP),
            MOV_L       (MOV_MSB, REG_ADDR, REG_ZERO, REG_TEMP),
            ALU_OP      (ALU_OP_OR, STATUS_FLAG_M, REG_ACCUM, REG_TEMP),
            MOV_RR      (REG_TEMP, REG_ACCUM),
            // CHK_ZN      (REG_ACCUM),
        }
    },

    [ORA_DIR_IND] = {
        .uops = {
            MOV_LPC     (MOV_LSB, REG_ADDR),
            ZEXT        (REG_ADDR),
            SKIPC       (1, STATUS_FLAG_DL),
            IO          /* DL != 0 */,
            ADDR_OFFR   (REG_D, ADDR_OFF_BANK_WRAP),
            MOV_L       (MOV_LSB, REG_ADDR, REG_ZERO, REG_TEMP),
            ADDR_OFFI   (1, ADDR_OFF_BANK_WRAP),
            MOV_L       (MOV_MSB, REG_ADDR, REG_ZERO, REG_TEMP),
            MOV_RRW     (REG_TEMP, REG_ADDR, MOV_RRW_WORD),
            MOV_L       (MOV_LSB, REG_ADDR, REG_DBR, REG_TEMP),
            SKIPS       (2, STATUS_FLAG_M),
            ADDR_OFFI   (1, ADDR_OFF_BANK_WRAP),
            MOV_L       (MOV_MSB, REG_ADDR, REG_DBR, REG_TEMP),
            ALU_OP      (ALU_OP_OR, STATUS_FLAG_M, REG_ACCUM, REG_TEMP),
            MOV_RR      (REG_TEMP, REG_ACCUM),
            // CHK_ZN      (REG_ACCUM),
        }
    },

    [ORA_DIR_INDL] = {
        .uops = {
            MOV_LPC     (MOV_LSB, REG_ADDR),
            ZEXT        (REG_ADDR),
            SKIPC       (1, STATUS_FLAG_DL),
            IO          /* DL != 0 */,
            ADDR_OFFR   (REG_D, ADDR_OFF_BANK_WRAP),
            MOV_L       (MOV_LSB, REG_ADDR, REG_ZERO, REG_TEMP),
            ADDR_OFFI   (1, ADDR_OFF_BANK_WRAP),
            MOV_L       (MOV_MSB, REG_ADDR, REG_ZERO, REG_TEMP),
            ADDR_OFFI   (1, ADDR_OFF_BANK_WRAP),
            MOV_L       (MOV_LSB, REG_ADDR, REG_ZERO, REG_BANK),
            MOV_RRW     (REG_TEMP, REG_ADDR, MOV_RRW_WORD),
            MOV_L       (MOV_LSB, REG_ADDR, REG_BANK, REG_TEMP),
            SKIPS       (2, STATUS_FLAG_M),
            ADDR_OFFI   (1, ADDR_OFF_BANK_WRAP),
            MOV_L       (MOV_MSB, REG_ADDR, REG_BANK, REG_TEMP),
            ALU_OP      (ALU_OP_OR, STATUS_FLAG_M, REG_ACCUM, REG_TEMP),
            MOV_RR      (REG_TEMP, REG_ACCUM),
            // CHK_ZN      (REG_ACCUM),
        }
    },

    [ORA_S_REL_IND_Y] = {
        .uops = {
            MOV_LPC     (MOV_LSB, REG_ADDR),
            ZEXT        (REG_ADDR),
            IO,
            ADDR_OFFR   (REG_S, ADDR_OFF_BANK_WRAP),
            MOV_L       (MOV_LSB, REG_ADDR, REG_ZERO, REG_TEMP),
            ADDR_OFFI   (1, ADDR_OFF_BANK_WRAP),
            MOV_L       (MOV_MSB, REG_ADDR, REG_ZERO, REG_TEMP),
            MOV_RRW     (REG_TEMP, REG_ADDR, MOV_RRW_WORD),
            MOV_RRW     (REG_DBR, REG_BANK, MOV_RRW_BYTE),
            IO,
            ADDR_OFFR   (REG_Y, ADDR_OFF_BANK_NEXT),
            MOV_L       (MOV_LSB, REG_ADDR, REG_BANK, REG_TEMP),
            SKIPS       (2, STATUS_FLAG_M),
            ADDR_OFFI   (1, ADDR_OFF_BANK_NEXT),
            MOV_L       (MOV_MSB, REG_ADDR, REG_BANK, REG_TEMP),
            ALU_OP      (ALU_OP_OR, STATUS_FLAG_M, REG_ACCUM, REG_TEMP),
            MOV_RR      (REG_TEMP, REG_ACCUM),
            // CHK_ZN      (REG_ACCUM),

        }
    },

    [ORA_DIR_X_IND] = {
        .uops = {
            MOV_LPC     (MOV_LSB, REG_ADDR),
            ZEXT        (REG_ADDR),
            SKIPC       (1, STATUS_FLAG_DL),
            IO          /* DL != 0 */,
            ADDR_OFFR   (REG_D, ADDR_OFF_BANK_WRAP),
            ADDR_OFFR   (REG_X, ADDR_OFF_BANK_WRAP),
            IO,
            MOV_L       (MOV_LSB, REG_ADDR, REG_ZERO, REG_TEMP),
            ADDR_OFFI   (1, ADDR_OFF_BANK_WRAP),
            MOV_L       (MOV_MSB, REG_ADDR, REG_ZERO, REG_TEMP),
            MOV_RRW     (REG_TEMP, REG_ADDR, MOV_RRW_WORD),
            MOV_L       (MOV_LSB, REG_ADDR, REG_DBR, REG_TEMP),
            SKIPS       (2, STATUS_FLAG_M),
            ADDR_OFFI   (1, ADDR_OFF_BANK_WRAP),
            MOV_L       (MOV_MSB, REG_ADDR, REG_DBR, REG_TEMP),
            ALU_OP      (ALU_OP_OR, STATUS_FLAG_M, REG_ACCUM, REG_TEMP),
            MOV_RR      (REG_TEMP, REG_ACCUM),
            // CHK_ZN      (REG_ACCUM),
        }
    },

    [ORA_DIR_IND_Y] = {
        .uops = {
            MOV_LPC     (MOV_LSB, REG_ADDR),
            ZEXT        (REG_ADDR),
            SKIPC       (1, STATUS_FLAG_DL),
            IO          /* DL != 0 */,
            ADDR_OFFR   (REG_D, ADDR_OFF_BANK_WRAP),
            MOV_L       (MOV_LSB, REG_ADDR, REG_ZERO, REG_TEMP),
            ADDR_OFFI   (1, ADDR_OFF_BANK_WRAP),
            MOV_L       (MOV_MSB, REG_ADDR, REG_ZERO, REG_TEMP),
            MOV_RRW     (REG_TEMP, REG_ADDR, MOV_RRW_WORD),
            MOV_RRW     (REG_DBR, REG_BANK, MOV_RRW_BYTE),
            ADDR_OFFR   (REG_Y, ADDR_OFF_BANK_NEXT),
            SKIPS       (2, STATUS_FLAG_X),
            SKIPC       (1, STATUS_FLAG_PAGE),
            IO          /* X = 0 or page crossed */,
            MOV_L       (MOV_LSB, REG_ADDR, REG_BANK, REG_TEMP),
            SKIPS       (2, STATUS_FLAG_M),
            ADDR_OFFI   (1, ADDR_OFF_BANK_NEXT),
            MOV_L       (MOV_MSB, REG_ADDR, REG_BANK, REG_TEMP),
            ALU_OP      (ALU_OP_OR, STATUS_FLAG_M, REG_ACCUM, REG_TEMP),
            MOV_RR      (REG_TEMP, REG_ACCUM),
            // CHK_ZN      (REG_ACCUM),
        }
    },

    [ORA_DIR_INDL_Y] = {
        .uops = {
            MOV_LPC     (MOV_LSB, REG_ADDR),
            ZEXT        (REG_ADDR),
            ADDR_OFFR   (REG_D, ADDR_OFF_BANK_WRAP),
            SKIPC       (1, STATUS_FLAG_DL),
            IO          /* DL != 0 */,
            MOV_L       (MOV_LSB, REG_ADDR, REG_ZERO, REG_TEMP),
            ADDR_OFFI   (1, ADDR_OFF_BANK_WRAP),
            MOV_L       (MOV_MSB, REG_ADDR, REG_ZERO, REG_TEMP),
            ADDR_OFFI   (1, ADDR_OFF_BANK_WRAP),
            MOV_L       (MOV_LSB, REG_ADDR, REG_ZERO, REG_BANK),
            MOV_RRW     (REG_TEMP, REG_ADDR, MOV_RRW_WORD),
            // MOV_RRW     (REG_DBR, REG_BANK, MOV_RRW_BYTE),
            ADDR_OFFR   (REG_Y, ADDR_OFF_BANK_NEXT),
            MOV_L       (MOV_LSB, REG_ADDR, REG_BANK, REG_TEMP),
            SKIPS       (2, STATUS_FLAG_M),
            ADDR_OFFI   (1, ADDR_OFF_BANK_NEXT),
            MOV_L       (MOV_MSB, REG_ADDR, REG_BANK, REG_TEMP),
            ALU_OP      (ALU_OP_OR, STATUS_FLAG_M, REG_ACCUM, REG_TEMP),
            MOV_RR      (REG_TEMP, REG_ACCUM),
            // CHK_ZN      (REG_ACCUM),
        }
    },

    [ORA_IMM] = {
        .uops = {
            MOV_LPC     (MOV_LSB, REG_TEMP),
            SKIPS       (1, STATUS_FLAG_M),
            MOV_LPC     (MOV_MSB, REG_TEMP),
            ALU_OP      (ALU_OP_OR, STATUS_FLAG_M, REG_ACCUM, REG_TEMP),
            MOV_RR      (REG_TEMP, REG_ACCUM),
            // CHK_ZN      (REG_ACCUM),
        }
    },

    /**************************************************************************************/
    /*                                      PEA                                           */
    /**************************************************************************************/

    [PEA_S] = {
        .uops = {
            MOV_LPC     (MOV_LSB, REG_TEMP),
            MOV_LPC     (MOV_MSB, REG_TEMP),
            MOV_S       (MOV_MSB, REG_S, REG_ZERO, REG_TEMP),
            DECS,
            MOV_S       (MOV_LSB, REG_S, REG_ZERO, REG_TEMP),
            DECS,
        }
    },

    /**************************************************************************************/
    /*                                      PEI                                           */
    /**************************************************************************************/

    [PEI_S] = {
        .uops = {
            MOV_LPC     (MOV_LSB, REG_ADDR),
            ZEXT        (REG_ADDR),
            SKIPC       (1, STATUS_FLAG_DL),
            IO,
            ADDR_OFFR   (REG_D, ADDR_OFF_BANK_WRAP),
            MOV_L       (MOV_LSB, REG_ADDR, REG_ZERO, REG_TEMP),
            ADDR_OFFI   (1, ADDR_OFF_BANK_WRAP),
            MOV_L       (MOV_MSB, REG_ADDR, REG_ZERO, REG_TEMP),
            MOV_S       (MOV_MSB, REG_S, REG_ZERO, REG_TEMP),
            DECS,
            MOV_S       (MOV_LSB, REG_S, REG_ZERO, REG_TEMP),
            DECS,
        }
    },

    /**************************************************************************************/
    /*                                      PER                                           */
    /**************************************************************************************/

    [PER_S] = {
        .uops = {
            MOV_LPC     (MOV_LSB, REG_ADDR),
            MOV_LPC     (MOV_MSB, REG_ADDR),
            ADDR_OFFRS  (REG_PC, ADDR_OFF_BANK_WRAP, ADDR_OFF_SIGNED),
            MOV_S       (MOV_MSB, REG_S, REG_ZERO, REG_ADDR),
            DECS,
            MOV_S       (MOV_LSB, REG_S, REG_ZERO, REG_ADDR),
            DECS,
        }
    },

    /**************************************************************************************/
    /*                                      PHA                                           */
    /**************************************************************************************/

    [PHA_S] = {
        .uops = {
            SKIPS       (3, STATUS_FLAG_M),
            IO          /* M = 0 */,
            MOV_S       (MOV_MSB, REG_S, REG_ZERO, REG_ACCUM),
            DECS,
            MOV_S       (MOV_LSB, REG_S, REG_ZERO, REG_ACCUM),
            DECS,
        }
    },

    /**************************************************************************************/
    /*                                      PHB                                           */
    /**************************************************************************************/

    [PHB_S] = {
        .uops = {
            MOV_S       (MOV_LSB, REG_S, REG_ZERO, REG_DBR),
            DECS,
        }
    },

    /**************************************************************************************/
    /*                                      PHD                                           */
    /**************************************************************************************/

    [PHD_S] = {
        .uops = {
            MOV_S       (MOV_MSB, REG_S, REG_ZERO, REG_D),
            DECS,
            MOV_S       (MOV_LSB, REG_S, REG_ZERO, REG_D),
            DECS,
        }
    },

    /**************************************************************************************/
    /*                                      PHK                                           */
    /**************************************************************************************/

    [PHK_S] = {
        .uops = {
            MOV_S       (MOV_LSB, REG_S, REG_ZERO, REG_PBR),
            DECS,
        }
    },

    /**************************************************************************************/
    /*                                      PHP                                           */
    /**************************************************************************************/

    [PHP_S] = {
        .uops = {
            MOV_P       (REG_P, REG_TEMP),
            MOV_S       (MOV_LSB, REG_S, REG_ZERO, REG_TEMP),
            DECS,
        }
    },

    /**************************************************************************************/
    /*                                      PHX                                           */
    /**************************************************************************************/

    [PHX_S] = {
        .uops = {
            SKIPS       (3, STATUS_FLAG_X),
            IO          /* X = 0 */,
            MOV_S       (MOV_MSB, REG_S, REG_ZERO, REG_X),
            DECS,
            MOV_S       (MOV_LSB, REG_S, REG_ZERO, REG_X),
            DECS,
        }
    },

    /**************************************************************************************/
    /*                                      PHY                                           */
    /**************************************************************************************/

    [PHY_S] = {
        .uops = {
            SKIPS       (3, STATUS_FLAG_X),
            IO          /* X = 0 */,
            MOV_S       (MOV_MSB, REG_S, REG_ZERO, REG_Y),
            DECS,
            MOV_S       (MOV_LSB, REG_S, REG_ZERO, REG_Y),
            DECS,
        }
    },

    /**************************************************************************************/
    /*                                      PLA                                           */
    /**************************************************************************************/

    [PLA_S] = {
        .uops = {
            IO,
            IO,
            INCS,
            MOV_L       (MOV_LSB, REG_S, REG_ZERO, REG_ACCUM),
            SKIPS       (2, STATUS_FLAG_M),
            INCS,
            MOV_L       (MOV_MSB, REG_S, REG_ZERO, REG_ACCUM),
            CHK_ZN      (REG_ACCUM),
        }
    },

    /**************************************************************************************/
    /*                                      PLB                                           */
    /**************************************************************************************/

    [PLB_S] = {
        .uops = {
            IO,
            IO,
            INCS,
            MOV_L       (MOV_LSB, REG_S, REG_ZERO, REG_DBR),
            CHK_ZN      (REG_DBR)
        }
    },

    /**************************************************************************************/
    /*                                      PLD                                           */
    /**************************************************************************************/

    [PLD_S] = {
        .uops = {
            IO,
            IO,
            INCS,
            MOV_L       (MOV_LSB, REG_S, REG_ZERO, REG_D),
            INCS,
            MOV_L       (MOV_MSB, REG_S, REG_ZERO, REG_D),
            CHK_ZN      (REG_D)
        }
    },

    /**************************************************************************************/
    /*                                      PLP                                           */
    /**************************************************************************************/

    [PLP_S] = {
        .uops = {
            IO,
            IO,
            INCS,
            MOV_L       (MOV_LSB, REG_S, REG_ZERO, REG_TEMP),
            MOV_P       (REG_TEMP, REG_P),
        }
    },

    /**************************************************************************************/
    /*                                      PLX                                           */
    /**************************************************************************************/
    [PLX_S] = {
        .uops = {
            IO,
            IO,
            INCS,
            MOV_L       (MOV_LSB, REG_S, REG_ZERO, REG_X),
            SKIPS       (2, STATUS_FLAG_X),
            INCS,
            MOV_L       (MOV_MSB, REG_S, REG_ZERO, REG_X),
            CHK_ZN      (REG_X)
        }
    },

    /**************************************************************************************/
    /*                                      PLY                                           */
    /**************************************************************************************/

    [PLY_S] = {
        .uops = {
            IO,
            IO,
            INCS,
            MOV_L       (MOV_LSB, REG_S, REG_ZERO, REG_Y),
            SKIPS       (2, STATUS_FLAG_X),
            INCS,
            MOV_L       (MOV_MSB, REG_S, REG_ZERO, REG_Y),
            CHK_ZN      (REG_Y)
        }
    },

    /**************************************************************************************/
    /*                                      REP                                           */
    /**************************************************************************************/

    [REP_IMM] = {
        .uops = {
            MOV_LPC     (MOV_LSB, REG_TEMP),
            IO,
            CLR_P       (STATUS_FLAG_LAST)
        }
    },

    /**************************************************************************************/
    /*                                      ROL                                           */
    /**************************************************************************************/

    [ROL_ABS] = {
        .uops = {
            MOV_LPC     (MOV_LSB, REG_ADDR),
            MOV_LPC     (MOV_MSB, REG_ADDR),
            MOV_L       (MOV_LSB, REG_ADDR, REG_DBR, REG_TEMP),
            SKIPS       (2, STATUS_FLAG_M),
            ADDR_OFFI   (1, ADDR_OFF_BANK_WRAP),
            MOV_L       (MOV_MSB, REG_ADDR, REG_DBR, REG_TEMP),
            IO,
            ALU_OP      (ALU_OP_ROL, STATUS_FLAG_M, REG_TEMP, REG_ZERO),
            // CHK_ZNW     (REG_TEMP, 0),
            SKIPS       (2, STATUS_FLAG_M),
            MOV_S       (MOV_MSB, REG_ADDR, REG_DBR, REG_TEMP),
            ADDR_OFFIS  (0xffff, ADDR_OFF_BANK_WRAP, ADDR_OFF_SIGNED),
            MOV_S       (MOV_LSB, REG_ADDR, REG_DBR, REG_TEMP),
        }
    },

    [ROL_ACC] = {
        .uops = {
            IO,
            ALU_OP      (ALU_OP_ROL, STATUS_FLAG_M, REG_ACCUM, REG_ZERO),
            MOV_RR      (REG_TEMP, REG_ACCUM),
            // CHK_ZN      (REG_ACCUM),
        }
    },

    [ROL_ABS_X] = {
        .uops = {
            MOV_LPC     (MOV_LSB, REG_ADDR),
            MOV_LPC     (MOV_MSB, REG_ADDR),
            MOV_RRW     (REG_DBR, REG_BANK, MOV_RRW_BYTE),
            IO,
            ADDR_OFFR   (REG_X, ADDR_OFF_BANK_NEXT),
            MOV_L       (MOV_LSB, REG_ADDR, REG_BANK, REG_TEMP),
            SKIPS       (2, STATUS_FLAG_M),
            ADDR_OFFI   (1, ADDR_OFF_BANK_NEXT),
            MOV_L       (MOV_MSB, REG_ADDR, REG_BANK, REG_TEMP),
            ALU_OP      (ALU_OP_ROL, STATUS_FLAG_M, REG_TEMP, REG_ZERO),
            // CHK_ZNW     (REG_TEMP, 0),
            SKIPS       (2, STATUS_FLAG_M),
            MOV_S       (MOV_MSB, REG_ADDR, REG_BANK, REG_TEMP),
            ADDR_OFFIS  (0xffff, ADDR_OFF_BANK_WRAP, ADDR_OFF_SIGNED),
            MOV_S       (MOV_LSB, REG_ADDR, REG_BANK, REG_TEMP),
        }
    },

    [ROL_DIR] = {
        .uops = {
            MOV_LPC     (MOV_LSB, REG_ADDR),
            ZEXT        (REG_ADDR),
            SKIPC       (1, STATUS_FLAG_DL),
            IO          /* DL != 0 */,
            ADDR_OFFR   (REG_D, ADDR_OFF_BANK_WRAP),
            MOV_L       (MOV_LSB, REG_ADDR, REG_ZERO, REG_TEMP),
            SKIPS       (2, STATUS_FLAG_M),
            ADDR_OFFI   (1, ADDR_OFF_BANK_WRAP),
            MOV_L       (MOV_MSB, REG_ADDR, REG_ZERO, REG_TEMP),
            IO,
            ALU_OP      (ALU_OP_ROL, STATUS_FLAG_M, REG_TEMP, REG_ZERO),
            // CHK_ZNW     (REG_TEMP, 0),
            SKIPS       (2, STATUS_FLAG_M),
            MOV_S       (MOV_MSB, REG_ADDR, REG_ZERO, REG_TEMP),
            ADDR_OFFIS  (0xffff, ADDR_OFF_BANK_WRAP, ADDR_OFF_SIGNED),
            MOV_S       (MOV_LSB, REG_ADDR, REG_ZERO, REG_TEMP),
        }
    },

    [ROL_DIR_X] = {
        .uops = {
            MOV_LPC     (MOV_LSB, REG_ADDR),
            ZEXT        (REG_ADDR),
            SKIPC       (1, STATUS_FLAG_DL),
            IO          /* DL != 0 */,
            IO,
            ADDR_OFFR   (REG_D, ADDR_OFF_BANK_WRAP),
            ADDR_OFFR   (REG_X, ADDR_OFF_BANK_WRAP),
            MOV_L       (MOV_LSB, REG_ADDR, REG_ZERO, REG_TEMP),
            SKIPS       (2, STATUS_FLAG_M),
            ADDR_OFFI   (1, ADDR_OFF_BANK_WRAP),
            MOV_L       (MOV_MSB, REG_ADDR, REG_ZERO, REG_TEMP),
            IO,
            ALU_OP      (ALU_OP_ROL, STATUS_FLAG_M, REG_TEMP, REG_ZERO),
            // CHK_ZNW     (REG_TEMP, 0),
            SKIPS       (2, STATUS_FLAG_M),
            MOV_S       (MOV_MSB, REG_ADDR, REG_ZERO, REG_TEMP),
            ADDR_OFFIS  (0xffff, ADDR_OFF_BANK_WRAP, ADDR_OFF_SIGNED),
            MOV_S       (MOV_LSB, REG_ADDR, REG_ZERO, REG_TEMP),
        }
    },

    /**************************************************************************************/
    /*                                      ROR                                           */
    /**************************************************************************************/

    [ROR_ABS] = {
        .uops = {
            MOV_LPC     (MOV_LSB, REG_ADDR),
            MOV_LPC     (MOV_MSB, REG_ADDR),
            MOV_L       (MOV_LSB, REG_ADDR, REG_DBR, REG_TEMP),
            SKIPS       (2, STATUS_FLAG_M),
            ADDR_OFFI   (1, ADDR_OFF_BANK_WRAP),
            MOV_L       (MOV_MSB, REG_ADDR, REG_DBR, REG_TEMP),
            IO,
            ALU_OP      (ALU_OP_ROR, STATUS_FLAG_M, REG_TEMP, REG_ZERO),
            // CHK_ZNW     (REG_TEMP, 0),
            SKIPS       (2, STATUS_FLAG_M),
            MOV_S       (MOV_MSB, REG_ADDR, REG_DBR, REG_TEMP),
            ADDR_OFFIS  (0xffff, ADDR_OFF_BANK_WRAP, ADDR_OFF_SIGNED),
            MOV_S       (MOV_LSB, REG_ADDR, REG_DBR, REG_TEMP),
        }
    },

    [ROR_ACC] = {
        .uops = {
            IO,
            ALU_OP      (ALU_OP_ROR, STATUS_FLAG_M, REG_ACCUM, REG_ZERO),
            MOV_RR      (REG_TEMP, REG_ACCUM),
            // CHK_ZN      (REG_ACCUM),
        }
    },

    [ROR_ABS_X] = {
        .uops = {
            MOV_LPC     (MOV_LSB, REG_ADDR),
            MOV_LPC     (MOV_MSB, REG_ADDR),
            MOV_RRW     (REG_DBR, REG_BANK, MOV_RRW_BYTE),
            IO,
            ADDR_OFFR   (REG_X, ADDR_OFF_BANK_NEXT),
            MOV_L       (MOV_LSB, REG_ADDR, REG_BANK, REG_TEMP),
            SKIPS       (2, STATUS_FLAG_M),
            ADDR_OFFI   (1, ADDR_OFF_BANK_NEXT),
            MOV_L       (MOV_MSB, REG_ADDR, REG_BANK, REG_TEMP),
            ALU_OP      (ALU_OP_ROR, STATUS_FLAG_M, REG_TEMP, REG_ZERO),
            // CHK_ZNW     (REG_TEMP, 0),
            SKIPS       (2, STATUS_FLAG_M),
            MOV_S       (MOV_MSB, REG_ADDR, REG_BANK, REG_TEMP),
            ADDR_OFFIS  (0xffff, ADDR_OFF_BANK_WRAP, ADDR_OFF_SIGNED),
            MOV_S       (MOV_LSB, REG_ADDR, REG_BANK, REG_TEMP),
        }
    },

    [ROR_DIR] = {
        .uops = {
            MOV_LPC     (MOV_LSB, REG_ADDR),
            ZEXT        (REG_ADDR),
            SKIPC       (1, STATUS_FLAG_DL),
            IO          /* DL != 0 */,
            ADDR_OFFR   (REG_D, ADDR_OFF_BANK_WRAP),
            MOV_L       (MOV_LSB, REG_ADDR, REG_ZERO, REG_TEMP),
            SKIPS       (2, STATUS_FLAG_M),
            ADDR_OFFI   (1, ADDR_OFF_BANK_WRAP),
            MOV_L       (MOV_MSB, REG_ADDR, REG_ZERO, REG_TEMP),
            IO,
            ALU_OP      (ALU_OP_ROR, STATUS_FLAG_M, REG_TEMP, REG_ZERO),
            // CHK_ZNW     (REG_TEMP, 0),
            SKIPS       (2, STATUS_FLAG_M),
            MOV_S       (MOV_MSB, REG_ADDR, REG_ZERO, REG_TEMP),
            ADDR_OFFIS  (0xffff, ADDR_OFF_BANK_WRAP, ADDR_OFF_SIGNED),
            MOV_S       (MOV_LSB, REG_ADDR, REG_ZERO, REG_TEMP),
        }
    },

    [ROR_DIR_X] = {
        .uops = {
            MOV_LPC     (MOV_LSB, REG_ADDR),
            ZEXT        (REG_ADDR),
            SKIPC       (1, STATUS_FLAG_DL),
            IO          /* DL != 0 */,
            IO,
            ADDR_OFFR   (REG_D, ADDR_OFF_BANK_WRAP),
            ADDR_OFFR   (REG_X, ADDR_OFF_BANK_WRAP),
            MOV_L       (MOV_LSB, REG_ADDR, REG_ZERO, REG_TEMP),
            SKIPS       (2, STATUS_FLAG_M),
            ADDR_OFFI   (1, ADDR_OFF_BANK_WRAP),
            MOV_L       (MOV_MSB, REG_ADDR, REG_ZERO, REG_TEMP),
            IO,
            ALU_OP      (ALU_OP_ROR, STATUS_FLAG_M, REG_TEMP, REG_ZERO),
            // CHK_ZNW     (REG_TEMP, 0),
            SKIPS       (2, STATUS_FLAG_M),
            MOV_S       (MOV_MSB, REG_ADDR, REG_ZERO, REG_TEMP),
            ADDR_OFFIS  (0xffff, ADDR_OFF_BANK_WRAP, ADDR_OFF_SIGNED),
            MOV_S       (MOV_LSB, REG_ADDR, REG_ZERO, REG_TEMP),
        }
    },

    /**************************************************************************************/
    /*                                      RTI                                           */
    /**************************************************************************************/

    [RTI_S] = {
        .uops = {
            IO,
            IO,
            INCS,
            MOV_L       (MOV_LSB, REG_S, REG_ZERO, REG_TEMP),
            MOV_P       (REG_TEMP, REG_P),
            INCS,
            MOV_L       (MOV_LSB, REG_S, REG_ZERO, REG_TEMP),
            INCS,
            MOV_L       (MOV_MSB, REG_S, REG_ZERO, REG_TEMP),
            MOV_RRW     (REG_TEMP, REG_PC, MOV_RRW_WORD),
            SKIPS       (2, STATUS_FLAG_E),
            INCS,
            MOV_L       (MOV_MSB, REG_S, REG_ZERO, REG_PBR),
        }
    },

    /**************************************************************************************/
    /*                                      RTL                                           */
    /**************************************************************************************/

    [RTL_S] = {
        .uops = {
            IO,
            IO,
            INCS,
            MOV_L       (MOV_LSB, REG_S, REG_ZERO, REG_ADDR),
            INCS,
            MOV_L       (MOV_MSB, REG_S, REG_ZERO, REG_ADDR),
            INCS,
            MOV_L       (MOV_LSB, REG_S, REG_ZERO, REG_BANK),
            IO,
            INC_RW      (REG_ADDR),
            MOV_RRW     (REG_ADDR, REG_PC, MOV_RRW_WORD),
            MOV_RRW     (REG_BANK, REG_PBR, MOV_RRW_BYTE),
        }
    },

    /**************************************************************************************/
    /*                                      RTS                                           */
    /**************************************************************************************/

    [RTS_S] = {
        .uops = {
            IO,
            IO,
            INCS,
            MOV_L       (MOV_LSB, REG_S, REG_ZERO, REG_ADDR),
            INCS,
            MOV_L       (MOV_MSB, REG_S, REG_ZERO, REG_ADDR),
            IO,
            INC_RW      (REG_ADDR),
            MOV_RRW     (REG_ADDR, REG_PC, MOV_RRW_WORD),
        }
    },

    /**************************************************************************************/
    /*                                      SBC                                           */
    /**************************************************************************************/

    [SBC_ABS] = {
        .uops = {
            MOV_LPC     (MOV_LSB, REG_ADDR),
            MOV_LPC     (MOV_MSB, REG_ADDR),
            MOV_L       (MOV_LSB, REG_ADDR, REG_DBR, REG_TEMP),
            SKIPS       (2, STATUS_FLAG_M),
            ADDR_OFFI   (1, ADDR_OFF_BANK_WRAP),
            MOV_L       (MOV_MSB, REG_ADDR, REG_DBR, REG_TEMP),
            ALU_OP      (ALU_OP_SUB, STATUS_FLAG_M, REG_ACCUM, REG_TEMP),
            MOV_RR      (REG_TEMP, REG_ACCUM),
            // CHK_ZN      (REG_ACCUM),
        }
    },
    [SBC_ABS_X] = {
        .uops = {
            MOV_LPC     (MOV_LSB, REG_ADDR),
            MOV_LPC     (MOV_MSB, REG_ADDR),
            MOV_RRW     (REG_DBR, REG_BANK, MOV_RRW_BYTE),
            ADDR_OFFR   (REG_X, ADDR_OFF_BANK_NEXT),
            SKIPC       (2, STATUS_FLAG_PAGE),
            SKIPS       (1, STATUS_FLAG_X),
            IO          /* X=0 or page boundary */,
            MOV_L       (MOV_LSB, REG_ADDR, REG_BANK, REG_TEMP),
            SKIPS       (2, STATUS_FLAG_M),
            ADDR_OFFI   (1, ADDR_OFF_BANK_NEXT),
            MOV_L       (MOV_MSB, REG_ADDR, REG_BANK, REG_TEMP),
            ALU_OP      (ALU_OP_SUB, STATUS_FLAG_M, REG_ACCUM, REG_TEMP),
            MOV_RR      (REG_TEMP, REG_ACCUM),
            // CHK_ZN      (REG_ACCUM),
        }
    },
    [SBC_ABS_Y] = {
        .uops = {
            MOV_LPC     (MOV_LSB, REG_ADDR),
            MOV_LPC     (MOV_MSB, REG_ADDR),
            MOV_RRW     (REG_DBR, REG_BANK, MOV_RRW_BYTE),
            ADDR_OFFR   (REG_Y, ADDR_OFF_BANK_NEXT),
            SKIPC       (2, STATUS_FLAG_PAGE),
            SKIPS       (1, STATUS_FLAG_X),
            IO          /* X=0 or page boundary */,
            MOV_L       (MOV_LSB, REG_ADDR, REG_BANK, REG_TEMP),
            SKIPS       (2, STATUS_FLAG_M),
            ADDR_OFFI   (1, ADDR_OFF_BANK_NEXT),
            MOV_L       (MOV_MSB, REG_ADDR, REG_BANK, REG_TEMP),
            ALU_OP      (ALU_OP_SUB, STATUS_FLAG_M, REG_ACCUM, REG_TEMP),
            MOV_RR      (REG_TEMP, REG_ACCUM),
            // CHK_ZN      (REG_ACCUM),
        }
    },
    [SBC_ABSL_X] = {
        .uops = {
            MOV_LPC     (MOV_LSB, REG_ADDR),
            MOV_LPC     (MOV_MSB, REG_ADDR),
            MOV_LPC     (MOV_LSB, REG_BANK),
            ADDR_OFFR   (REG_X, ADDR_OFF_BANK_NEXT),
            MOV_L       (MOV_LSB, REG_ADDR, REG_BANK, REG_TEMP),
            SKIPS       (2, STATUS_FLAG_M),
            ADDR_OFFI   (1, ADDR_OFF_BANK_NEXT),
            MOV_L       (MOV_MSB, REG_ADDR, REG_BANK, REG_TEMP),
            ALU_OP      (ALU_OP_SUB, STATUS_FLAG_M, REG_ACCUM, REG_TEMP),
            MOV_RR      (REG_TEMP, REG_ACCUM),
            // CHK_ZN      (REG_ACCUM),
        }
    },
    [SBC_DIR] = {
        .uops = {
            MOV_LPC     (MOV_LSB, REG_ADDR),
            ZEXT        (REG_ADDR),
            ADDR_OFFR   (REG_D, ADDR_OFF_BANK_WRAP),
            SKIPC       (1, STATUS_FLAG_DL),
            IO          /* DL != 0 */,
            MOV_L       (MOV_LSB, REG_ADDR, REG_ZERO, REG_TEMP),
            SKIPS       (2, STATUS_FLAG_M),
            ADDR_OFFI   (1, ADDR_OFF_BANK_WRAP),
            MOV_L       (MOV_MSB, REG_ADDR, REG_ZERO, REG_TEMP),
            ALU_OP      (ALU_OP_SUB, STATUS_FLAG_M, REG_ACCUM, REG_TEMP),
            MOV_RR      (REG_TEMP, REG_ACCUM),
            // CHK_ZN      (REG_ACCUM),
        }
    },
    [SBC_S_REL] = {
        .uops = {
            MOV_LPC     (MOV_LSB, REG_ADDR),
            ZEXT        (REG_ADDR),
            ADDR_OFFR   (REG_S, ADDR_OFF_BANK_WRAP),
            IO,
            MOV_L       (MOV_LSB, REG_ADDR, REG_ZERO, REG_TEMP),
            SKIPS       (2, STATUS_FLAG_M),
            ADDR_OFFI   (1, ADDR_OFF_BANK_WRAP),
            MOV_L       (MOV_MSB, REG_ADDR, REG_ZERO, REG_TEMP),
            ALU_OP      (ALU_OP_SUB, STATUS_FLAG_M, REG_ACCUM, REG_TEMP),
            MOV_RR      (REG_TEMP, REG_ACCUM),
            // CHK_ZN      (REG_ACCUM),
        },
    },
    [SBC_DIR_X] = {
        .uops = {
            MOV_LPC     (MOV_LSB, REG_ADDR),
            ZEXT        (REG_ADDR),
            SKIPC       (1, STATUS_FLAG_DL),
            IO          /* DL != 0 */,
            ADDR_OFFR   (REG_D, ADDR_OFF_BANK_WRAP),
            ADDR_OFFR   (REG_X, ADDR_OFF_BANK_WRAP),
            IO,
            MOV_L       (MOV_LSB, REG_ADDR, REG_ZERO, REG_TEMP),
            SKIPS       (2, STATUS_FLAG_M),
            ADDR_OFFI   (1, ADDR_OFF_BANK_WRAP),
            MOV_L       (MOV_MSB, REG_ADDR, REG_ZERO, REG_TEMP),
            ALU_OP      (ALU_OP_SUB, STATUS_FLAG_M, REG_ACCUM, REG_TEMP),
            MOV_RR      (REG_TEMP, REG_ACCUM),
            // CHK_ZN      (REG_ACCUM),
        }
    },

    [SBC_DIR_IND] = {
        .uops = {
            MOV_LPC     (MOV_LSB, REG_ADDR),
            ZEXT        (REG_ADDR),
            SKIPC       (1, STATUS_FLAG_DL),
            IO          /* DL != 0 */,
            ADDR_OFFR   (REG_D, 0),
            MOV_L       (MOV_LSB, REG_ADDR, REG_ZERO, REG_TEMP),
            ADDR_OFFI   (1, ADDR_OFF_BANK_WRAP),
            MOV_L       (MOV_MSB, REG_ADDR, REG_ZERO, REG_TEMP),
            MOV_RRW     (REG_TEMP, REG_ADDR, MOV_RRW_WORD),
            MOV_L       (MOV_LSB, REG_ADDR, REG_DBR, REG_TEMP),
            SKIPS       (2, STATUS_FLAG_M),
            ADDR_OFFI   (1, ADDR_OFF_BANK_WRAP),
            MOV_L       (MOV_MSB, REG_ADDR, REG_DBR, REG_TEMP),
            ALU_OP      (ALU_OP_SUB, STATUS_FLAG_M, REG_ACCUM, REG_TEMP),
            MOV_RR      (REG_TEMP, REG_ACCUM),
            // CHK_ZN      (REG_ACCUM),
        }
    },

    [SBC_DIR_INDL] = {
        .uops = {
            MOV_LPC     (MOV_LSB, REG_ADDR),
            ZEXT        (REG_ADDR),
            SKIPC       (1, STATUS_FLAG_DL),
            IO          /* DL != 0 */,
            ADDR_OFFR   (REG_D, ADDR_OFF_BANK_WRAP),
            MOV_L       (MOV_LSB, REG_ADDR, REG_ZERO, REG_TEMP),
            ADDR_OFFI   (1, ADDR_OFF_BANK_WRAP),
            MOV_L       (MOV_MSB, REG_ADDR, REG_ZERO, REG_TEMP),
            ADDR_OFFI   (1, ADDR_OFF_BANK_WRAP),
            MOV_L       (MOV_LSB, REG_ADDR, REG_ZERO, REG_BANK),
            MOV_RRW     (REG_TEMP, REG_ADDR, MOV_RRW_WORD),
            MOV_L       (MOV_LSB, REG_ADDR, REG_BANK, REG_TEMP),
            SKIPS       (2, STATUS_FLAG_M),
            ADDR_OFFI   (1, ADDR_OFF_BANK_WRAP),
            MOV_L       (MOV_MSB, REG_ADDR, REG_BANK, REG_TEMP),
            ALU_OP      (ALU_OP_SUB, STATUS_FLAG_M, REG_ACCUM, REG_TEMP),
            MOV_RR      (REG_TEMP, REG_ACCUM),
            // CHK_ZN      (REG_ACCUM),
        }
    },

    [SBC_S_REL_IND_Y] = {
        .uops = {
            MOV_LPC     (MOV_LSB, REG_ADDR),
            ZEXT        (REG_ADDR),
            IO,
            ADDR_OFFR   (REG_S, ADDR_OFF_BANK_WRAP),
            MOV_L       (MOV_LSB, REG_ADDR, REG_ZERO, REG_TEMP),
            ADDR_OFFI   (1, ADDR_OFF_BANK_WRAP),
            MOV_L       (MOV_MSB, REG_ADDR, REG_ZERO, REG_TEMP),
            MOV_RRW     (REG_TEMP, REG_ADDR, MOV_RRW_WORD),
            MOV_RRW     (REG_DBR, REG_BANK, MOV_RRW_BYTE),
            IO,
            ADDR_OFFR   (REG_Y, ADDR_OFF_BANK_NEXT),
            MOV_L       (MOV_LSB, REG_ADDR, REG_BANK, REG_TEMP),
            SKIPS       (2, STATUS_FLAG_M),
            ADDR_OFFI   (1, ADDR_OFF_BANK_NEXT),
            MOV_L       (MOV_MSB, REG_ADDR, REG_BANK, REG_TEMP),
            ALU_OP      (ALU_OP_SUB, STATUS_FLAG_M, REG_ACCUM, REG_TEMP),
            MOV_RR      (REG_TEMP, REG_ACCUM),
            // CHK_ZN      (REG_ACCUM),

        }
    },

    [SBC_DIR_X_IND] = {
        .uops = {
            MOV_LPC     (MOV_LSB, REG_ADDR),
            ZEXT        (REG_ADDR),
            SKIPC       (1, STATUS_FLAG_DL),
            IO          /* DL != 0 */,
            ADDR_OFFR   (REG_D, ADDR_OFF_BANK_WRAP),
            ADDR_OFFR   (REG_X, ADDR_OFF_BANK_WRAP),
            IO,
            MOV_L       (MOV_LSB, REG_ADDR, REG_ZERO, REG_TEMP),
            ADDR_OFFI   (1, ADDR_OFF_BANK_WRAP),
            MOV_L       (MOV_MSB, REG_ADDR, REG_ZERO, REG_TEMP),
            MOV_RRW     (REG_TEMP, REG_ADDR, MOV_RRW_WORD),
            MOV_L       (MOV_LSB, REG_ADDR, REG_DBR, REG_TEMP),
            SKIPS       (2, STATUS_FLAG_M),
            ADDR_OFFI   (1, ADDR_OFF_BANK_WRAP),
            MOV_L       (MOV_MSB, REG_ADDR, REG_DBR, REG_TEMP),
            ALU_OP      (ALU_OP_SUB, STATUS_FLAG_M, REG_ACCUM, REG_TEMP),
            MOV_RR      (REG_TEMP, REG_ACCUM),
            // CHK_ZN      (REG_ACCUM),
        }
    },

    [SBC_DIR_IND_Y] = {
        .uops = {
            MOV_LPC     (MOV_LSB, REG_ADDR),
            ZEXT        (REG_ADDR),
            SKIPC       (1, STATUS_FLAG_DL),
            IO          /* DL != 0 */,
            ADDR_OFFR   (REG_D, 0),
            MOV_L       (MOV_LSB, REG_ADDR, REG_ZERO, REG_TEMP),
            ADDR_OFFI   (1, ADDR_OFF_BANK_WRAP),
            MOV_L       (MOV_MSB, REG_ADDR, REG_ZERO, REG_TEMP),
            MOV_RRW     (REG_TEMP, REG_ADDR, MOV_RRW_WORD),
            MOV_RRW     (REG_DBR, REG_BANK, MOV_RRW_BYTE),
            ADDR_OFFR   (REG_Y, ADDR_OFF_BANK_NEXT),
            SKIPS       (2, STATUS_FLAG_X),
            SKIPC       (1, STATUS_FLAG_PAGE),
            IO          /* X = 0 or page crossed */,
            MOV_L       (MOV_LSB, REG_ADDR, REG_BANK, REG_TEMP),
            SKIPS       (2, STATUS_FLAG_M),
            ADDR_OFFI   (1, ADDR_OFF_BANK_NEXT),
            MOV_L       (MOV_MSB, REG_ADDR, REG_BANK, REG_TEMP),
            ALU_OP      (ALU_OP_SUB, STATUS_FLAG_M, REG_ACCUM, REG_TEMP),
            MOV_RR      (REG_TEMP, REG_ACCUM),
            // CHK_ZN      (REG_ACCUM),
        }
    },

    [SBC_DIR_INDL_Y] = {
        .uops = {
            MOV_LPC     (MOV_LSB, REG_ADDR),
            ZEXT        (REG_ADDR),
            ADDR_OFFR   (REG_D, 0),
            SKIPC       (1, STATUS_FLAG_DL),
            IO          /* DL != 0 */,
            MOV_L       (MOV_LSB, REG_ADDR, REG_ZERO, REG_TEMP),
            ADDR_OFFI   (1, ADDR_OFF_BANK_WRAP),
            MOV_L       (MOV_MSB, REG_ADDR, REG_ZERO, REG_TEMP),
            ADDR_OFFI   (1, ADDR_OFF_BANK_WRAP),
            MOV_L       (MOV_LSB, REG_ADDR, REG_ZERO, REG_BANK),
            MOV_RRW     (REG_TEMP, REG_ADDR, MOV_RRW_WORD),
            // MOV_RRW     (REG_DBR, REG_BANK, MOV_RRW_BYTE),
            ADDR_OFFR   (REG_Y, ADDR_OFF_BANK_NEXT),
            MOV_L       (MOV_LSB, REG_ADDR, REG_BANK, REG_TEMP),
            SKIPS       (2, STATUS_FLAG_M),
            ADDR_OFFI   (1, ADDR_OFF_BANK_NEXT),
            MOV_L       (MOV_MSB, REG_ADDR, REG_BANK, REG_TEMP),
            ALU_OP      (ALU_OP_SUB, STATUS_FLAG_M, REG_ACCUM, REG_TEMP),
            MOV_RR      (REG_TEMP, REG_ACCUM),
            // CHK_ZN      (REG_ACCUM),
        }
    },

    [SBC_IMM] = {
        .uops = {
            MOV_LPC     (MOV_LSB, REG_TEMP),
            SKIPS       (1, STATUS_FLAG_M),
            MOV_LPC     (MOV_MSB, REG_TEMP),
            ALU_OP      (ALU_OP_SUB, STATUS_FLAG_M, REG_ACCUM, REG_TEMP),
            MOV_RR      (REG_TEMP, REG_ACCUM),
            // CHK_ZN      (REG_ACCUM),
        }
    },

    /**************************************************************************************/
    /*                                      SEC                                           */
    /**************************************************************************************/

    [SEC_IMP] = {
        .uops = {
            IO,
            SET_P       (STATUS_FLAG_C)
        },
    },

    /**************************************************************************************/
    /*                                      SED                                           */
    /**************************************************************************************/

    [SED_IMP] = {
        .uops = {
            IO,
            SET_P       (STATUS_FLAG_D)
        },
    },

    /**************************************************************************************/
    /*                                      SEI                                           */
    /**************************************************************************************/

    [SEI_IMP] = {
        .uops = {
            IO,
            SET_P       (STATUS_FLAG_I)
        },
    },

    /**************************************************************************************/
    /*                                      SEP                                           */
    /**************************************************************************************/

    [SEP_IMM] = {
        .uops = {
            MOV_LPC     (MOV_LSB, REG_TEMP),
            IO,
            SET_P       (STATUS_FLAG_LAST)
        }
    },

    /**************************************************************************************/
    /*                                      STA                                           */
    /**************************************************************************************/

    [STA_ABS] = {
        .uops = {
            MOV_LPC     (MOV_LSB, REG_ADDR),
            MOV_LPC     (MOV_MSB, REG_ADDR),
            MOV_S       (MOV_LSB, REG_ADDR, REG_DBR, REG_ACCUM),
            SKIPS       (2, STATUS_FLAG_M),
            ADDR_OFFI   (1, ADDR_OFF_BANK_WRAP),
            MOV_S       (MOV_MSB, REG_ADDR, REG_DBR, REG_ACCUM),
        }
    },
    [STA_ABS_X] = {
        .uops = {
            MOV_LPC     (MOV_LSB, REG_ADDR),
            MOV_LPC     (MOV_MSB, REG_ADDR),
            MOV_RRW     (REG_DBR, REG_BANK, MOV_RRW_BYTE),
            ADDR_OFFR   (REG_X, ADDR_OFF_BANK_NEXT),
            IO          /* extra cycle due to write */,
            MOV_S       (MOV_LSB, REG_ADDR, REG_BANK, REG_ACCUM),
            SKIPS       (2, STATUS_FLAG_M),
            ADDR_OFFI   (1, ADDR_OFF_BANK_NEXT),
            MOV_S       (MOV_MSB, REG_ADDR, REG_BANK, REG_ACCUM),
        }
    },
    [STA_ABS_Y] = {
        .uops = {
            MOV_LPC     (MOV_LSB, REG_ADDR),
            MOV_LPC     (MOV_MSB, REG_ADDR),
            MOV_RRW     (REG_DBR, REG_BANK, MOV_RRW_BYTE),
            ADDR_OFFR   (REG_Y, ADDR_OFF_BANK_NEXT),
            IO          /* extra cycle due to write */,
            MOV_S       (MOV_LSB, REG_ADDR, REG_BANK, REG_ACCUM),
            SKIPS       (2, STATUS_FLAG_M),
            ADDR_OFFI   (1, ADDR_OFF_BANK_NEXT),
            MOV_S       (MOV_MSB, REG_ADDR, REG_BANK, REG_ACCUM),
        }
    },
    [STA_ABSL] = {
        .uops = {
            MOV_LPC     (MOV_LSB, REG_ADDR),
            MOV_LPC     (MOV_MSB, REG_ADDR),
            MOV_LPC     (MOV_LSB, REG_BANK),
            MOV_S       (MOV_LSB, REG_ADDR, REG_BANK, REG_ACCUM),
            SKIPS       (2, STATUS_FLAG_M),
            ADDR_OFFI   (1, ADDR_OFF_BANK_WRAP),
            MOV_S       (MOV_MSB, REG_ADDR, REG_BANK, REG_ACCUM),
        }
    },
    [STA_ABSL_X] = {
        .uops = {
            MOV_LPC     (MOV_LSB, REG_ADDR),
            MOV_LPC     (MOV_MSB, REG_ADDR),
            MOV_LPC     (MOV_LSB, REG_BANK),
            ADDR_OFFR   (REG_X, ADDR_OFF_BANK_NEXT),
            MOV_S       (MOV_LSB, REG_ADDR, REG_BANK, REG_ACCUM),
            SKIPS       (2, STATUS_FLAG_M),
            ADDR_OFFI   (1, ADDR_OFF_BANK_NEXT),
            MOV_S       (MOV_MSB, REG_ADDR, REG_BANK, REG_ACCUM),
        }
    },
    [STA_DIR] = {
        .uops = {
            MOV_LPC     (MOV_LSB, REG_ADDR),
            ZEXT        (REG_ADDR),
            ADDR_OFFR   (REG_D, ADDR_OFF_BANK_WRAP),
            SKIPC       (1, STATUS_FLAG_DL),
            IO          /* DL != 0 */,
            MOV_S       (MOV_LSB, REG_ADDR, REG_ZERO, REG_ACCUM),
            SKIPS       (2, STATUS_FLAG_M),
            ADDR_OFFI   (1, ADDR_OFF_BANK_WRAP),
            MOV_S       (MOV_MSB, REG_ADDR, REG_ZERO, REG_ACCUM),
        }
    },
    [STA_S_REL] = {
        .uops = {
            MOV_LPC     (MOV_LSB, REG_ADDR),
            ZEXT        (REG_ADDR),
            ADDR_OFFR   (REG_S, ADDR_OFF_BANK_WRAP),
            IO,
            MOV_S       (MOV_LSB, REG_ADDR, REG_ZERO, REG_ACCUM),
            SKIPS       (2, STATUS_FLAG_M),
            ADDR_OFFI   (1, ADDR_OFF_BANK_WRAP),
            MOV_S       (MOV_MSB, REG_ADDR, REG_ZERO, REG_ACCUM),
        },
    },

    [STA_DIR_X] = {
        .uops = {
            MOV_LPC     (MOV_LSB, REG_ADDR),
            ZEXT        (REG_ADDR),
            SKIPC       (1, STATUS_FLAG_DL),
            IO          /* DL != 0 */,
            ADDR_OFFR   (REG_D, ADDR_OFF_BANK_WRAP),
            ADDR_OFFR   (REG_X, ADDR_OFF_BANK_WRAP),
            IO,
            MOV_S       (MOV_LSB, REG_ADDR, REG_ZERO, REG_ACCUM),
            SKIPS       (2, STATUS_FLAG_M),
            ADDR_OFFI   (1, ADDR_OFF_BANK_WRAP),
            MOV_S       (MOV_MSB, REG_ADDR, REG_ZERO, REG_ACCUM),
        }
    },

    [STA_DIR_IND] = {
        .uops = {
            MOV_LPC     (MOV_LSB, REG_ADDR),
            ZEXT        (REG_ADDR),
            SKIPC       (1, STATUS_FLAG_DL),
            IO          /* DL != 0 */,
            ADDR_OFFR   (REG_D, ADDR_OFF_BANK_WRAP),
            MOV_L       (MOV_LSB, REG_ADDR, REG_ZERO, REG_TEMP),
            ADDR_OFFI   (1, ADDR_OFF_BANK_WRAP),
            MOV_L       (MOV_MSB, REG_ADDR, REG_ZERO, REG_TEMP),
            MOV_RRW     (REG_TEMP, REG_ADDR, MOV_RRW_WORD),
            MOV_S       (MOV_LSB, REG_ADDR, REG_DBR, REG_ACCUM),
            SKIPS       (2, STATUS_FLAG_M),
            ADDR_OFFI   (1, ADDR_OFF_BANK_WRAP),
            MOV_S       (MOV_MSB, REG_ADDR, REG_DBR, REG_ACCUM),
        }
    },

    [STA_DIR_INDL] = {
        .uops = {
            MOV_LPC     (MOV_LSB, REG_ADDR),
            ZEXT        (REG_ADDR),
            SKIPC       (1, STATUS_FLAG_DL),
            IO          /* DL != 0 */,
            ADDR_OFFR   (REG_D, ADDR_OFF_BANK_WRAP),
            MOV_L       (MOV_LSB, REG_ADDR, REG_ZERO, REG_TEMP),
            ADDR_OFFI   (1, ADDR_OFF_BANK_WRAP),
            MOV_L       (MOV_MSB, REG_ADDR, REG_ZERO, REG_TEMP),
            ADDR_OFFI   (1, ADDR_OFF_BANK_WRAP),
            MOV_L       (MOV_LSB, REG_ADDR, REG_ZERO, REG_BANK),
            MOV_RRW     (REG_TEMP, REG_ADDR, MOV_RRW_WORD),
            MOV_S       (MOV_LSB, REG_ADDR, REG_BANK, REG_ACCUM),
            SKIPS       (2, STATUS_FLAG_M),
            ADDR_OFFI   (1, ADDR_OFF_BANK_WRAP),
            MOV_S       (MOV_MSB, REG_ADDR, REG_BANK, REG_ACCUM),
        }
    },

    [STA_S_REL_IND_Y] = {
        .uops = {
            MOV_LPC     (MOV_LSB, REG_ADDR),
            ZEXT        (REG_ADDR),
            IO,
            ADDR_OFFR   (REG_S, ADDR_OFF_BANK_WRAP),
            MOV_L       (MOV_LSB, REG_ADDR, REG_ZERO, REG_TEMP),
            ADDR_OFFI   (1, ADDR_OFF_BANK_WRAP),
            MOV_L       (MOV_MSB, REG_ADDR, REG_ZERO, REG_TEMP),
            MOV_RRW     (REG_TEMP, REG_ADDR, MOV_RRW_WORD),
            MOV_RRW     (REG_DBR, REG_BANK, MOV_RRW_BYTE),
            IO,
            ADDR_OFFR   (REG_Y, ADDR_OFF_BANK_NEXT),
            MOV_S       (MOV_LSB, REG_ADDR, REG_BANK, REG_ACCUM),
            SKIPS       (2, STATUS_FLAG_M),
            ADDR_OFFI   (1, ADDR_OFF_BANK_NEXT),
            MOV_S       (MOV_MSB, REG_ADDR, REG_BANK, REG_ACCUM),
        }
    },

    [STA_DIR_X_IND] = {
        .uops = {
            MOV_LPC     (MOV_LSB, REG_ADDR),
            ZEXT        (REG_ADDR),
            SKIPC       (1, STATUS_FLAG_DL),
            IO          /* DL != 0 */,
            ADDR_OFFR   (REG_D, ADDR_OFF_BANK_WRAP),
            ADDR_OFFR   (REG_X, ADDR_OFF_BANK_WRAP),
            IO,
            MOV_L       (MOV_LSB, REG_ADDR, REG_ZERO, REG_TEMP),
            ADDR_OFFI   (1, ADDR_OFF_BANK_WRAP),
            MOV_L       (MOV_MSB, REG_ADDR, REG_ZERO, REG_TEMP),
            MOV_RRW     (REG_TEMP, REG_ADDR, MOV_RRW_WORD),
            MOV_S       (MOV_LSB, REG_ADDR, REG_DBR, REG_ACCUM),
            SKIPS       (2, STATUS_FLAG_M),
            ADDR_OFFI   (1, ADDR_OFF_BANK_WRAP),
            MOV_S       (MOV_MSB, REG_ADDR, REG_DBR, REG_ACCUM),
        }
    },

    [STA_DIR_IND_Y] = {
        .uops = {
            MOV_LPC     (MOV_LSB, REG_ADDR),
            ZEXT        (REG_ADDR),
            SKIPC       (1, STATUS_FLAG_DL),
            IO          /* DL != 0 */,
            ADDR_OFFR   (REG_D, ADDR_OFF_BANK_WRAP),
            MOV_L       (MOV_LSB, REG_ADDR, REG_ZERO, REG_TEMP),
            ADDR_OFFI   (1, ADDR_OFF_BANK_WRAP),
            MOV_L       (MOV_MSB, REG_ADDR, REG_ZERO, REG_TEMP),
            MOV_RRW     (REG_TEMP, REG_ADDR, MOV_RRW_WORD),
            MOV_RRW     (REG_DBR, REG_BANK, MOV_RRW_BYTE),
            ADDR_OFFR   (REG_Y, ADDR_OFF_BANK_NEXT),
            SKIPS       (2, STATUS_FLAG_X),
            SKIPC       (1, STATUS_FLAG_PAGE),
            IO          /* X = 0 or page crossed */,
            MOV_S       (MOV_LSB, REG_ADDR, REG_BANK, REG_ACCUM),
            SKIPS       (2, STATUS_FLAG_M),
            ADDR_OFFI   (1, ADDR_OFF_BANK_NEXT),
            MOV_S       (MOV_MSB, REG_ADDR, REG_BANK, REG_ACCUM),
        }
    },

    [STA_DIR_INDL_Y] = {
        .uops = {
            MOV_LPC     (MOV_LSB, REG_ADDR),
            ZEXT        (REG_ADDR),
            ADDR_OFFR   (REG_D, 0),
            SKIPC       (1, STATUS_FLAG_DL),
            IO          /* DL != 0 */,
            MOV_L       (MOV_LSB, REG_ADDR, REG_ZERO, REG_TEMP),
            ADDR_OFFI   (1, ADDR_OFF_BANK_WRAP),
            MOV_L       (MOV_MSB, REG_ADDR, REG_ZERO, REG_TEMP),
            ADDR_OFFI   (1, ADDR_OFF_BANK_WRAP),
            MOV_L       (MOV_LSB, REG_ADDR, REG_ZERO, REG_BANK),
            MOV_RRW     (REG_TEMP, REG_ADDR, MOV_RRW_WORD),
            // MOV_RRW     (REG_DBR, REG_BANK, MOV_RRW_BYTE),
            ADDR_OFFR   (REG_Y, ADDR_OFF_BANK_NEXT),
            MOV_S       (MOV_LSB, REG_ADDR, REG_BANK, REG_ACCUM),
            SKIPS       (2, STATUS_FLAG_M),
            ADDR_OFFI   (1, ADDR_OFF_BANK_NEXT),
            MOV_S       (MOV_MSB, REG_ADDR, REG_BANK, REG_ACCUM),
        }
    },

    /**************************************************************************************/
    /*                                      STX                                           */
    /**************************************************************************************/

    [STX_ABS] = {
        .uops = {
            MOV_LPC     (MOV_LSB, REG_ADDR),
            MOV_LPC     (MOV_MSB, REG_ADDR),
            MOV_S       (MOV_LSB, REG_ADDR, REG_DBR, REG_X),
            SKIPS       (2, STATUS_FLAG_X),
            ADDR_OFFI   (1, ADDR_OFF_BANK_WRAP),
            MOV_S       (MOV_MSB, REG_ADDR, REG_DBR, REG_X),
        }
    },
    [STX_DIR] = {
        .uops = {
            MOV_LPC     (MOV_LSB, REG_ADDR),
            ZEXT        (REG_ADDR),
            ADDR_OFFR   (REG_D, ADDR_OFF_BANK_WRAP),
            SKIPC       (1, STATUS_FLAG_DL),
            IO          /* DL != 0  */,
            MOV_S       (MOV_LSB, REG_ADDR, REG_ZERO, REG_X),
            SKIPS       (2, STATUS_FLAG_X),
            ADDR_OFFI   (1, ADDR_OFF_BANK_WRAP),
            MOV_S       (MOV_MSB, REG_ADDR, REG_ZERO, REG_X),
        }
    },
    [STX_DIR_Y] = {
        .uops = {
            MOV_LPC     (MOV_LSB, REG_ADDR),
            ZEXT        (REG_ADDR),
            ADDR_OFFR   (REG_D, ADDR_OFF_BANK_WRAP),
            ADDR_OFFR   (REG_Y, ADDR_OFF_BANK_WRAP),
            SKIPC       (1, STATUS_FLAG_DL),
            IO          /* DL != 0 */,
            IO,
            MOV_S       (MOV_LSB, REG_ADDR, REG_ZERO, REG_X),
            SKIPS       (2, STATUS_FLAG_X),
            ADDR_OFFI   (1, ADDR_OFF_BANK_WRAP),
            MOV_S       (MOV_MSB, REG_ADDR, REG_ZERO, REG_X),
        }
    },

    /**************************************************************************************/
    /*                                      STY                                           */
    /**************************************************************************************/

    [STY_ABS] = {
        .uops = {
            MOV_LPC     (MOV_LSB, REG_ADDR),
            MOV_LPC     (MOV_MSB, REG_ADDR),
            MOV_S       (MOV_LSB, REG_ADDR, REG_DBR, REG_Y),
            SKIPS       (2, STATUS_FLAG_X),
            ADDR_OFFI   (1, ADDR_OFF_BANK_WRAP),
            MOV_S       (MOV_MSB, REG_ADDR, REG_DBR, REG_Y),
        }
    },
    [STY_DIR] = {
        .uops = {
            MOV_LPC     (MOV_LSB, REG_ADDR),
            ZEXT        (REG_ADDR),
            ADDR_OFFR   (REG_D, ADDR_OFF_BANK_WRAP),
            SKIPC       (1, STATUS_FLAG_DL),
            IO          /* DL !=0  */,
            MOV_S       (MOV_LSB, REG_ADDR, REG_ZERO, REG_Y),
            SKIPS       (2, STATUS_FLAG_X),
            ADDR_OFFI   (1, ADDR_OFF_BANK_WRAP),
            MOV_S       (MOV_MSB, REG_ADDR, REG_ZERO, REG_Y),
        }
    },
    [STY_DIR_X] = {
        .uops = {
            MOV_LPC     (MOV_LSB, REG_ADDR),
            ZEXT        (REG_ADDR),
            ADDR_OFFR   (REG_D, ADDR_OFF_BANK_WRAP),
            ADDR_OFFR   (REG_X, ADDR_OFF_BANK_WRAP),
            SKIPC       (1, STATUS_FLAG_DL),
            IO          /* DL != 0 */,
            IO,
            MOV_S       (MOV_LSB, REG_ADDR, REG_ZERO, REG_Y),
            SKIPS       (2, STATUS_FLAG_X),
            ADDR_OFFI   (1, ADDR_OFF_BANK_WRAP),
            MOV_S       (MOV_MSB, REG_ADDR, REG_ZERO, REG_Y),
        }
    },

    /**************************************************************************************/
    /*                                      STZ                                           */
    /**************************************************************************************/

    [STZ_ABS] = {
        .uops = {
            MOV_LPC     (MOV_LSB, REG_ADDR),
            MOV_LPC     (MOV_MSB, REG_ADDR),
            MOV_S       (MOV_LSB, REG_ADDR, REG_DBR, REG_ZERO),
            SKIPS       (2, STATUS_FLAG_M),
            ADDR_OFFI   (1, ADDR_OFF_BANK_WRAP),
            MOV_S       (MOV_MSB, REG_ADDR, REG_DBR, REG_ZERO),
        }
    },

    [STZ_ABS_X] = {
        .uops = {
            MOV_LPC     (MOV_LSB, REG_ADDR),
            MOV_LPC     (MOV_MSB, REG_ADDR),
            MOV_RRW     (REG_DBR, REG_BANK, MOV_RRW_BYTE),
            ADDR_OFFR   (REG_X, ADDR_OFF_BANK_NEXT),
            IO          /* extra cycle due to write */,
            MOV_S       (MOV_LSB, REG_ADDR, REG_BANK, REG_ZERO),
            SKIPS       (2, STATUS_FLAG_M),
            ADDR_OFFI   (1, ADDR_OFF_BANK_NEXT),
            MOV_S       (MOV_MSB, REG_ADDR, REG_BANK, REG_ZERO),
        }
    },

    [STZ_DIR] = {
        .uops = {
            MOV_LPC     (MOV_LSB, REG_ADDR),
            ZEXT        (REG_ADDR),
            ADDR_OFFR   (REG_D, ADDR_OFF_BANK_WRAP),
            SKIPC       (1, STATUS_FLAG_DL),
            IO          /* DL !=0  */,
            MOV_S       (MOV_LSB, REG_ADDR, REG_ZERO, REG_ZERO),
            SKIPS       (2, STATUS_FLAG_M),
            ADDR_OFFI   (1, ADDR_OFF_BANK_WRAP),
            MOV_S       (MOV_MSB, REG_ADDR, REG_ZERO, REG_ZERO),
        }
    },

    [STZ_DIR_X] = {
        .uops = {
            MOV_LPC     (MOV_LSB, REG_ADDR),
            ZEXT        (REG_ADDR),
            ADDR_OFFR   (REG_D, ADDR_OFF_BANK_WRAP),
            ADDR_OFFR   (REG_X, ADDR_OFF_BANK_WRAP),
            SKIPC       (1, STATUS_FLAG_DL),
            IO          /* DL != 0 */,
            IO,
            MOV_S       (MOV_LSB, REG_ADDR, REG_ZERO, REG_ZERO),
            SKIPS       (2, STATUS_FLAG_M),
            ADDR_OFFI   (1, ADDR_OFF_BANK_WRAP),
            MOV_S       (MOV_MSB, REG_ADDR, REG_ZERO, REG_ZERO),
        }
    },

    /**************************************************************************************/
    /*                                      TAX                                           */
    /**************************************************************************************/


    [TAX_IMP] = {
        .uops = {
            IO,
            MOV_RR      (REG_ACCUM, REG_X),
            CHK_ZN      (REG_X),
        }
    },

    /**************************************************************************************/
    /*                                      TAY                                           */
    /**************************************************************************************/

    [TAY_IMP] = {
        .uops = {
            IO,
            MOV_RR      (REG_ACCUM, REG_Y),
            CHK_ZN      (REG_Y),
        }
    },

    /**************************************************************************************/
    /*                                      TCD                                           */
    /**************************************************************************************/

    [TCD_IMP] = {
        .uops = {
            IO,
            MOV_RRW     (REG_ACCUM, REG_D, MOV_RRW_WORD),
            CHK_ZNW     (REG_D, CHK_ZW_WORD),
        }
    },

    /**************************************************************************************/
    /*                                      TRB                                           */
    /**************************************************************************************/

    [TRB_ABS] = {
        .uops = {
            MOV_LPC     (MOV_LSB, REG_ADDR),
            MOV_LPC     (MOV_MSB, REG_ADDR),
            MOV_L       (MOV_LSB, REG_ADDR, REG_DBR, REG_TEMP),
            SKIPS       (2, STATUS_FLAG_M),
            ADDR_OFFI   (1, ADDR_OFF_BANK_WRAP),
            MOV_L       (MOV_MSB, REG_ADDR, REG_DBR, REG_TEMP),
            ALU_OP      (ALU_OP_TRB, STATUS_FLAG_M, REG_TEMP, REG_ZERO),
            IO,
            SKIPS       (2, STATUS_FLAG_M),
            MOV_S       (MOV_MSB, REG_ADDR, REG_DBR, REG_TEMP),
            ADDR_OFFIS  (0xffff, ADDR_OFF_BANK_WRAP, ADDR_OFF_SIGNED),
            MOV_S       (MOV_LSB, REG_ADDR, REG_DBR, REG_TEMP)
        }
    },

    [TRB_DIR] = {
        .uops = {
            MOV_LPC     (MOV_LSB, REG_ADDR),
            ZEXT        (REG_ADDR),
            ADDR_OFFR   (REG_D, ADDR_OFF_BANK_WRAP),
            MOV_L       (MOV_LSB, REG_ADDR, REG_DBR, REG_TEMP),
            SKIPS       (2, STATUS_FLAG_M),
            ADDR_OFFI   (1, ADDR_OFF_BANK_WRAP),
            MOV_L       (MOV_MSB, REG_ADDR, REG_DBR, REG_TEMP),
            ALU_OP      (ALU_OP_TRB, STATUS_FLAG_M, REG_TEMP, REG_ZERO),
            IO,
            SKIPS       (2, STATUS_FLAG_M),
            MOV_S       (MOV_MSB, REG_ADDR, REG_DBR, REG_TEMP),
            ADDR_OFFIS  (0xffff, ADDR_OFF_BANK_WRAP, ADDR_OFF_SIGNED),
            MOV_S       (MOV_LSB, REG_ADDR, REG_DBR, REG_TEMP)
        }
    },

    /**************************************************************************************/
    /*                                      TSB                                           */
    /**************************************************************************************/

    [TSB_ABS] = {
        .uops = {
            MOV_LPC     (MOV_LSB, REG_ADDR),
            MOV_LPC     (MOV_MSB, REG_ADDR),
            MOV_L       (MOV_LSB, REG_ADDR, REG_DBR, REG_TEMP),
            SKIPS       (2, STATUS_FLAG_M),
            ADDR_OFFI   (1, ADDR_OFF_BANK_WRAP),
            MOV_L       (MOV_MSB, REG_ADDR, REG_DBR, REG_TEMP),
            ALU_OP      (ALU_OP_TSB, STATUS_FLAG_M, REG_TEMP, REG_ZERO),
            IO,
            SKIPS       (2, STATUS_FLAG_M),
            MOV_S       (MOV_MSB, REG_ADDR, REG_DBR, REG_TEMP),
            ADDR_OFFIS  (0xffff, ADDR_OFF_BANK_WRAP, ADDR_OFF_SIGNED),
            MOV_S       (MOV_LSB, REG_ADDR, REG_DBR, REG_TEMP)
        }
    },

    [TSB_DIR] = {
        .uops = {
            MOV_LPC     (MOV_LSB, REG_ADDR),
            ZEXT        (REG_ADDR),
            ADDR_OFFR   (REG_D, ADDR_OFF_BANK_WRAP),
            MOV_L       (MOV_LSB, REG_ADDR, REG_DBR, REG_TEMP),
            SKIPS       (2, STATUS_FLAG_M),
            ADDR_OFFI   (1, ADDR_OFF_BANK_WRAP),
            MOV_L       (MOV_MSB, REG_ADDR, REG_DBR, REG_TEMP),
            ALU_OP      (ALU_OP_TSB, STATUS_FLAG_M, REG_TEMP, REG_ZERO),
            IO,
            SKIPS       (2, STATUS_FLAG_M),
            MOV_S       (MOV_MSB, REG_ADDR, REG_DBR, REG_TEMP),
            ADDR_OFFIS  (0xffff, ADDR_OFF_BANK_WRAP, ADDR_OFF_SIGNED),
            MOV_S       (MOV_LSB, REG_ADDR, REG_DBR, REG_TEMP)
        }
    },

    /**************************************************************************************/
    /*                                      TCS                                           */
    /**************************************************************************************/

    [TCS_IMP] = {
        .uops = {
            IO,
            MOV_RRW     (REG_ACCUM, REG_S, MOV_RRW_WORD),
        }
    },

    /**************************************************************************************/
    /*                                      TDC                                           */
    /**************************************************************************************/

    [TDC_IMP] = {
        .uops = {
            IO,
            MOV_RRW     (REG_D, REG_ACCUM, MOV_RRW_WORD),
            CHK_ZN      (REG_ACCUM),
        }
    },

    /**************************************************************************************/
    /*                                      TSC                                           */
    /**************************************************************************************/

    [TSC_ACC] = {
        .uops = {
            IO,
            MOV_RRW     (REG_S, REG_ACCUM, MOV_RRW_WORD),
            CHK_ZN      (REG_ACCUM),
        }
    },

    /**************************************************************************************/
    /*                                      TSX                                           */
    /**************************************************************************************/

    [TSX_ACC] = {
        .uops = {
            IO,
            MOV_RR      (REG_S, REG_X),
            CHK_ZN      (REG_X),
        }
    },

    /**************************************************************************************/
    /*                                      TXA                                           */
    /**************************************************************************************/

    [TXA_ACC] = {
        .uops = {
            IO,
            MOV_RR      (REG_X, REG_ACCUM),
            CHK_ZN      (REG_ACCUM),
        }
    },

    /**************************************************************************************/
    /*                                      TXS                                           */
    /**************************************************************************************/

    [TXS_ACC] = {
        .uops = {
            IO,
            MOV_RRW     (REG_X, REG_S, MOV_RRW_WORD),
        }
    },

    /**************************************************************************************/
    /*                                      TXY                                           */
    /**************************************************************************************/

    [TXY_ACC] = {
        .uops = {
            IO,
            MOV_RR      (REG_X, REG_Y),
            CHK_ZN      (REG_Y),
        }
    },

    /**************************************************************************************/
    /*                                      TYA                                           */
    /**************************************************************************************/

    [TYA_ACC] = {
        .uops = {
            IO,
            MOV_RR      (REG_Y, REG_ACCUM),
            CHK_ZN      (REG_ACCUM),
        }
    },

    /**************************************************************************************/
    /*                                      TYX                                           */
    /**************************************************************************************/

    [TYX_ACC] = {
        .uops = {
            IO,
            MOV_RR      (REG_Y, REG_X),
            CHK_ZN      (REG_X),
        }
    },

    /**************************************************************************************/
    /*                                      WAI                                           */
    /**************************************************************************************/

    [WAI_ACC] = {
        .uops = {
            IO,
            IO,
            WAI
        }
    },

    /**************************************************************************************/
    /*                                      WDM                                           */
    /**************************************************************************************/

    [WDM_ACC] = {
        .uops = {
            SKIPC(0, 0)
        }
    },

    /**************************************************************************************/
    /*                                      XBA                                           */
    /**************************************************************************************/

    [XBA_ACC] = {
        .uops = {
            IO,
            IO,
            XBA,
            CHK_ZNW     (REG_ACCUM, CHK_ZW_BYTE)
        }
    },

    /**************************************************************************************/
    /*                                      XCE                                           */
    /**************************************************************************************/

    [XCE_ACC] = {
        .uops = {
            IO,
            XCE
        }
    },


    [FETCH] = {
        .uops = {
            MOV_LPC     (MOV_LSB, REG_INST),
            DECODE
        }
    }
};

struct opcode_t opcode_matrix[256] =
{
    [0x61] = OPCODE(OPCODE_ADC, OPCODE_CLASS_ALU, ADDRESS_MODE_DIRECT_INDEXED_INDIRECT),
    [0x63] = OPCODE(OPCODE_ADC, OPCODE_CLASS_ALU, ADDRESS_MODE_STACK_RELATIVE),
    [0x65] = OPCODE(OPCODE_ADC, OPCODE_CLASS_ALU, ADDRESS_MODE_DIRECT),
    [0x67] = OPCODE(OPCODE_ADC, OPCODE_CLASS_ALU, ADDRESS_MODE_DIRECT_INDIRECT_LONG),
    [0x69] = OPCODE(OPCODE_ADC, OPCODE_CLASS_ALU, ADDRESS_MODE_IMMEDIATE),
    [0x6d] = OPCODE(OPCODE_ADC, OPCODE_CLASS_ALU, ADDRESS_MODE_ABSOLUTE),
    [0x6f] = OPCODE(OPCODE_ADC, OPCODE_CLASS_ALU, ADDRESS_MODE_ABSOLUTE_LONG),
    [0x71] = OPCODE(OPCODE_ADC, OPCODE_CLASS_ALU, ADDRESS_MODE_DIRECT_INDIRECT_INDEXED),
    [0x72] = OPCODE(OPCODE_ADC, OPCODE_CLASS_ALU, ADDRESS_MODE_DIRECT_INDIRECT),
    [0x73] = OPCODE(OPCODE_ADC, OPCODE_CLASS_ALU, ADDRESS_MODE_STACK_RELATIVE_INDIRECT_INDEXED),
    [0x75] = OPCODE(OPCODE_ADC, OPCODE_CLASS_ALU, ADDRESS_MODE_DIRECT_INDEXED_X),
    [0x77] = OPCODE(OPCODE_ADC, OPCODE_CLASS_ALU, ADDRESS_MODE_DIRECT_INDIRECT_LONG_INDEXED),
    [0x79] = OPCODE(OPCODE_ADC, OPCODE_CLASS_ALU, ADDRESS_MODE_ABSOLUTE_INDEXED_Y),
    [0x7d] = OPCODE(OPCODE_ADC, OPCODE_CLASS_ALU, ADDRESS_MODE_ABSOLUTE_INDEXED_X),
    [0x7f] = OPCODE(OPCODE_ADC, OPCODE_CLASS_ALU, ADDRESS_MODE_ABSOLUTE_LONG_INDEXED_X),

    [0x21] = OPCODE(OPCODE_AND, OPCODE_CLASS_ALU, ADDRESS_MODE_DIRECT_INDEXED_INDIRECT),
    [0x23] = OPCODE(OPCODE_AND, OPCODE_CLASS_ALU, ADDRESS_MODE_STACK_RELATIVE),
    [0x25] = OPCODE(OPCODE_AND, OPCODE_CLASS_ALU, ADDRESS_MODE_DIRECT),
    [0x27] = OPCODE(OPCODE_AND, OPCODE_CLASS_ALU, ADDRESS_MODE_DIRECT_INDIRECT_LONG),
    [0x29] = OPCODE(OPCODE_AND, OPCODE_CLASS_ALU, ADDRESS_MODE_IMMEDIATE),
    [0x2d] = OPCODE(OPCODE_AND, OPCODE_CLASS_ALU, ADDRESS_MODE_ABSOLUTE),
    [0x2f] = OPCODE(OPCODE_AND, OPCODE_CLASS_ALU, ADDRESS_MODE_ABSOLUTE_LONG),
    [0x31] = OPCODE(OPCODE_AND, OPCODE_CLASS_ALU, ADDRESS_MODE_DIRECT_INDIRECT_INDEXED),
    [0x32] = OPCODE(OPCODE_AND, OPCODE_CLASS_ALU, ADDRESS_MODE_DIRECT_INDIRECT),
    [0x33] = OPCODE(OPCODE_AND, OPCODE_CLASS_ALU, ADDRESS_MODE_STACK_RELATIVE_INDIRECT_INDEXED),
    [0x35] = OPCODE(OPCODE_AND, OPCODE_CLASS_ALU, ADDRESS_MODE_DIRECT_INDEXED_X),
    [0x37] = OPCODE(OPCODE_AND, OPCODE_CLASS_ALU, ADDRESS_MODE_DIRECT_INDIRECT_LONG_INDEXED),
    [0x39] = OPCODE(OPCODE_AND, OPCODE_CLASS_ALU, ADDRESS_MODE_ABSOLUTE_INDEXED_Y),
    [0x3d] = OPCODE(OPCODE_AND, OPCODE_CLASS_ALU, ADDRESS_MODE_ABSOLUTE_INDEXED_X),
    [0x3f] = OPCODE(OPCODE_AND, OPCODE_CLASS_ALU, ADDRESS_MODE_ABSOLUTE_LONG_INDEXED_X),

    [0x06] = OPCODE(OPCODE_ASL, OPCODE_CLASS_ALU, ADDRESS_MODE_DIRECT),
    [0x0a] = OPCODE(OPCODE_ASL, OPCODE_CLASS_ALU, ADDRESS_MODE_ACCUMULATOR),
    [0x0e] = OPCODE(OPCODE_ASL, OPCODE_CLASS_ALU, ADDRESS_MODE_ABSOLUTE),
    [0x16] = OPCODE(OPCODE_ASL, OPCODE_CLASS_ALU, ADDRESS_MODE_DIRECT_INDEXED_X),
    [0x1e] = OPCODE(OPCODE_ASL, OPCODE_CLASS_ALU, ADDRESS_MODE_ABSOLUTE_INDEXED_X),

    [0x10] = OPCODE(OPCODE_BPL, OPCODE_CLASS_BRANCH, ADDRESS_MODE_PROGRAM_COUNTER_RELATIVE),
    [0x30] = OPCODE(OPCODE_BMI, OPCODE_CLASS_BRANCH, ADDRESS_MODE_PROGRAM_COUNTER_RELATIVE),
    [0x50] = OPCODE(OPCODE_BVC, OPCODE_CLASS_BRANCH, ADDRESS_MODE_PROGRAM_COUNTER_RELATIVE),
    [0x70] = OPCODE(OPCODE_BVS, OPCODE_CLASS_BRANCH, ADDRESS_MODE_PROGRAM_COUNTER_RELATIVE),
    [0x80] = OPCODE(OPCODE_BRA, OPCODE_CLASS_BRANCH, ADDRESS_MODE_PROGRAM_COUNTER_RELATIVE),
    [0x82] = OPCODE(OPCODE_BRL, OPCODE_CLASS_BRANCH, ADDRESS_MODE_PROGRAM_COUNTER_RELATIVE_LONG),
    [0x90] = OPCODE(OPCODE_BCC, OPCODE_CLASS_BRANCH, ADDRESS_MODE_PROGRAM_COUNTER_RELATIVE),
    [0xb0] = OPCODE(OPCODE_BCS, OPCODE_CLASS_BRANCH, ADDRESS_MODE_PROGRAM_COUNTER_RELATIVE),
    [0xd0] = OPCODE(OPCODE_BNE, OPCODE_CLASS_BRANCH, ADDRESS_MODE_PROGRAM_COUNTER_RELATIVE),
    [0xf0] = OPCODE(OPCODE_BEQ, OPCODE_CLASS_BRANCH, ADDRESS_MODE_PROGRAM_COUNTER_RELATIVE),

    [0x24] = OPCODE(OPCODE_BIT, OPCODE_CLASS_ALU, ADDRESS_MODE_DIRECT),
    [0x2c] = OPCODE(OPCODE_BIT, OPCODE_CLASS_ALU, ADDRESS_MODE_ABSOLUTE),
    [0x34] = OPCODE(OPCODE_BIT, OPCODE_CLASS_ALU, ADDRESS_MODE_DIRECT_INDEXED_X),
    [0x3c] = OPCODE(OPCODE_BIT, OPCODE_CLASS_ALU, ADDRESS_MODE_ABSOLUTE_INDEXED_X),
    [0x89] = OPCODE(OPCODE_BIT, OPCODE_CLASS_ALU, ADDRESS_MODE_IMMEDIATE),

    [0x00] = OPCODE(OPCODE_BRK, OPCODE_CLASS_STANDALONE, ADDRESS_MODE_STACK),

    [0x18] = OPCODE(OPCODE_CLC, OPCODE_CLASS_STANDALONE, ADDRESS_MODE_IMPLIED),
    [0x38] = OPCODE(OPCODE_SEC, OPCODE_CLASS_STANDALONE, ADDRESS_MODE_IMPLIED),
    [0x58] = OPCODE(OPCODE_CLI, OPCODE_CLASS_STANDALONE, ADDRESS_MODE_IMPLIED),
    [0x78] = OPCODE(OPCODE_SEI, OPCODE_CLASS_STANDALONE, ADDRESS_MODE_IMPLIED),
    [0xb8] = OPCODE(OPCODE_CLV, OPCODE_CLASS_STANDALONE, ADDRESS_MODE_IMPLIED),
    [0xd8] = OPCODE(OPCODE_CLD, OPCODE_CLASS_STANDALONE, ADDRESS_MODE_IMPLIED),
    [0xf8] = OPCODE(OPCODE_SED, OPCODE_CLASS_STANDALONE, ADDRESS_MODE_IMPLIED),

    [0xc2] = OPCODE(OPCODE_REP, OPCODE_CLASS_STANDALONE, ADDRESS_MODE_IMMEDIATE),
    [0xe2] = OPCODE(OPCODE_SEP, OPCODE_CLASS_STANDALONE, ADDRESS_MODE_IMMEDIATE),

    [0xc1] = OPCODE(OPCODE_CMP, OPCODE_CLASS_ALU, ADDRESS_MODE_DIRECT_INDEXED_INDIRECT),
    [0xc3] = OPCODE(OPCODE_CMP, OPCODE_CLASS_ALU, ADDRESS_MODE_STACK_RELATIVE),
    [0xc5] = OPCODE(OPCODE_CMP, OPCODE_CLASS_ALU, ADDRESS_MODE_DIRECT),
    [0xc7] = OPCODE(OPCODE_CMP, OPCODE_CLASS_ALU, ADDRESS_MODE_DIRECT_INDIRECT_LONG),
    [0xc9] = OPCODE(OPCODE_CMP, OPCODE_CLASS_ALU, ADDRESS_MODE_IMMEDIATE),
    [0xcd] = OPCODE(OPCODE_CMP, OPCODE_CLASS_ALU, ADDRESS_MODE_ABSOLUTE),
    [0xcf] = OPCODE(OPCODE_CMP, OPCODE_CLASS_ALU, ADDRESS_MODE_ABSOLUTE_LONG),
    [0xd1] = OPCODE(OPCODE_CMP, OPCODE_CLASS_ALU, ADDRESS_MODE_DIRECT_INDIRECT_INDEXED),
    [0xd2] = OPCODE(OPCODE_CMP, OPCODE_CLASS_ALU, ADDRESS_MODE_DIRECT_INDIRECT),
    [0xd3] = OPCODE(OPCODE_CMP, OPCODE_CLASS_ALU, ADDRESS_MODE_STACK_RELATIVE_INDIRECT_INDEXED),
    [0xd5] = OPCODE(OPCODE_CMP, OPCODE_CLASS_ALU, ADDRESS_MODE_DIRECT_INDEXED_X),
    [0xd7] = OPCODE(OPCODE_CMP, OPCODE_CLASS_ALU, ADDRESS_MODE_DIRECT_INDIRECT_LONG_INDEXED),
    [0xd9] = OPCODE(OPCODE_CMP, OPCODE_CLASS_ALU, ADDRESS_MODE_ABSOLUTE_INDEXED_Y),
    [0xdd] = OPCODE(OPCODE_CMP, OPCODE_CLASS_ALU, ADDRESS_MODE_ABSOLUTE_INDEXED_X),
    [0xdf] = OPCODE(OPCODE_CMP, OPCODE_CLASS_ALU, ADDRESS_MODE_ABSOLUTE_LONG_INDEXED_X),

    [0x02] = OPCODE(OPCODE_COP, OPCODE_CLASS_STANDALONE, ADDRESS_MODE_STACK),

    [0xe0] = OPCODE(OPCODE_CPX, OPCODE_CLASS_ALU, ADDRESS_MODE_IMMEDIATE),
    [0xe4] = OPCODE(OPCODE_CPX, OPCODE_CLASS_ALU, ADDRESS_MODE_DIRECT),
    [0xec] = OPCODE(OPCODE_CPX, OPCODE_CLASS_ALU, ADDRESS_MODE_ABSOLUTE),

    [0xc0] = OPCODE(OPCODE_CPY, OPCODE_CLASS_ALU, ADDRESS_MODE_IMMEDIATE),
    [0xc4] = OPCODE(OPCODE_CPY, OPCODE_CLASS_ALU, ADDRESS_MODE_DIRECT),
    [0xcc] = OPCODE(OPCODE_CPY, OPCODE_CLASS_ALU, ADDRESS_MODE_ABSOLUTE),

    [0x3a] = OPCODE(OPCODE_DEC, OPCODE_CLASS_ALU, ADDRESS_MODE_ACCUMULATOR),
    [0xc6] = OPCODE(OPCODE_DEC, OPCODE_CLASS_ALU, ADDRESS_MODE_DIRECT),
    [0xce] = OPCODE(OPCODE_DEC, OPCODE_CLASS_ALU, ADDRESS_MODE_ABSOLUTE),
    [0xd6] = OPCODE(OPCODE_DEC, OPCODE_CLASS_ALU, ADDRESS_MODE_DIRECT_INDEXED_X),
    [0xde] = OPCODE(OPCODE_DEC, OPCODE_CLASS_ALU, ADDRESS_MODE_ABSOLUTE_INDEXED_X),

    [0xca] = OPCODE(OPCODE_DEX, OPCODE_CLASS_ALU, ADDRESS_MODE_IMPLIED),
    [0x88] = OPCODE(OPCODE_DEY, OPCODE_CLASS_ALU, ADDRESS_MODE_IMPLIED),

    [0x41] = OPCODE(OPCODE_EOR, OPCODE_CLASS_ALU, ADDRESS_MODE_DIRECT_INDEXED_INDIRECT),
    [0x43] = OPCODE(OPCODE_EOR, OPCODE_CLASS_ALU, ADDRESS_MODE_STACK_RELATIVE),
    [0x45] = OPCODE(OPCODE_EOR, OPCODE_CLASS_ALU, ADDRESS_MODE_DIRECT),
    [0x47] = OPCODE(OPCODE_EOR, OPCODE_CLASS_ALU, ADDRESS_MODE_DIRECT_INDIRECT_LONG),
    [0x49] = OPCODE(OPCODE_EOR, OPCODE_CLASS_ALU, ADDRESS_MODE_IMMEDIATE),
    [0x4d] = OPCODE(OPCODE_EOR, OPCODE_CLASS_ALU, ADDRESS_MODE_ABSOLUTE),
    [0x4f] = OPCODE(OPCODE_EOR, OPCODE_CLASS_ALU, ADDRESS_MODE_ABSOLUTE_LONG),
    [0x51] = OPCODE(OPCODE_EOR, OPCODE_CLASS_ALU, ADDRESS_MODE_DIRECT_INDIRECT_INDEXED),
    [0x52] = OPCODE(OPCODE_EOR, OPCODE_CLASS_ALU, ADDRESS_MODE_DIRECT_INDIRECT),
    [0x53] = OPCODE(OPCODE_EOR, OPCODE_CLASS_ALU, ADDRESS_MODE_STACK_RELATIVE_INDIRECT_INDEXED),
    [0x55] = OPCODE(OPCODE_EOR, OPCODE_CLASS_ALU, ADDRESS_MODE_DIRECT_INDEXED_X),
    [0x57] = OPCODE(OPCODE_EOR, OPCODE_CLASS_ALU, ADDRESS_MODE_DIRECT_INDIRECT_LONG_INDEXED),
    [0x59] = OPCODE(OPCODE_EOR, OPCODE_CLASS_ALU, ADDRESS_MODE_ABSOLUTE_INDEXED_Y),
    [0x5d] = OPCODE(OPCODE_EOR, OPCODE_CLASS_ALU, ADDRESS_MODE_ABSOLUTE_INDEXED_X),
    [0x5f] = OPCODE(OPCODE_EOR, OPCODE_CLASS_ALU, ADDRESS_MODE_ABSOLUTE_LONG_INDEXED_X),

    [0x1a] = OPCODE(OPCODE_INC, OPCODE_CLASS_ALU, ADDRESS_MODE_ACCUMULATOR),
    [0xe6] = OPCODE(OPCODE_INC, OPCODE_CLASS_ALU, ADDRESS_MODE_DIRECT),
    [0xee] = OPCODE(OPCODE_INC, OPCODE_CLASS_ALU, ADDRESS_MODE_ABSOLUTE),
    [0xf6] = OPCODE(OPCODE_INC, OPCODE_CLASS_ALU, ADDRESS_MODE_DIRECT_INDEXED_X),
    [0xfe] = OPCODE(OPCODE_INC, OPCODE_CLASS_ALU, ADDRESS_MODE_ABSOLUTE_INDEXED_X),

    [0xe8] = OPCODE(OPCODE_INX, OPCODE_CLASS_ALU, ADDRESS_MODE_IMPLIED),
    [0xc8] = OPCODE(OPCODE_INY, OPCODE_CLASS_ALU, ADDRESS_MODE_IMPLIED),

    [0x4c] = OPCODE(OPCODE_JMP, OPCODE_CLASS_BRANCH, ADDRESS_MODE_ABSOLUTE),
    [0x5c] = OPCODE(OPCODE_JMP, OPCODE_CLASS_BRANCH, ADDRESS_MODE_ABSOLUTE_LONG),
    [0x6c] = OPCODE(OPCODE_JMP, OPCODE_CLASS_BRANCH, ADDRESS_MODE_ABSOLUTE_INDIRECT),
    [0x7c] = OPCODE(OPCODE_JMP, OPCODE_CLASS_BRANCH, ADDRESS_MODE_ABSOLUTE_INDEXED_INDIRECT),
    [0xdc] = OPCODE(OPCODE_JML, OPCODE_CLASS_BRANCH, ADDRESS_MODE_ABSOLUTE_INDIRECT),

    [0x22] = OPCODE(OPCODE_JSL, OPCODE_CLASS_BRANCH, ADDRESS_MODE_ABSOLUTE_LONG),

    [0x20] = OPCODE(OPCODE_JSR, OPCODE_CLASS_BRANCH, ADDRESS_MODE_ABSOLUTE),
    [0xfc] = OPCODE(OPCODE_JSR, OPCODE_CLASS_BRANCH, ADDRESS_MODE_ABSOLUTE_INDEXED_INDIRECT),

    [0xa1] = OPCODE(OPCODE_LDA, OPCODE_CLASS_LOAD, ADDRESS_MODE_DIRECT_INDEXED_INDIRECT),
    [0xa3] = OPCODE(OPCODE_LDA, OPCODE_CLASS_LOAD, ADDRESS_MODE_STACK_RELATIVE),
    [0xa5] = OPCODE(OPCODE_LDA, OPCODE_CLASS_LOAD, ADDRESS_MODE_DIRECT),
    [0xa7] = OPCODE(OPCODE_LDA, OPCODE_CLASS_LOAD, ADDRESS_MODE_DIRECT_INDIRECT_LONG),
    [0xa9] = OPCODE(OPCODE_LDA, OPCODE_CLASS_LOAD, ADDRESS_MODE_IMMEDIATE),
    [0xad] = OPCODE(OPCODE_LDA, OPCODE_CLASS_LOAD, ADDRESS_MODE_ABSOLUTE),
    [0xaf] = OPCODE(OPCODE_LDA, OPCODE_CLASS_LOAD, ADDRESS_MODE_ABSOLUTE_LONG),
    [0xb1] = OPCODE(OPCODE_LDA, OPCODE_CLASS_LOAD, ADDRESS_MODE_DIRECT_INDIRECT_INDEXED),
    [0xb2] = OPCODE(OPCODE_LDA, OPCODE_CLASS_LOAD, ADDRESS_MODE_DIRECT_INDIRECT),
    [0xb3] = OPCODE(OPCODE_LDA, OPCODE_CLASS_LOAD, ADDRESS_MODE_STACK_RELATIVE_INDIRECT_INDEXED),
    [0xb5] = OPCODE(OPCODE_LDA, OPCODE_CLASS_LOAD, ADDRESS_MODE_DIRECT_INDEXED_X),
    [0xb7] = OPCODE(OPCODE_LDA, OPCODE_CLASS_LOAD, ADDRESS_MODE_DIRECT_INDIRECT_LONG_INDEXED),
    [0xb9] = OPCODE(OPCODE_LDA, OPCODE_CLASS_LOAD, ADDRESS_MODE_ABSOLUTE_INDEXED_Y),
    [0xbd] = OPCODE(OPCODE_LDA, OPCODE_CLASS_LOAD, ADDRESS_MODE_ABSOLUTE_INDEXED_X),
    [0xbf] = OPCODE(OPCODE_LDA, OPCODE_CLASS_LOAD, ADDRESS_MODE_ABSOLUTE_LONG_INDEXED_X),

    [0xa2] = OPCODE(OPCODE_LDX, OPCODE_CLASS_LOAD, ADDRESS_MODE_IMMEDIATE),
    [0xa6] = OPCODE(OPCODE_LDX, OPCODE_CLASS_LOAD, ADDRESS_MODE_DIRECT),
    [0xae] = OPCODE(OPCODE_LDX, OPCODE_CLASS_LOAD, ADDRESS_MODE_ABSOLUTE),
    [0xb6] = OPCODE(OPCODE_LDX, OPCODE_CLASS_LOAD, ADDRESS_MODE_DIRECT_INDEXED_Y),
    [0xbe] = OPCODE(OPCODE_LDX, OPCODE_CLASS_LOAD, ADDRESS_MODE_ABSOLUTE_INDEXED_Y),

    [0xa0] = OPCODE(OPCODE_LDY, OPCODE_CLASS_LOAD, ADDRESS_MODE_IMMEDIATE),
    [0xa4] = OPCODE(OPCODE_LDY, OPCODE_CLASS_LOAD, ADDRESS_MODE_DIRECT),
    [0xac] = OPCODE(OPCODE_LDY, OPCODE_CLASS_LOAD, ADDRESS_MODE_ABSOLUTE),
    [0xb4] = OPCODE(OPCODE_LDY, OPCODE_CLASS_LOAD, ADDRESS_MODE_DIRECT_INDEXED_X),
    [0xbc] = OPCODE(OPCODE_LDY, OPCODE_CLASS_LOAD, ADDRESS_MODE_ABSOLUTE_INDEXED_Y),

    [0x46] = OPCODE(OPCODE_LSR, OPCODE_CLASS_ALU, ADDRESS_MODE_DIRECT),
    [0x4a] = OPCODE(OPCODE_LSR, OPCODE_CLASS_ALU, ADDRESS_MODE_ACCUMULATOR),
    [0x4e] = OPCODE(OPCODE_LSR, OPCODE_CLASS_ALU, ADDRESS_MODE_ABSOLUTE),
    [0x56] = OPCODE(OPCODE_LSR, OPCODE_CLASS_ALU, ADDRESS_MODE_DIRECT_INDEXED_X),
    [0x5e] = OPCODE(OPCODE_LSR, OPCODE_CLASS_ALU, ADDRESS_MODE_ABSOLUTE_INDEXED_X),

    [0x54] = OPCODE(OPCODE_MVN, OPCODE_CLASS_STANDALONE, ADDRESS_MODE_BLOCK_MOVE),
    [0x44] = OPCODE(OPCODE_MVP, OPCODE_CLASS_STANDALONE, ADDRESS_MODE_BLOCK_MOVE),

    [0xea] = OPCODE(OPCODE_NOP, OPCODE_CLASS_STANDALONE, ADDRESS_MODE_IMPLIED),

    [0x01] = OPCODE(OPCODE_ORA, OPCODE_CLASS_ALU, ADDRESS_MODE_DIRECT_INDEXED_INDIRECT),
    [0x03] = OPCODE(OPCODE_ORA, OPCODE_CLASS_ALU, ADDRESS_MODE_STACK_RELATIVE),
    [0x05] = OPCODE(OPCODE_ORA, OPCODE_CLASS_ALU, ADDRESS_MODE_DIRECT),
    [0x07] = OPCODE(OPCODE_ORA, OPCODE_CLASS_ALU, ADDRESS_MODE_DIRECT_INDIRECT_LONG),
    [0x09] = OPCODE(OPCODE_ORA, OPCODE_CLASS_ALU, ADDRESS_MODE_IMMEDIATE),
    [0x0d] = OPCODE(OPCODE_ORA, OPCODE_CLASS_ALU, ADDRESS_MODE_ABSOLUTE),
    [0x0f] = OPCODE(OPCODE_ORA, OPCODE_CLASS_ALU, ADDRESS_MODE_ABSOLUTE_LONG),
    [0x11] = OPCODE(OPCODE_ORA, OPCODE_CLASS_ALU, ADDRESS_MODE_DIRECT_INDIRECT_INDEXED),
    [0x12] = OPCODE(OPCODE_ORA, OPCODE_CLASS_ALU, ADDRESS_MODE_DIRECT_INDIRECT),
    [0x13] = OPCODE(OPCODE_ORA, OPCODE_CLASS_ALU, ADDRESS_MODE_STACK_RELATIVE_INDIRECT_INDEXED),
    [0x15] = OPCODE(OPCODE_ORA, OPCODE_CLASS_ALU, ADDRESS_MODE_DIRECT_INDEXED_X),
    [0x17] = OPCODE(OPCODE_ORA, OPCODE_CLASS_ALU, ADDRESS_MODE_DIRECT_INDIRECT_LONG_INDEXED),
    [0x19] = OPCODE(OPCODE_ORA, OPCODE_CLASS_ALU, ADDRESS_MODE_ABSOLUTE_INDEXED_Y),
    [0x1d] = OPCODE(OPCODE_ORA, OPCODE_CLASS_ALU, ADDRESS_MODE_ABSOLUTE_INDEXED_X),
    [0x1f] = OPCODE(OPCODE_ORA, OPCODE_CLASS_ALU, ADDRESS_MODE_ABSOLUTE_LONG_INDEXED_X),


    [0xf4] = OPCODE(OPCODE_PEA, OPCODE_CLASS_STACK, ADDRESS_MODE_STACK),
    [0xd4] = OPCODE(OPCODE_PEI, OPCODE_CLASS_STACK, ADDRESS_MODE_STACK),
    [0x62] = OPCODE(OPCODE_PER, OPCODE_CLASS_STACK, ADDRESS_MODE_STACK),
    [0x48] = OPCODE(OPCODE_PHA, OPCODE_CLASS_STACK, ADDRESS_MODE_STACK),
    [0x8b] = OPCODE(OPCODE_PHB, OPCODE_CLASS_STACK, ADDRESS_MODE_STACK),
    [0x0b] = OPCODE(OPCODE_PHD, OPCODE_CLASS_STACK, ADDRESS_MODE_STACK),
    [0x4b] = OPCODE(OPCODE_PHK, OPCODE_CLASS_STACK, ADDRESS_MODE_STACK),
    [0x08] = OPCODE(OPCODE_PHP, OPCODE_CLASS_STACK, ADDRESS_MODE_STACK),
    [0xda] = OPCODE(OPCODE_PHX, OPCODE_CLASS_STACK, ADDRESS_MODE_STACK),
    [0x5a] = OPCODE(OPCODE_PHY, OPCODE_CLASS_STACK, ADDRESS_MODE_STACK),
    [0x68] = OPCODE(OPCODE_PLA, OPCODE_CLASS_STACK, ADDRESS_MODE_STACK),
    [0xab] = OPCODE(OPCODE_PLB, OPCODE_CLASS_STACK, ADDRESS_MODE_STACK),
    [0x2b] = OPCODE(OPCODE_PLD, OPCODE_CLASS_STACK, ADDRESS_MODE_STACK),
    [0x28] = OPCODE(OPCODE_PLP, OPCODE_CLASS_STACK, ADDRESS_MODE_STACK),
    [0xfa] = OPCODE(OPCODE_PLX, OPCODE_CLASS_STACK, ADDRESS_MODE_STACK),
    [0x7a] = OPCODE(OPCODE_PLY, OPCODE_CLASS_STACK, ADDRESS_MODE_STACK),

    [0x26] = OPCODE(OPCODE_ROL, OPCODE_CLASS_ALU, ADDRESS_MODE_DIRECT),
    [0x2a] = OPCODE(OPCODE_ROL, OPCODE_CLASS_ALU, ADDRESS_MODE_ACCUMULATOR),
    [0x2e] = OPCODE(OPCODE_ROL, OPCODE_CLASS_ALU, ADDRESS_MODE_ABSOLUTE),
    [0x36] = OPCODE(OPCODE_ROL, OPCODE_CLASS_ALU, ADDRESS_MODE_DIRECT_INDEXED_X),
    [0x3e] = OPCODE(OPCODE_ROL, OPCODE_CLASS_ALU, ADDRESS_MODE_ABSOLUTE_INDEXED_X),

    [0x66] = OPCODE(OPCODE_ROR, OPCODE_CLASS_ALU, ADDRESS_MODE_DIRECT),
    [0x6a] = OPCODE(OPCODE_ROR, OPCODE_CLASS_ALU, ADDRESS_MODE_ACCUMULATOR),
    [0x6e] = OPCODE(OPCODE_ROR, OPCODE_CLASS_ALU, ADDRESS_MODE_ABSOLUTE),
    [0x76] = OPCODE(OPCODE_ROR, OPCODE_CLASS_ALU, ADDRESS_MODE_DIRECT_INDEXED_X),
    [0x7e] = OPCODE(OPCODE_ROR, OPCODE_CLASS_ALU, ADDRESS_MODE_ABSOLUTE_INDEXED_X),

    [0x40] = OPCODE(OPCODE_RTI, OPCODE_CLASS_BRANCH, ADDRESS_MODE_STACK),
    [0x6b] = OPCODE(OPCODE_RTL, OPCODE_CLASS_BRANCH, ADDRESS_MODE_STACK),
    [0x60] = OPCODE(OPCODE_RTS, OPCODE_CLASS_BRANCH, ADDRESS_MODE_STACK),

    [0xe1] = OPCODE(OPCODE_SBC, OPCODE_CLASS_ALU, ADDRESS_MODE_DIRECT_INDEXED_INDIRECT),
    [0xe3] = OPCODE(OPCODE_SBC, OPCODE_CLASS_ALU, ADDRESS_MODE_STACK_RELATIVE),
    [0xe5] = OPCODE(OPCODE_SBC, OPCODE_CLASS_ALU, ADDRESS_MODE_DIRECT),
    [0xe7] = OPCODE(OPCODE_SBC, OPCODE_CLASS_ALU, ADDRESS_MODE_DIRECT_INDIRECT_LONG),
    [0xe9] = OPCODE(OPCODE_SBC, OPCODE_CLASS_ALU, ADDRESS_MODE_IMMEDIATE),
    [0xed] = OPCODE(OPCODE_SBC, OPCODE_CLASS_ALU, ADDRESS_MODE_ABSOLUTE),
    [0xef] = OPCODE(OPCODE_SBC, OPCODE_CLASS_ALU, ADDRESS_MODE_ABSOLUTE_LONG),
    [0xf1] = OPCODE(OPCODE_SBC, OPCODE_CLASS_ALU, ADDRESS_MODE_DIRECT_INDIRECT_INDEXED),
    [0xf2] = OPCODE(OPCODE_SBC, OPCODE_CLASS_ALU, ADDRESS_MODE_DIRECT_INDIRECT),
    [0xf3] = OPCODE(OPCODE_SBC, OPCODE_CLASS_ALU, ADDRESS_MODE_STACK_RELATIVE_INDIRECT_INDEXED),
    [0xf5] = OPCODE(OPCODE_SBC, OPCODE_CLASS_ALU, ADDRESS_MODE_DIRECT_INDEXED_X),
    [0xf7] = OPCODE(OPCODE_SBC, OPCODE_CLASS_ALU, ADDRESS_MODE_DIRECT_INDIRECT_LONG_INDEXED),
    [0xf9] = OPCODE(OPCODE_SBC, OPCODE_CLASS_ALU, ADDRESS_MODE_ABSOLUTE_INDEXED_Y),
    [0xfd] = OPCODE(OPCODE_SBC, OPCODE_CLASS_ALU, ADDRESS_MODE_ABSOLUTE_INDEXED_X),
    [0xff] = OPCODE(OPCODE_SBC, OPCODE_CLASS_ALU, ADDRESS_MODE_ABSOLUTE_LONG_INDEXED_X),

    [0xdb] = OPCODE(OPCODE_STP, OPCODE_CLASS_STANDALONE, ADDRESS_MODE_IMPLIED),

    [0x81] = OPCODE(OPCODE_STA, OPCODE_CLASS_STORE, ADDRESS_MODE_DIRECT_INDEXED_INDIRECT),
    [0x83] = OPCODE(OPCODE_STA, OPCODE_CLASS_STORE, ADDRESS_MODE_STACK_RELATIVE),
    [0x85] = OPCODE(OPCODE_STA, OPCODE_CLASS_STORE, ADDRESS_MODE_DIRECT),
    [0x87] = OPCODE(OPCODE_STA, OPCODE_CLASS_STORE, ADDRESS_MODE_DIRECT_INDIRECT_LONG),
    [0x8d] = OPCODE(OPCODE_STA, OPCODE_CLASS_STORE, ADDRESS_MODE_ABSOLUTE),
    [0x8f] = OPCODE(OPCODE_STA, OPCODE_CLASS_STORE, ADDRESS_MODE_ABSOLUTE_LONG),
    [0x91] = OPCODE(OPCODE_STA, OPCODE_CLASS_STORE, ADDRESS_MODE_DIRECT_INDIRECT_INDEXED),
    [0x92] = OPCODE(OPCODE_STA, OPCODE_CLASS_STORE, ADDRESS_MODE_DIRECT_INDIRECT),
    [0x93] = OPCODE(OPCODE_STA, OPCODE_CLASS_STORE, ADDRESS_MODE_STACK_RELATIVE_INDIRECT_INDEXED),
    [0x95] = OPCODE(OPCODE_STA, OPCODE_CLASS_STORE, ADDRESS_MODE_DIRECT_INDEXED_X),
    [0x97] = OPCODE(OPCODE_STA, OPCODE_CLASS_STORE, ADDRESS_MODE_DIRECT_INDIRECT_LONG_INDEXED),
    [0x99] = OPCODE(OPCODE_STA, OPCODE_CLASS_STORE, ADDRESS_MODE_ABSOLUTE_INDEXED_Y),
    [0x9d] = OPCODE(OPCODE_STA, OPCODE_CLASS_STORE, ADDRESS_MODE_ABSOLUTE_INDEXED_X),
    [0x9f] = OPCODE(OPCODE_STA, OPCODE_CLASS_STORE, ADDRESS_MODE_ABSOLUTE_LONG_INDEXED_X),

    [0x86] = OPCODE(OPCODE_STX, OPCODE_CLASS_STORE, ADDRESS_MODE_DIRECT),
    [0x8e] = OPCODE(OPCODE_STX, OPCODE_CLASS_STORE, ADDRESS_MODE_ABSOLUTE),
    [0x96] = OPCODE(OPCODE_STX, OPCODE_CLASS_STORE, ADDRESS_MODE_DIRECT_INDEXED_Y),

    [0x84] = OPCODE(OPCODE_STY, OPCODE_CLASS_STORE, ADDRESS_MODE_DIRECT),
    [0x8c] = OPCODE(OPCODE_STY, OPCODE_CLASS_STORE, ADDRESS_MODE_ABSOLUTE),
    [0x94] = OPCODE(OPCODE_STY, OPCODE_CLASS_STORE, ADDRESS_MODE_DIRECT_INDEXED_X),

    [0x64] = OPCODE(OPCODE_STZ, OPCODE_CLASS_STORE, ADDRESS_MODE_DIRECT),
    [0x74] = OPCODE(OPCODE_STZ, OPCODE_CLASS_STORE, ADDRESS_MODE_DIRECT_INDEXED_X),
    [0x9c] = OPCODE(OPCODE_STZ, OPCODE_CLASS_STORE, ADDRESS_MODE_ABSOLUTE),
    [0x9e] = OPCODE(OPCODE_STZ, OPCODE_CLASS_STORE, ADDRESS_MODE_ABSOLUTE_INDEXED_X),

    [0xaa] = OPCODE(OPCODE_TAX, OPCODE_CLASS_TRANSFER, ADDRESS_MODE_IMPLIED),
    [0xa8] = OPCODE(OPCODE_TAY, OPCODE_CLASS_TRANSFER, ADDRESS_MODE_IMPLIED),
    [0x5b] = OPCODE(OPCODE_TCD, OPCODE_CLASS_TRANSFER, ADDRESS_MODE_IMPLIED),
    [0x1b] = OPCODE(OPCODE_TCS, OPCODE_CLASS_TRANSFER, ADDRESS_MODE_IMPLIED),
    [0x7b] = OPCODE(OPCODE_TDC, OPCODE_CLASS_TRANSFER, ADDRESS_MODE_IMPLIED),

    [0x14] = OPCODE(OPCODE_TRB, OPCODE_CLASS_ALU, ADDRESS_MODE_DIRECT),
    [0x1c] = OPCODE(OPCODE_TRB, OPCODE_CLASS_ALU, ADDRESS_MODE_ABSOLUTE),

    [0x04] = OPCODE(OPCODE_TSB, OPCODE_CLASS_ALU, ADDRESS_MODE_DIRECT),
    [0x0c] = OPCODE(OPCODE_TSB, OPCODE_CLASS_ALU, ADDRESS_MODE_ABSOLUTE),

    [0x3b] = OPCODE(OPCODE_TSC, OPCODE_CLASS_TRANSFER, ADDRESS_MODE_ACCUMULATOR),
    [0xba] = OPCODE(OPCODE_TSX, OPCODE_CLASS_TRANSFER, ADDRESS_MODE_ACCUMULATOR),
    [0x8a] = OPCODE(OPCODE_TXA, OPCODE_CLASS_TRANSFER, ADDRESS_MODE_ACCUMULATOR),
    [0x9a] = OPCODE(OPCODE_TXS, OPCODE_CLASS_TRANSFER, ADDRESS_MODE_ACCUMULATOR),
    [0x9b] = OPCODE(OPCODE_TXY, OPCODE_CLASS_TRANSFER, ADDRESS_MODE_ACCUMULATOR),
    [0x98] = OPCODE(OPCODE_TYA, OPCODE_CLASS_TRANSFER, ADDRESS_MODE_ACCUMULATOR),
    [0xbb] = OPCODE(OPCODE_TYX, OPCODE_CLASS_TRANSFER, ADDRESS_MODE_ACCUMULATOR),
    [0xcb] = OPCODE(OPCODE_WAI, OPCODE_CLASS_STANDALONE, ADDRESS_MODE_ACCUMULATOR),
    [0x42] = OPCODE(OPCODE_WDM, OPCODE_CLASS_STANDALONE, ADDRESS_MODE_ACCUMULATOR),
    [0xeb] = OPCODE(OPCODE_XBA, OPCODE_CLASS_STANDALONE, ADDRESS_MODE_ACCUMULATOR),
    [0xfb] = OPCODE(OPCODE_XCE, OPCODE_CLASS_STANDALONE, ADDRESS_MODE_ACCUMULATOR)
};

const char *opcode_strs[] = {
    [OPCODE_BRK] = "BRK",
    [OPCODE_BIT] = "BIT",
    [OPCODE_BCC] = "BCC",
    [OPCODE_BCS] = "BCS",
    [OPCODE_BNE] = "BNE",
    [OPCODE_BEQ] = "BEQ",
    [OPCODE_BPL] = "BPL",
    [OPCODE_BMI] = "BMI",
    [OPCODE_BVC] = "BVC",
    [OPCODE_BVS] = "BVS",
    [OPCODE_BRA] = "BRA",
    [OPCODE_BRL] = "BRL",
    [OPCODE_CLC] = "CLC",
    [OPCODE_CLD] = "CLD",
    [OPCODE_CLV] = "CLV",
    [OPCODE_CLI] = "CLI",
    [OPCODE_CMP] = "CMP",
    [OPCODE_CPX] = "CPX",
    [OPCODE_CPY] = "CPY",
    [OPCODE_ADC] = "ADC",
    [OPCODE_AND] = "AND",
    [OPCODE_SBC] = "SBC",
    [OPCODE_EOR] = "EOR",
    [OPCODE_ORA] = "ORA",
    [OPCODE_ROL] = "ROL",
    [OPCODE_ROR] = "ROR",
    [OPCODE_DEC] = "DEC",
    [OPCODE_INC] = "INC",
    [OPCODE_ASL] = "ASL",
    [OPCODE_LSR] = "LSR",
    [OPCODE_DEX] = "DEX",
    [OPCODE_INX] = "INX",
    [OPCODE_DEY] = "DEY",
    [OPCODE_INY] = "INY",
    [OPCODE_TRB] = "TRB",
    [OPCODE_TSB] = "TSB",
    [OPCODE_JMP] = "JMP",
    [OPCODE_JML] = "JML",
    [OPCODE_JSL] = "JSL",
    [OPCODE_JSR] = "JSR",
    [OPCODE_LDA] = "LDA",
    [OPCODE_LDX] = "LDX",
    [OPCODE_LDY] = "LDY",
    [OPCODE_STA] = "STA",
    [OPCODE_STX] = "STX",
    [OPCODE_STY] = "STY",
    [OPCODE_STZ] = "STZ",
    [OPCODE_MVN] = "MVN",
    [OPCODE_MVP] = "MVP",
    [OPCODE_NOP] = "NOP",
    [OPCODE_PEA] = "PEA",
    [OPCODE_PEI] = "PEI",
    [OPCODE_PER] = "PER",
    [OPCODE_PHA] = "PHA",
    [OPCODE_PHB] = "PHB",
    [OPCODE_PHD] = "PHD",
    [OPCODE_PHK] = "PHK",
    [OPCODE_PHP] = "PHP",
    [OPCODE_PHX] = "PHX",
    [OPCODE_PHY] = "PHY",
    [OPCODE_PLA] = "PLA",
    [OPCODE_PLB] = "PLB",
    [OPCODE_PLD] = "PLD",
    [OPCODE_PLP] = "PLP",
    [OPCODE_PLX] = "PLX",
    [OPCODE_PLY] = "PLY",
    [OPCODE_REP] = "REP",
    [OPCODE_RTI] = "RTI",
    [OPCODE_RTL] = "RTL",
    [OPCODE_RTS] = "RTS",
    [OPCODE_SEP] = "SEP",
    [OPCODE_SEC] = "SEC",
    [OPCODE_SED] = "SED",
    [OPCODE_SEI] = "SEI",
    [OPCODE_TAX] = "TAX",
    [OPCODE_TAY] = "TAY",
    [OPCODE_TCD] = "TCD",
    [OPCODE_TCS] = "TCS",
    [OPCODE_TDC] = "TDC",
    [OPCODE_TSC] = "TSC",
    [OPCODE_TSX] = "TSX",
    [OPCODE_TXA] = "TXA",
    [OPCODE_TXS] = "TXS",
    [OPCODE_TXY] = "TXY",
    [OPCODE_TYA] = "TYA",
    [OPCODE_TYX] = "TYX",
    [OPCODE_WAI] = "WAI",
    [OPCODE_WDM] = "WDM",
    [OPCODE_XBA] = "XBA",
    [OPCODE_STP] = "STP",
    [OPCODE_COP] = "COP",
    [OPCODE_XCE] = "XCE",
    [OPCODE_UNKNOWN] = "UNKNOWN",
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


uint32_t opcode_width(struct opcode_t *opcode)
{
    uint32_t width = 0;
    switch(opcode->address_mode)
    {
        case ADDRESS_MODE_ABSOLUTE:
        case ADDRESS_MODE_ABSOLUTE_INDEXED_INDIRECT:
        case ADDRESS_MODE_ABSOLUTE_INDEXED_X:
        case ADDRESS_MODE_ABSOLUTE_INDEXED_Y:
        case ADDRESS_MODE_BLOCK_MOVE:
        case ADDRESS_MODE_PROGRAM_COUNTER_RELATIVE_LONG:
            width = 3;
        break;

        case ADDRESS_MODE_ABSOLUTE_INDIRECT:
            if(opcode->opcode == OPCODE_JML)
            {
                width = 4;
            }
            else
            {
                width = 3;
            }
        break;

        case ADDRESS_MODE_ABSOLUTE_LONG:
            width = 4;
        break;

        case ADDRESS_MODE_ACCUMULATOR:
        case ADDRESS_MODE_IMPLIED:
        case ADDRESS_MODE_STACK:
            width = 1;
        break;

        case ADDRESS_MODE_DIRECT_INDEXED_INDIRECT:
        case ADDRESS_MODE_DIRECT_INDEXED_X:
        case ADDRESS_MODE_DIRECT_INDEXED_Y:
        case ADDRESS_MODE_DIRECT_INDIRECT_INDEXED:
        case ADDRESS_MODE_DIRECT_INDIRECT_LONG_INDEXED:
        case ADDRESS_MODE_DIRECT_INDIRECT_LONG:
        case ADDRESS_MODE_DIRECT_INDIRECT:
        case ADDRESS_MODE_DIRECT:
        case ADDRESS_MODE_PROGRAM_COUNTER_RELATIVE:
        case ADDRESS_MODE_STACK_RELATIVE:
        case ADDRESS_MODE_STACK_RELATIVE_INDIRECT_INDEXED:
            width = 2;
        break;

        case ADDRESS_MODE_IMMEDIATE:
            switch(opcode->opcode)
            {
                case OPCODE_LDA:
                case OPCODE_CMP:
                case OPCODE_AND:
                case OPCODE_EOR:
                case OPCODE_ORA:
                case OPCODE_ADC:
                case OPCODE_BIT:
                case OPCODE_SBC:
                case OPCODE_ROR:
                case OPCODE_ROL:
                case OPCODE_LSR:
                case OPCODE_ASL:
                    if(cpu_state.reg_p.m)
                    {
                        width = 2;
                    }
                    else
                    {
                        width = 3;
                    }
                break;

                case OPCODE_LDX:
                case OPCODE_LDY:
                case OPCODE_CPX:
                case OPCODE_CPY:
                    if(cpu_state.reg_p.x)
                    {
                        width = 2;
                    }
                    else
                    {
                        width = 3;
                    }
                break;

                default:
                    width = 2;
                break;
            }
        break;
    }

    return width;
}

char instruction_str_buffer[512];

char *instruction_str(uint32_t effective_address)
{
//    char *opcode_str;
    const char *opcode_str;
    char addr_mode_str[128];
    char temp_str[64];
    int32_t width = 0;
    struct opcode_t opcode;

    opcode = opcode_matrix[read_byte(effective_address)];
    width = opcode_width(&opcode);

    switch(opcode.address_mode)
    {
        case ADDRESS_MODE_ABSOLUTE:
            strcpy(addr_mode_str, "absolute addr (");
            sprintf(temp_str, "DBR(%02x):", cpu_state.regs[REG_DBR].word);
            strcat(addr_mode_str, temp_str);
            // for(int32_t index = width - 1; index > 0; index--)
            // {
            sprintf(temp_str, "%04x", read_word(effective_address + 1));
            strcat(addr_mode_str, temp_str);
            // }
            strcat(addr_mode_str, ")");
        break;

        case ADDRESS_MODE_ABSOLUTE_INDEXED_INDIRECT:
        {
            strcpy(addr_mode_str, "absolute addr ( pointer (");
            strcat(addr_mode_str, "00:");
            uint32_t pointer = read_word(effective_address + 1);
            // for(int32_t index = width - 1; index > 0; index--)
            // {
                // sprintf(temp_str, "%02x", read_byte(effective_address + index));
            sprintf(temp_str, "%04x", pointer);
            strcat(addr_mode_str, temp_str);
            // }

            strcat(addr_mode_str, ") + ");
            sprintf(temp_str, "X(%04x) ) = ", cpu_state.regs[REG_X].word);
            strcat(addr_mode_str, temp_str);
            sprintf(temp_str, "(%04x)", read_word(pointer));
            strcat(addr_mode_str, temp_str);
        }
        break;

        case ADDRESS_MODE_ABSOLUTE_INDEXED_X:
            strcpy(addr_mode_str, "absolute addr (");
            sprintf(temp_str, "DBR(%02x):", cpu_state.regs[REG_DBR].word);
            strcat(addr_mode_str, temp_str);
            // for(int32_t index = width - 1; index > 0; index--)
            // {
            sprintf(temp_str, "%04x", read_word(effective_address + 1));
            strcat(addr_mode_str, temp_str);
            // }

            strcat(addr_mode_str, ") + ");
            sprintf(temp_str, "X(%04x) )", cpu_state.regs[REG_X].word);
            strcat(addr_mode_str, temp_str);
        break;

        case ADDRESS_MODE_ABSOLUTE_INDEXED_Y:
            strcpy(addr_mode_str, "absolute addr (");
            sprintf(temp_str, "DBR(%02x):", cpu_state.regs[REG_DBR].word);
            strcat(addr_mode_str, temp_str);
            // for(int32_t index = width - 1; index > 0; index--)
            // {
            sprintf(temp_str, "%04x", read_word(effective_address + 1));
            strcat(addr_mode_str, temp_str);
            // }

            strcat(addr_mode_str, ") + ");
            sprintf(temp_str, "Y(%04x)", cpu_state.regs[REG_Y].word);
            strcat(addr_mode_str, temp_str);
        break;

        case ADDRESS_MODE_ABSOLUTE_INDIRECT:
        {
            strcpy(addr_mode_str, "absolute addr ( pointer (");
            uint32_t pointer = read_word(effective_address + 1);
            // for(int32_t index = width - 1; index > 0; index--)
            // {
            sprintf(temp_str, "%04x", pointer);
            strcat(addr_mode_str, temp_str);
            // }
            strcat(addr_mode_str, ") ) = ");
            sprintf(temp_str, "(%04x)", read_word(pointer));
            strcat(addr_mode_str, temp_str);
        }
        break;

        case ADDRESS_MODE_ABSOLUTE_LONG_INDEXED_X:
        {
            strcpy(addr_mode_str, "absolute long addr (");
            uint32_t pointer = read_word(effective_address + 1);
            pointer |= (uint32_t)read_byte(effective_address + 3) << 16;
            // for(int32_t index = width - 1; index > 0; index--)
            // {
            sprintf(temp_str, "%06x", pointer);
            strcat(addr_mode_str, temp_str);
            // }

            strcat(addr_mode_str, ") + ");
            sprintf(temp_str, "X(%04x)", cpu_state.regs[REG_X].word);
            strcat(addr_mode_str, temp_str);
        }
        break;

        case ADDRESS_MODE_ABSOLUTE_LONG:
        {
            strcpy(addr_mode_str, "absolute long addr (");
            uint32_t pointer = read_word(effective_address + 1);
            pointer |= (uint32_t)read_byte(effective_address + 3) << 16;

            // for(int32_t index = width - 1; index > 0; index--)
            // {
            sprintf(temp_str, "%06x", pointer);
            strcat(addr_mode_str, temp_str);
            // }

            strcat(addr_mode_str, ")");
        }
        break;

        case ADDRESS_MODE_ACCUMULATOR:
            addr_mode_str[0] = '\0';
            if(opcode.opcode != OPCODE_XCE && opcode.opcode != OPCODE_TXS && opcode.opcode != OPCODE_WDM)
            {
                sprintf(addr_mode_str, "accumulator(%04x)", cpu_state.regs[REG_ACCUM].word);
            }
        break;

        case ADDRESS_MODE_BLOCK_MOVE:
            sprintf(addr_mode_str, "dst addr(%02x:%04x), src addr(%02x:%04x)", read_byte(effective_address + 1), cpu_state.regs[REG_Y].word,
                                                                               read_byte(effective_address + 2), cpu_state.regs[REG_X].word);
        break;

        case ADDRESS_MODE_DIRECT_INDEXED_INDIRECT:
            strcpy(addr_mode_str, "direct indexed indirect - (d,x)");
        break;

        case ADDRESS_MODE_DIRECT_INDEXED_X:
            sprintf(addr_mode_str, "D(%04x) + X(%04x)", cpu_state.regs[REG_D].word, cpu_state.regs[REG_X].word);
        break;

        case ADDRESS_MODE_DIRECT_INDEXED_Y:
            sprintf(addr_mode_str, "D(%04x) + Y(%04x)", cpu_state.regs[REG_D].word, cpu_state.regs[REG_Y].word);
        break;

        case ADDRESS_MODE_DIRECT_INDIRECT_INDEXED:
            uint8_t offset = read_byte(effective_address + 1);
            uint16_t mem_address = read_word(cpu_state.regs[REG_D].word + offset);
            sprintf(addr_mode_str, "[pointer(D(%04x) + offset(%02x))](%04x) + Y(%04x)", cpu_state.regs[REG_D].word, offset,
                                                                                        mem_address, cpu_state.regs[REG_Y].word);
        break;

        case ADDRESS_MODE_DIRECT_INDIRECT_LONG_INDEXED:
            strcpy(addr_mode_str, "direct indirect long indexed - [d],y");
        break;

        case ADDRESS_MODE_DIRECT_INDIRECT_LONG:
            strcpy(addr_mode_str, "direct indirect long - [d]");
        break;

        case ADDRESS_MODE_DIRECT_INDIRECT:
            sprintf(addr_mode_str, "pointer(D(%04x) + offset(%02x))", cpu_state.regs[REG_D].word, read_byte(effective_address + 1));
        break;

        case ADDRESS_MODE_DIRECT:
            sprintf(addr_mode_str, "D(%04x) + offset(%02x)", cpu_state.regs[REG_D].word, read_byte(effective_address + 1));
        break;

        case ADDRESS_MODE_IMMEDIATE:
            strcpy(addr_mode_str, "immediate (");

            for(int32_t index = width - 1; index > 0; index--)
            {
                sprintf(temp_str, "%02x", read_byte(effective_address + index));
                strcat(addr_mode_str, temp_str);
            }
            strcat(addr_mode_str, ")");
        break;

        case ADDRESS_MODE_IMPLIED:
            strcpy(addr_mode_str, "");
        break;

        case ADDRESS_MODE_PROGRAM_COUNTER_RELATIVE_LONG:
            strcpy(addr_mode_str, "program counter relative long - rl");
        break;

        case ADDRESS_MODE_PROGRAM_COUNTER_RELATIVE:
            sprintf(addr_mode_str, "pc(%04x) + offset(%02x)", cpu_state.regs[REG_PC].word, read_byte(effective_address + 1));
        break;

        case ADDRESS_MODE_STACK:
            addr_mode_str[0] = '\0';
            if(opcode.opcode == OPCODE_PEA)
            {
                sprintf(temp_str, "immediate (%04x)", read_word(effective_address + 1));
                strcat(addr_mode_str, temp_str);
            }
        break;

        case ADDRESS_MODE_STACK_RELATIVE:
            sprintf(addr_mode_str, "S(%04x) + offset(%02x)", cpu_state.regs[REG_S].word, read_byte(effective_address + 1));
        break;

        case ADDRESS_MODE_STACK_RELATIVE_INDIRECT_INDEXED:
            strcpy(addr_mode_str, "stack relative indirect indexed - (d,s),y");
        break;

        default:
        case ADDRESS_MODE_UNKNOWN:
            strcpy(addr_mode_str, "unknown");
        break;
    }

    opcode_str = opcode_strs[opcode.opcode];

    sprintf(instruction_str_buffer, "[%02x:%04x]: ", (effective_address >> 16) & 0xff, effective_address & 0xffff);

    for(int32_t i = 0; i < width; i++)
    {
        sprintf(temp_str, "%02x", read_byte(effective_address + i));
        strcat(instruction_str_buffer, temp_str);
    }

    strcat(instruction_str_buffer, "; ");
    strcat(instruction_str_buffer, opcode_str);

    if(addr_mode_str[0])
    {
        strcat(instruction_str_buffer, " ");
        strcat(instruction_str_buffer, addr_mode_str);
    }

    return instruction_str_buffer;
}

int dump_cpu(int show_registers)
{
    uint32_t address;
    int opcode_offset;
    struct opcode_t opcode;
//    unsigned char opcode_byte;
//    int i;
    char *op_str;
    unsigned char *opcode_address;

    // address = EFFECTIVE_ADDRESS(cpu_state.regs[REG_PBR].byte[0], cpu_state.regs[REG_PC].word);
    address = cpu_state.instruction_address;
//    opcode_address = memory_pointer(address);
//    opcode = opcode_matrix[opcode_address[0]];
    op_str = instruction_str(address);

    printf("===================== CPU ========================\n");

    if(show_registers)
    {
        // printf("=========== REGISTERS ===========\n");
        printf("[A: %04x] | ", cpu_state.regs[REG_ACCUM].word);
        printf("[X: %04x] | ", cpu_state.regs[REG_X].word);
        printf("[Y: %04x] | ", cpu_state.regs[REG_Y].word);
        printf("[S: %04x] | ", cpu_state.regs[REG_S].word);
        printf("[D: %04x]\n", cpu_state.regs[REG_D].word);
        printf("[DBR: %02x] | ", cpu_state.regs[REG_DBR].byte[0]);
        printf("[PBR: %02x] | ", cpu_state.regs[REG_PBR].byte[0]);

        printf("[P: N:%d V:%d ", cpu_state.reg_p.n, cpu_state.reg_p.v);

        if(cpu_state.reg_p.e)
        {
            printf("B:%d ", cpu_state.reg_p.b);
        }
        else
        {
            printf("M:%d X:%d ", cpu_state.reg_p.m, cpu_state.reg_p.x);
        }

        printf("D:%d Z:%d I:%d C:%d]\n", cpu_state.reg_p.d, cpu_state.reg_p.z, cpu_state.reg_p.i, cpu_state.reg_p.c);
        printf("  [E: %02x] | [PC: %04x]", cpu_state.reg_p.e, cpu_state.regs[REG_PC].word);

        if(cpu_state.regs[REG_INST].byte[0] == BRK_S)
        {
            char *interrupt_str = "";
            switch(cpu_state.cur_interrupt)
            {
                case CPU_INT_BRK:
                    interrupt_str = "BRK";
                break;

                case CPU_INT_IRQ:
                    interrupt_str = "IRQ";
                break;

                case CPU_INT_NMI:
                    interrupt_str = "NMI";
                break;
            }

            printf(" | handling %s", interrupt_str);
        }
        printf("\n");

        // printf("=========== REGISTERS ===========\n");
        printf("-------------------------------------\n");
    }

    printf("%s\n", op_str);

    printf("\n");

    return 0;
}

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

        return mem_speed_map[index][bank_region2 && (ram1_regs[CPU_REG_MEMSEL] & 1)];
    }
    else if(ram1_regs[CPU_REG_MEMSEL] & 1)
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
    if(!cpu_state.wai)
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
    cpu_state.regs[REG_PC].word = read_word(0xfffc);
    cpu_state.regs[REG_DBR].byte[0] = 0;
    cpu_state.regs[REG_PBR].byte[0] = 0;
    cpu_state.regs[REG_D].word = 0;
    cpu_state.regs[REG_X].byte[1] = 0;
    cpu_state.regs[REG_Y].byte[1] = 0;
    cpu_state.regs[REG_S].byte[1] = 0x01;
    cpu_state.regs[REG_ZERO].word = 0;
    cpu_state.reg_p.e = 1;
    cpu_state.reg_p.i = 1;
    cpu_state.reg_p.d = 0;
    cpu_state.reg_p.x = 1;
    cpu_state.reg_p.m = 1;

    // cpu_state.in_rdy = 1;
    // cpu_state.in_irqb = 1;
    // cpu_state.in_nmib = 1;
    // cpu_state.in

    // cpu_state.pins[CPU_PIN_IRQ] = 1;
    // cpu_state.pins[CPU_PIN_NMI] = 1;
    // cpu_state.pins[CPU_PIN_RDY] = 1;

    cpu_state.interrupts[CPU_INT_BRK] = 0;
    cpu_state.interrupts[CPU_INT_IRQ] = 0;
    cpu_state.interrupts[CPU_INT_NMI] = 0;
    cpu_state.nmi0 = 0;
    cpu_state.nmi1 = 0;
    cpu_state.irq = 0;
    cpu_state.rdy = 1;

    cpu_state.cur_interrupt = CPU_INT_BRK;

    cpu_state.instruction_address = EFFECTIVE_ADDRESS(cpu_state.regs[REG_PBR].byte[0], cpu_state.regs[REG_PC].word);
    cpu_state.regs[REG_INST].word = FETCH;
    load_instruction();
//    cpu_state.instruction = instructions + FETCH;
//    cpu_state.cur_uop_index = 0;
//    cpu_state.cur_uop = cpu_state.instruction->uops + cpu_state.cur_uop_index;

    ram1_regs[CPU_REG_MEMSEL] = 0;
    ram1_regs[CPU_REG_TIMEUP] = 0;
    ram1_regs[CPU_REG_HTIMEL] = 0xff;
    ram1_regs[CPU_REG_HTIMEH] = 0x01;
    ram1_regs[CPU_REG_VTIMEL] = 0xff;
    ram1_regs[CPU_REG_VTIMEH] = 0x01;
    ram1_regs[CPU_REG_MDMAEN] = 0;
    ram1_regs[CPU_REG_HDMAEN] = 0;
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
    cpu_state.instruction = instructions + cpu_state.regs[REG_INST].word;
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

uint32_t step_cpu(int32_t cycle_count)
{
    uint32_t end_of_instruction = 0;
    if(cpu_state.rdy)
    {
        cpu_state.uop_cycles += cycle_count;
        cpu_state.instruction_cycles += cycle_count;

        while(cpu_state.uop->func)
        {
            if(!cpu_state.uop->func(cpu_state.uop->arg))
            {
                break;
            }

            if(cpu_state.reg_p.e)
            {
                cpu_state.reg_p.m = 1;
                cpu_state.reg_p.x = 1;
                cpu_state.regs[REG_S].byte[1] = 0x01;
            }

            if(cpu_state.reg_p.x)
            {
                cpu_state.regs[REG_X].byte[1] = 0;
                cpu_state.regs[REG_Y].byte[1] = 0;
            }

            if(cpu_state.last_uop)
            {
                if(cpu_state.irq && !cpu_state.reg_p.i && !cpu_state.interrupts[CPU_INT_NMI])
                {
                    cpu_state.interrupts[CPU_INT_IRQ] = 1;
                }
            }

            cpu_state.reg_p.dl = cpu_state.regs[REG_D].byte[0] != 0;
            cpu_state.reg_p.am = cpu_state.regs[REG_ACCUM].word == 0xffff;
            next_uop();
        }
    }
    else if(cpu_state.wai)
    {
        if(cpu_state.irq && !cpu_state.reg_p.i && !cpu_state.interrupts[CPU_INT_NMI])
        {
            cpu_state.interrupts[CPU_INT_IRQ] = 1;
        }
    }

    if(!cpu_state.uop->func)
    {
        if(cpu_state.interrupts[CPU_INT_NMI])
        {
            cpu_state.rdy = 1;
            cpu_state.regs[REG_INST].word = BRK_S;
            cpu_state.cur_interrupt = CPU_INT_NMI;
            cpu_state.interrupts[CPU_INT_NMI] = 0;
        }
        else if(cpu_state.interrupts[CPU_INT_IRQ])
        {
            if(cpu_state.wai)
            {
                cpu_state.wai = 0;
                cpu_state.rdy = 1;
                cpu_state.interrupts[CPU_INT_IRQ] = 0;
                cpu_state.regs[REG_INST].word = FETCH;
                cpu_state.cur_interrupt = CPU_INT_BRK;
            }
            else
            {
                cpu_state.regs[REG_INST].word = BRK_S;
                cpu_state.cur_interrupt = CPU_INT_IRQ;
            }
            cpu_state.interrupts[CPU_INT_IRQ] = 0;
        }
        else
        {
            cpu_state.regs[REG_INST].word = FETCH;
            cpu_state.cur_interrupt = CPU_INT_BRK;
        }

        if(!cpu_state.wai)
        {
            cpu_state.interrupts[cpu_state.cur_interrupt] = 0;
            cpu_state.instruction_cycles = cpu_state.uop_cycles;
            cpu_state.instruction_address = EFFECTIVE_ADDRESS(cpu_state.regs[REG_PBR].byte[0], cpu_state.regs[REG_PC].word);
            cpu_state.uop_index = 0;
            load_instruction();
        }

        end_of_instruction = 1;
    }

    return end_of_instruction;
}

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
    ram1_regs[CPU_REG_NMITIMEN] = value;

    if(value & (CPU_NMITIMEN_FLAG_HTIMER_EN | CPU_NMITIMEN_FLAG_VTIMER_EN))
    {
        update_irq_counter();
    }
    else
    {
        ram1_regs[CPU_REG_TIMEUP] = 0;
    }
}

uint8_t timeup_read(uint32_t effective_address)
{
    uint8_t value = ram1_regs[CPU_REG_TIMEUP];
    ram1_regs[CPU_REG_TIMEUP] = 0;
    /* bits 6-0 are open bus, so we put the last thing that was on the data bus in them */
    return value | (last_bus_value & 0x7f);
}

uint8_t rdnmi_read(uint32_t effective_address)
{
    uint8_t value = ram1_regs[CPU_REG_RDNMI];
    ram1_regs[CPU_REG_RDNMI] &= ~CPU_RDNMI_BLANK_NMI;
    /* bits 6-4 are open bus, so we put the last thing that was on the data bus in them */
    return value | (last_bus_value & 0x70);
}

uint8_t hvbjoy_read(uint32_t effective_address)
{
    uint8_t value = ram1_regs[CPU_REG_HVBJOY];
    /* bits 5-1 are open bus, so we put the last thing that was on the data bus in them */
    return value | (last_bus_value & 0x3e);
}










