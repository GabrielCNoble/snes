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
//uint64_t irq_cycle = 0xffffffff;
int32_t  irq_hold_timer = 0;
uint32_t cur_irq_counter = 0;
uint32_t irq_count = 0;
uint32_t last_draw_scanline = 0;
uint32_t wmdata_address = 0;

struct dot_t *framebuffer;


uint8_t color_lut[] = {
    0,
    255 * (1.0f / 31.0f),
    255 * (2.0f / 31.0f),
    255 * (3.0f / 31.0f),
    255 * (4.0f / 31.0f),
    255 * (5.0f / 31.0f),
    255 * (6.0f / 31.0f),
    255 * (7.0f / 31.0f),
    255 * (8.0f / 31.0f),
    255 * (9.0f / 31.0f),
    255 * (10.0f / 31.0f),
    255 * (11.0f / 31.0f),
    255 * (12.0f / 31.0f),
    255 * (13.0f / 31.0f),
    255 * (14.0f / 31.0f),
    255 * (15.0f / 31.0f),
    255 * (16.0f / 31.0f),
    255 * (17.0f / 31.0f),
    255 * (18.0f / 31.0f),
    255 * (19.0f / 31.0f),
    255 * (20.0f / 31.0f),
    255 * (21.0f / 31.0f),
    255 * (22.0f / 31.0f),
    255 * (23.0f / 31.0f),
    255 * (24.0f / 31.0f),
    255 * (25.0f / 31.0f),
    255 * (26.0f / 31.0f),
    255 * (27.0f / 31.0f),
    255 * (28.0f / 31.0f),
    255 * (29.0f / 31.0f),
    255 * (30.0f / 31.0f),
    255,
};

// #define PPU_REG_OFFSET(reg) (reg-PPU_REGS_START_ADDRESS)

// char *ppu_reg_names[PPU_REGS_END_ADDRESS - PPU_REGS_START_ADDRESS] = {
//     [PPU_REG_OFFSET(INIDISP_ADDRESS)] = "INIDSP",
//     [PPU_REG_OFFSET(OBJSEL_ADDRESS)] = "OBJSEL",
//     [PPU_REG_OFFSET(OAMADDRL_ADDRESS)] = "OAMADDRL",
//     [PPU_REG_OFFSET(OAMADDRH_ADDRESS)] = "OAMADDRH",
//     [PPU_REG_OFFSET(OAMDATA_ADDRESS)] = "OAMDATA",
// };

#define VERBOSE

// uint32_t oamdata_byte_index = 0;

// union
// {
//     struct { uint8_t bytes[2]; };
//     uint16_t word;
// }oamdata_buffer;

extern uint8_t *    ram1_regs;
extern uint8_t *    ram2;
extern uint64_t     master_cycles;
uint32_t            cur_field = 0;
int32_t             ppu_cycle_count = 0;

uint16_t            oam_addr = 0;
uint16_t            draw_oam_addr = 0;
uint8_t             oam_byte = 0;
uint8_t *           oam;

uint16_t            cgram_addr = 0;
uint8_t *           cgram = NULL;

uint32_t            last_vmdata_reg = 0;
uint32_t            vram_addr = 0;
uint8_t *           vram = NULL;

uint32_t            bg_bitdepth = 0;
struct background_t backgrounds[4];

uint32_t            main_screen_bg_count = 0;
struct bg_draw_t    main_screen[5];
uint32_t            sub_screen_bg_count = 0;
struct bg_draw_t    sub_screen[5];

uint32_t            line_obj_count = 0;
uint32_t            line_chr_count = 0;
struct line_obj_t   line_objs[128];
struct chr4_t *     obj_chr_base[2];
// void  *             obj_chr_base[2];
// uint8_t             line_objs[128];

// uint32_t

// uint8_t cgram[512];

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

uint32_t obj_sizes[2] = {8, 16};

void init_ppu()
{
    framebuffer = calloc(FRAMEBUFFER_WIDTH * FRAMEBUFFER_HEIGHT, sizeof(struct dot_t));
    oam = calloc(1, PPU_OAM_SIZE);
    cgram = calloc(1, PPU_CGRAM_SIZE);
    vram = calloc(1, PPU_VRAM_SIZE << 1);
    last_draw_scanline = 224;
}

void shutdown_ppu()
{
    free(framebuffer);
    free(cgram);
    free(vram);
}

void reset_ppu()
{
    vcounter = 0;
    hcounter = 0;

    backgrounds[0] = (struct background_t){.chr_base = vram, .data_base = (struct bg_sc_data_t *)vram, .pal_base = vram};
    backgrounds[1] = (struct background_t){.chr_base = vram, .data_base = (struct bg_sc_data_t *)vram, .pal_base = vram};
    backgrounds[2] = (struct background_t){.chr_base = vram, .data_base = (struct bg_sc_data_t *)vram, .pal_base = vram};
    backgrounds[3] = (struct background_t){.chr_base = vram, .data_base = (struct bg_sc_data_t *)vram, .pal_base = vram};

    obj_chr_base[0] = vram;
    obj_chr_base[1] = vram;

    ram1_regs[PPU_REG_STAT77] = 0;
    ram1_regs[PPU_REG_STAT78] = 0;
}

