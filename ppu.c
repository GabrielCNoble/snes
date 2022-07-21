#include <stdio.h>
#include <stdlib.h>
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

uint16_t vcounter = 0;
uint16_t hcounter = 0;
uint16_t latched_counters[2];

struct dot_t *framebuffer;


#define PPU_REG_OFFSET(reg) (reg-PPU_REGS_START_ADDRESS)

char *ppu_reg_names[PPU_REGS_END_ADDRESS - PPU_REGS_START_ADDRESS] = {
    [PPU_REG_OFFSET(INIDISP_ADDRESS)] = "INIDSP",
    [PPU_REG_OFFSET(OBJSEL_ADDRESS)] = "OBJSEL",
    [PPU_REG_OFFSET(OAMADDRL_ADDRESS)] = "OAMADDRL",
    [PPU_REG_OFFSET(OAMADDRH_ADDRESS)] = "OAMADDRH",
    [PPU_REG_OFFSET(OAMDATA_ADDRESS)] = "OAMDATA",
};

#define VERBOSE

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
uint32_t            cur_field = 0;
int32_t             ppu_cycle_count = 0;

uint16_t oam_addr = 0;
uint16_t draw_oam_addr = 0;
uint8_t oam_byte = 0;
uint8_t oam[544];

uint32_t vmadd_increment_shifts[] = {
    [PPU_VMADD_INC1]    = 0,
    [PPU_VMADD_INC32]   = 5,
    [PPU_VMADD_INC64]   = 7,
    [PPU_VMADD_INC128]  = 7,
};

void init_ppu()
{
    framebuffer = calloc(FRAMEBUFFER_WIDTH * FRAMEBUFFER_HEIGHT, sizeof(struct dot_t));
}

void shutdown_ppu()
{
    free(framebuffer);
}

void reset_ppu()
{
    vcounter = 0;
    hcounter = 0;
}

