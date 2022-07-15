#include <stdio.h>
#include "SDL2/SDL.h"

#include "ppu.h"
#include "addr.h"
#include "cpu.h"
#include "mem.h"



#define OBJ_ATTR_FLIP_V_P(obj_attr) (((obj_attr)->fpcn >> 15) & 0x1)
#define OBJ_ATTR_FLIP_H_P(obj_attr) (((obj_attr)->fpcn >> 14) & 0x1)
#define OBJ_ATTR_PRI_P(obj_attr) (((obj_attr)->fpcn >> 12) & 0x3)
#define OBJ_ATTR_COLOR_P(obj_attr) (((obj_attr)->fpcn >> 9) & 0x7)
#define OBJ_ATTR_NAME_P(obj_attr) ((obj_attr)->fpcn & 0x1ff)

/* those are here to allow using this macro for both instances of struct obj_attr_t and pointers to it */
#define OBJ_ATTR_FLIP_V(obj_attr) _Generic(obj_attr, struct obj_attr_t: OBJ_ATTR_FLIP_V_P(&obj_attr), default: OBJ_ATTR_FLIP_V_P(obj_attr))
#define OBJ_ATTR_FLIP_H(obj_attr) _Generic(obj_attr, struct obj_attr_t: OBJ_ATTR_FLIP_H_P(&obj_attr), default: OBJ_ATTR_FLIP_H_P(obj_attr))
#define OBJ_ATTR_PRI(obj_attr) _Generic(obj_attr, struct obj_attr_t: OBJ_ATTR_PRI_P(&obj_attr), default: OBJ_ATTR_PRI_P(obj_attr))
#define OBJ_ATTR_COLOR(obj_attr) _Generic(obj_attr, struct obj_attr_t: OBJ_ATTR_COLOR_P(&obj_attr), default: OBJ_ATTR_COLOR_P(obj_attr))
#define OBJ_ATTR_NAME(obj_attr) _Generic(obj_attr, struct obj_attr_t: OBJ_ATTR_NAME_P(&obj_attr), default: OBJ_ATTR_NAME_P(obj_attr))


#define INIDISP_ADDRESS 0x2100
#define OBJSEL_ADDRESS 0x2101
#define OAMADDRL_ADDRESS 0x2102
#define OAMADDRH_ADDRESS 0x2103
#define OAMDATA_ADDRESS 0x2104

#define SLHV_ADDRESS 0x2137
#define STAT78_ADDRESS 0x213f
#define OPHCT_ADDRESS 0x213c
#define OPVCT_ADDRESS 0x213d
#define VMDATALW_ADDRESS 0x2118
#define VMDATAHW_ADDRESS 0x2119
#define VMDATALR_ADDRESS 0x2139
#define VMDATAHR_ADDRESS 0x213a
#define HVBJOY_ADDRESS 0x4212

#define SCANLINE_DOT_LENGTH 340
#define SCANLINE_COUNT 262
#define H_BLANK_END_DOT 1
#define H_BLANK_START_DOT 274

#define V_BLANK_END_LINE 0 

/* https://www.raphnet.net/divers/retro_challenge_2019_03/qsnesdoc.html */

/* 
    https://en.wikibooks.org/wiki/Super_NES_Programming/SNES_Specs 
    
    THANK FUCKING GOD FOR TELLING ME THE OBVIOUS THING THAT EVERY OTHER FREAKING
    DOCUMENT SEEMED TO SKIP, FOR SOME REASON. THANKS FOR TELLING ME THAT VRAM IS 
    ACTUALLY A *SEPARATE CHIP* IN THE SNES. BECAUSE WHY WOULD MENTIONING THAT 
    WOULD BE IMPORTANT, RIGHT? IT'S NOT LIKE ANY MORON EMULATOR WRITTER WILL
    FIRST ASSUME VRAM RESIDES IN THE 120K WRAM AREA. NO, NOT AT ALL.
*/
// uint16_t vram[0x7fff];

#define H_COUNTER 0
#define V_COUNTER 1
#define H_BLANK_FLAG 0x40
#define V_BLANK_FLAG 0x80

struct 
{
    uint16_t counter;
    
