#ifndef APU_H
#define APU_H
#include <stdint.h>

#define APU_RAM_SIZE 0x10000

struct apu_io_reg_t
{
    uint16_t read;
    uint16_t write;
};

struct apu_port_t
{
    uint8_t read;
    uint8_t write;
};

struct apu_io_reg_funcs_t
{
    void    (*write)(uint16_t address, uint8_t value);
    uint8_t (*read)(uint16_t address);
};

enum APU_MEM_REGS
{
    APU_MEM_REG_IO0     = 0x2140,
    APU_MEM_REG_IO1     = 0x2141,
    APU_MEM_REG_IO2     = 0x2142,
    APU_MEM_REG_IO3     = 0x2143,
};

enum APU_IO_REGS
{
    APU_IO_REG_TEST,
    APU_IO_REG_CONTROL,
    APU_IO_REG_DSPADDR,
    APU_IO_REG_DSPDATA,
    APU_IO_REG_PORT0,
    APU_IO_REG_PORT1,
    APU_IO_REG_PORT2,
    APU_IO_REG_PORT3,
    APU_IO_REG_AUX04,
    APU_IO_REG_AUX05,
    APU_IO_REG_T0DIV,
    APU_IO_REG_T1DIV,
    APU_IO_REG_T2DIV,
    APU_IO_REG_T0OUT,
    APU_IO_REG_T1OUT,
    APU_IO_REG_T2OUT,
    APU_IO_REG_COUNT,
};

enum APU_IO_CONTROL_REG_FLAGS
{
    APU_IO_CONTROL_REG_FLAG_ST0     = 1,
    APU_IO_CONTROL_REG_FLAG_ST1     = 1 << 1,
    APU_IO_CONTROL_REG_FLAG_ST2     = 1 << 2,
    APU_IO_CONTROL_REG_FLAG_PC10    = 1 << 4,
    APU_IO_CONTROL_REG_FLAG_PC23    = 1 << 5,
};

#define APU_RAM_RANGE0_START    0x0000
#define APU_RAM_RANGE0_END      0x00ef
#define APU_IO_REGS_START       0x00f0
#define APU_IO_REGS_END         0x00ff
#define APU_RAM_RANGE1_START    0x0100
#define APU_RAM_RANGE1_END      0xffbf
#define APU_ROM_START           0xffc0
#define APU_ROM_END             0xffff

enum APU_ACCESS_LOCATIONS
{
    APU_ACCESS_LOCATION_RAM,
    APU_ACCESS_LOCATION_IO,
    APU_ACCESS_LOCATION_ROM,
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
    APU_INST_AND,
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
    APU_INST_DAA,
    APU_INST_DAS,
    APU_INST_DBNZ,
    APU_INST_DEC,
    APU_INST_DI,
    APU_INST_DIV,
    APU_ISNT_EI,
    APU_INST_EOR,
    APU_INST_EOR1,
    APU_INST_INC,
    APU_INST_JMP,
    APU_INST_LSR,
    APU_INST_MOV,
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

enum APU_ADDR_MODES
{
    APU_ADDR_MODE_IMM,
    APU_ADDR_MODE_X,
    APU_ADDR_MODE_X_INCR,
    APU_ADDR_MODE_DP,  
    APU_ADDR_MODE_DP_X,
    APU_ADDR_MODE_DP_Y,
    APU_ADDR_MODE_ABS,
    APU_ADDR_MODE_ABS_X,
    APU_ADDR_MODE_ABS_Y,
    APU_ADDR_MODE_DP_X_IND,
    APU_ADDR_MODE_DP_IND_Y,
    APU_ADDR_MODE_X_Y,
    APU_ADDR_MODE_D_S,
    APU_ADDR_MODE_DP_IMM,
    APU_ADDR_MODE_IMP,
    APU_ADDR_MODE_PC_REL
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
    APU_OPCODE_ADC_X_Y                  = 0x99,

    /* A = A + #imm8 + C */
    APU_OPCODE_ADC_A_IMM                = 0x88,