uint8_t bg_chr0_dot_col(void *chr_base, uint32_t index, uint32_t size16, uint32_t dot_h, uint32_t dot_v)
{
    return 0;
}

uint8_t bg_chr2_dot_col(void *chr_base, uint32_t index, uint32_t size16, uint32_t dot_h, uint32_t dot_v)
{
    uint32_t chr_size = 1 << (3 + size16);
    uint32_t chr_dot_h = dot_h % 8;
    uint32_t chr_dot_v = dot_v % 8;
    uint8_t color_index;

    index += (dot_h % chr_size) > 7;
    index += ((dot_v % chr_size) > 7) << 4;

    struct chr2_t *chr = (struct chr2_t *)chr_base + index;
    color_index  =  (chr->p01[chr_dot_v] & (0x80 >> chr_dot_h)) && 1;
    color_index |= ((chr->p01[chr_dot_v] & (0x8000 >> chr_dot_h)) && 1) << 1;
    return color_index;
}

uint8_t bg_chr4_dot_col(void *chr_base, uint32_t index, uint32_t size16, uint32_t dot_h, uint32_t dot_v)
{
    uint32_t chr_size = 1 << (3 + size16);
    uint32_t chr_dot_h = dot_h % 8;
    uint32_t chr_dot_v = dot_v % 8;
    uint8_t color_index;

    index += (dot_h % chr_size) > 7;
    index += ((dot_v % chr_size) > 7) << 4;

    struct chr4_t *chr = (struct chr4_t *)chr_base + index;
    color_index  =  (chr->p01[chr_dot_v] & (0x80 >> chr_dot_h)) && 1;
    color_index |= ((chr->p01[chr_dot_v] & (0x8000 >> chr_dot_h)) && 1) << 1;
    color_index |= ((chr->p23[chr_dot_v] & (0x80 >> chr_dot_h)) && 1) << 2;
    color_index |= ((chr->p23[chr_dot_v] & (0x8000 >> chr_dot_h)) && 1) << 3;
    return color_index;
}

uint8_t bg_chr8_dot_col(void *chr_base, uint32_t index, uint32_t size16, uint32_t dot_h, uint32_t dot_v)
{
    uint32_t chr_size = 1 << (3 + size16);
    uint32_t chr_dot_h = dot_h % 8;
    uint32_t chr_dot_v = dot_v % 8;
    uint8_t color_index;

    index += (dot_h % chr_size) > 7;
    index += ((dot_v % chr_size) > 7) << 4;

    struct chr8_t *chr = (struct chr8_t *)chr_base + index;
    color_index  =  (chr->p01[chr_dot_v] & (0x80 >> chr_dot_h)) && 1;
    color_index |= ((chr->p01[chr_dot_v] & (0x8000 >> chr_dot_h)) && 1) << 1;
    color_index |= ((chr->p23[chr_dot_v] & (0x80 >> chr_dot_h)) && 1) << 2;
    color_index |= ((chr->p23[chr_dot_v] & (0x8000 >> chr_dot_h)) && 1) << 3;
    color_index |= ((chr->p45[chr_dot_v] & (0x80 >> chr_dot_h)) && 1) << 4;
    color_index |= ((chr->p45[chr_dot_v] & (0x8000 >> chr_dot_h)) && 1) << 5;
    color_index |= ((chr->p67[chr_dot_v] & (0x80 >> chr_dot_h)) && 1) << 6;
    color_index |= ((chr->p67[chr_dot_v] & (0x8000 >> chr_dot_h)) && 1) << 7;
    return color_index;
}

// uint8_t bg7_chr8_dot_col(void *chr_base, uint32_t index, uint32_t dot_h, uint32_t dot_v)
// {
//     uint32_t chr_dot_h = dot_h % 8;
//     uint32_t chr_dot_v = dot_v % 8;
//     index <<= 6;

// }

struct bg_tile_t bg_tile_entry(uint32_t dot_h, uint32_t dot_v, struct background_t *background)
{
    uint32_t index_shift = 3 + background->chr_size;
    uint32_t data_index = (dot_h >> index_shift) + (dot_v >> index_shift) * 32;
    struct bg_sc_data_t *bg_data = background->data_base + data_index;
    struct bg_tile_t tile = {
        .chr_index = bg_data->data & BG_SC_DATA_NAME_MASK,
        .pal_index = (bg_data->data >> BG_SC_DATA_PAL_SHIFT) & BG_SC_DATA_PAL_MASK
    };
    return tile;
}

// struct bg7_tile_t bg7_tile_entry(uint32_t dot_h, uint32_t dot_v, struct background_t *background)
// {
//     uint32_t data_index = (dot_h >> 3) + (dot_v >> 3) * 128;
//     struct bg7_sc_data_t *bg_data = background->data_base + data_index;
//     struct bg7_tile_t tile = {
//         .chr_data = bg_data->chr,
//         .chr_name = bg_data->name
//     };
//     return tile;
// }