    union
    {
        struct { uint8_t bytes[2]; };
        uint16_t word;
    }latched;
    
    uint16_t byte_select;
}counters[2];


#define PPU_REG_OFFSET(reg) (reg-PPU_REGS_START_ADDRESS)

char *ppu_reg_names[PPU_REGS_END_ADDRESS - PPU_REGS_START_ADDRESS] = {
    [PPU_REG_OFFSET(INIDISP_ADDRESS)] = "INIDSP",
    [PPU_REG_OFFSET(OBJSEL_ADDRESS)] = "OBJSEL",
    [PPU_REG_OFFSET(OAMADDRL_ADDRESS)] = "OAMADDRL",
    [PPU_REG_OFFSET(OAMADDRH_ADDRESS)] = "OAMADDRH",
    [PPU_REG_OFFSET(OAMDATA_ADDRESS)] = "OAMDATA",
};

#define VERBOSE

// union
// {
//     uint8_t ppu_regs[0x80];

//     struct
//     {
//         /* INIDISP (0x2100)
            
//             D0-D3   -> screen brightness. 0xf is the brightest, 0x0 is the darkest.
//             D7      -> force blanking. Does not reset any counters. 
//         */
//         uint8_t inidisp;
        
        
//         uint8_t objsel;
        
//         /* OAMADDR (0x2102, 0x2103) - WORD address that will be used to write OAM data. Note, again, it's the 
//         WORD address, meaning that incrementing it will advance TWO bytes, NOT ONE.  */
//         union
//         {
//             struct 
//             {
//                 uint8_t oamaddrl;
//                 uint8_t oamaddrh;
//             };
            
//             uint16_t oamaddr;
//         };
        
//         /* OAMDATAW (0x2104) - data to be written into the OAM data. Two reads are necessary to trigger the transfer,
//         and they must be done low byte first, high byte second. The first write will be latched into a buffer, and the
//         second one will trigger the whole transfer. After the write, the value in the OAMADDR will be incremented by one,
//         pointing at the next word to be written. */
//         uint8_t oamdataw;
//         uint8_t bgmode;
//         uint8_t mosaic;
//         uint8_t bg1sc;
//         uint8_t bg2sc;
//         uint8_t bg3sc;
//         uint8_t bg4sc;
//         uint8_t bg12nba;
//         uint8_t bg34nba;
//         uint8_t bg1h0fs;
//         uint8_t bg1v0fs;
//         uint8_t bg2h0fs;
//         uint8_t bg2v0fs;
//         uint8_t bg3h0fs;
//         uint8_t bg3v0fs;
//         uint8_t bg4h0fs;
//         uint8_t bg4v0fs;
//         uint8_t vmainc;
        
//         /* VMADDL (0x2116) and VMADDH (0x2117). The address for reading or writing WORDS
//         to VRAM. At each read/write to VRAM, this address gets incremented, which means
//         it points to another WORD, not BYTE. The increment value depends on the value of
//         bits 0 and 1 of register VMAINC (0x2115). 
        
//         VMAINC.1  VMAINC.0
        
//             0       0           ->      increment the address by 1   (1 word)
//             0       1           ->      increment the address by 32  (32 words)
//             1       0           ->      increment the address by 64  (64 words)
//             1       1           ->      increment the address by 128 (128 words)
        
//         Since this address points to a word, the getting the address of a byte is only a 
//         matter of multiplying it by 2*/
//         union
//         {
//             struct
//             {
//                 uint8_t vmaddl;
//                 uint8_t vmaddh;
//             };
//             uint16_t vmadd;
//         };
        
        
        
//         /* VMDATAL (W 0x2118) and VMDATAH (W 0x2119). The contents of these registers get
//         written to VRAM. The way the increment is triggered depends on the value of bit 7 of
//         register VMAINC (0x2115).
        
//         VMAINC.7
//             0           ->          the address will be incremented when the low byte
//                                     (VMDATAL) gets written
            
//             1           ->          the address will be incremented when the high byte
//                                     (VMDATAH) gets written.
//          */
//         uint8_t vmdatal_write;
//         uint8_t vmdatah_write;
        
        
        