    /* A = A + [X] + C */
    APU_OPCODE_ADC_A_X                  = 0x86,

    /* A = A + [[dp8] + Y] + C */
    APU_OPCODE_ADC_A_DP_IND_Y           = 0x97,

    /* A = A + [[dp8 + X]] + C */
    APU_OPCODE_ADC_A_DP_X_IND           = 0x87,

    /* A = A + [dp8] + C*/
    APU_OPCODE_ADC_A_DP                 = 0x84,

    /* A = A + [dp8 + X] + C */
    APU_OPCODE_ADC_A_DP_X               = 0x94,

    /* A = A + [abs16] + C */
    APU_OPCODE_ADC_A_ABS                = 0x85,

    /* A = A + [abs16 + X] + C */
    APU_OPCODE_ADC_A_ABX_X              = 0x95,

    /* A = A + [abs16 + Y] + C */
    APU_OPCODE_ADC_A_ABS_Y              = 0x96,

    /* [d8] = [d8] + [s8] + C */
    APU_OPCODE_ADC_D_S                  = 0x89,

    /* [d8] = [d8] + #imm8 + C */
    APU_OPCODE_ADC_DP_IMM               = 0x98,

    /* YA = YA + [dp8] + C */
    APU_OPCODE_ADDW_YA_DP               = 0x7a,


    /******************* AND *******************/
    /* [X] = [X] & [Y] */
    APU_OPCODE_AND_X_Y                  = 0x39,

    /* A = A & #imm8 */
    APU_OPCODE_AND_A_IMM                = 0x28,

    /* A = A & [X] */
    APU_OPCODE_AND_A_X                  = 0x26,

    /* A = A & [[dp8] + Y] */
    APU_OPCODE_AND_A_DP_IND_Y           = 0x37,

    /* A = A & [[dp8 + X]] */
    APU_OPCODE_AND_A_DP_X_IND           = 0x27,

    /* A = A & [dp8] */
    APU_OPCODE_AND_A_DP                 = 0x24,

    /* A = A & [dp8 + X] */
    APU_OPCODE_AND_A_DP_X               = 0x34,

    /* A = A & [abs16] */
    APU_OPCODE_AND_A_ABS                = 0x25,

    /* A = A & [abs16 + X] */
    APU_OPCODE_AND_A_ABS_X              = 0x35,

    /* A = A & [abs16 + Y] */
    APU_OPCODE_AND_A_ABS_Y              = 0x36,

    /* [d8] = [d8] & [s8] */
    APU_OPCODE_AND_D_S                  = 0x29,

    /* [d8] = [d8] & #imm8 */
    APU_OPCODE_AND_DP_IMM               = 0x38,
    
    /* C = C & ![dp8].bit */
    APU_OPCODE_AND1N_DP                 = 0x6a,

    /* C = C & [dp8].bit */
    APU_OPCODE_AND1_DP                  = 0x4a,

    /******************* ASL *******************/

    /* A = A << 1 */
    APU_OPCODE_ASL_A                    = 0x1c,

    /* [dp8] = [dp8] << 1 */
    APU_OPCODE_ASL_DP                   = 0x0b,

    /* [dp8 + X] = [dp8 + X] << 1 */
    APU_OPCODE_ASL_DP_X                 = 0x1b,

    /* [abs16] = [abs16] << 1 */
    APU_OPCODE_ASL_ABS                  = 0x0c,

    /******************* BBC *******************/
    APU_OPCODE_BBC0_PC_REL              = 0x13,
    APU_OPCODE_BBC1_PC_REL              = 0x33,
    APU_OPCODE_BBC2_PC_REL              = 0x53,
    APU_OPCODE_BBC3_PC_REL              = 0x73,
    APU_OPCODE_BBC4_PC_REL              = 0x93,
    APU_OPCODE_BBC5_PC_REL              = 0xb3,
    APU_OPCODE_BBC6_PC_REL              = 0xd3,
    APU_OPCODE_BBC7_PC_REL              = 0xf3,

