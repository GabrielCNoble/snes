#include <stdio.h>
#include <stdlib.h>
#include "SDL2/SDL.h"

#include "ppu.h"
#include "addr.h"
#include "cpu/cpu.h"
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



/* https://www.raphnet.net/divers/retro_challenge_2019_03/qsnesdoc.html */
/* https://en.wikibooks.org/wiki/Super_NES_Programming/SNES_Specs */

#define H_COUNTER 0
#define V_COUNTER 1
#define H_BLANK_FLAG 0x40
#define V_BLANK_FLAG 0x80

uint16_t vcounter = 0;
uint16_t hcounter = 0;
uint8_t  sub_dot = 0;
uint8_t  dot_length = 4;
uint16_t latched_counters[2];
uint32_t scanline_cycles = 0;

struct dot_t *framebuffer;


// #define PPU_REG_OFFSET(reg) (reg-PPU_REGS_START_ADDRESS)

// char *ppu_reg_names[PPU_REGS_END_ADDRESS - PPU_REGS_START_ADDRESS] = {
//     [PPU_REG_OFFSET(INIDISP_ADDRESS)] = "INIDSP",
//     [PPU_REG_OFFSET(OBJSEL_ADDRESS)] = "OBJSEL",
//     [PPU_REG_OFFSET(OAMADDRL_ADDRESS)] = "OAMADDRL",
//     [PPU_REG_OFFSET(OAMADDRH_ADDRESS)] = "OAMADDRH",
//     [PPU_REG_OFFSET(OAMDATA_ADDRESS)] = "OAMDATA",
// };

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
extern uint64_t     master_cycles;
uint32_t            cur_field = 0;
int32_t             ppu_cycle_count = 0;

struct reg_write_t *    free_writes = NULL;
struct reg_write_t *    pending_writes = NULL;
struct reg_write_t *    last_pending_write = NULL;

uint16_t oam_addr = 0;
uint16_t draw_oam_addr = 0;
uint8_t oam_byte = 0;
uint8_t oam[544];

// uint8_t cgram[];

uint32_t vmadd_increment_shifts[] = {
    [PPU_VMADD_INC1]    = 0,
    [PPU_VMADD_INC32]   = 5,
    [PPU_VMADD_INC64]   = 7,
    [PPU_VMADD_INC128]  = 7,
};

uint32_t objsel_size_sel_sizes[][2] = {
    [PPU_OBJSEL_SIZE_SEL_8_16]      = {8, 16},
    [PPU_OBJSEL_SIZE_SEL_8_32]      = {8, 32},
    [PPU_OBJSEL_SIZE_SEL_8_64]      = {8, 64},
    [PPU_OBJSEL_SIZE_SEL_16_32]     = {16, 32},
    [PPU_OBJSEL_SIZE_SEL_16_64]     = {16, 64},
    [PPU_OBJSEL_SIZE_SEL_32_64]     = {32, 64},
};

struct bg_offset_t bg_offsets[4];

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

    bg_offsets[0] = (struct bg_offset_t){};
    bg_offsets[1] = (struct bg_offset_t){};
    bg_offsets[2] = (struct bg_offset_t){};
    bg_offsets[3] = (struct bg_offset_t){};
}

struct reg_write_t *queue_write(uint16_t reg, uint64_t cycle, uint8_t value)
{
    struct reg_write_t *write = NULL;

//    if(free_writes)
//    {
//        write = free_writes;
//        free_writes = free_writes->next;
//        write->next = NULL;
//    }
//    else
//    {
//        write = calloc(1, sizeof(struct reg_write_t));
//    }

//    write->reg = reg;
//    write->cycle = cycle;
//    write->value = value;
//
//    if(!pending_writes)
//    {
//        pending_writes = write;
//    }
//    else
//    {
//        last_pending_write->next = write;
//    }
//    last_pending_write = write;

    return write;
}

void apply_writes()
{
//    if(pending_writes)
//    {
//        uint64_t ppu_master_cycle = master_cycles + ppu_cycle_count;
//        while(pending_writes && pending_writes->cycle < ppu_master_cycle)
//        {
//            ram1_regs[pending_writes->reg] = pending_writes->value;
//            struct reg_write_t *next_write = pending_writes->next;
//            pending_writes->next = free_writes;
//            free_writes = pending_writes;
//            pending_writes = next_write;
//        }
//    }
}