//         uint8_t m7sel;
//         uint8_t m7a;
//         uint8_t m7b;
//         uint8_t m7c;
//         uint8_t m7d;
//         uint8_t m7x;
//         uint8_t m7y;
//         uint8_t cgadd;
//         uint8_t cgdata_write;
//         uint8_t w12sel;
//         uint8_t w34sel;
//         uint8_t wobjsel;
//         uint8_t wh0;
//         uint8_t wh1;
//         uint8_t wh2;
//         uint8_t wh3;
//         uint8_t wbglog;
//         uint8_t wobjlog;
//         uint8_t tm;
//         uint8_t ts;
//         uint8_t tmw;
//         uint8_t tsw;
//         uint8_t cgswsel;
//         uint8_t cgadsub;
//         uint8_t coldata;
        
//         /*
//             SETINI (0x2133) Initial screen setting
//                 0 - interlacing (0) or non-interlacing (1) scan modes
//                 2 - whether to use 224 (0) or 239 (1) lines. Changes where V-blank starts.
//         */
//         uint8_t setini;
//         uint8_t mpyl;
//         uint8_t mpym;
//         uint8_t mpyh;

//         /*
//             SLHV (0x2137) - Latch H/V-Counter by software
//                 7-0     Not used

//             Reading from this register latches the current H/V counter values into
//             OPHCT/OPVCT, Ports 213Ch and 213Dh. Reading here works "as if" dragging
//             IO7 low (but it does NOT actually output a LOW level to IO7 on joypad 2)
//         */
//         uint8_t slhv;


//         uint8_t oamdata_read;
//         uint8_t vmdatal_read;
//         uint8_t vmdatah_read;
//         uint8_t cgdata_read;

//         /*
//             OPHCT (0x213C) - Horizontal Counter Latch (R)
//             OPVCT (0x213D) - Vertical Counter Latch (R)

//             There are three situations that do load H/V counter values into the latches:

//                 - Doing a dummy-read from SLHV (Port 2137h) by software
//                 - Switching WRIO (Port 4201h) Bit7 from 1-to-0 by software
//                 - Lightgun High-to-Low transition (Pin6 of 2nd Controller connector)

//             All three methods are working only if WRIO.Bit7 is (or was) set. If so, data is latched, and (in all three cases) the latch flag in 213Fh.Bit6 is set.

//                 - 1st read  Lower 8bit
//                 - 2nd read  Upper 1bit (other 7bit PPU2 open bus; last value read from PPU2)

//             There are two separate 1st/2nd-read flipflops (one for OPHCT, one for OPVCT), both flipflops can be reset by reading from Port 213Fh (STAT78), 
//             the flipflops aren't automatically reset when latching occurs.

//                 H Counter values range from 0 to 339, with 22-277 being visible on the
//                 screen. V Counter values range from 0 to 261 in NTSC mode (262 is
//                 possible every other frame when interlace is active) and 0 to 311 in
//                 PAL mode (312 in interlace?), with 1-224 (or 1-239(?) if overscan is
//                 enabled) visible on the screen.

//         */
//         uint8_t ophct;
//         uint8_t opvct;



//         uint8_t stat77;
//         uint8_t stat78;
//         uint8_t apuio0;
//         uint8_t apuio1;
//         uint8_t apuio2;
//         uint8_t apuio3;
//         uint8_t unused[0x3c];
//         uint8_t wmdata;
//         uint8_t wmaddl;
//         uint8_t wmaddm;
//         uint8_t wmaddh;
//     };
// }ppu_regs;

uint32_t oamdata_byte_index = 0;

union
{
    struct { uint8_t bytes[2]; };
    uint16_t word;
}oamdata_buffer;

uint32_t            vram_addr;
uint32_t            last_vmdata_reg;
extern uint8_t *    vram;
extern uint8_t *    ram1_regs;

void *ppu_regs_pointer(uint32_t effective_address)
{
//    uint32_t ppu_address = (effective_address & 0xffff) - PPU_START_ADDRESS;
//    return (ppu_regs.ppu_regs + ppu_address);
}

void ppu_write_byte(uint32_t effective_address, uint8_t data)
{
    
}