uint16_t bg_pal4_col(uint32_t dot_h, uint32_t dot_v, struct background_t *background)
{
    struct pal4_t *palletes = (struct pal4_t *)background->pal_base;
    struct bg_tile_t tile = bg_tile_entry(dot_h, dot_v, background);
    uint8_t color_index = bg_chr2_dot_col(background->chr_base, tile.chr_index, background->chr_size, dot_h, dot_v);
    return palletes[tile.pal_index].colors[color_index];
}

uint16_t bg_pal16_col(uint32_t dot_h, uint32_t dot_v, struct background_t *background)
{
    struct pal16_t *palletes = (struct pal16_t *)background->pal_base;
    struct bg_tile_t tile = bg_tile_entry(dot_h, dot_v, background);
    uint8_t color_index = bg_chr4_dot_col(background->chr_base, tile.chr_index, background->chr_size, dot_h, dot_v);
    return palletes[tile.pal_index].colors[color_index];
}

uint16_t bg_pal256_col(uint32_t dot_h, uint32_t dot_v, struct background_t *background)
{
    struct pal256_t *palletes = (struct pal256_t *)background->pal_base;
    struct bg_tile_t tile = bg_tile_entry(dot_h, dot_v, background);
    uint8_t color_index = bg_chr8_dot_col(background->chr_base, tile.chr_index, background->chr_size, dot_h, dot_v);
    return palletes[tile.pal_index].colors[color_index];
}

uint16_t bg7_pal256_col(uint32_t dot_h, uint32_t dot_v, struct background_t *background)
{
    struct bg7_sc_data_t *data_base = (struct bg7_sc_data_t *)vram;
    struct pal256_t *pallete = (struct pal256_t *)background->pal_base;
    uint32_t data_index = (dot_h >> 3) + (dot_v >> 3) * 128;
    struct bg7_sc_data_t *bg_data = data_base + data_index;

    uint32_t chr_dot_h = dot_h % 8;
    uint32_t chr_dot_v = dot_v % 8;
    uint32_t chr_index = ((uint32_t)bg_data->name << 6) + (chr_dot_v << 3) + chr_dot_h;

    struct bg7_sc_data_t *chr_data = data_base + chr_index;
    return pallete->colors[chr_data->chr];
}

uint16_t obj_pal16_col(uint32_t dot_h, uint32_t dot_v, struct line_obj_t *obj)
{
    struct oam_t *oam_tables = (struct oam_t *)oam;
    struct obj_attr1_t *attr1 = oam_tables->table1 + obj->index;
    uint32_t chr_name = attr1->fpcn & OBJ_ATTR1_NAME_MASK;
    void *chr_base = obj_chr_base[(chr_name & 0x100) != 0];

//    uint32_t chr_size = 1 << (3 + size16);
    uint32_t chr_dot_h = dot_h % 8;
    uint32_t chr_dot_v = dot_v % 8;
    uint8_t color_index;

    return color_index;
}

void update_line_objs(uint16_t line)
{
    line_obj_count = 0;
    line_chr_count = 0;
    struct oam_t *oam_tables = (struct oam_t *)oam;

    for(uint32_t index = 0; index < 128; index++)
    {
        struct obj_attr1_t *attr1 = oam_tables->table1 + index;
        struct obj_attr2_t *attr2 = oam_tables->table2 + (index >> 3);
        uint16_t size_pos = (attr2->size_hpos >> ((index & 0x07) << 1)) & OBJ_ATTR2_DATA_MASK;
        uint16_t obj_size = obj_sizes[size_pos >> 1];

        uint16_t hpos = (uint16_t)attr1->h_pos | ((size_pos & 1) << 8);
        uint16_t vpos = attr1->v_pos;

        if(line >= vpos && line < vpos + obj_size && hpos + obj_size > DRAW_START_DOT && hpos < DRAW_END_DOT)
        {
            line_objs[line_obj_count] = (struct line_obj_t){
                .vpos = vpos,
                .hpos = hpos,
                .size = obj_size,
                .index = index
            };
            line_obj_count++;
            line_chr_count += obj_size / 8;
        }
    }
}