    /******************* BBC *******************/
    APU_OPCODE_BBS0_PC_REL              = 0x03,
    APU_OPCODE_BBS1_PC_REL              = 0x23,
    APU_OPCODE_BBS2_PC_REL              = 0x43,
    APU_OPCODE_BBS3_PC_REL              = 0x63,
    APU_OPCODE_BBS4_PC_REL              = 0x83,
    APU_OPCODE_BBS5_PC_REL              = 0xa3,
    APU_OPCODE_BBS6_PC_REL              = 0xc3,
    APU_OPCODE_BBS7_PC_REL              = 0xe3,

    /******************* BCC *******************/
    APU_OPCODE_BCC_PC_REL               = 0x90,

    /******************* BCS *******************/
    APU_OPCODE_BCS_PC_REL               = 0xb0,

    /******************* BEQ *******************/
    APU_OPCODE_BEQ_PC_REL               = 0xf0,

    /******************* BMI *******************/
    APU_OPCODE_BMI_PC_REL               = 0x30,

    /******************* BNE *******************/
    APU_OPCODE_BNE_PC_REL               = 0xd0,

    /******************* BPL *******************/
    APU_OPCODE_BPL_PC_REL               = 0x10,

    /******************* BVC *******************/
    APU_OPCODE_BVC_PC_REL               = 0x50,

    /******************* BVS *******************/
    APU_OPCODE_BVS_PC_REL               = 0x70,

    /******************* BRA *******************/
    APU_OPCODE_BRA_PC_REL               = 0x2f,

    /******************* BRK *******************/
    APU_OPCODE_BRK_IMP                  = 0x0f,

    /******************* CALL *******************/
    APU_OPCODE_CALL_ABS                 = 0x3f,

    /******************* CNBE *******************/
    APU_OPCODE_CNBE_DP_X                = 0xde,
    APU_OPCODE_CNBE_DP                  = 0xde,
    
    /******************* CLR1 *******************/
    APU_OPCODE_CLR10_DP                 = 0x12,
    APU_OPCODE_CLR11_DP                 = 0x32,
    APU_OPCODE_CLR12_DP                 = 0x52,
    APU_OPCODE_CLR13_DP                 = 0x72,
    APU_OPCODE_CLR14_DP                 = 0x92,
    APU_OPCODE_CLR15_DP                 = 0xb2,
    APU_OPCODE_CLR16_DP                 = 0xd2,
    APU_OPCODE_CLR17_DP                 = 0xf2,

    /******************* CLRC *******************/
    APU_OPCODE_CLRC_IMP                 = 0x60,

    /******************* CLRP *******************/
    APU_OPCODE_CLRP_IMP                 = 0x20,

    /******************* CLRV *******************/
    APU_OPCODE_CLRV_IMP                 = 0xe0,

    /******************* CMP *******************/
    /* [X] - [Y] */
    APU_OPCODE_CMP_X_Y                  = 0x79,

    /* A - #imm8 */
    APU_OPCODE_CMP_A_IMM                = 0x68,

    /* A - [X] */
    APU_OPCODE_CMP_A_X                  = 0x66,

    /* A - [[dp8] + Y] */
    APU_OPCODE_CMP_A_DP_IND_Y           = 0x77,

    /* A - [[dp8 + X]] */
    APU_OPCODE_CMP_A_DP_X_IND           = 0x67,

    /* A - [dp8] */
    APU_OPCODE_CMP_A_DP                 = 0x64,

    /* A - [dp8 + X] */
    APU_OPCODE_CMP_A_DP_X               = 0x74,

    /* A - [abs16] */
    APU_OPCODE_CMP_A_ABS                = 0x65,

    /* A - [abs16 + X] */
    APU_OPCODE_CMP_A_ABS_X              = 0x75,

    /* A - [abs16 + Y] */
    APU_OPCODE_CMP_A_ABS_Y              = 0x76,

    /* X - #imm8 */
    APU_OPCODE_CMP_X_IMM                = 0xc8,

    /* X - [dp8] */
    APU_OPCODE_CMP_X_DP                 = 0x3e,

