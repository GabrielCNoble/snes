#ifndef APU_H
#define APU_H
#include <stdint.h>

struct apu_io_reg_t
{
    uint16_t read;
    uint16_t write;
};

enum APU_MEM_REGS
{
    APU_MEM_REG_IO0     = 0x2140,
    APU_MEM_REG_IO1     = 0x2141,
    APU_MEM_REG_IO2     = 0x2142,
    APU_MEM_REG_IO3     = 0x2143,
};

enum APU_STATES
{
    APU_STATE_IDLE,
    APU_STATE_START_TRANSFER,
    // APU_STATE_WAITING_DATA,
    // APU_STATE_DATA_RECEIVED,
    APU_STATE_TRANSFER,
    APU_STATE_END_TRANSFER,
};

enum APU_STATUS_FLAGS
{
    APU_STATUS_FLAG_C,
    APU_STATUS_FLAG_Z,
    APU_STATUS_FLAG_I,
    APU_STATUS_FLAG_H,
    APU_STATUS_FLAG_B,
    APU_STATUS_FLAG_P,
    APU_STATUS_FLAG_V,
    APU_STATUS_FLAG_N,
    APU_STATUS_FLAG_LAST
};

enum APU_INSTS
{
    APU_INST_ADC,
    APU_INST_ADDW,
    APU_INST_AND,
    APU_INST_AND1,
    APU_INST_ASL,
    APU_INST_BBC,
    APU_INST_BBS,
    APU_INST_BCC,
    APU_INST_BCS,
    APU_INST_BEQ,
    APU_INST_BMI,
    APU_INST_BNE,
    APU_INST_BPL,
    APU_INST_BVC,
    APU_INST_BVS,
    APU_INST_BRA,
    APU_INST_BRK,
    APU_INST_CALL,
    APU_INST_CBNE,
    APU_INST_CLR1,
    APU_INST_CLRC,
    APU_INST_CLRP,
    APU_INST_CLRV,
    APU_INST_CMP,
    APU_INST_CMPW,
    APU_INST_DAA,
    APU_INST_DAS,
    APU_INST_DBNZ,
    APU_INST_DEC,
    APU_INST_DECW,
    APU_INST_DI,
    APU_INST_DIV,
    APU_ISNT_EI,
    APU_INST_EOR,
    APU_INST_EOR1,
    APU_INST_INC,
    APU_INST_JMP,
    APU_INST_LSR,
    APU_INST_MOV,
    APU_INST_MOVW,
    APU_INST_MUL,
    APU_INST_NOP,
    APU_INST_NOT1,
    APU_INST_NOTC,
    APU_INST_OR,
    APU_INST_OR1,
    APU_INST_PCALL,
    APU_INST_POP,
    APU_INST_PUSH,
    APU_INST_RET,
    APU_INST_RETI,
    APU_INST_ROL,
    APU_INST_ROR,
    APU_INST_SBC,
    APU_INST_SET1,
    APU_INST_SETC,
    APU_INST_SETP,
    APU_INST_SLEEP,
    APU_INST_STOP,
    APU_INST_TCALL,
    APU_INST_TCLR1,
    APU_INST_TSET1,
    APU_INST_XCN
};

/* 
    #IMM8   - immediate 8 bit value present in the program stream, following the opcode byte 
    #IMM16  - immediate 16 bit value present in the programs stream, following the opcode byte
    #SIMM8  - source immediate 8 bit value present in the program stream, somewhere after opcode byte
    #DIMM8  - destination immediate 8 bit value present in the program stream, somewhere after opcode byte
    []      - x86-style indirection
*/
enum APU_OPCODES
{
    /******************* ADC *******************/
    /* [X] = [X] + [Y] + C */
    APU_OPCODE_ADC_X_IND_Y_IND          = 0x99,

    /* A = A + #IMM8 + C */
    APU_OPCODE_ADC_A_IMM8               = 0x88,

    /* A = A + [X] + C */
    APU_OPCODE_ADC_A_X_IND              = 0x86,

    /* A = A + [[#IMM8] + Y] + C */
    APU_OPCODE_ADC_A_IMM8_IND_Y_IND     = 0x97,

    /* A = A + [[#IMM8 + X]] + C */
    APU_OPCODE_ADC_A_IMM8_X_IND_IND     = 0x87,

    /* A = A + [#IMM8] + C*/
    APU_OPCODE_ADC_A_IMM8_IND           = 0x84,

    /* A = A + [#IMM8 + X] + C */
    APU_OPCODE_ADC_A_IMM8_X_IND         = 0x94,

    /* A = A + [#IMM16] + C */
    APU_OPCODE_ADC_A_IMM16_IND          = 0x85,

    /* A = A + [#IMM16 + X] + C */
    APU_OPCODE_ADC_A_IMM16_X_IND        = 0x95,

