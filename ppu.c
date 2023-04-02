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

uint16_t mhcounter = 0;
uint16_t vcounter = 0;
uint16_t hcounter = 0;
uint8_t  sub_dot = 0;
uint8_t  dot_length = 4;
uint16_t latched_counters[2];
uint32_t scanline_cycles = 0;
//uint64_t irq_cycle = 0xffffffff;
int32_t  irq_hold_timer = 0;
uint32_t cur_irq_counter = 0;
uint32_t irq_counter_reload = 0;
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

extern uint8_t *                    ram1_regs;
extern uint8_t *                    ram2;
extern uint64_t                     master_cycles;
uint32_t                            cur_field = 0;
int32_t                             ppu_cycle_count = 0;

uint16_t                            oam_addr = 0;
uint16_t                            draw_oam_addr = 0;
union oam_t                         oam;

uint16_t                            cgram_addr = 0;
uint8_t *                           cgram = NULL;

uint32_t                            vram_addr = 0;
uint8_t *                           vram = NULL;

struct background_t                 backgrounds[4];
float                               cur_brightness = 0.0;
struct dot_t                        cur_backdrop = {};

uint32_t                            main_screen_bg_count = 0;
struct background_t *               main_screen[5];
//struct bg_draw_t                    main_screen[5];
uint32_t                            sub_screen_bg_count = 0;
struct bg_draw_t                    sub_screen[5];

uint16_t                            rot_values_bytes = 0;
int16_t                             rot_values[4];
int16_t                             pos_values[2];

struct obj_t                        objects[128];
//struct dot_objs_t *                 line_objects;
//struct dot_obj_priorities_t *       scanline_objs;

struct draw_tile_t *                obj_tiles;
uint32_t                            obj_tile_count;

struct draw_tile_t *                bg_tiles;
uint32_t                            bg_tile_count;

struct dot_tiles_t *                main_scanline_tiles;
struct dot_tiles_t *                sub_scanline_tiles;


//struct tile_dot_priorities_t        main_screen_tiles;
int32_t                             vram_offset = 0;
uint8_t                             vram_prefetch[2];
uint8_t                             ppu1_last_bus_value = 0;
uint8_t                             ppu2_last_bus_value = 0;
extern uint8_t                      last_bus_value;
//struct pal16e_t                     obj_palletes[8];
//struct layer_dot_priorities_t   main_screen;
//struct layer_dot_priorities_t   sub_screen;


uint8_t (*chr_dot_funcs[4])(void *chr_base, uint32_t index, uint32_t dot_h, uint32_t dot_v) = {
    [COLOR_FUNC_CHR0] = chr0_dot,
    [COLOR_FUNC_CHR2] = chr2_dot,
    [COLOR_FUNC_CHR4] = chr4_dot,
    [COLOR_FUNC_CHR8] = chr8_dot,
};

struct col_t (*pal_col_funcs[4])(void *pal_base, uint8_t pallete, uint8_t index) = {
    [COLOR_FUNC_CHR0] = NULL,
    [COLOR_FUNC_CHR2] = pal4_col,
    [COLOR_FUNC_CHR4] = pal16_col,
    [COLOR_FUNC_CHR8] = pal256_col,
};

uint32_t                            line_obj_count = 0;
uint32_t                            line_chr_count = 0;
//struct line_obj_t   line_objs[128];
struct chr4_t *                     obj_chr_base[2];
// void  *             obj_chr_base[2];
// uint8_t             line_objs[128];

// uint32_t

// uint8_t cgram[512];