uint32_t step_ppu(int32_t cycle_count)
{
    /* TODO: handle 6 cycle dots */
    ppu_cycle_count += cycle_count;
    int32_t iterations = ppu_cycle_count / PPU_CYCLES_PER_DOT;

//    cycle_count >>= 2;
    uint8_t value;
    uint32_t vblank = 0;

    struct obj_attr_t *obj_attrs = (struct obj_attr_t *)oam;

    if(iterations)
    {
        ppu_cycle_count -= PPU_CYCLES_PER_DOT * iterations;

        for(uint32_t iteration = 0; iteration < iterations; iteration++)
        {
            hcounter++;
            if(hcounter == H_BLANK_END_DOT)
            {
                ram1_regs[CPU_REG_HVBJOY] &= ~H_BLANK_FLAG;
            }
            else if(hcounter == H_BLANK_START_DOT)
            {
                ram1_regs[CPU_REG_HVBJOY] |= H_BLANK_FLAG;
            }
            if(hcounter == SCANLINE_DOT_LENGTH)
            {
                vcounter++;
            }

            hcounter %= SCANLINE_DOT_LENGTH;

            if(vcounter == V_BLANK_END_LINE)
            {
                ram1_regs[CPU_REG_HVBJOY] &= ~V_BLANK_FLAG;
            }
            else
            {
                uint32_t last_scanline;
                uint8_t setini = ram1_regs[PPU_REG_SETINI];

                if(setini & PPU_SETINI_FLAG_BGV_SEL)
                {
                    last_scanline = 240;
                }
                else
                {
                    last_scanline = 225;
                }

                if(vcounter == last_scanline)
                {
                    ram1_regs[CPU_REG_HVBJOY] |= V_BLANK_FLAG;
                    vblank = 1;
                }
            }

            vcounter %= SCANLINE_COUNT;

            uint8_t hvbjoy = ram1_regs[CPU_REG_HVBJOY];
            if(!(hvbjoy & V_BLANK_FLAG) && !(hvbjoy & H_BLANK_FLAG))
            {
                uint8_t inidisp = ram1_regs[PPU_REG_INIDISP];
                float brightness = (float)(inidisp & 0xf) / 15.0;
                struct dot_t *dot = framebuffer + vcounter * FRAMEBUFFER_WIDTH + hcounter;

                uint32_t intersect_count = 0;
                for(uint32_t index = 0; index < 128; index++)
                {
                    struct obj_attr_t *attr = obj_attrs + index;
                    if(hcounter >= attr->h_pos && hcounter < attr->h_pos + 16 &&
                       vcounter >= attr->v_pos && vcounter < attr->v_pos + 16)
                    {
                        dot->r = 255 * brightness;
                        dot->g = 255 * brightness;
                        dot->b = 255 * brightness;
                        intersect_count++;
                    }
                }

                if(!intersect_count)
                {
                    dot->r = 0;
                    dot->g = 0;
                    dot->b = 0;
                }

//                dot->r = 255 * (((float)vcounter / (float)FRAMEBUFFER_HEIGHT) * brightness);
//                dot->g = 255 * (((float)vcounter / (float)FRAMEBUFFER_HEIGHT) * brightness);
//                dot->b = 255 * (((float)vcounter / (float)FRAMEBUFFER_HEIGHT) * brightness);


//                dot->r = (rand() % 255) * brightness;
//                dot->g = (rand() % 255) * brightness;
//                dot->b = (rand() % 255) * brightness;
                dot->a = 255;
            }
        }
    }

    return vblank;
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

uint8_t slhv_read(uint32_t effective_address)
{
    latched_counters[0] = hcounter;
    latched_counters[1] = vcounter;
    return 0;
}

uint8_t opct_read(uint32_t effective_address)
{
    uint32_t reg = effective_address & 0xffff;
    uint32_t index = PPU_REG_OPVCT - reg;
    uint8_t value = latched_counters[index];
    latched_counters[index] >>= 8;
    return value;
}

void vmadd_write(uint32_t effective_address, uint8_t value)
{
    uint32_t reg = PPU_REG_VMADDH - (effective_address & 0xffff);
    ram1_regs[reg] = value;
}

void vmdata_write(uint32_t effective_address, uint8_t value)
{
    uint32_t write_order = ram1_regs[PPU_REG_VMAINC] & 0x80;
    uint32_t reg = effective_address & 0xffff;
    uint32_t addr = vram_addr * 2;
    uint32_t increment_shift = vmadd_increment_shifts[ram1_regs[PPU_REG_VMAINC] & 0x3];

    if(ram1_regs[CPU_REG_HVBJOY] & V_BLANK_FLAG)
    {
        addr += PPU_REG_VMDATAWH - reg;
        vram[addr] = value;
        vram_addr += ((write_order == PPU_VMDATA_ADDR_INC_LH && reg == PPU_REG_VMDATAWH) ||
                      (write_order == PPU_VMDATA_ADDR_INC_HL && reg == PPU_REG_VMDATAWL)) << increment_shift;
    }
}

uint8_t vmdata_read(uint32_t effective_address)
{
    uint32_t read_order = ram1_regs[PPU_REG_VMAINC] & 0x80;
    uint32_t reg = effective_address & 0xffff;
    uint32_t addr = vram_addr * 2;
    uint32_t increment_shift = vmadd_increment_shifts[ram1_regs[PPU_REG_VMAINC] & 0x3];
    uint8_t value;

    if(ram1_regs[CPU_REG_HVBJOY] & V_BLANK_FLAG || ram1_regs[CPU_REG_HVBJOY] & H_BLANK_FLAG)
    {
        addr += PPU_REG_VMDATARH - reg;
        value = vram[addr];
        vram_addr += ((read_order == PPU_VMDATA_ADDR_INC_LH && reg == PPU_REG_VMDATARH) ||
                      (read_order == PPU_VMDATA_ADDR_INC_HL && reg == PPU_REG_VMDATARL)) << increment_shift;
    }
}

void oamadd_write(uint32_t effective_address, uint8_t value)
{
    uint32_t reg = effective_address & 0xffff;
    ram1_regs[reg] = value;
    oam_addr = ((uint16_t)ram1_regs[PPU_REG_OAMADDL] | ((uint16_t)ram1_regs[PPU_REG_OAMADDH] << 8)) & 0x01ff;
    oam_addr <<= 1;
    draw_oam_addr = oam_addr;
    // printf("oamaddr: %d\n", oam_addr);
}

void oamdata_write(uint32_t effective_address, uint8_t value)
{
    oam[oam_addr] = value;
    // printf("oamdata: %d (%x)\n", oam_addr, value);
    oam_addr++;
}