    /* X - [abs16] */
    APU_OPCODE_CMP_X_ABS                = 0x1e,

    /* Y - #imm8 */
    APU_OPCODE_CMP_Y_IMM                = 0xad,

    /* Y - [dp8] */
    APU_OPCODE_CMP_Y_DP                 = 0x7e,

    /* Y - [abs16] */
    APU_OPCODE_CMP_Y_ABS                = 0x5e,

    /* [d8] - [s8] */
    APU_OPCODE_CMP_D_S                  = 0x69,

    /* [dp8] - #imm8 */
    APU_OPCODE_CMP_DP_IMM               = 0x78,

    /* YA - [dp8] */
    APU_OPCODE_CMPW_YA_DP               = 0x5a,

    /******************* DAA *******************/
    APU_OPCODE_DAA_IMP                  = 0xdf,

    /******************* DAS *******************/
    APU_OPCODE_DAS_IMP                  = 0xbe,

    /******************* DBNZ *******************/
    APU_OPCODE_DBNZ_Y_IMP               = 0xfe,

    APU_OPCODE_DBNZ_DP                  = 0xfe,

    /******************* DEC *******************/

    /* A = A - 1 */
    APU_OPCODE_DEC_A_IMP                = 0x9c,

    /* X = X - 1 */
    APU_OPCODE_DEC_X_IMP                = 0x1d,

    /* Y = Y - 1 */
    APU_OPCODE_DEC_Y_IMP                = 0xdc,

    /* [dp8] = [dp8] - 1 */
    APU_OPCODE_DEC_DP                   = 0x8b,

    /* [dp8 + X] = [dp8 + X] - 1 */
    APU_OPCODE_DEC_DP_X                 = 0x9b,

    /* [abs16] = [abs16] - 1 */
    APU_OPCODE_DEC_ABS                  = 0x8c,

    /* [abs16] = [abs16] - 1 */
    APU_OPCODE_DECW_DP                  = 0x1a,

    /******************* DI *******************/

    APU_OPCODE_DI_IMP                   = 0xc0,

    /******************* DIV *******************/

    APU_OPCODE_DIV_YA_IMP               = 0x9e,

    /******************* EI *******************/

    APU_OPCODE_EI_IMP                   = 0xa0,

    /******************* EOR *******************/

    /* [X] = [X] ^ [Y] */
    APU_OPCODE_EOR_X_Y                  = 0x59,

    /* A = A ^ #imm8 */
    APU_OPCODE_EOR_A_IMM                = 0x48,

    /* A = A ^ [X] */
    APU_OPCODE_EOR_A_X                  = 0x46,

    /* A = A ^ [[dp8] + Y] */
    APU_OPCODE_EOR_A_DP_IND_Y           = 0x57,

    /* A = A ^ [[dp8 + X]] */
    APU_OPCODE_EOR_A_DP_X_IND           = 0x47,

    /* A = A ^ [dp8] */
    APU_OPCODE_EOR_A_DP                 = 0x44,

    /* A = A ^ [dp8 + X] */
    APU_OPCODE_EOR_A_DP_X               = 0x54,

    /* A = A ^ [abs16] */
    APU_OPCODE_EOR_A_ABS                = 0x45,

    /* A = A ^ [abs16 + X] */
    APU_OPCODE_EOR_A_ABS_X              = 0x55,

    /* A = A ^ [abs16 + Y] */
    APU_OPCODE_EOR_A_ABS_Y              = 0x56,

    /* [d8] = [d8] ^ [s8] */
    APU_OPCODE_EOR_D_S                  = 0x49,

    /* [d8] = [d8] ^ #imm8 */
    APU_OPCODE_EOR_DP_IMM               = 0x58,

    /******************* EOR *******************/

    /* A = A - 1 */
    APU_OPCODE_INC_A_IMP                = 0xbc,

    /* X = X - 1 */
    APU_OPCODE_INC_X_IMP                = 0x3d,