    /* A = A + [#IMM16 + Y] + C */
    APU_OPCODE_ADC_A_IMM16_Y_IND        = 0x96,

    /* [#DIMM8] = [#DIMM] + [#SIMM8] + C */
    APU_OPCODE_ADC_DIMM8_IND_SIMM8_IND  = 0x89,

    /* [D] = [D] + #IMM8 + C */
    APU_OPCODE_ADC_DIMM8_IND_IMM8       = 0x98,

    /* YA = YA + [#IMM8] + C */
    APU_OPCODE_ADDW_YA_IMM8_IND         = 0x7a,


    /******************* AND *******************/
    /* [X] = [X] & [Y] */
    APU_OPCODE_AND_X_IND_Y_IND          = 0x39,
    /* A = A & #IMM8 */
    APU_OPCODE_AND_A_IMM8               = 0x28,
    /* A = A & [X] */
    APU_OPCODE_AND_A_X_IND              = 0x26,
    /* A = A & [[#IMM8] + Y] */
    APU_OPCODE_AND_A_IMM8_IND_Y_IND     = 0x37,
    /* A = A & [[#IMM8 + X]] */
    APU_OPCODE_AND_A_IMM8_X_IND_IND     = 0x27,
    /* A = A & [#IMM8] */
    APU_OPCODE_AND_A_IMM8_IND           = 0x24,
    APU_OPCODE_AND_A_IMM8_X_IND         = 0x34,
    APU_OPCODE_AND_A_IMM16_IND          = 0x25,
    APU_OPCODE_AND_A_IMM16_X_IND        = 0x35,
    APU_OPCODE_AND_A_IMM16_Y_IND        = 0x36,
    APU_OPCODE_AND_DIMM8_IND_SIMM8_IND  = 0x29,
    APU_OPCODE_AND_DMM8_IND_IMM8        = 0x38,
    
    APU_OPCODE_AND1_N                   = 0x6a,
    APU_OPCODE_AND1                     = 0x4a,

    /******************* ASL *******************/
    APU_OPCODE_ASL_A                    = 0x1c,
    APU_OPCODE_ASL_IMM8_IND             = 0x0b,
    APU_OPCODE_ASL_IMM8_X_IND           = 0x1b,
    APU_OPCODE_ASL_IMM16_IND            = 0x0c,

    /******************* BBC *******************/
    APU_OPCODE_BBC0                     = 0x13,
    APU_OPCODE_BBC1                     = 0x33,
    APU_OPCODE_BBC2                     = 0x53,
    APU_OPCODE_BBC3                     = 0x73,
    APU_OPCODE_BBC4                     = 0x93,
    APU_OPCODE_BBC5                     = 0xb3,
    APU_OPCODE_BBC6                     = 0xd3,
    APU_OPCODE_BBC7                     = 0xf3,

    /******************* BBC *******************/
    APU_OPCODE_BBS0                     = 0x03,
    APU_OPCODE_BBS1                     = 0x23,
    APU_OPCODE_BBS2                     = 0x43,
    APU_OPCODE_BBS3                     = 0x63,
    APU_OPCODE_BBS4                     = 0x83,
    APU_OPCODE_BBS5                     = 0xa3,
    APU_OPCODE_BBS6                     = 0xc3,
    APU_OPCODE_BBS7                     = 0xe3,

    /******************* BCC *******************/
    APU_OPCODE_BCC_IMM8                 = 0x90,

    /******************* BCS *******************/
    APU_OPCODE_BCS_IMM8                 = 0xb0,

    /******************* BEQ *******************/
    APU_OPCODE_BEQ_IMM8                 = 0xf0,

    /******************* BMI *******************/
    APU_OPCODE_BMI_IMM8                 = 0x30,

    /******************* BNE *******************/
    APU_OPCODE_BNE_IMM8                 = 0xd0,

    /******************* BPL *******************/
    APU_OPCODE_BPL_IMM8                 = 0x10,

    /******************* BVC *******************/
    APU_OPCODE_BVC_IMM8                 = 0x50,

    /******************* BVS *******************/
    APU_OPCODE_BVS_IMM8                 = 0x70,

    /******************* BRA *******************/
    APU_OPCODE_BRA_IMM8                 = 0x2f,

    /******************* BRK *******************/
    APU_OPCODE_BRK                      = 0x0f,

    /******************* CALL *******************/
    APU_OPCODE_CALL_IMM16               = 0x3f,

    /******************* CNBE *******************/
    APU_OPCODE_CNBE_IMM8_X_IND          = 0xde,
    APU_OPCODE_CNBE_IMM8_IND            = 0xde,
    