uint32_t step_ppu(int32_t cycle_count)
{
    ppu_cycle_count += cycle_count;
    uint32_t vblank = 0;

//    struct oam_t *oam_tables = (struct oam_t *)oam;
//    uint32_t obj_size = (ram1_regs[PPU_REG_OBJSEL] >> PPU_OBJSEL_SIZE_SHIFT) & PPU_OBJSEL_SIZE_MASK;
//    uint32_t *obj_sizes = objsel_size_sel_sizes + obj_size;



    while(ppu_cycle_count)
    {
        sub_dot++;
        scanline_cycles++;

        if(sub_dot == dot_length)
        {
            sub_dot = 0;
//            uint32_t *obj_sizes = objsel_size_sel_sizes[ram1_regs[PPU_REG_OBJSEL] >> 5];
            hcounter++;
            cur_irq_counter--;

            if(hcounter == SCANLINE_DOT_LENGTH)
            {
                vcounter = (vcounter + 1) % SCANLINE_COUNT;
                hcounter = 0;
                scanline_cycles = 0;
                update_line_objs(vcounter);

                if(line_obj_count > 32)
                {
                   ram1_regs[PPU_REG_STAT77] |= PPU_STAT77_FLAG_33_RANGE_OVER;
                }

                if(line_chr_count > 34)
                {
                    ram1_regs[PPU_REG_STAT77] |= PPU_STAT77_FLAG_35_TIME_OVER;
                }
            }

            if(hcounter == H_BLANK_END_DOT)
            {
                ram1_regs[CPU_REG_HVBJOY] &= ~CPU_HVBJOY_FLAG_HBLANK;

                if(vcounter == 0)
                {
                    ram1_regs[PPU_REG_STAT78] ^= PPU_STAT78_FLAG_FIELD;
                }
            }
            else if(hcounter == H_BLANK_START_DOT)
            {
                ram1_regs[CPU_REG_HVBJOY] |= CPU_HVBJOY_FLAG_HBLANK;
            }


            if((hcounter == 323 || hcounter == 327) && (vcounter != 240 || !(ram1_regs[PPU_REG_STAT78] & PPU_STAT78_FLAG_FIELD) ))
            {
                dot_length = 6;
            }
            else
            {
                dot_length = 4;
            }


            if(vcounter == V_BLANK_END_LINE)
            {
                if(hcounter == 0)
                {
                    ram1_regs[CPU_REG_HVBJOY] &= ~CPU_HVBJOY_FLAG_VBLANK;
                    ram1_regs[CPU_REG_RDNMI] &= ~CPU_RDNMI_BLANK_NMI;
                    ram1_regs[PPU_REG_STAT77] &= ~(PPU_STAT77_FLAG_33_RANGE_OVER | PPU_STAT77_FLAG_35_TIME_OVER);
                }
            }
            else
            {
                if(vcounter == last_draw_scanline && hcounter == 0)
                {
                    if(!(ram1_regs[CPU_REG_HVBJOY] & CPU_HVBJOY_FLAG_VBLANK))
                    {
                        vblank = 1;
                        ram1_regs[CPU_REG_RDNMI] |= CPU_RDNMI_BLANK_NMI;
                    }
                    ram1_regs[CPU_REG_HVBJOY] |= CPU_HVBJOY_FLAG_VBLANK;
                    if(ram1_regs[CPU_REG_NMITIMEN] & CPU_NMITIMEN_FLAG_NMI_ENABLE)
                    {
                        assert_nmi(2);
                        deassert_nmi(2);
                    }
                }
            }

            if(!cur_irq_counter)
            {
                if(ram1_regs[CPU_REG_NMITIMEN] & (CPU_NMITIMEN_FLAG_HTIMER_EN | CPU_NMITIMEN_FLAG_VTIMER_EN))
                {
                    ram1_regs[CPU_REG_TIMEUP] |= 1 << 7;
                    irq_hold_timer = 8;
                }

                cur_irq_counter = irq_count;
            }

            if(irq_hold_timer > 0)
            {
                irq_hold_timer -= cycle_count;
                ram1_regs[CPU_REG_TIMEUP] |= 1 << 7;
            }

            uint8_t hvbjoy = ram1_regs[CPU_REG_HVBJOY];

            if(!(hvbjoy & CPU_HVBJOY_FLAG_VBLANK) && vcounter >= DRAW_START_LINE
               && hcounter >= DRAW_START_DOT && hcounter <= DRAW_END_DOT)
            {
                uint8_t inidisp = ram1_regs[PPU_REG_INIDISP];
                float brightness = (float)(inidisp & 0xf) / 15.0;
                uint32_t dot_x = hcounter - DRAW_START_DOT;
                uint32_t dot_y = vcounter - DRAW_START_LINE;

                if(ram1_regs[PPU_REG_INIDISP] & PPU_INIDISP_FLAG_FBLANK)
                {
                    brightness = 0.0;
                }

                struct mode0_cgram_t *mode0_cgram = (struct mode0_cgram_t *)cgram;
                struct dot_t *dot = framebuffer + dot_y * FRAMEBUFFER_WIDTH + dot_x;
                uint16_t backdrop = mode0_cgram->bg1_colors[0].colors[0];

                dot->r = color_lut[(backdrop >> COL_DATA_R_SHIFT) & COL_DATA_MASK];
                dot->g = color_lut[(backdrop >> COL_DATA_G_SHIFT) & COL_DATA_MASK];
                dot->b = color_lut[(backdrop >> COL_DATA_B_SHIFT) & COL_DATA_MASK];
                dot->a = 255;

                for(uint32_t index = 0; index < main_screen_bg_count; index++)
                {
                    struct bg_draw_t *bg_draw = main_screen + index;
                    uint16_t chr_size = 1 << (3 + bg_draw->background->chr_size);
                    uint16_t bg_size = chr_size << 5;
                    uint16_t bg_dot_x = (dot_x + bg_draw->background->offset.offsets[0]) % bg_size;
                    uint16_t bg_dot_y = (dot_y + bg_draw->background->offset.offsets[1]) % bg_size;
                    uint16_t color = bg_draw->color_func(bg_dot_x, bg_dot_y, bg_draw->background);
                    dot->r = color_lut[(color >> COL_DATA_R_SHIFT) & COL_DATA_MASK];
                    dot->g = color_lut[(color >> COL_DATA_G_SHIFT) & COL_DATA_MASK];
                    dot->b = color_lut[(color >> COL_DATA_B_SHIFT) & COL_DATA_MASK];
                }

                struct oam_t *oam_tables = (struct oam_t *)oam;

                if(ram1_regs[PPU_REG_TMAIN] & PPU_TMAIN_FLAG_OBJ)
                {
                    for(uint32_t index = 0; index < line_obj_count; index++)
                    {
                        struct line_obj_t *obj = line_objs + index;

                        if(hcounter >= obj->hpos && hcounter <= obj->hpos + obj->size)
                        {
                            uint16_t obj_dot_x = hcounter - obj->hpos;
                            uint16_t obj_dot_y = vcounter - obj->vpos;
                            struct obj_attr1_t *attr1 = oam_tables->table1 + obj->index;
                            uint32_t obj_name = attr1->fpcn & OBJ_ATTR1_NAME_MASK;
                            uint32_t obj_pal = (attr1->fpcn >> OBJ_ATTR1_PAL_SHIFT) & OBJ_ATTR1_PAL_MASK;
                            uint8_t color_index = bg_chr4_dot_col(obj_chr_base[0], obj_name, 1, obj_dot_x, obj_dot_y);

                            if(color_index)
                            {
                                uint16_t color = mode0_cgram->obj_colors[obj_pal].colors[color_index];
                                dot->r = color_lut[(color >> COL_DATA_R_SHIFT) & COL_DATA_MASK];
                                dot->g = color_lut[(color >> COL_DATA_G_SHIFT) & COL_DATA_MASK];
                                dot->b = color_lut[(color >> COL_DATA_B_SHIFT) & COL_DATA_MASK];
                            }

                            break;
                        }
                    }
                }

                dot->r *= brightness;
                dot->g *= brightness;
                dot->b *= brightness;
            }
        }

        ppu_cycle_count--;
    }

    return vblank;
}