    /* Y = Y - 1 */
    APU_OPCODE_INC_Y_IMP                = 0xfc,

    /* [dp8] = [dp8] - 1 */
    APU_OPCODE_INC_DP                   = 0xab,

    /* [dp8 + X] = [dp8 + X] - 1 */
    APU_OPCODE_INC_DP_X                 = 0xbb,

    /* [abs16] = [abs16] - 1 */
    APU_OPCODE_INC_ABS                  = 0xac,

    /* [abs16] = [abs16] - 1 */
    APU_OPCODE_INCW_DP                  = 0x3a,

    /******************* JMP *******************/

    APU_OPCODE_JMP_ABS                  = 0x5f,

    APU_OPCODE_JMP_ABS_X                = 0x1f,

    /******************* LSR *******************/

    /* A = A >> 1 */
    APU_OPCODE_LSR_A                    = 0x5c,

    /* [dp8] = [dp8] >> 1 */
    APU_OPCODE_LSR_DP                   = 0x4b,

    /* [dp8 + X] = [dp8 + X] >> 1 */
    APU_OPCODE_LSR_DP_X                 = 0x5b,

    /* [abs16] = [abs16] >> 1 */
    APU_OPCODE_LSR_ABS                  = 0x4c,

    /******************* MOV *******************/

    APU_OPCODE_MOV_X_INCR_A             = 0xaf,

    APU_OPCODE_MOV_X_A                  = 0xc6,

    APU_OPCODE_MOV_DP_IND_Y_A           = 0xd7,

    APU_OPCODE_MOV_DP_X_IND_A           = 0xc7,

    APU_OPCODE_MOV_A_IMM                = 0xe8,

    APU_OPCODE_MOV_A_X                  = 0xe6,

    APU_OPCODE_MOV_A_X_INCR             = 0xbf,

    APU_OPCODE_MOV_A_DP_IND_Y           = 0xf7,

    APU_OPCODE_MOV_A_DP_X_IND           = 0xe7,

    APU_OPCODE_MOV_A_X_IMP              = 0x7d,

    APU_OPCODE_MOV_A_Y_IMP              = 0xdd,

    APU_OPCODE_MOV_A_DP                 = 0xe4,

    APU_OPCODE_MOV_A_DP_X               = 0xf4,

    APU_OPCODE_MOV_A_ABS                = 0xe5,

    APU_OPCODE_MOV_A_ABS_X              = 0xf5,

    APU_OPCODE_MOV_A_ABS_Y              = 0xf6,

    APU_OPCODE_MOV_SP_X_IMP             = 0xbd,

    APU_OPCODE_MOV_X_IMM                = 0xcd,

    APU_OPCODE_MOV_X_A_IMP              = 0x5d,

    APU_OPCODE_MOV_X_SP_IMP             = 0x9d,

    APU_OPCODE_MOV_X_DP                 = 0xf8,

    APU_OPCODE_MOV_X_DP_Y               = 0xf9,

    APU_OPCODE_MOV_X_ABS                = 0xe9,

    APU_OPCODE_MOV_Y_IMM                = 0x8d,

    APU_OPCODE_MOV_Y_A                  = 0xfd,

    APU_OPCODE_MOV_Y_DP                 = 0xeb,

    APU_OPCODE_MOV_Y_DP_X               = 0xfb,

    APU_OPCODE_MOV_Y_ABS                = 0xec,

    APU_OPCODE_MOV_D_S                  = 0xfa,

    APU_OPCODE_MOV_DP_X_A               = 0xd4,

    APU_OPCODE_MOV_DP_X_Y               = 0xdb,

    APU_OPCODE_MOV_DP_Y_X               = 0xd9,

    APU_OPCODE_MOV_DP_IMM               = 0x8f,

    APU_OPCODE_MOV_DP_A                 = 0xc4,

    APU_OPCODE_MOV_DP_X                 = 0xd8,

    APU_OPCODE_MOV_DP_Y                 = 0xcb,

    APU_OPCODE_MOV_ABS_X_A              = 0xd5,