uint32_t step_ppu(int32_t cycle_count)
{
    /* TODO: handle 6 cycle dots */
    ppu_cycle_count += cycle_count;
    uint64_t ppu_master_cycles = master_cycles;
    int32_t dot_count = ppu_cycle_count / PPU_CYCLES_PER_DOT;
    uint8_t value;
    uint32_t vblank = 0;

    struct obj_attr_t *obj_tab1 = (struct obj_attr_t *)oam;
    uint16_t *obj_tab2 = (uint16_t *)(oam + 512);
//    uint32_t dot_length;

    while(ppu_cycle_count)
    {
//        while(pending_writes && pending_writes->cycle <= ppu_master_cycles)
//        {
//            struct reg_write_t *next_write = pending_writes->next;
//            ram1_regs[pending_writes->reg] = pending_writes->value;
//            pending_writes->next = free_writes;
//            free_writes = pending_writes;
//            pending_writes = next_write;
//        }

        sub_dot++;
        scanline_cycles++;

        if(sub_dot == dot_length)
        {
            sub_dot = 0;

            uint32_t *obj_sizes = objsel_size_sel_sizes[ram1_regs[PPU_REG_OBJSEL] >> 5];
            hcounter++;
            if(hcounter == H_BLANK_END_DOT)
            {
                ram1_regs[CPU_REG_HVBJOY] &= ~CPU_HVBJOY_FLAG_HBLANK;
            }
            else if(hcounter == H_BLANK_START_DOT)
            {
                ram1_regs[CPU_REG_HVBJOY] |= CPU_HVBJOY_FLAG_HBLANK;
            }
            if(hcounter == SCANLINE_DOT_LENGTH)
            {
                vcounter++;
                hcounter = 0;
                scanline_cycles = 0;
            }

            if(hcounter == 322 || hcounter == 326)
            {
                dot_length = 6;
            }
            else
            {
                dot_length = 4;
            }

            if(vcounter == V_BLANK_END_LINE)
            {
                ram1_regs[CPU_REG_HVBJOY] &= ~CPU_HVBJOY_FLAG_VBLANK;
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
                    if(!(ram1_regs[CPU_REG_HVBJOY] & CPU_HVBJOY_FLAG_VBLANK))
                    {
                        vblank = 1;
                    }
                    ram1_regs[CPU_REG_HVBJOY] |= CPU_HVBJOY_FLAG_VBLANK;
                }
            }

            vcounter %= SCANLINE_COUNT;

            uint8_t hvbjoy = ram1_regs[CPU_REG_HVBJOY];
            if(!(hvbjoy & CPU_HVBJOY_FLAG_VBLANK) && !(hvbjoy & CPU_HVBJOY_FLAG_HBLANK))
            {
                uint8_t inidisp = ram1_regs[PPU_REG_INIDISP];
                float brightness = (float)(inidisp & 0xf) / 15.0;

                if(ram1_regs[PPU_REG_INIDISP] & PPU_INIDISP_FLAG_FBLANK)
                {
                    brightness = 0.0;
                }

                struct dot_t *dot = framebuffer + vcounter * FRAMEBUFFER_WIDTH + hcounter;
                dot->r = 255 * brightness;
                dot->g = 255 * brightness;
                dot->b = 255 * brightness;
                dot->a = 255;
            }
        }

        ppu_master_cycles++;
        ppu_cycle_count--;
    }

    return vblank;
}

void mode0_draw()
{
    uint8_t bg_mode = ram1_regs[PPU_REG_BGMODE];


}

void dump_ppu()
{
   printf("===================== PPU ========================\n");
   printf("current dot: (H: %d, V: %d)\n", hcounter, vcounter);
   printf("v-blank: %d -- h-blank: %d\n", (ram1_regs[CPU_REG_HVBJOY] & CPU_HVBJOY_FLAG_VBLANK) && 1, (ram1_regs[CPU_REG_HVBJOY] & CPU_HVBJOY_FLAG_HBLANK) && 1);
   printf("==================================================\n");
}


void inidisp_write(uint32_t effective_address, uint8_t value)
{
    ram1_regs[PPU_REG_INIDISP] = value;
//    queue_write(PPU_REG_INIDISP, master_cycle, value);
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
    vram_addr = (uint16_t)ram1_regs[PPU_REG_VMADDL] | ((uint16_t)ram1_regs[PPU_REG_VMADDH] << 8);
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

    return value;
}

void oamadd_write(uint32_t effective_address, uint8_t value)
{
    uint32_t reg = effective_address & 0xffff;
    ram1_regs[reg] = value;
    oam_addr = ((uint16_t)ram1_regs[PPU_REG_OAMADDL] | ((uint16_t)ram1_regs[PPU_REG_OAMADDH] << 8)) & 0x01ff;
    oam_addr <<= 1;
    draw_oam_addr = oam_addr;
//    printf("oamaddr: %d\n", oam_addr);
}

void oamdata_write(uint32_t effective_address, uint8_t value)
{
    oam[oam_addr] = value;
    oam_addr = (oam_addr + 1) % PPU_OAM_SIZE;
}

void bgmode_write(uint32_t effective_address, uint8_t value)
{
    ram1_regs[PPU_REG_BGMODE] = value;
}

void bgsc_write(uint32_t effective_address, uint8_t value)
{
    uint32_t reg = effective_address & 0xffff;
    ram1_regs[reg] = value;
}

void bgoffs_write(uint32_t effective_address, uint8_t value)
{
    uint32_t reg = (effective_address & 0xffff) - PPU_REG_BG1HOFS;
    /* two registers per background (h and v offsets), so bit 1 gives us the
    offset of the background we want to */
    struct bg_offset_t *bg_offset = &bg_offsets[reg >> 1];
    /* for each background, the first register is the horizontal offset, the second
    is vertical offset. */
    uint32_t offset_index = reg & 1;

    if(bg_offset->lsb_written[offset_index])
    {
        bg_offset->offsets[offset_index] &= 0x00ff;
        bg_offset->offsets[offset_index] |= ((uint16_t)value << 8) & 0x1fff;

        // printf("offset for bg %d is h: %d -- v: %d\n", reg >> 1, bg_offset->offsets[0], bg_offset->offsets[1]);
    }
    else
    {
        bg_offset->offsets[offset_index] &= 0xff00;
        bg_offset->offsets[offset_index] |= value;
    }

    bg_offset->lsb_written[offset_index] ^= 1;
}

void coldata_write(uint32_t effective_address, uint8_t value)
{
    ram1_regs[PPU_REG_COLDATA] = value;
}