void dump_ppu()
{
   printf("===================== PPU ========================\n");
   printf("current dot: (H: %d, V: %d)\n", hcounter, vcounter);
   printf("v-blank: %d -- h-blank: %d\n", (ram1_regs[CPU_REG_HVBJOY] & CPU_HVBJOY_FLAG_VBLANK) && 1, (ram1_regs[CPU_REG_HVBJOY] & CPU_HVBJOY_FLAG_HBLANK) && 1);
   printf("--------------------------------------------------\n");
   printf("BG Mode: Mode %d\n", ram1_regs[PPU_REG_BGMODE] & PPU_BGMODE_MODE_MASK);
   printf("\n");
}


void inidisp_write(uint32_t effective_address, uint8_t value)
{
    ram1_regs[PPU_REG_INIDISP] = value;
}

void objsel_write(uint32_t effective_address, uint8_t value)
{
    ram1_regs[PPU_REG_OBJSEL] = value;
    uint32_t obj_size_select = (value >> PPU_OBJSEL_SIZE_SHIFT) & PPU_OBJSEL_SIZE_MASK;
    obj_sizes[0] = objsel_size_sel_sizes[obj_size_select][0];
    obj_sizes[1] = objsel_size_sel_sizes[obj_size_select][1];
    uint32_t chr_base = ((value & PPU_OBJSEL_NAME_BASE_MASK) << 13) & 0x7fff;
    uint32_t name_sel = (chr_base + (((value >> PPU_OBJSEL_NAME_SEL_SHIFT) & PPU_OBJSEL_NAME_SEL_MASK) << 12)) & 0x7fff;
    obj_chr_base[0] = (struct chr4_t *)(vram + (chr_base << 1));
    obj_chr_base[1] = (struct chr4_t *)(vram + (name_sel << 1));
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
    uint32_t index = reg - PPU_REG_OPHCT;
    uint8_t value = latched_counters[index];
    latched_counters[index] >>= 8;
    return value;
}

void vmadd_write(uint32_t effective_address, uint8_t value)
{
    uint32_t reg = effective_address & 0xffff;
    ram1_regs[reg] = value;
    vram_addr = (uint16_t)ram1_regs[PPU_REG_VMADDL] | ((uint16_t)ram1_regs[PPU_REG_VMADDH] << 8);
}