uint32_t vmadd_increment_shifts[] = {
    [PPU_VMADD_INC1]    = 0,
    [PPU_VMADD_INC32]   = 5,
    [PPU_VMADD_INC64]   = 6,
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

uint32_t cur_obj_sizes[2]           = {8, 16};

void init_ppu()
{
//    framebuffer = calloc(FRAMEBUFFER_WIDTH * FRAMEBUFFER_HEIGHT, sizeof(struct dot_t));
//    oam = calloc(1, PPU_OAM_SIZE);
    cgram = calloc(1, PPU_CGRAM_SIZE);
    vram = calloc(1, PPU_VRAM_SIZE << 1);
//    line_objects = calloc(256, sizeof(struct dot_objs_t));

//    scanline_obj_tiles = calloc(SCANLINE_DOT_LENGTH, sizeof(struct dot_obj_priorities_t));
    /* worst case scenario where all objs are 32x32 wide */
    obj_tiles = calloc(MAX_OBJ_COUNT * 16, sizeof(struct draw_tile_t));
    bg_tiles = calloc(8, sizeof(struct draw_tile_t));
    main_scanline_tiles = calloc(SCANLINE_DOT_LENGTH, sizeof(struct dot_tiles_t));
    sub_scanline_tiles = calloc(SCANLINE_DOT_LENGTH, sizeof(struct dot_tiles_t));
    last_draw_scanline = 224;
}

void shutdown_ppu()
{
//    free(framebuffer);
    free(obj_tiles);
    free(main_scanline_tiles);
    free(sub_scanline_tiles);
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

    obj_chr_base[0] = (struct chr4_t *)vram;
    obj_chr_base[1] = (struct chr4_t *)vram;

    ram1_regs[PPU_REG_STAT77] = 0;
    ram1_regs[PPU_REG_STAT78] = 0;

//    clear_scanline_objs();
}

uint8_t bg_chr0_dot_col(void *chr_base, uint32_t index, uint32_t size16, uint32_t dot_h, uint32_t dot_v)
{
    return 0;
}

uint8_t chr0_dot(void *chr_base, uint32_t name, uint32_t dot_x, uint32_t dot_y)
{
    return 0;
}

uint8_t bg_chr2_dot_col(void *chr_base, uint32_t index, uint32_t size, uint32_t dot_h, uint32_t dot_v)
{
//    uint32_t chr_size = size;
    uint32_t chr_dot_h = dot_h & 0x07;
    uint32_t chr_dot_v = dot_v & 0x07;
    uint8_t color_index;

//    index += (dot_h >> 3);
//    index += (dot_v >> 3) << 4;
    uint32_t offset = 8 * 2 * index;

//    struct chr2_t *chr = (struct chr2_t *)chr_base + index;
    struct chr2_t *chr = (struct chr2_t *)((uint8_t *)chr_base + offset);
    color_index  =  (chr->p01[chr_dot_v] & (0x80 >> chr_dot_h)) && 1;
    color_index |= ((chr->p01[chr_dot_v] & (0x8000 >> chr_dot_h)) && 1) << 1;
    return color_index;
}

uint8_t chr2_dot(void *chr_base, uint32_t name, uint32_t dot_x, uint32_t dot_y)
{
    struct chr2_t *chr = (struct chr2_t *)chr_base + name;
    uint8_t color_index;
    color_index  =  (chr->p01[dot_y] & (0x80 >> dot_x)) && 1;
    color_index |= ((chr->p01[dot_y] & (0x8000 >> dot_x)) && 1) << 1;
    return color_index;
}

struct col_t pal4_col(void *pal_base, uint8_t pallete, uint8_t index)
{
    struct pal4_t *pal = (struct pal4_t *)pal_base;
    uint16_t packed_color = pal[pallete].colors[index];
    struct col_t color;
    color.r = color_lut[(packed_color >> COL_DATA_R_SHIFT) & COL_DATA_MASK];
    color.g = color_lut[(packed_color >> COL_DATA_G_SHIFT) & COL_DATA_MASK];
    color.b = color_lut[(packed_color >> COL_DATA_B_SHIFT) & COL_DATA_MASK];
    return color;
}

uint8_t bg_chr4_dot_col(void *chr_base, uint32_t index, uint32_t size, uint32_t dot_h, uint32_t dot_v)
{
//    uint32_t chr_size = 1 << (3 + size16);
//    uint32_t chr_size = size;
    uint32_t chr_dot_h = dot_h & 0x07;
    uint32_t chr_dot_v = dot_v & 0x07;
    uint8_t color_index;

//    index += (dot_h >> 3);
//    index += (dot_v >> 3) << 4;

    uint32_t offset = 8 * 4 * index;

    uint16_t mask1 = 0x80 >> chr_dot_h;
    uint16_t mask2 = 0x8000 >> chr_dot_h;

//    struct chr4_t *chr = (struct chr4_t *)chr_base + index;
    struct chr4_t *chr = (struct chr4_t *)((uint8_t *)chr_base + offset);
    color_index  =  (chr->p01[chr_dot_v] & mask1) && 1;
    color_index |= ((chr->p01[chr_dot_v] & mask2) && 1) << 1;
    color_index |= ((chr->p23[chr_dot_v] & mask1) && 1) << 2;
    color_index |= ((chr->p23[chr_dot_v] & mask2) && 1) << 3;
    return color_index;
}

uint8_t chr4_dot(void *chr_base, uint32_t name, uint32_t dot_x, uint32_t dot_y)
{
    uint16_t mask1 = 0x80 >> dot_x;
    uint16_t mask2 = 0x8000 >> dot_x;

    struct chr4_t *chr = (struct chr4_t *)chr_base + name;
    uint8_t color_index;
    color_index  =  (chr->p01[dot_y] & mask1) && 1;
    color_index |= ((chr->p01[dot_y] & mask2) && 1) << 1;
    color_index |= ((chr->p23[dot_y] & mask1) && 1) << 2;
    color_index |= ((chr->p23[dot_y] & mask2) && 1) << 3;
    return color_index;
}

struct col_t pal16_col(void *pal_base, uint8_t pallete, uint8_t index)
{
    struct pal16_t *pal = (struct pal16_t *)pal_base;
    uint16_t packed_color = pal[pallete].colors[index];
    struct col_t color;
    color.r = color_lut[(packed_color >> COL_DATA_R_SHIFT) & COL_DATA_MASK];
    color.g = color_lut[(packed_color >> COL_DATA_G_SHIFT) & COL_DATA_MASK];
    color.b = color_lut[(packed_color >> COL_DATA_B_SHIFT) & COL_DATA_MASK];
    return color;
}

uint8_t bg_chr8_dot_col(void *chr_base, uint32_t index, uint32_t size, uint32_t dot_h, uint32_t dot_v)
{
//    uint32_t chr_size = 1 << (3 + size16);
    uint32_t chr_dot_h = dot_h & 0x07;
    uint32_t chr_dot_v = dot_v & 0x07;
    uint8_t color_index;

//    index += (dot_h % chr_size) > 7;
//    index += ((dot_v % chr_size) > 7) << 4;

//    index += (dot_h >> 3);
//    index += (dot_v >> 3) << 4;

    uint32_t offset = 8 * 8 * index;

    uint16_t mask1 = 0x80 >> chr_dot_h;
    uint16_t mask2 = 0x8000 >> chr_dot_h;

//    struct chr8_t *chr = (struct chr8_t *)chr_base + index;
    struct chr8_t *chr = (struct chr8_t *)((uint8_t *)chr_base + offset);
    color_index  =  (chr->p01[chr_dot_v] & mask1) && 1;
    color_index |= ((chr->p01[chr_dot_v] & mask2) && 1) << 1;
    color_index |= ((chr->p23[chr_dot_v] & mask1) && 1) << 2;
    color_index |= ((chr->p23[chr_dot_v] & mask2) && 1) << 3;
    color_index |= ((chr->p45[chr_dot_v] & mask1) && 1) << 4;
    color_index |= ((chr->p45[chr_dot_v] & mask2) && 1) << 5;
    color_index |= ((chr->p67[chr_dot_v] & mask1) && 1) << 6;
    color_index |= ((chr->p67[chr_dot_v] & mask2) && 1) << 7;
    return color_index;
}

uint8_t chr8_dot(void *chr_base, uint32_t name, uint32_t dot_x, uint32_t dot_y)
{
    uint16_t mask1 = 0x80 >> dot_x;
    uint16_t mask2 = 0x8000 >> dot_x;

    struct chr8_t *chr = (struct chr8_t *)chr_base + name;
    uint8_t color_index;
    color_index  =  (chr->p01[dot_y] & mask1) && 1;
    color_index |= ((chr->p01[dot_y] & mask2) && 1) << 1;
    color_index |= ((chr->p23[dot_y] & mask1) && 1) << 2;
    color_index |= ((chr->p23[dot_y] & mask2) && 1) << 3;
    color_index |= ((chr->p45[dot_y] & mask1) && 1) << 4;
    color_index |= ((chr->p45[dot_y] & mask2) && 1) << 5;
    color_index |= ((chr->p67[dot_y] & mask1) && 1) << 6;
    color_index |= ((chr->p67[dot_y] & mask2) && 1) << 7;
    return color_index;
}

struct col_t pal256_col(void *pal_base, uint8_t pallete, uint8_t index)
{
    struct pal256_t *pal = (struct pal4_t *)pal_base;
    uint16_t packed_color = pal[pallete].colors[index];
    struct col_t color;
    color.r = color_lut[(packed_color >> COL_DATA_R_SHIFT) & COL_DATA_MASK];
    color.g = color_lut[(packed_color >> COL_DATA_G_SHIFT) & COL_DATA_MASK];
    color.b = color_lut[(packed_color >> COL_DATA_B_SHIFT) & COL_DATA_MASK];
    return color;
}

// uint8_t bg7_chr8_dot_col(void *chr_base, uint32_t index, uint32_t dot_h, uint32_t dot_v)
// {
//     uint32_t chr_dot_h = dot_h % 8;
//     uint32_t chr_dot_v = dot_v % 8;
//     index <<= 6;

// }

struct bg_tile_t bg_tile_entry(uint32_t dot_h, uint32_t dot_v, struct background_t *background)
{
    int16_t tile_dot_x = dot_h & (background->chr_size - 1);
    int16_t tile_dot_y = dot_v & (background->chr_size - 1);
    uint32_t tile_x = (dot_h / background->chr_size) & 0x3f;
    uint32_t tile_y = (dot_v / background->chr_size) & 0x3f;
    uint32_t screen = ((tile_x >> 5) & 0x1) + ((tile_y >> 4) & 0x2);

    tile_x &= 0x1f;
    tile_y &= 0x1f;

    uint32_t data_index = tile_x + (tile_y << 5);
    struct bg_sc_data_t *bg_data = background->data_base[screen] + data_index + vram_offset;

    if(bg_data->data & (BG_SC_DATA_FLIP_MASK << BG_SC_DATA_H_FLIP_SHIFT))
    {
        tile_dot_x = (background->chr_size - 1) - tile_dot_x;
    }

    if(bg_data->data & (BG_SC_DATA_FLIP_MASK << BG_SC_DATA_V_FLIP_SHIFT))
    {
        tile_dot_y = (background->chr_size - 1) - tile_dot_y;
    }

//    tile_dot_x = dot_h;
//    tile_dot_y = dot_v;

    struct bg_tile_t tile = {
        .chr_index      = bg_data->data & BG_SC_DATA_NAME_MASK,
        .pal_index      = (bg_data->data >> BG_SC_DATA_PAL_SHIFT) & BG_SC_DATA_PAL_MASK,
        .tile_dot_x     = tile_dot_x,
        .tile_dot_y     = tile_dot_y,
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
//    uint8_t color_index = bg_chr2_dot_col(background->chr_base, tile.chr_index, background->chr_size, dot_h, dot_v);
    uint8_t color_index = bg_chr2_dot_col(background->chr_base, tile.chr_index, background->chr_size, tile.tile_dot_x, tile.tile_dot_y);

    if(color_index == 0)
    {
        return 0xffff;
    }

    return palletes[tile.pal_index].colors[color_index];
}

uint16_t bg_pal16_col(uint32_t dot_h, uint32_t dot_v, struct background_t *background)
{
    struct pal16_t *palletes = (struct pal16_t *)background->pal_base;
    struct bg_tile_t tile = bg_tile_entry(dot_h, dot_v, background);
//    uint8_t color_index = bg_chr4_dot_col(background->chr_base, tile.chr_index, background->chr_size, dot_h, dot_v);
    uint8_t color_index = bg_chr4_dot_col(background->chr_base, tile.chr_index, background->chr_size, tile.tile_dot_x, tile.tile_dot_y);

    if(color_index == 0)
    {
        return 0xffff;
    }

    return palletes[tile.pal_index].colors[color_index];
}

uint16_t bg_pal256_col(uint32_t dot_h, uint32_t dot_v, struct background_t *background)
{
    struct pal256_t *palletes = (struct pal256_t *)background->pal_base;
    struct bg_tile_t tile = bg_tile_entry(dot_h, dot_v, background);
//    uint8_t color_index = bg_chr8_dot_col(background->chr_base, tile.chr_index, background->chr_size, dot_h, dot_v);
    uint8_t color_index = bg_chr8_dot_col(background->chr_base, tile.chr_index, background->chr_size, tile.tile_dot_x, tile.tile_dot_y);

    if(color_index == 0)
    {
        return 0xffff;
    }

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

uint16_t obj_pal16_col(uint32_t dot_h, uint32_t dot_v, struct obj_t *obj)
{
//    struct oam_t *oam_tables = (struct oam_t *)oam;
//    struct obj_attr1_t *attr1 = oam_tables->table1 + obj->index;
//    uint32_t chr_name = attr1->fpcn & OBJ_ATTR1_NAME_MASK;
//    void *chr_base = obj_chr_base[(chr_name & 0x100) != 0];
//
////    uint32_t chr_size = 1 << (3 + size16);
//    uint32_t chr_dot_h = dot_h % 8;
//    uint32_t chr_dot_v = dot_v % 8;
//    uint8_t color_index;
//
//    return color_index;
}

//void update_objs()
//{
//    struct oam_t *oam_tables = (struct oam_t *)oam;
//
//    for(uint32_t index = 0; index < 128; index++)
//    {
//        struct obj_attr1_t *attr1 = oam_tables->table1 + index;
//        struct obj_attr2_t *attr2 = oam_tables->table2 + (index >> 3);
//        struct obj_t *obj = objects + index;
//        uint16_t size_pos = (attr2->size_hpos >> ((index & 0x07) << 1)) & OBJ_ATTR2_DATA_MASK;
//
//        obj->hpos = (uint16_t)attr1->h_pos | ((size_pos & 1) << 8);
//        obj->vpos = attr1->v_pos;
//        obj->name = attr1->fpcn & OBJ_ATTR1_NAME_MASK;
//        obj->size = obj_sizes[size_pos >> 1];
//        obj->vflip = (attr1->fpcn & OBJ_ATTR1_VFLIP) && 1;
//        obj->hflip = (attr1->fpcn & OBJ_ATTR1_HFLIP) && 1;
//        obj->pal = (attr1->fpcn >> OBJ_ATTR1_PAL_SHIFT) & OBJ_ATTR1_PAL_MASK;
//
//        int16_t obj_end_hpos = obj->hpos + obj->size;
//        int16_t obj_end_vpos = obj->vpos + obj->size;
//
//        if(obj->hpos > DRAW_END_DOT || obj_end_hpos < DRAW_START_DOT ||
//           obj->vpos > last_draw_scanline || obj_end_vpos < DRAW_START_LINE)
//        {
//            obj->size = 0;
//            continue;
//        }
//
//        int16_t start_dot = obj->hpos - DRAW_START_DOT;
//
//        if(start_dot < 0)
//        {
//            obj->start_dot = 0;
//        }
//        else
//        {
//            obj->start_dot = start_dot;
//        }
//
//        int16_t end_dot = start_dot + obj->size;
//
//        if(end_dot > DRAW_END_DOT - DRAW_START_DOT)
//        {
//            obj->end_dot = DRAW_END_DOT - DRAW_START_DOT;
//        }
//        else
//        {
//            obj->end_dot = end_dot;
//        }
//    }
//}

//void clear_scanline_objs()
//{
//    for(uint32_t index = 0; index < SCANLINE_DOT_LENGTH; index++)
//    {
//        scanline_objs[index].priorities[0].count = 0;
//        scanline_objs[index].priorities[1].count = 0;
//        scanline_objs[index].priorities[2].count = 0;
//        scanline_objs[index].priorities[3].count = 0;
//    }
//}

//void update_scanline_objs(uint16_t line)
//{
//    for(uint32_t index = 0; index < 128; index++)
//    {
//        union obj_attr1_t *attr1 = oam.tables.table1 + index;
//        struct obj_attr2_t *attr2 = oam.tables.table2 + (index >> 3);
////        union obj_attr1_t *attr1 = table1 + index;
////        struct obj_attr2_t *attr2 = table2 + (index >> 3);
//        struct obj_t *obj = objects + index;
//        uint16_t size_pos = (attr2->size_hpos >> ((index & 0x07) << 1)) & OBJ_ATTR2_DATA_MASK;
//
//        obj->hpos = (uint16_t)attr1->h_pos | ((size_pos & 1) << 8);
//
//        if(obj->hpos & 0x100)
//        {
//            obj->hpos = -obj->hpos;
//        }
//
//        obj->vpos = (uint16_t)attr1->v_pos;
//        obj->name = attr1->fpcn & OBJ_ATTR1_NAME_MASK;
//        obj->size = obj_sizes[size_pos >> 1];
////        obj->vflip = (attr1->fpcn & OBJ_ATTR1_VFLIP) && 1;
////        obj->hflip = (attr1->fpcn & OBJ_ATTR1_HFLIP) && 1;
//        obj->pal = (attr1->fpcn >> OBJ_ATTR1_PAL_SHIFT) & OBJ_ATTR1_PAL_MASK;
//        uint8_t priority = (attr1->fpcn >> OBJ_ATTR1_PRI_SHIFT) & OBJ_ATTR1_PRI_MASK;
//
//        int16_t obj_end_hpos = obj->hpos + (obj->size);
//        int16_t obj_end_vpos = obj->vpos + (obj->size);
//
//        if(obj->hpos > DRAW_END_DOT || obj_end_hpos <= DRAW_START_DOT ||
//           obj->vpos > line || obj_end_vpos <= line)
//        {
//            obj->size = 0;
//            continue;
//        }
//
//        int16_t start_dot = obj->hpos - DRAW_START_DOT;
//
//        if(start_dot < 0)
//        {
//            start_dot = 0;
//        }
//
//        int16_t end_dot = start_dot + obj->size;
//
//        if(end_dot > DRAW_END_DOT - DRAW_START_DOT)
//        {
//            end_dot = DRAW_END_DOT - DRAW_START_DOT;
//        }
//
//        for(uint32_t dot = start_dot; dot < end_dot; dot++)
//        {
//            struct dot_objs_t *dot_objs = scanline_objs[dot].priorities + priority;
//            dot_objs->objects[dot_objs->count] = index;
//            dot_objs->count++;
//        }
//
//        /* move the obj position forward by its size in case it's flipped.
//        This allows computing mirrored obj pixel coords without condintionals. */
//        if(attr1->fpcn & OBJ_ATTR1_HFLIP)
//        {
//            obj->hpos += obj->size - 1;
//        }
//
//        if(attr1->fpcn & OBJ_ATTR1_VFLIP)
//        {
//            obj->vpos += obj->size - 1;
//        }
//    }
//}

void update_scanline_obj_tiles(uint16_t line)
{
    obj_tile_count = 0;

    uint16_t chr_names[4];

    struct dot_tiles_t *dot_tiles = NULL;

    if(ram1_regs[PPU_REG_TMAIN] & PPU_TMAIN_FLAG_OBJ)
    {
        dot_tiles = main_scanline_tiles;
    }
    else if(ram1_regs[PPU_REG_TSUB] & PPU_TSUB_FLAG_OBJ)
    {
        dot_tiles = sub_scanline_tiles;
    }
    else
    {
        return;
    }

    for(uint32_t index = 0; index < 128; index++)
    {
        union obj_attr1_t *attr1 = oam.tables.table1 + index;
        struct obj_attr2_t *attr2 = oam.tables.table2 + (index >> 3);

        uint16_t size_pos = (attr2->size_hpos >> ((index & 0x07) << 1)) & OBJ_ATTR2_DATA_MASK;
        int16_t hpos = (uint16_t)attr1->h_pos | ((size_pos & 1) << 8);

        if(hpos & 0x100)
        {
            hpos = -hpos;
        }

        int16_t vpos = (uint16_t)attr1->v_pos;
        uint16_t name = attr1->fpcn & OBJ_ATTR1_NAME_MASK;
        int16_t size = cur_obj_sizes[size_pos >> 1];

        uint16_t pal = (attr1->fpcn >> OBJ_ATTR1_PAL_SHIFT) & OBJ_ATTR1_PAL_MASK;
        uint8_t priority = (attr1->fpcn >> OBJ_ATTR1_PRI_SHIFT) & OBJ_ATTR1_PRI_MASK;

        int16_t obj_end_hpos = hpos + size;
        int16_t obj_end_vpos = vpos + size;

        if(hpos > DRAW_END_DOT || obj_end_hpos <= DRAW_START_DOT ||
           vpos > line || obj_end_vpos <= (int16_t)line)
        {
            continue;
        }

        int16_t start_dot = hpos - DRAW_START_DOT;

        if(start_dot < 0)
        {
            start_dot = 0;
        }

        int16_t end_dot = start_dot + size;

        if(end_dot > DRAW_END_DOT - DRAW_START_DOT)
        {
            end_dot = DRAW_END_DOT - DRAW_START_DOT;
        }

        uint32_t tile_count = size / TILE_SIZE;
        int16_t tile_y_index = (line - vpos) / TILE_SIZE;
        int16_t tile_start_dot_y = vpos + (tile_y_index * TILE_SIZE);

        if(attr1->fpcn & OBJ_ATTR1_VFLIP)
        {
            name += ((tile_count - 1) - tile_y_index) << 4;
            tile_start_dot_y += TILE_SIZE - 1;
        }
        else
        {
            name += tile_y_index << 4;
        }


        int16_t tile_start_dot_x = start_dot;
        uint32_t first_obj_tile = obj_tile_count;

        if(attr1->fpcn & OBJ_ATTR1_HFLIP)
        {
            name += tile_count - 1;
            tile_start_dot_x += TILE_SIZE - 1;

            chr_names[0] = name;
            chr_names[1] = name - 1;
            chr_names[2] = name - 2;
            chr_names[3] = name - 3;
        }
        else
        {
            chr_names[0] = name;
            chr_names[1] = name + 1;
            chr_names[2] = name + 2;
            chr_names[3] = name + 3;
        }

        for(uint32_t tile_index = 0; tile_index < tile_count; tile_index++)
        {
            struct draw_tile_t *tile = obj_tiles + obj_tile_count;
            obj_tile_count++;
            tile->start_x = tile_start_dot_x;
            tile->start_y = tile_start_dot_y;
            tile->pallete = pal;
            tile->name = chr_names[tile_index];
            tile->color_func = COLOR_FUNC_CHR4;
            tile_start_dot_x += TILE_SIZE;
        }

        uint32_t dot_count = 0;
        for(uint32_t dot = start_dot; dot < end_dot; dot++)
        {
            struct dot_obj_tiles_t *dot_obj_tiles = dot_tiles[dot].obj_tiles + priority;
            dot_obj_tiles->tiles[dot_obj_tiles->tile_count] = first_obj_tile + (dot_count >> 3);
            dot_obj_tiles->tile_count++;
            dot_count++;
        }
    }
}

void update_scanline_bg_tiles(uint16_t line, uint16_t dot)
{
    bg_tile_count = 0;

    for(uint32_t background_index = 0; background_index < main_screen_bg_count; background_index++)
    {
        struct background_t *background = backgrounds + background_index;

        int16_t bg_dot_x = ((int16_t)dot + (int16_t)background->offset.offsets[0]);
        int16_t bg_dot_y = ((int16_t)line + (int16_t)background->offset.offsets[1]);

        uint32_t tile_x = (bg_dot_x / background->chr_size) & 0x3f;
        uint32_t tile_y = (bg_dot_y / background->chr_size) & 0x3f;
        uint32_t screen = ((tile_x >> 5) & 0x1) + ((tile_y >> 4) & 0x2);

        tile_x &= 0x1f;
        tile_y &= 0x1f;

        uint32_t data_index = tile_x + (tile_y << 5);
        struct bg_sc_data_t *bg_data = background->data_base[screen] + data_index;
        int16_t tile_dot_x = bg_dot_x & TILE_SIZE;
        int16_t tile_dot_y = bg_dot_y & TILE_SIZE;
        uint16_t name = bg_data->data & BG_SC_DATA_NAME_MASK;
        uint16_t pallete = (bg_data->data >> BG_SC_DATA_PAL_SHIFT) & BG_SC_DATA_PAL_MASK;

        if(bg_data->data & (BG_SC_DATA_FLIP_MASK << BG_SC_DATA_H_FLIP_SHIFT))
        {
            tile_dot_x += TILE_SIZE - 1;
            name += background->chr_size > TILE_SIZE;
        }

        if(bg_data->data & (BG_SC_DATA_FLIP_MASK << BG_SC_DATA_V_FLIP_SHIFT))
        {
            tile_dot_y += TILE_SIZE - 1;
            name += (background->chr_size > TILE_SIZE) << 4;
        }

        struct draw_tile_t *tile = bg_tiles + bg_tile_count;
        bg_tile_count++;

        tile->start_x = tile_dot_x;
        tile->start_y = tile_dot_y;
        tile->name = name;
        tile->pallete = pallete;
    }
}

//void update_dot_objs(uint16_t line)
//{
//    // line_obj_count = 0;
//    line_chr_count = 0;
//
//    for(uint32_t index = 0; index < 128; index++)
//    {
//        // struct obj_attr1_t *attr1 = oam_tables->table1 + index;
//        // struct obj_attr2_t *attr2 = oam_tables->table2 + (index >> 3);
//        // uint16_t size_pos = (attr2->size_hpos >> ((index & 0x07) << 1)) & OBJ_ATTR2_DATA_MASK;
//        // uint16_t obj_size = obj_sizes[size_pos >> 1];
//
//        // uint16_t hpos = (uint16_t)attr1->h_pos | ((size_pos & 1) << 8);
//        // uint16_t vpos = attr1->v_pos;
//
//        struct obj_t *object = objects + index;
//
//        if(object->size && line >= object->vpos && line < object->vpos + object->size)
//        {
////            uint32_t start_index = object->hpos - DRAW_START_DOT;
////            uint32_t end_index = start_index + object->size;
////            if(end_index > DRAW_END_DOT)
////            {
////                end_index = DRAW_END_DOT;
////            }
//
//            for(int32_t dot_index = object->start_dot; dot_index < object->end_dot; dot_index++)
//            {
//                struct dot_objs_t *dot_objects = line_objects + dot_index;
//                dot_objects->objects[dot_objects->count] = index;
//                dot_objects->count++;
//            }
//        }
//
//
//        // if(line >= vpos && line < vpos + obj_size && hpos + obj_size > DRAW_START_DOT && hpos < DRAW_END_DOT)
//        // {
//        //     line_objs[line_obj_count] = (struct line_obj_t){
//        //         .vpos = vpos,
//        //         .hpos = hpos,
//        //         .size = obj_size,
//        //         .index = index
//        //     };
//        //     line_obj_count++;
//        //     line_chr_count += obj_size / 8;
//        // }
//    }
//}

uint32_t step_ppu(int32_t cycle_count)
{
    ppu_cycle_count += cycle_count;
    uint32_t vblank = 0;
    uint32_t irq_triggered = 0;
    while(ppu_cycle_count)
    {
        sub_dot++;
        scanline_cycles++;
        if(sub_dot == dot_length)
        {
            sub_dot = 0;
            hcounter++;
            cur_irq_counter--;
//            scanline_cycles += dot_length;

            if(hcounter == SCANLINE_DOT_LENGTH)
            {
                vcounter = (vcounter + 1) % SCANLINE_COUNT;
                hcounter = 0;
                scanline_cycles = 0;

                if(line_obj_count > 32)
                {
                   ram1_regs[PPU_REG_STAT77] |= PPU_STAT77_FLAG_33_RANGE_OVER;
                }

                if(line_chr_count > 34)
                {
                    ram1_regs[PPU_REG_STAT77] |= PPU_STAT77_FLAG_35_TIME_OVER;
                }
            }

            if(hcounter == 0 && vcounter < last_draw_scanline)
            {
//                update_scanline_objs(vcounter);
                update_scanline_obj_tiles(vcounter);
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

            if(!cur_irq_counter)
            {
                if(ram1_regs[CPU_REG_NMITIMEN] & (CPU_NMITIMEN_FLAG_HTIMER_EN | CPU_NMITIMEN_FLAG_VTIMER_EN))
                {
                    ram1_regs[CPU_REG_TIMEUP] |= 1 << 7;
                    irq_triggered = 1;
                    irq_hold_timer = 8;
                }

                cur_irq_counter = irq_counter_reload;
            }

            if(irq_hold_timer > 0)
            {
                irq_hold_timer -= cycle_count;
                ram1_regs[CPU_REG_TIMEUP] |= 1 << 7;
            }

            uint8_t hvbjoy = ram1_regs[CPU_REG_HVBJOY];

            if(!(hvbjoy & CPU_HVBJOY_FLAG_VBLANK) && vcounter >= DRAW_START_LINE
               && vcounter < last_draw_scanline && hcounter <= DRAW_END_DOT)
            {
                update_scanline_bg_tiles(vcounter, hcounter);
                uint8_t inidisp = ram1_regs[PPU_REG_INIDISP];
//                float brightness = (float)(inidisp & 0xf) / 15.0;
                float brightness = cur_brightness;
                uint32_t dot_x = hcounter;
                uint32_t dot_y = vcounter - DRAW_START_LINE;

                if(ram1_regs[PPU_REG_INIDISP] & PPU_INIDISP_FLAG_FBLANK)
                {
                    brightness = 0.0;
                }

                struct mode0_cgram_t *mode0_cgram = (struct mode0_cgram_t *)cgram;
                struct dot_t *main_dot = framebuffer + dot_y * FRAMEBUFFER_WIDTH + dot_x;
                struct dot_t sub_dot = {};

                *main_dot = cur_backdrop;

                for(uint32_t index = 0; index < main_screen_bg_count; index++)
                {
//                    struct bg_draw_t *bg_draw = main_screen + index;
                    struct background_t *background = main_screen[index];
                    if(background == NULL)
                    {
                        continue;
                    }
                    uint16_t chr_size = background->chr_size;
                    int16_t bg_dot_x = ((int16_t)hcounter + (int16_t)background->offset.offsets[0]);
                    int16_t bg_dot_y = ((int16_t)vcounter + (int16_t)background->offset.offsets[1]);



                    if(bg_dot_x >= 0 && bg_dot_y >= 0)
                    {
                        uint16_t color = background->color_func(bg_dot_x, bg_dot_y, background);

//                        main_dot->r = color_lut[8 + bg_dot_x % 8];
//                        main_dot->g = color_lut[8 + bg_dot_y % 8];
//                        main_dot->b = 0;
//                        main_dot->b = color_lut[index << 2];

                        if(color != 0xffff)
                        {
                            main_dot->r = color_lut[(color >> COL_DATA_R_SHIFT) & COL_DATA_MASK];
                            main_dot->g = color_lut[(color >> COL_DATA_G_SHIFT) & COL_DATA_MASK];
                            main_dot->b = color_lut[(color >> COL_DATA_B_SHIFT) & COL_DATA_MASK];
                            break;
                        }
                    }
                }

//                struct oam_t *oam_tables = (struct oam_t *)oam;

//                if(ram1_regs[PPU_REG_TMAIN] & PPU_TMAIN_FLAG_OBJ)
//                {

//                if(irq_triggered)
//                {
//                    main_dot->r = 255;
//                    main_dot->g = 255;
//                    main_dot->b = 255;
//                }

                struct dot_tiles_t *dot_tiles = NULL;
                struct dot_t *obj_dot = NULL;

                if(ram1_regs[PPU_REG_TMAIN] & PPU_TMAIN_FLAG_OBJ)
                {
                    dot_tiles = main_scanline_tiles + hcounter;
                    obj_dot = main_dot;
                }
                else if(ram1_regs[PPU_REG_TSUB] & PPU_TSUB_FLAG_OBJ)
                {
                    dot_tiles = sub_scanline_tiles + hcounter;
                    obj_dot = &sub_dot;
                }

//                obj_dot = NULL;

                if(obj_dot)
                {
                    for(uint32_t priority = 4; priority > 0;)
                    {
                        priority--;
                        struct dot_obj_tiles_t *dot_obj_tiles = dot_tiles->obj_tiles + priority;

                        for(uint32_t dot_tile_index = 0; dot_tile_index < dot_obj_tiles->tile_count; dot_tile_index++)
                        {
                            struct draw_tile_t *tile = obj_tiles + dot_obj_tiles->tiles[dot_tile_index];
                            int16_t tile_dot_x = abs((int16_t)hcounter - (int16_t)tile->start_x);
                            int16_t tile_dot_y = abs((int16_t)vcounter - (int16_t)tile->start_y);

                            uint8_t color_index = chr_dot_funcs[tile->color_func](obj_chr_base[tile->name >= 0x100], tile->name, tile_dot_x, tile_dot_y);

                            if(color_index)
                            {
                                uint16_t color = mode0_cgram->obj_colors[tile->pallete].colors[color_index];
                                obj_dot->r = color_lut[(color >> COL_DATA_R_SHIFT) & COL_DATA_MASK];
                                obj_dot->g = color_lut[(color >> COL_DATA_G_SHIFT) & COL_DATA_MASK];
                                obj_dot->b = color_lut[(color >> COL_DATA_B_SHIFT) & COL_DATA_MASK];
                                priority = 0;
                                break;
                            }
                        }
                    }

                    for(uint32_t priority = 0; priority < 4; priority++)
                    {
                        dot_tiles->obj_tiles[priority].tile_count = 0;
                    }
                }

                main_dot->r *= brightness;
                main_dot->g *= brightness;
                main_dot->b *= brightness;
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

void dump_vram(uint32_t start, uint32_t end)
{
    if(start > PPU_VRAM_SIZE)
    {
        start = PPU_VRAM_SIZE;
    }

    if(end > PPU_VRAM_SIZE)
    {
        end = PPU_VRAM_SIZE;
    }

    if(start > end)
    {
        uint32_t temp = start;
        start = end;
        end = temp;
    }

    start &= 0xfffe;
    printf("----------- VRAM -----------\n");
    while(start < end)
    {
        uint32_t count = end - start;

        if(count > 16)
        {
            count = 16;
        }

        printf("%04x: ", start);

        for(uint32_t byte = 0; byte < count; byte++)
        {
            printf("%02x ", vram[start]);
            start++;
        }

        printf("\n");
    }

    printf("----------------------------\n");

//    for(uint32_t line = 0; line <= lines; line++)
//    {
//        for(uint32_t byte = 0; byte < 16 && ; byte++)
//        {
//            printf("")
//        }
//    }
}


void inidisp_write(uint32_t effective_address, uint8_t value)
{
    ram1_regs[PPU_REG_INIDISP] = value;
    cur_brightness = (float)(value & 0xf) / 15.0;
}

uint8_t inidisp_read(uint32_t effective_address)
{
    return last_bus_value;
}

/*
==================================================================================
==================================================================================
==================================================================================
*/

void objsel_write(uint32_t effective_address, uint8_t value)
{
    ram1_regs[PPU_REG_OBJSEL] = value;
    uint32_t obj_size_select = (value >> PPU_OBJSEL_SIZE_SHIFT) & PPU_OBJSEL_SIZE_MASK;
    cur_obj_sizes[0] = objsel_size_sel_sizes[obj_size_select][0];
    cur_obj_sizes[1] = objsel_size_sel_sizes[obj_size_select][1];
    uint32_t chr_base = ((value & PPU_OBJSEL_NAME_BASE_MASK) << 13) & 0x7fff;
    uint32_t name_sel = (chr_base + (((value >> PPU_OBJSEL_NAME_SEL_SHIFT) & PPU_OBJSEL_NAME_SEL_MASK) << 12)) & 0x7fff;
    obj_chr_base[0] = (struct chr4_t *)(vram + (chr_base << 1));
    obj_chr_base[1] = (struct chr4_t *)(vram + (name_sel << 1));
}

uint8_t objsel_read(uint32_t effective_address)
{
    return last_bus_value;
}

/*
==================================================================================
==================================================================================
==================================================================================
*/

void oamadd_write(uint32_t effective_address, uint8_t value)
{
    uint32_t reg = effective_address & 0xffff;
    ram1_regs[reg] = value;
    oam_addr = (((uint16_t)ram1_regs[PPU_REG_OAMADDL]) | (((uint16_t)ram1_regs[PPU_REG_OAMADDH] << 8) & 0x01ff));
    oam_addr <<= 1;
}

uint8_t oamadd_read(uint32_t effective_address)
{
    return last_bus_value;
}

/*
==================================================================================
==================================================================================
==================================================================================
*/

uint8_t opct_read(uint32_t effective_address)
{
    uint32_t reg = effective_address & 0xffff;
    uint32_t index = reg - PPU_REG_OPHCT;
    uint8_t value = latched_counters[index];
    latched_counters[index] >>= 8;
    return value;
}

/*
==================================================================================
==================================================================================
==================================================================================
*/

void oamdataw_write(uint32_t effective_address, uint8_t value)
{
    oam.bytes[oam_addr] = value;
    // uint32_t obj_index;

    // if(oam_addr < PPU_OAM_TABLE2_START)
    // {
    //     obj_index = oam_addr / sizeof(struct obj_attr1_t);
    // }
    // else
    // {
    //     obj_index = (oam_addr / (sizeof(struct obj_attr2_t) * 8)) + (oam_addr & 0x7);
    // }

    // struct oam_t *oam_tables = (struct oam_t *)oam;
    // struct obj_t *obj = objects + obj_index;
    // struct obj_attr1_t *attr1 = oam_tables->table1 + obj_index;
    // struct obj_attr2_t *attr2 = oam_tables->table2 + obj_index;

    // obj->vpos = oam_tables->table1[obj_index].;

    oam_addr = (oam_addr + 1) % PPU_OAM_SIZE;
}

uint8_t oamdataw_read(uint32_t effective_address)
{
    return ppu1_last_bus_value;
}

/*
==================================================================================
==================================================================================
==================================================================================
*/

uint8_t oamdatar_read(uint32_t effective_address)
{
    ppu1_last_bus_value = oam.bytes[oam_addr];
    oam_addr = (oam_addr + 1) % PPU_OAM_SIZE;
    return ppu1_last_bus_value;
}

void update_bg_state()
{
    uint8_t bg_mode = ram1_regs[PPU_REG_BGMODE];
    uint8_t through_main = ram1_regs[PPU_REG_TMAIN];
    uint32_t last_main_background = 0;
    uint8_t through_sub = ram1_regs[PPU_REG_TSUB];

//    struct bg_draw_t main_screen_backgrounds[5];
    struct background_t *main_screen_backgrounds[5];
    struct bg_draw_t sub_backgrounds[5];

    backgrounds[0].chr_size = 8 << ((bg_mode >> PPU_BGMODE_BG1_CHR_SIZE_SHIFT) & PPU_BGMODE_BG_CHR_SIZE_MASK);
    backgrounds[1].chr_size = 8 << ((bg_mode >> PPU_BGMODE_BG2_CHR_SIZE_SHIFT) & PPU_BGMODE_BG_CHR_SIZE_MASK);
    backgrounds[2].chr_size = 8 << ((bg_mode >> PPU_BGMODE_BG3_CHR_SIZE_SHIFT) & PPU_BGMODE_BG_CHR_SIZE_MASK);
    backgrounds[3].chr_size = 8 << ((bg_mode >> PPU_BGMODE_BG4_CHR_SIZE_SHIFT) & PPU_BGMODE_BG_CHR_SIZE_MASK);

    bg_mode &= PPU_BGMODE_MODE_MASK;

    switch(bg_mode)
    {
        case PPU_BGMODE_MODE0:
        {
            struct mode0_cgram_t *mode0_cgram = (struct mode0_cgram_t *)cgram;
            backgrounds[0].pal_base = mode0_cgram->bg1_colors;
            backgrounds[0].color_func = bg_pal4_col;

            backgrounds[1].pal_base = mode0_cgram->bg2_colors;
            backgrounds[1].color_func = bg_pal4_col;

            backgrounds[2].pal_base = mode0_cgram->bg3_colors;
            backgrounds[2].color_func = bg_pal4_col;

            backgrounds[3].pal_base = mode0_cgram->bg4_colors;
            backgrounds[3].color_func = bg_pal4_col;

            main_screen_backgrounds[0] = &backgrounds[0];
            main_screen_backgrounds[1] = &backgrounds[1];
            main_screen_backgrounds[2] = &backgrounds[2];
            main_screen_backgrounds[3] = &backgrounds[3];

            last_main_background = 3;
        }
        break;

        case PPU_BGMODE_MODE1:
        {
            struct mode12_cgram_t *mode12_cgram = (struct mode12_cgram_t *)cgram;
            backgrounds[0].pal_base = mode12_cgram->bg12_colors;
            backgrounds[0].color_func = bg_pal16_col;

            backgrounds[1].pal_base = mode12_cgram->bg12_colors;
            backgrounds[1].color_func = bg_pal16_col;

            backgrounds[2].pal_base = mode12_cgram->bg3_colors;
            backgrounds[2].color_func = bg_pal4_col;

//            main_screen_backgrounds[0] = &backgrounds[0];
//            main_screen_backgrounds[1] = &backgrounds[1];
//            main_screen_backgrounds[2] = &backgrounds[2];

            main_screen_backgrounds[0] = &backgrounds[0];
            main_screen_backgrounds[1] = &backgrounds[1];
            main_screen_backgrounds[2] = &backgrounds[2];

            last_main_background = 2;
        }
        break;

        case PPU_BGMODE_MODE5:
        case PPU_BGMODE_MODE6:
        {
            struct mode56_cgram_t *mode56_cgram = (struct mode56_cgram_t *)cgram;
            backgrounds[0].pal_base = mode56_cgram->bg1_colors;
            backgrounds[0].color_func = bg_pal16_col;

            backgrounds[1].pal_base = mode56_cgram->bg2_colors;
            backgrounds[1].color_func = bg_pal4_col;

            main_screen_backgrounds[0] = &backgrounds[0];
            main_screen_backgrounds[1] = &backgrounds[1];

            last_main_background = 1;
        }
        break;

        case PPU_BGMODE_MODE7:
        {
            struct mode7_cgram_t *mode7_cgram = (struct mode7_cgram_t *)cgram;
            backgrounds[0].pal_base = &mode7_cgram->bg1_colors;
            backgrounds[0].color_func = bg7_pal256_col;

            main_screen_backgrounds[0] = &backgrounds[0];

//            main_screen_backgrounds[0] = (struct bg_draw_t){.background = &backgrounds[0], .color_func = bg7_pal256_col};
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

/*
==================================================================================
==================================================================================
==================================================================================
*/

void bgmode_write(uint32_t effective_address, uint8_t value)
{
    ram1_regs[PPU_REG_BGMODE] = value;
    update_bg_state();
}

uint8_t bgmode_read(uint32_t effective_address)
{
    return ppu1_last_bus_value;
}

/*
==================================================================================
==================================================================================
==================================================================================
*/

void mosaic_write(uint32_t effective_address, uint8_t value)
{

}

uint8_t mosaic_read(uint32_t effective_address)
{
    return ppu1_last_bus_value;
}


/*
==================================================================================
==================================================================================
==================================================================================
*/

void bgsc_write(uint32_t effective_address, uint8_t value)
{
    uint32_t reg = effective_address & 0xffff;
    ram1_regs[reg] = value;
    value = (value >> 2) & 0x3f;
    uint32_t offset = value << 11;
//    uint32_t offset = ((uint32_t)value * 0x800 * 2);
//    uint32_t offset = (((uint32_t)(value & 0xfc) << 8) & 0x7fff) << 1;

    uint32_t background_index = reg - PPU_REG_BG1SC;
    struct background_t *background = backgrounds + background_index;
    background->data_base[0] = (struct bg_sc_data_t *)(vram + offset);

    /* snes programmer manual, vol 1, page A-14 */
    switch(value & 0x3)
    {
        case 0:
            background->data_base[1] = background->data_base[0];
            background->data_base[2] = background->data_base[0];
            background->data_base[3] = background->data_base[0];
        break;

        case 1:
            background->data_base[1] = (struct bg_sc_data_t *)((uintptr_t)(background->data_base[0]) + 0x800);
            background->data_base[2] = background->data_base[0];
            background->data_base[3] = background->data_base[1];
        break;

        case 2:
            background->data_base[1] = background->data_base[0];
            background->data_base[2] = (struct bg_sc_data_t *)((uintptr_t)(background->data_base[0]) + 0x800);
            background->data_base[3] = background->data_base[2];
        break;

        case 3:
            background->data_base[1] = (struct bg_sc_data_t *)((uintptr_t)(background->data_base[0]) + 0x800);
            background->data_base[2] = (struct bg_sc_data_t *)((uintptr_t)(background->data_base[0]) + 0x1000);
            background->data_base[3] = (struct bg_sc_data_t *)((uintptr_t)(background->data_base[0]) + 0x1800);
        break;
    }
}

uint8_t bgsc_read(uint32_t effective_address)
{
    if((effective_address & 0xffff) == PPU_REG_BG1SC)
    {
        return last_bus_value;
    }

    return ppu1_last_bus_value;
}

/*
==================================================================================
==================================================================================
==================================================================================
*/

void bgnba_write(uint32_t effective_address, uint8_t value)
{
    uint32_t reg = effective_address & 0xffff;
    uint32_t background_index = (reg - PPU_REG_BG12NBA) << 1;
    ram1_regs[reg] = value;

    uint32_t offset = (((uint32_t)(value & 0x0f) << 12) & 0x7fff) << 1;
//    uint32_t offset = ((uint32_t)(value & 0x0f)) * 0x2000;
    struct background_t *background = backgrounds + background_index;
    background_index++;
    background->chr_base = vram + offset;
//    value >>= 4;
    offset = (((uint32_t)(value & 0xf0) << 8) & 0x7fff) << 1;
//    offset = ((uint32_t)value) * 0x2000;
    background = backgrounds + background_index;
    background->chr_base = vram + offset;
}

uint8_t bgnba_read(uint32_t effective_address)
{
    return last_bus_value;
}

/*
==================================================================================
==================================================================================
==================================================================================
*/

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

uint8_t bgoffs_read(uint32_t effective_address)
{
    if((effective_address & 0xffff) < PPU_REG_BG4VOFS)
    {
        return last_bus_value;
    }
    return ppu1_last_bus_value;
}

/*
==================================================================================
==================================================================================
==================================================================================
*/

void vmainc_write(uint32_t effective_address, uint8_t value)
{
    ram1_regs[PPU_REG_VMAINC] = value;
//    uint32_t bitdepth = (value >> PPU_VMAINC_BITDEPTH_SHIFT) & PPU_VMAINC_BITDEPTH_MASK;
//    bg_chr_dot_col = bg_chr_dot_col_funcs[bitdepth];
}

uint8_t vmainc_read(uint32_t effective_address)
{
    return ppu1_last_bus_value;
}

/*
==================================================================================
==================================================================================
==================================================================================
*/

void vmadd_write(uint32_t effective_address, uint8_t value)
{
    uint32_t reg = effective_address & 0xffff;
    ram1_regs[reg] = value;
    vram_addr = (uint16_t)ram1_regs[PPU_REG_VMADDL] | (((uint16_t)ram1_regs[PPU_REG_VMADDH]) << 8);
    vram_read_prefetch();
}

uint8_t vmadd_read(uint32_t effective_address)
{
    return ppu1_last_bus_value;
}

/*
==================================================================================
==================================================================================
==================================================================================
*/

void vmdataw_write(uint32_t effective_address, uint8_t value)
{
    uint32_t write_order = ram1_regs[PPU_REG_VMAINC] & 0x80;
    uint32_t reg = effective_address & 0xffff;
    uint32_t write_addr = vram_addr << 1;
    uint32_t increment_shift = vmadd_increment_shifts[ram1_regs[PPU_REG_VMAINC] & 0x3];

    if(vram_addr == 35 || vram_addr == 0x00)
    {
        printf("holy shit...\n");
    }

//    if(ram1_regs[PPU_REG_VMAINC] & 0x0c)
//    {
//        printf("holy shit...\n");
//    }

    if((ram1_regs[CPU_REG_HVBJOY] & CPU_HVBJOY_FLAG_VBLANK) || (ram1_regs[PPU_REG_INIDISP] & PPU_INIDISP_FLAG_FBLANK))
    {
        write_addr |= PPU_REG_VMDATAWH == reg;
        vram[write_addr] = value;
        vram_addr += ((write_order == PPU_VMDATA_ADDR_INC_LH && reg == PPU_REG_VMDATAWH) ||
                      (write_order == PPU_VMDATA_ADDR_INC_HL && reg == PPU_REG_VMDATAWL)) << increment_shift;
        vram_addr &= 0x7fff;
    }
//    else
//    {
//        printf("write to vmdata blocked\n");
//    }
}

uint8_t vmdataw_read(uint32_t effective_address)
{
    return ppu1_last_bus_value;
}

/*
==================================================================================
==================================================================================
==================================================================================
*/

void m7sel_write(uint32_t effective_address, uint8_t value)
{

}

uint8_t m7sel_read(uint32_t effective_address)
{
    return ppu1_last_bus_value;
}

/*
==================================================================================
==================================================================================
==================================================================================
*/

void mrot_write(uint32_t effective_address, uint8_t value)
{
    uint32_t reg = effective_address & 0xffff;
    rot_values[reg] = ((uint16_t)value << 8) | ((rot_values[reg] & 0xff00) >> 8);
}

uint8_t mrot_read(uint32_t effective_address)
{
    return last_bus_value;
}

/*
==================================================================================
==================================================================================
==================================================================================
*/

void mpos_write(uint32_t effective_address, uint8_t value)
{
    uint32_t reg = (effective_address & 0xffff) - PPU_REG_M7X;
//    pos_values[reg] =
}

uint8_t mpos_read(uint32_t effective_address)
{

}

/*
==================================================================================
==================================================================================
==================================================================================
*/

void cgadd_write(uint32_t effective_address, uint8_t value)
{
    uint32_t reg = effective_address & 0xffff;
    ram1_regs[reg] = value;
    cgram_addr = value << 1;
}

uint8_t cgadd_read(uint32_t effective_address)
{
    return last_bus_value;
}

/*
==================================================================================
==================================================================================
==================================================================================
*/

void cgdataw_write(uint32_t effective_address, uint8_t value)
{
    if((ram1_regs[CPU_REG_HVBJOY] & (CPU_HVBJOY_FLAG_HBLANK | CPU_HVBJOY_FLAG_VBLANK)) || (ram1_regs[PPU_REG_INIDISP] & PPU_INIDISP_FLAG_FBLANK))
    {
        cgram[cgram_addr] = value;
        cgram_addr = (cgram_addr + 1) % PPU_CGRAM_SIZE;

        /* snes programmer manual, vol 1, page A-17 */
        if(cgram_addr <= 2)
        {
            /* address 0 of cgram is background, so recompute the backdrop color whenever
            the first two bytes of the cgram are modified */
            struct mode0_cgram_t *mode0_cgram = (struct mode0_cgram_t *)cgram;
            uint16_t backdrop = mode0_cgram->bg1_colors[0].colors[0];

            cur_backdrop.r = color_lut[(backdrop >> COL_DATA_R_SHIFT) & COL_DATA_MASK];
            cur_backdrop.g = color_lut[(backdrop >> COL_DATA_G_SHIFT) & COL_DATA_MASK];
            cur_backdrop.b = color_lut[(backdrop >> COL_DATA_B_SHIFT) & COL_DATA_MASK];
            cur_backdrop.a = 255;
        }
    }
}

uint8_t cgdataw_read(uint32_t effective_address)
{
    return last_bus_value;
}

/*
==================================================================================
==================================================================================
==================================================================================
*/

void wsel_write(uint32_t effective_address, uint8_t value)
{

}

uint8_t wsel_read(uint32_t effective_address)
{
    if((effective_address & 0xffff) == PPU_REG_W12SEL)
    {
        return last_bus_value;
    }

    return ppu1_last_bus_value;
}

/*
==================================================================================
==================================================================================
==================================================================================
*/

void wobjcolsel_write(uint32_t effective_address, uint8_t value)
{

}

uint8_t wobjcolsel_read(uint32_t effective_address)
{
    return ppu1_last_bus_value;
}

/*
==================================================================================
==================================================================================
==================================================================================
*/

void wlr_write(uint32_t effective_address, uint8_t value)
{

}

uint8_t wlr_read(uint32_t effective_address)
{
    if((effective_address & 0xffff) == PPU_REG_W1R)
    {
        return last_bus_value;
    }

    return ppu1_last_bus_value;
}

/*
==================================================================================
==================================================================================
==================================================================================
*/

void wbglog_write(uint32_t effective_address, uint8_t value)
{

}

uint8_t wbglog_read(uint32_t effective_address)
{
    return ppu1_last_bus_value;
}

/*
==================================================================================
==================================================================================
==================================================================================
*/

void wcolobjlog_write(uint32_t effective_address, uint8_t value)
{

}

uint8_t wcolobjlog_read(uint32_t effective_address)
{
    return last_bus_value;
}

/*
==================================================================================
==================================================================================
==================================================================================
*/

void tmainsub_write(uint32_t effective_address, uint8_t value)
{
    uint32_t reg = effective_address & 0xffff;
    ram1_regs[reg] = value;
    update_bg_state();
}

uint8_t tmainsub_read(uint32_t effective_address)
{
    return last_bus_value;
}

/*
==================================================================================
==================================================================================
==================================================================================
*/

void tmainsubwm_write(uint32_t effective_address, uint8_t value)
{
    uint32_t reg = effective_address & 0xffff;
    ram1_regs[reg] = value;
}

uint8_t tmainsubwm_read(uint32_t effective_address)
{
    return last_bus_value;
}

/*
==================================================================================
==================================================================================
==================================================================================
*/

void cgswsel_write(uint32_t effective_address, uint8_t value)
{

}

uint8_t cgswsel_read(uint32_t effective_address)
{
    return last_bus_value;
}

/*
==================================================================================
==================================================================================
==================================================================================
*/

void cgadsub_write(uint32_t effective_address, uint8_t value)
{

}

uint8_t cgadsub_read(uint32_t effective_address)
{
    return last_bus_value;
}

/*
==================================================================================
==================================================================================
==================================================================================
*/

void coldata_write(uint32_t effective_address, uint8_t value)
{
    ram1_regs[PPU_REG_COLDATA] = value;
}

uint8_t coldata_read(uint32_t effective_address)
{
    return last_bus_value;
}

/*
==================================================================================
==================================================================================
==================================================================================
*/

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

uint8_t setinit_read(uint32_t effective_address)
{
    return last_bus_value;
}

/*
==================================================================================
==================================================================================
==================================================================================
*/

uint8_t slhv_read(uint32_t effective_address)
{
    latched_counters[0] = hcounter;
    latched_counters[1] = vcounter;
    return last_bus_value;
}

/*
==================================================================================
==================================================================================
==================================================================================
*/

uint8_t cgdatar_read(uint32_t effective_address)
{
    uint8_t value = 0;

    if((ram1_regs[CPU_REG_HVBJOY] & (CPU_HVBJOY_FLAG_HBLANK | CPU_HVBJOY_FLAG_VBLANK)) || (ram1_regs[PPU_REG_INIDISP] & PPU_INIDISP_FLAG_FBLANK))
    {
        value = cgram[cgram_addr];

        if(cgram_addr & 1)
        {
            value |= ppu2_last_bus_value & 0x80;
        }

        ppu2_last_bus_value = value;

        cgram_addr = (cgram_addr + 1) % PPU_CGRAM_SIZE;
    }

    return value;
}

/*
==================================================================================
==================================================================================
==================================================================================
*/

uint8_t stat77_read(uint32_t effective_address)
{
    ppu1_last_bus_value = ram1_regs[PPU_REG_STAT77] | (ppu1_last_bus_value & 0x10);
    return ppu1_last_bus_value;
}

/*
==================================================================================
==================================================================================
==================================================================================
*/

uint8_t stat78_read(uint32_t effective_address)
{
    ppu2_last_bus_value = ram1_regs[PPU_REG_STAT78] | (ppu2_last_bus_value & 0x20);
    return ppu2_last_bus_value;
}

/*
==================================================================================
==================================================================================
==================================================================================
*/

void vram_read_prefetch()
{
    uint32_t read_address = vram_addr << 1;
    vram_prefetch[0] = vram[read_address];
    vram_prefetch[1] = vram[read_address + 1];
}

uint8_t vmdatar_read(uint32_t effective_address)
{
    uint32_t read_order = ram1_regs[PPU_REG_VMAINC] & 0x80;
    uint32_t reg = effective_address & 0xffff;
    uint32_t read_addr = vram_addr << 1;
    uint32_t increment_shift = vmadd_increment_shifts[ram1_regs[PPU_REG_VMAINC] & 0x3];

//    if(ram1_regs[PPU_REG_VMAINC] & 0x0c)
//    {
//        printf("holy shit...\n");
//    }

    if((ram1_regs[CPU_REG_HVBJOY] & V_BLANK_FLAG) || (ram1_regs[CPU_REG_HVBJOY] & H_BLANK_FLAG))
    {
//        read_addr |= PPU_REG_VMDATARH == reg;
//        value = vram[read_addr];
        ppu1_last_bus_value = vram_prefetch[PPU_REG_VMDATARH == reg];

        if((read_order == PPU_VMDATA_ADDR_INC_LH && reg == PPU_REG_VMDATARH) ||
           (read_order == PPU_VMDATA_ADDR_INC_HL && reg == PPU_REG_VMDATARL))
        {
            vram_read_prefetch();
            vram_addr += 1 << increment_shift;
            vram_addr &= 0x7fff;
        }

//        vram_addr += ((read_order == PPU_VMDATA_ADDR_INC_LH && reg == PPU_REG_VMDATARH) ||
//                      (read_order == PPU_VMDATA_ADDR_INC_HL && reg == PPU_REG_VMDATARL)) << increment_shift;
//        vram_addr &= 0x7fff;
    }

    return ppu1_last_bus_value;
}


void update_irq_counter()
{
    uint16_t vtimer = (uint16_t)ram1_regs[CPU_REG_VTIMEL] | ((uint16_t)ram1_regs[CPU_REG_VTIMEH] << 8);
    uint16_t htimer = (uint16_t)ram1_regs[CPU_REG_HTIMEL] | ((uint16_t)ram1_regs[CPU_REG_HTIMEH] << 8);

    switch(ram1_regs[CPU_REG_NMITIMEN] & (CPU_NMITIMEN_FLAG_HTIMER_EN | CPU_NMITIMEN_FLAG_VTIMER_EN))
    {
        case CPU_NMITIMEN_FLAG_HTIMER_EN:
            irq_counter_reload = SCANLINE_DOT_LENGTH;

            if(htimer >= hcounter)
            {
                cur_irq_counter = htimer - hcounter;
            }
            else
            {
                cur_irq_counter = irq_counter_reload - (hcounter - htimer);
            }
        break;

        case CPU_NMITIMEN_FLAG_VTIMER_EN:
            irq_counter_reload = SCANLINE_COUNT * SCANLINE_DOT_LENGTH;

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
                        irq_counter_reload -= hcounter;
                        /* this will be decremented at the start of the ppu update, triggering the irq. */
                        cur_irq_counter = 1;
                    }
                    else
                    {
                        /* we're outside the retrigger interval, so just update cur_irq_counter to fire next
                        frame, at H = ~3.5 */
                        cur_irq_counter = irq_counter_reload - (hcounter + 3);
                    }
                }
                else
                {
                    cur_irq_counter -= hcounter;
                }
            }
            else
            {
                cur_irq_counter = irq_counter_reload - (vcounter - vtimer) * SCANLINE_DOT_LENGTH - (hcounter + 3);
            }
        break;

        case CPU_NMITIMEN_FLAG_HTIMER_EN | CPU_NMITIMEN_FLAG_VTIMER_EN:
            irq_counter_reload = SCANLINE_COUNT * SCANLINE_DOT_LENGTH;

            if(vtimer >= vcounter)
            {
                cur_irq_counter = (vtimer - vcounter) * SCANLINE_DOT_LENGTH;
            }
            else
            {
                cur_irq_counter = irq_counter_reload - (vcounter - vtimer) * SCANLINE_DOT_LENGTH;
            }

            if(cur_irq_counter == 0)
            {
                if(htimer >= hcounter)
                {
                    cur_irq_counter = htimer - hcounter;
                }
                else
                {
                    cur_irq_counter = irq_counter_reload - (hcounter - htimer);
                }
            }
            else
            {
                cur_irq_counter -= hcounter;
                cur_irq_counter += htimer;
//                if(htimer >= hcounter)
//                {
//                    cur_irq_counter -= hcounter;
//                }
//                else
//                {
//                    cur_irq_counter = irq_counter_reloavoid cgadd_write(uint32_t effective_address, uint8_t value);d - (hcounter - htimer);
//                }
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



uint8_t mpy_read(uint32_t effective_address)
{
    uint32_t reg = effective_address & 0xffff;
    ppu1_last_bus_value = ram1_regs[reg];
    return ppu1_last_bus_value;
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