    /******************* CLR1 *******************/
    APU_OPCODE_CLR10_IMM8_IND           = 0x12,
    APU_OPCODE_CLR11_IMM8_IND           = 0x32,
    APU_OPCODE_CLR12_IMM8_IND           = 0x52,
    APU_OPCODE_CLR13_IMM8_IND           = 0x72,
    APU_OPCODE_CLR14_IMM8_IND           = 0x92,
    APU_OPCODE_CLR15_IMM8_IND           = 0xb2,
    APU_OPCODE_CLR16_IMM8_IND           = 0xd2,
    APU_OPCODE_CLR17_IMM8_IND           = 0xf2,

    /******************* CLRC *******************/
    APU_OPCODE_CLRC_IMP                 = 0x60,

    /******************* CLRP *******************/
    APU_OPCODE_CLRP_IMP                 = 0x20,

    /******************* CLRV *******************/
    APU_OPCODE_CLRV_IMP                 = 0xe0,

    /******************* CMP *******************/
    /* [X] - [Y] */
    APU_OPCODE_CMP_X_IND_Y_IND          = 0x79,

    /* A - #IMM8 */
    APU_OPCODE_CMP_A_IMM8               = 0x68,

    /* A - [X] */
    APU_OPCODE_CMP_A_X_IND              = 0x66,

    /* A - [[#IMM8] + Y] */
    APU_OPCODE_CMP_A_IMM8_IND_Y_IND     = 0x77,

    /* A - [[#IMM8 + X]] */
    APU_OPCODE_CMP_A_IMM8_X_IND_IND     = 0x67,

    /* A - [#IMM8] */
    APU_OPCODE_CMP_A_IMM8_IND           = 0x64,

    /* A - [#IMM8 + X] */
    APU_OPCODE_CMP_A_IMM8_X_IND         = 0x74,

    /* A - [#IMM16] */
    APU_OPCODE_CMP_A_IMM16_IND          = 0x65,

    /* A - [#IMM16 + X] */
    APU_OPCODE_CMP_A_IMM16_X_IND        = 0x75,

    /* A - [#IMM16 + Y] */
    APU_OPCODE_CMP_A_IMM16_Y_IND        = 0x76,

    /* X - #IMM8 */
    APU_OPCODE_CMP_X_IMM8               = 0xc8,

    /* X - [#IMM8] */
    APU_OPCODE_CMP_X_IMM8_IND           = 0x3e,

    /* X - [#IMM16] */
    APU_OPCODE_CMP_X_IMM16_IND          = 0x1e,

    /* Y - #IMM8 */
    APU_OPCODE_CMP_Y_IMM8               = 0xad,

    /* Y - [#IMM8] */
    APU_OPCODE_CMP_Y_IMM8_IND           = 0x7e,

    /* Y - [#IMM16] */
    APU_OPCODE_CMP_Y_IMM16_IND          = 0x5e,

    /* [#DIMM8] - [#SIMM8] */
    APU_OPCODE_CMP_DIMM8_IND_SIMM8_IND  = 0x69,

    /* [#IMM8] - #IMM8 */
    APU_OPCODE_CMP_IMM8_IND_IMM8        = 0x78,

    /* YA - [#IMM8] */
    APU_OPCODE_CMPW_YA_IMM8_IND         = 0x5a,

    /******************* DAA *******************/
    APU_OPCODE_DAA_IMP                  = 0xdf,

    /******************* DAS *******************/
    APU_OPCODE_DAS_IMP                  = 0xbe


    // APU_OPCODE_MOV_A_IMM            = 0xe8,
    // APU_OPCODE_MOV_A_X_IND          = 0xe6,
    // APU_OPCODE_MOV_A_X_IND_INC      = 0xbf,
    // APU_OPCODE_MOV_A_DP             = 0xe4,
    // APU_OPCODE_MOV_A_DP_X           = 0xf4,
    // APU_OPCODE_MOV_A_ABS            = 0xe5,
    // APU_OPCODE_MOV_A_ABS_X          = 0xf5,
    // APU_OPCODE_MOV_A_ABS_Y          = 0xf6,
    // APU_OPCODE_MOV_A_DP_X_IND       =
};

enum APU_REGS
{
    APU_REG_X,
    APU_REG_Y,
    APU_REG_SP,
    APU_REG_PSW,
    APU_REG_YA,
    APU_REG_A = APU_REG_YA,
    APU_REG_PC,
    APU_REG_LAST
};

union apu_reg_t
{
    uint16_t    word;
    uint8_t     byte[2];
};

struct apu_state_t
{
    union
    {
        struct
        {
            uint8_t c;
            uint8_t z;
            uint8_t i;
            uint8_t h;
            uint8_t b;
            uint8_t p;
            uint8_t v;
            uint8_t n;
        };

        uint8_t flags[APU_STATUS_FLAG_LAST];
    } reg_psw;

    union apu_reg_t regs[APU_REG_LAST];
};

void init_apu();

void reset_apu();

void step_apu(int32_t cycles);

uint8_t apuio_read(uint32_t effective_address);

void apuio_write(uint32_t effective_address, uint8_t data);

#endif