void vmdata_write(uint32_t effective_address, uint8_t value)
{
    uint32_t write_order = ram1_regs[PPU_REG_VMAINC] & 0x80;
    uint32_t reg = effective_address & 0xffff;
    uint32_t write_addr = vram_addr << 1;
    uint32_t increment_shift = vmadd_increment_shifts[ram1_regs[PPU_REG_VMAINC] & 0x3];

    if((ram1_regs[CPU_REG_HVBJOY] & CPU_HVBJOY_FLAG_VBLANK) || (ram1_regs[PPU_REG_INIDISP] & PPU_INIDISP_FLAG_FBLANK))
    {
        write_addr += PPU_REG_VMDATAWH == reg;
        vram[write_addr] = value;
        vram_addr += ((write_order == PPU_VMDATA_ADDR_INC_LH && reg == PPU_REG_VMDATAWH) ||
                      (write_order == PPU_VMDATA_ADDR_INC_HL && reg == PPU_REG_VMDATAWL)) << increment_shift;
        vram_addr &= 0x7fff;
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
        vram_addr &= 0x7fff;
    }

    return value;
}

void oamadd_write(uint32_t effective_address, uint8_t value)
{
    uint32_t reg = effective_address & 0xffff;
    ram1_regs[reg] = value;
    oam_addr = ((uint16_t)ram1_regs[PPU_REG_OAMADDL] | ((uint16_t)ram1_regs[PPU_REG_OAMADDH] << 8)) & 0x01ff;
    oam_addr <<= 1;
}

void oamdata_write(uint32_t effective_address, uint8_t value)
{
    oam[oam_addr] = value;
    oam_addr = (oam_addr + 1) % PPU_OAM_SIZE;
}

void update_bg_state()
{
    uint8_t bg_mode = ram1_regs[PPU_REG_BGMODE];
    uint8_t through_main = ram1_regs[PPU_REG_TMAIN];
    uint32_t last_main_background = 0;
    uint8_t through_sub = ram1_regs[PPU_REG_TSUB];

    struct bg_draw_t main_screen_backgrounds[5];
    struct bg_draw_t sub_backgrounds[5];

    backgrounds[0].chr_size = (bg_mode >> PPU_BGMODE_BG1_CHR_SIZE_SHIFT) && PPU_BGMODE_BG_CHR_SIZE_MASK;
    backgrounds[1].chr_size = (bg_mode >> PPU_BGMODE_BG2_CHR_SIZE_SHIFT) && PPU_BGMODE_BG_CHR_SIZE_MASK;
    backgrounds[2].chr_size = (bg_mode >> PPU_BGMODE_BG3_CHR_SIZE_SHIFT) && PPU_BGMODE_BG_CHR_SIZE_MASK;
    backgrounds[3].chr_size = (bg_mode >> PPU_BGMODE_BG4_CHR_SIZE_SHIFT) && PPU_BGMODE_BG_CHR_SIZE_MASK;

    bg_mode &= PPU_BGMODE_MODE_MASK;

    switch(bg_mode)
    {
        case PPU_BGMODE_MODE0:
        {
            struct mode0_cgram_t *mode0_cgram = (struct mode0_cgram_t *)cgram;
            backgrounds[0].pal_base = mode0_cgram->bg1_colors;
            backgrounds[1].pal_base = mode0_cgram->bg2_colors;
            backgrounds[2].pal_base = mode0_cgram->bg3_colors;
            backgrounds[3].pal_base = mode0_cgram->bg4_colors;

            main_screen_backgrounds[0] = (struct bg_draw_t){.background = &backgrounds[0], .color_func = bg_pal4_col};
            main_screen_backgrounds[1] = (struct bg_draw_t){.background = &backgrounds[1], .color_func = bg_pal4_col};
            main_screen_backgrounds[2] = (struct bg_draw_t){.background = &backgrounds[2], .color_func = bg_pal4_col};
            main_screen_backgrounds[3] = (struct bg_draw_t){.background = &backgrounds[3], .color_func = bg_pal4_col};

            last_main_background = 3;
        }
        break;

        case PPU_BGMODE_MODE1:
        {
            struct mode12_cgram_t *mode12_cgram = (struct mode12_cgram_t *)cgram;
            backgrounds[0].pal_base = mode12_cgram->bg12_colors;
            backgrounds[1].pal_base = mode12_cgram->bg12_colors;
            backgrounds[2].pal_base = mode12_cgram->bg3_colors;

            main_screen_backgrounds[0] = (struct bg_draw_t){.background = &backgrounds[0], .color_func = bg_pal16_col};
            main_screen_backgrounds[1] = (struct bg_draw_t){.background = &backgrounds[1], .color_func = bg_pal16_col};
            main_screen_backgrounds[2] = (struct bg_draw_t){.background = &backgrounds[2], .color_func = bg_pal4_col};

            last_main_background = 2;
        }
        break;

        case PPU_BGMODE_MODE5:
        case PPU_BGMODE_MODE6:
        {
            struct mode56_cgram_t *mode56_cgram = (struct mode56_cgram_t *)cgram;
            backgrounds[0].pal_base = mode56_cgram->bg1_colors;
            backgrounds[1].pal_base = mode56_cgram->bg2_colors;

            main_screen_backgrounds[0] = (struct bg_draw_t){.background = &backgrounds[0], .color_func = bg_pal16_col};
            main_screen_backgrounds[1] = (struct bg_draw_t){.background = &backgrounds[1], .color_func = bg_pal4_col};

            last_main_background = 1;
        }
        break;

        case PPU_BGMODE_MODE7:
        {
            struct mode7_cgram_t *mode7_cgram = (struct mode7_cgram_t *)cgram;
            backgrounds[0].pal_base = &mode7_cgram->bg1_colors;

            main_screen_backgrounds[0] = (struct bg_draw_t){.background = &backgrounds[0], .color_func = bg7_pal256_col};
            last_main_background = 0;
        }
        break;
    }

    main_screen_bg_count = 0;
    for(uint32_t index = 0; index <= last_main_background; index++)
    {
        uint32_t background_index = last_main_background - index;
        if(through_main & (PPU_TMAIN_FLAG_BG1 << background_index))
        {
            main_screen[main_screen_bg_count] = main_screen_backgrounds[background_index];
            main_screen_bg_count++;
        }
    }
}