void ppu_regs_write(uint32_t effective_address, uint8_t data)
{    
//     effective_address &= 0x7fff;
    
// //    #ifdef VERBOSE
// //    
// //    printf("write %x to [%s]\n", data, ppu_reg_names[PPU_REG_OFFSET(effective_address)]);
// //    
// //    #endif // VERBOSE
    
//     switch(effective_address)
//     {
//         case OAMADDRL_ADDRESS:
//             ppu_regs.oamaddrl = data;
//             oamdata_byte_index = 0;
//         break;
        
//         case OAMADDRH_ADDRESS:
//             ppu_regs.oamaddrh = data;
//             oamdata_byte_index = 0;
//         break;
        
//         case OAMDATA_ADDRESS:
//             oamdata_buffer.bytes[oamdata_byte_index] = data;
//             oamdata_byte_index++;
            
//             if(oamdata_byte_index > 1)
//             {
//                 vram[ppu_regs.oamaddr] = oamdata_buffer.word;
// //                printf("word written to vram at %x\n", ppu_regs.oamaddr);
//                 ppu_regs.oamaddr++;
//                 oamdata_byte_index = 0;
//             }
//         break;
        
//         default:
//             ppu_regs.ppu_regs[effective_address - PPU_REGS_START_ADDRESS] = data;
//         break;
        
// //        case VMDATALW_ADDRESS:
// //            ppu_regs.vmdatal_write = data;
// //            pointer = vram + ppu_regs.vmadd * 2;
// //            if(!(ppu_regs.vmainc & 0x80))
// //            {
// //                ppu_regs.vmadd++;
// //            }
// //        break;
// //        
// //        case VMDATAHW_ADDRESS:
// //            ppu_regs.vmdatah_write = data;
// //            pointer = vram + ppu_regs.vmadd * 2 + 1;
// //            if(ppu_regs.vmainc & 0x80)
// //            {
// //                ppu_regs.vmadd++;
// //            }
// //        break;
//     }
    
    
}

uint8_t ppu_regs_read(uint32_t effective_address)
{
    // effective_address &= 0x7fff;
    
    // switch(effective_address)
    // {
    //     case SLHV_ADDRESS:
    //         counters[H_COUNTER].latched.word = counters[H_COUNTER].counter;
    //         counters[V_COUNTER].latched.word = counters[V_COUNTER].counter;
    //     break;
        
    //     case STAT78_ADDRESS:
    //         counters[H_COUNTER].byte_select = 0;
    //         counters[V_COUNTER].byte_select = 0;
    //     break;
        
    //     case OPHCT_ADDRESS:
    //         ppu_regs.ophct = counters[H_COUNTER].latched.bytes[counters[H_COUNTER].byte_select];
    //         counters[H_COUNTER].byte_select = 1;
    //     break;
        
    //     case OPVCT_ADDRESS:
    //         ppu_regs.ophct = counters[V_COUNTER].latched.bytes[counters[V_COUNTER].byte_select];
    //         counters[V_COUNTER].byte_select = 1;
    //     break;
    // }
    
    // return ppu_regs.ppu_regs[effective_address - PPU_REGS_START_ADDRESS];
}

void reset_ppu()
{
    counters[H_COUNTER].counter = 0;
    counters[H_COUNTER].latched.word = 0;
    counters[H_COUNTER].byte_select = 0;
    
    counters[V_COUNTER].counter = 0;
    counters[V_COUNTER].latched.word = 0;
    counters[V_COUNTER].byte_select = 0;
    
    // ppu_regs.inidisp = 0;
}