    APU_OPCODE_MOV_ABS_Y_A              = 0xd6,

    APU_OPCODE_MOV_ABS_A                = 0xc5,

    APU_OPCODE_MOV_ABS_X                = 0xc9,

    APU_OPCODE_MOV_ABS_Y                = 0xcc,

    APU_OPCODE_MOV1_C_ABS               = 0xaa,

    APU_OPCODE_MOV1_ABS_C               = 0xaa,

    APU_OPCODE_MOVW_YA_DP               = 0xba,

    APU_OPCODE_MOVW_DP_YA               = 0xda,

    /******************* MUL *******************/

    APU_OPCODE_MUL_IMP                  = 0xcf,

    /******************* NOP *******************/

    APU_OPCODE_NOP_IMP                  = 0x00,

    /******************* NOT *******************/

    APU_OPCODE_NOT1_DP                  = 0xea,
    APU_OPCODE_NOTC_IMP                 = 0xed,

    /******************* OR *******************/

    /* [X] = [X] & [Y] */
    APU_OPCODE_OR_X_Y                   = 0x19,

    /* A = A & #imm8 */
    APU_OPCODE_OR_A_IMM                 = 0x08,

    /* A = A & [X] */
    APU_OPCODE_OR_A_X                   = 0x06,

    /* A = A & [[dp8] + Y] */
    APU_OPCODE_OR_A_DP_IND_Y            = 0x17,

    /* A = A & [[dp8 + X]] */
    APU_OPCODE_OR_A_DP_X_IND            = 0x07,

    /* A = A & [dp8] */
    APU_OPCODE_OR_A_DP                  = 0x04,

    /* A = A & [dp8 + X] */
    APU_OPCODE_OR_A_DP_X                = 0x14,

    /* A = A & [abs16] */
    APU_OPCODE_OR_A_ABS                 = 0x05,

    /* A = A & [abs16 + X] */
    APU_OPCODE_OR_A_ABS_X               = 0x15,

    /* A = A & [abs16 + Y] */
    APU_OPCODE_OR_A_ABS_Y               = 0x16,

    /* [d8] = [d8] & [s8] */
    APU_OPCODE_OR_D_S                   = 0x09,

    /* [d8] = [d8] & #imm8 */
    APU_OPCODE_OR_DP_IMM                = 0x18,

    /* C = C | ![dp8].bit */
    APU_OPCODE_OR1N_DP                  = 0x2a,

    /* C = C | [dp8].bit */
    APU_OPCODE_OR1_DP                   = 0x0a,


    APU_OPCODE_FETCH                    = 0x100
    APU_OPCODE_LAST
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
    APU_REG_INST,
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

    union apu_reg_t     regs[APU_REG_LAST];
    struct apu_port_t   ports[4];
};

void apu_Init();

void apu_Reset();

void apu_Step(int32_t cycles);

uint8_t apu_ReadByte(uint16_t address);

void apu_WriteByte(uint16_t address, uint8_t value);

void apu_IoTestWrite(uint16_t address, uint8_t value);

uint8_t apu_IoTestRead(uint16_t address);

void apu_IoControlWrite(uint16_t address, uint8_t value);

uint8_t apu_IoControlRead(uint16_t address);

void apu_IoDspAddrWrite(uint16_t address, uint8_t value);

uint8_t apu_IoDspAddrRead(uint16_t address, uint8_t value);

void apu_IoDspDataWrite(uint16_t address, uint8_t value);

uint8_t apu_IoDspDataRead(uint16_t address);

void apu_IoPortWrite(uint16_t address, uint8_t value);

uint8_t apu_IoPortRead(uint16_t address);

void apu_IoTimerDivWrite(uint16_t address, uint8_t value);

uint8_t apu_IoTimerDivRead(uint16_t address);

void apu_IoTimerOutWrite(uint16_t address, uint8_t value);

uint8_t apu_IoTimerOutRead(uint16_t address);

uint8_t apuio_read(uint32_t effective_address);

void apuio_write(uint32_t effective_address, uint8_t data);

#endif