void bgmode_write(uint32_t effective_address, uint8_t value)
{
    ram1_regs[PPU_REG_BGMODE] = value;
    update_bg_state();
}

void bgsc_write(uint32_t effective_address, uint8_t value)
{
    uint32_t reg = effective_address & 0xffff;
    ram1_regs[reg] = value;
    uint32_t offset = (((uint32_t)(value & 0xfc) << 8) & 0x7fff) << 1;

    uint32_t background_index = reg - PPU_REG_BG1SC;
    backgrounds[background_index].data_base = (struct bg_sc_data_t *)(vram + offset);
}

void bgnba_write(uint32_t effective_address, uint8_t value)
{
    uint32_t reg = effective_address & 0xffff;
    uint32_t background_index = (reg - PPU_REG_BG12NBA) << 1;
    ram1_regs[reg] = value;

    uint32_t offset = (((uint32_t)(value & 0x0f) << 12) & 0x7fff) << 1;
    struct background_t *background = backgrounds + background_index;
    background_index++;
    background->chr_base = vram + offset;

    offset = (((uint32_t)(value & 0xf0) << 8) & 0x7fff) << 1;
    background = backgrounds + background_index;
    background->chr_base = vram + offset;
}

void bgoffs_write(uint32_t effective_address, uint8_t value)
{
    uint32_t reg = (effective_address & 0xffff) - PPU_REG_BG1HOFS;
    /* two registers per background (h and v offsets), so bit 1 gives us the
    offset of the background we want to */
    // struct bg_offset_t *bg_offset = &bg_offsets[reg >> 1];
    struct background_t *background = backgrounds + (reg >> 1);
    /* for each background, the first register is the horizontal offset, the second
    is vertical offset. */
    uint32_t offset_index = reg & 1;

    if(background->offset.lsb_written[offset_index])
    {
        background->offset.offsets[offset_index] &= 0x00ff;
        background->offset.offsets[offset_index] |= ((uint16_t)value << 8) & 0x1fff;
    }
    else
    {
        background->offset.offsets[offset_index] &= 0xff00;
        background->offset.offsets[offset_index] |= value;
    }

    background->offset.lsb_written[offset_index] ^= 1;
}

void vmainc_write(uint32_t effective_address, uint8_t value)
{
    ram1_regs[PPU_REG_VMAINC] = value;
//    uint32_t bitdepth = (value >> PPU_VMAINC_BITDEPTH_SHIFT) & PPU_VMAINC_BITDEPTH_MASK;
//    bg_chr_dot_col = bg_chr_dot_col_funcs[bitdepth];
}

void tmain_write(uint32_t effective_address, uint8_t value)
{
    ram1_regs[PPU_REG_TMAIN] = value;
    update_bg_state();
}

void tsub_write(uint32_t effective_address, uint8_t value)
{
    ram1_regs[PPU_REG_TSUB] = value;
    update_bg_state();
}

void coldata_write(uint32_t effective_address, uint8_t value)
{
    ram1_regs[PPU_REG_COLDATA] = value;
}