void step_ppu(uint32_t cycle_count)
{
    cycle_count >>= 2;
    uint8_t value;
    
    for(uint32_t cycle = 0; cycle < cycle_count; cycle++)
    {
        counters[H_COUNTER].counter++;
        if(counters[H_COUNTER].counter == H_BLANK_END_DOT)
        {
            // value = (uint8_t)cpu_regs_read(HVBJOY_ADDRESS);
            value = read_byte(HVBJOY_ADDRESS);
            value &= ~H_BLANK_FLAG;
            // cpu_regs_write(HVBJOY_ADDRESS, value, 1);
            write_byte(HVBJOY_ADDRESS, value);
        }
        else if(counters[H_COUNTER].counter == H_BLANK_START_DOT)
        {
            // value = (uint8_t)cpu_regs_read(HVBJOY_ADDRESS);
            value = read_byte(HVBJOY_ADDRESS);
            value |= H_BLANK_FLAG;
            // cpu_regs_write(HVBJOY_ADDRESS, value, 1);
            write_byte(HVBJOY_ADDRESS, value);
        }
        if(counters[H_COUNTER].counter == SCANLINE_DOT_LENGTH)
        {
            counters[V_COUNTER].counter++;
        }
        
        counters[H_COUNTER].counter %= SCANLINE_DOT_LENGTH;
        
         
        if(counters[V_COUNTER].counter == V_BLANK_END_LINE)
        {
            // value = (uint8_t)cpu_regs_read(HVBJOY_ADDRESS);
            value = read_byte(HVBJOY_ADDRESS);
            value &= ~V_BLANK_FLAG;
            // cpu_regs_write(HVBJOY_ADDRESS, value, 1);
            write_byte(HVBJOY_ADDRESS, value);
        }
        else
        {
            uint32_t last_scanline;
            
            // if(ppu_regs.setini)
            // {
            //     last_scanline = 240;
            // }
            // else
            {
                last_scanline = 225;
            }
            
            if(counters[V_COUNTER].counter == last_scanline)
            {
                // value = (uint8_t)cpu_regs_read(HVBJOY_ADDRESS);
                value = read_byte(HVBJOY_ADDRESS);
                value |= V_BLANK_FLAG;
                // cpu_regs_write(HVBJOY_ADDRESS, value, 1);
                write_byte(HVBJOY_ADDRESS, value);
            }
        }
        
        counters[V_COUNTER].counter %= SCANLINE_COUNT;
    }
}

void dump_ppu()
{
//    uint32_t hvbjoy = cpu_regs_read(HVBJOY_ADDRESS);
//    printf("==================================================\n");
//    printf("======================PPU=========================\n");
//    printf("current dot: (%d, %d)\n", counters[H_COUNTER].counter, counters[V_COUNTER].counter);
//    printf("v-blank: %d -- h-blank: %d\n", hvbjoy & V_BLANK_FLAG, hvbjoy & H_BLANK_FLAG);
//    printf("==================================================\n");
//    printf("==================================================\n");
}

void vmadd_write(uint32_t effective_address)
{
    // vram_addr = (uint32_t)ram1_regs[PPU_REG_VMADDL] | (((uint32_t)ram1_regs[PPU_REG_VMADDH]) << 8);
    // vram_addr *= 2;

    if((effective_address & 0xffff) == PPU_REG_VMADDL)
    {
        vram_addr &= 0xff00;
        vram_addr |= (uint16_t)ram1_regs[PPU_REG_VMADDL]; 
    }
    else
    {
        vram_addr &= 0x00ff;
        vram_addr |= ((uint16_t)ram1_regs[PPU_REG_VMADDL]) << 8;
    }
}

void vmdata_write(uint32_t effective_address)
{
    uint32_t write_order = ram1_regs[PPU_REG_VMAINC] & 0x80;
    uint32_t reg = effective_address & 0xffff;

    uint32_t addr = vram_addr * 2;
    
    if(write_order == PPU_VMDATA_WRITE_LH)
    {
        if(reg == PPU_REG_VMDATAL)
        {
            vram[addr] = ram1_regs[PPU_REG_VMDATAL];

            if(last_vmdata_reg == PPU_REG_VMDATAH)
            {
                vram[addr + 1] = ram1_regs[PPU_REG_VMDATAH];
            }

            // printf("write data %x%x to vram word %x\n", vram[addr], vram[addr + 1], vram_addr);
            vram_addr++;
        }
    }
    else
    {
        if(reg == PPU_REG_VMDATAH)
        {
            if(last_vmdata_reg == PPU_REG_VMDATAL)
            {
                vram[addr] = ram1_regs[PPU_REG_VMDATAL];
            }

            vram[addr + 1] = ram1_regs[PPU_REG_VMDATAH];
            // printf("write data %x%x to vram word %x\n", vram[addr], vram[addr + 1], vram_addr);
            vram_addr++;
        }
    }

    last_vmdata_reg = reg;
}