void update_irq_counter()
{
    uint16_t vtimer = (uint16_t)ram1_regs[CPU_REG_VTIMEL] | (uint16_t)ram1_regs[CPU_REG_VTIMEH] << 8;
    uint16_t htimer = (uint16_t)ram1_regs[CPU_REG_VTIMEL] | (uint16_t)ram1_regs[CPU_REG_VTIMEH] << 8;

    switch(ram1_regs[CPU_REG_NMITIMEN] & (CPU_NMITIMEN_FLAG_HTIMER_EN | CPU_NMITIMEN_FLAG_VTIMER_EN))
    {
        case CPU_NMITIMEN_FLAG_HTIMER_EN:
            irq_count = SCANLINE_DOT_LENGTH;

            if(htimer >= hcounter)
            {
                cur_irq_counter = htimer - hcounter;
            }
            else
            {
                cur_irq_counter = irq_count - (hcounter - htimer);
            }
        break;

        case CPU_NMITIMEN_FLAG_VTIMER_EN:
            irq_count = SCANLINE_COUNT * SCANLINE_DOT_LENGTH;

            if(vtimer >= vcounter)
            {
                cur_irq_counter = (vtimer - vcounter) * SCANLINE_DOT_LENGTH;

                /* if only virq is enabled and the time irq counter value is zero, it means either the current scanline value got
                written to 0x4211 or virq got disabled and reenabled. In this case, the virq may be fired immediately if H = ~3.5*/
                if(!cur_irq_counter)
                {
                    if(hcounter <= 3)
                    {
                        /* hcounter is within the refire interval, so setup cur_irq_counter to trigger an irq at the
                        next ppu step */
                        irq_count -= hcounter;
                        /* this will be decremented at the start of the ppu update, triggering the irq. */
                        cur_irq_counter = 1;
                    }
                    else
                    {
                        /* we're outside the retrigger interval, so just update cur_irq_counter to fire next
                        frame, at H = ~3.5 */
                        cur_irq_counter = irq_count - (hcounter + 3);
                    }
                }
                else
                {
                    cur_irq_counter -= hcounter;
                }
            }
            else
            {
                cur_irq_counter = irq_count - (vcounter - vtimer) * SCANLINE_DOT_LENGTH - (hcounter + 3);
            }
        break;

        case CPU_NMITIMEN_FLAG_HTIMER_EN | CPU_NMITIMEN_FLAG_VTIMER_EN:
            irq_count = SCANLINE_COUNT * SCANLINE_DOT_LENGTH;

            if(vtimer >= vcounter)
            {
                cur_irq_counter = (vtimer - vcounter) * SCANLINE_DOT_LENGTH;
            }
            else
            {
                cur_irq_counter = irq_count - (vcounter - vtimer) * SCANLINE_DOT_LENGTH;
            }

            cur_irq_counter += htimer;

            if(htimer >= hcounter)
            {
                cur_irq_counter += htimer - hcounter;
            }
            else
            {
                cur_irq_counter -= hcounter - htimer;
            }

        break;
    }
}

void vhtime_write(uint32_t effective_address, uint8_t value)
{
    uint32_t reg = effective_address & 0xffff;
    ram1_regs[reg] = value;
    update_irq_counter();
}

void cgadd_write(uint32_t effective_address, uint8_t value)
{
    uint32_t reg = effective_address & 0xffff;
    ram1_regs[reg] = value;
    cgram_addr = value << 1;
}

uint8_t cgadd_read(uint32_t effective_address)
{

}

void cgdata_write(uint32_t effective_address, uint8_t value)
{
    if((ram1_regs[CPU_REG_HVBJOY] & (CPU_HVBJOY_FLAG_HBLANK | CPU_HVBJOY_FLAG_VBLANK)) || (ram1_regs[PPU_REG_INIDISP] & PPU_INIDISP_FLAG_FBLANK))
    {
        cgram[cgram_addr] = value;
        cgram_addr = (cgram_addr + 1) % PPU_CGRAM_SIZE;
    }
}

uint8_t cgdata_read(uint32_t effective_address)
{
    uint8_t value = 0;

    if((ram1_regs[CPU_REG_HVBJOY] & (CPU_HVBJOY_FLAG_HBLANK | CPU_HVBJOY_FLAG_VBLANK)) || (ram1_regs[PPU_REG_INIDISP] & PPU_INIDISP_FLAG_FBLANK))
    {
        value = cgram[cgram_addr];
        cgram_addr = (cgram_addr + 1) % PPU_CGRAM_SIZE;
    }

    return value;
}

void setinit_write(uint32_t effective_address, uint8_t value)
{
    ram1_regs[PPU_REG_SETINI] = value;
    if(value & PPU_SETINI_FLAG_BGV_SEL)
    {
        last_draw_scanline = 239;
    }
    else
    {
        last_draw_scanline = 224;
    }
}

void wmdata_write(uint32_t effective_address, uint8_t value)
{
    write_byte(wmdata_address, value);
    wmdata_address++;
}

uint8_t wmdata_read(uint32_t effective_address)
{
    uint8_t value = read_byte(wmdata_address);
    wmdata_address++;
    return value;
}

void wmadd_write(uint32_t effective_address, uint8_t value)
{
    uint32_t reg = effective_address & 0xffff;
    ram1_regs[reg] = value;
    wmdata_address = (uint32_t)ram1_regs[PPU_REG_WMADDL] | ((uint32_t)ram1_regs[PPU_REG_WMADDM] << 8) | ((uint32_t)ram1_regs[PPU_REG_WMADDH] << 16);
    wmdata_address = PPU_WMDATA_BASE | (wmdata_address & PPU_WMADDR_MASK);
}

