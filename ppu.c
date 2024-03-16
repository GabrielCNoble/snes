#include <stdio.h>
#include <stdlib.h>
#include "SDL2/SDL.h"

#include "ppu.h"
#include "addr.h"
#include "cpu.h"
#include "mem.h"
#include "emu.h"



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
// uint8_t  sub_dot = 0;
// uint8_t  dot_length = 4;
uint32_t latched_counter_reads[2] = {};
uint16_t latched_counters[2];
// uint32_t scanline_cycles = 0;
//uint64_t irq_cycle = 0xffffffff;
int32_t  irq_hold_timer = 0;
uint32_t cur_irq_counter = 0;
uint32_t irq_counter_reload = 0;
uint32_t last_draw_scanline = 0;
uint32_t wmdata_address = 0;

// struct dot_t *framebuffer;
extern struct dot_t *emu_framebuffer;

uint32_t ppu_dot_length[] = {
    4, 6
};


uint8_t ppu_color_lut[] = {
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

// extern uint8_t *                    ram1_regs;
// extern uint8_t *                    ram2;
extern uint8_t *                    mem_regs;
// extern uint8_t *                    ram2;
extern uint64_t                     master_cycles;
// uint32_t                            cur_field = 0;
int32_t                             ppu_cycle_count = 0;
int32_t                             ppu_oam_scan_cycle_count = 0;



// uint16_t                            ppu_obj_low_word_latch;
union obj_attr1_t                   ppu_obj_low_attr;


// struct obj_attr2_t                  ppu_obj_high_attr;
// uint32_t                            ppu_scanline_potential_entry;

void (*ppu_SpriteStepFunc[])() = {
    [PPU_SPRITE_STEP_EVAL_OBJS]     = ppu_EvalObjsSpriteStep,
    [PPU_SPRITE_STEP_LOAD_TILES]    = ppu_LoadTilesSpriteStep,
    [PPU_SPRITE_STEP_NONE]          = ppu_NoneSpriteStep
};

uint32_t                            ppu_scanline_master_cycles = 0;
// uint32_t                            ppu_sprite_step = PPU_SPRITE_STEP_NONE;
// uint32_t                            ppu_sprite_substep = 0;
// uint16_t                            ppu_sprite_x_pos;

struct line_pixel_t                 ppu_line_buffer[256];

uint16_t                            ppu_reg_oam_addr = 0;
uint8_t                             ppu_oam_lsb_latch = 0;
uint8_t                             ppu_oam_lsb_toggle = 0;
uint16_t                            draw_oam_addr = 0;
union oam_t                         oam;

uint16_t                            ppu_reg_cgram_addr = 0;
uint8_t                             ppu_cgram_lsb_latch = 0;
uint8_t                             ppu_cgram_lsb_toggle = 0;
// uint16_t                            ppu_cgram[PPU_CGRAM_WORD_COUNT];
uint8_t *                           ppu_cgram = NULL;

uint32_t                            vram_addr = 0;
uint8_t *                           vram = NULL;

struct background_t                 backgrounds[4];
float                               cur_brightness = 0.0;
uint16_t                            backdrop_color;
// struct dot_t                        cur_backdrop = {};

uint32_t                            main_screen_bg_count = 0;
struct background_t *               main_screen[5];
//struct bg_draw_t                    main_screen[5];
uint32_t                            sub_screen_bg_count = 0;
struct bg_draw_t                    sub_screen[5];

uint16_t                            rot_values_bytes = 0;
int16_t                             rot_values[4];
int16_t                             pos_values[2];

struct 
{
    uint8_t         background_count;

    struct
    {
        uint8_t     color_depth;
        uint16_t    pallete_offset;

    }               backgrounds[4];
    
} ppu_bgmode_info[] = {

    [PPU_BGMODE_MODE0] = {
        .background_count = 4,
        .backgrounds[0] = { .color_depth = 2, .pallete_offset = offsetof(union mode0_cgram_t, bg1_colors) },
        .backgrounds[1] = { .color_depth = 2, .pallete_offset = offsetof(union mode0_cgram_t, bg2_colors) },
        .backgrounds[2] = { .color_depth = 2, .pallete_offset = offsetof(union mode0_cgram_t, bg3_colors) },
        .backgrounds[3] = { .color_depth = 2, .pallete_offset = offsetof(union mode0_cgram_t, bg4_colors) },
    },
    [PPU_BGMODE_MODE1] = {
        .background_count = 3,
        .backgrounds[0] = { .color_depth = 4, .pallete_offset = offsetof(union mode12_cgram_t, bg12_colors) },
        .backgrounds[1] = { .color_depth = 4, .pallete_offset = offsetof(union mode12_cgram_t, bg12_colors) },
        .backgrounds[2] = { .color_depth = 2, .pallete_offset = offsetof(union mode12_cgram_t, bg3_colors) },
    },
    [PPU_BGMODE_MODE2] = {
        .background_count = 2,
        .backgrounds[0] = { .color_depth = 4, .pallete_offset = offsetof(union mode12_cgram_t, bg12_colors) },
        .backgrounds[1] = { .color_depth = 4, .pallete_offset = offsetof(union mode12_cgram_t, bg12_colors) },
    },
    [PPU_BGMODE_MODE3] = {
        .background_count = 2,
        .backgrounds[0] = { .color_depth = 8, .pallete_offset = offsetof(union mode347_cgram_t, bg1_colors) },
        .backgrounds[1] = { .color_depth = 4, .pallete_offset = offsetof(union mode347_cgram_t, mode3_bg2_colors) },
    },
    [PPU_BGMODE_MODE4] = {
        .background_count = 2,
        .backgrounds[0] = { .color_depth = 8, .pallete_offset = offsetof(union mode347_cgram_t, bg1_colors) },
        .backgrounds[1] = { .color_depth = 2, .pallete_offset = offsetof(union mode347_cgram_t, mode4_bg2_colors) },
    },
    [PPU_BGMODE_MODE5] = {
        .background_count = 2,
        .backgrounds[0] = { .color_depth = 4, .pallete_offset = offsetof(union mode56_cgram_t, bg1_colors) },
        .backgrounds[1] = { .color_depth = 2, .pallete_offset = offsetof(union mode56_cgram_t, bg2_colors) },
    },
    [PPU_BGMODE_MODE6] = {
        .background_count = 1,
        .backgrounds[0] = { .color_depth = 4, .pallete_offset = offsetof(union mode56_cgram_t, bg1_colors) },
    },
    [PPU_BGMODE_MODE7] = {
        .background_count = 1,
        .backgrounds[0] = { .color_depth = 8, .pallete_offset = offsetof(union mode347_cgram_t, bg1_colors) },
    }
};

struct
{
    uint32_t                            step;
    uint32_t                            substep;

    uint16_t                            sprite_y_pos;
    uint16_t                            sprite_tile_x;
    uint16_t                            sprite_name;
    uint8_t                             sprite_size;
    uint16_t                            sprite_flip;
    uint8_t                             sprite_priority;
    uint8_t                             sprite_pallete;
    uint16_t                            sprite_spans[2];
    uint8_t                             sprite_span_index;
    uint16_t                            sprite_tile_pixel_offset;
    uint16_t                            sprite_tile_pixel_count;
    struct chr16_t *                    sprite_tile;

    uint8_t                             scanline_sprites[34];
    uint32_t                            scanline_sprite_count;
    uint32_t                            scanline_tile_count;

    uint32_t                            sprite_eval_index;
} ppu_sprite_state;

struct
{
    uint32_t                            step;
    uint32_t                            substep;

    uint8_t                             names[4];
    uint16_t                            next_spans[12];
    uint16_t                            cur_spans[12];

    uint8_t                             cur_background;

    struct
    {
                                        // void *
    }                                   backgrounds[4];

} ppu_background_state;

// struct obj_t                        objects[128];
//struct dot_objs_t *                 line_objects;
//struct dot_obj_priorities_t *       scanline_objs;

// struct draw_tile_t *                obj_draw_tiles;
// uint32_t                            obj_draw_tile_count;

// struct draw_tile_t *                bg_tiles;
// uint32_t                            bg_tile_count;

// struct dot_tiles_t *                main_scanline_tiles;
// struct dot_tiles_t *                sub_scanline_tiles;


//struct tile_dot_priorities_t        main_screen_tiles;
// int32_t                             vram_offset = 0;
uint8_t                             vram_prefetch[2];
uint8_t                             ppu1_last_bus_value = 0;
uint8_t                             ppu2_last_bus_value = 0;
extern uint8_t                      last_bus_value;


// extern struct breakpoint_list_t emu_breakpoints[];
extern struct thrd_t                emu_main_thread;
extern struct thrd_t *              emu_emulation_thread;
extern uint8_t *                    emu_vram_breakpoint_bitmask;
//struct pal16e_t                     obj_palletes[8];
//struct layer_dot_priorities_t   main_screen;
//struct layer_dot_priorities_t   sub_screen;

const char *ppu_reg_strs[PPU_REG_WMADDH - PPU_REG_INIDISP] = {
    [PPU_REG_INIDISP - PPU_REG_INIDISP]         = "INIDISP",
    [PPU_REG_OBJSEL - PPU_REG_INIDISP]          = "OBJSEL",
    [PPU_REG_OAMADDL - PPU_REG_INIDISP]         = "OAMADDL",
    [PPU_REG_OAMADDH - PPU_REG_INIDISP]         = "OAMADDH",
    [PPU_REG_OAMDATAW - PPU_REG_INIDISP]        = "OAMDATAW",
    [PPU_REG_BGMODE - PPU_REG_INIDISP]          = "BGMODE",
    [PPU_REG_MOSAIC - PPU_REG_INIDISP]          = "MOSAIC",
    [PPU_REG_BG1SC - PPU_REG_INIDISP]           = "BG1SC",
    [PPU_REG_BG2SC - PPU_REG_INIDISP]           = "BG2SC",
    [PPU_REG_BG3SC - PPU_REG_INIDISP]           = "BG3SC",
    [PPU_REG_BG4SC - PPU_REG_INIDISP]           = "BG4SC",
    [PPU_REG_BG12NBA - PPU_REG_INIDISP]         = "BG12NBA",
    [PPU_REG_BG34NBA - PPU_REG_INIDISP]         = "BG34NBA",
    [PPU_REG_BG1HOFS - PPU_REG_INIDISP]         = "BG1HOFS",
    [PPU_REG_BG1VOFS - PPU_REG_INIDISP]         = "BG1VOFS",
    [PPU_REG_BG2HOFS - PPU_REG_INIDISP]         = "BG2HOFS",
    [PPU_REG_BG2VOFS - PPU_REG_INIDISP]         = "BG2VOFS",
    [PPU_REG_BG3HOFS - PPU_REG_INIDISP]         = "BG3HOFS",
    [PPU_REG_BG3VOFS - PPU_REG_INIDISP]         = "BG3VOFS",
    [PPU_REG_BG4HOFS - PPU_REG_INIDISP]         = "BG4HOFS",
    [PPU_REG_BG4VOFS - PPU_REG_INIDISP]         = "BG4VOFS",
    [PPU_REG_VMAINC - PPU_REG_INIDISP]          = "VMAINC",
    [PPU_REG_VMADDL - PPU_REG_INIDISP]          = "VMADDL",
    [PPU_REG_VMADDH - PPU_REG_INIDISP]          = "VMADDH",
    [PPU_REG_VMDATAWL - PPU_REG_INIDISP]        = "VMDATAWL",
    [PPU_REG_VMDATAWH - PPU_REG_INIDISP]        = "VMDATAWL",
    [PPU_REG_M7SEL - PPU_REG_INIDISP]           = "VMDATAWH",
    // [PPU_REG_M7A]             = 0x211b,
    // [PPU_REG_M7B]             = 0x211c,
    // [PPU_REG_M7C]             = 0x211d,
    // [PPU_REG_M7D]             = 0x211e,
    // [PPU_REG_M7X]             = 0x211f,
    // [PPU_REG_M7Y]             = 0x2120,
    // [PPU_REG_CGADD]           = 0x2121,
    // [PPU_REG_CGDATAW]         = 0x2122,
    [PPU_REG_W12SEL - PPU_REG_INIDISP]          = "W12SEL",
    [PPU_REG_W34SEL - PPU_REG_INIDISP]          = "W34SEL",
    [PPU_REG_WCOLOBJSEL - PPU_REG_INIDISP]      = "WCOLOBJSEL",
    [PPU_REG_W1L - PPU_REG_INIDISP]             = "W1L",
    [PPU_REG_W1R - PPU_REG_INIDISP]             = "W1R",
    [PPU_REG_W2L - PPU_REG_INIDISP]             = "W2L",
    [PPU_REG_W2R - PPU_REG_INIDISP]             = "W2R",
    [PPU_REG_WBGLOG - PPU_REG_INIDISP]          = "WWBGLOG",
    [PPU_REG_WCOLOBJLOG - PPU_REG_INIDISP]      = "WCOLOBJLOG",
    [PPU_REG_TMAIN - PPU_REG_INIDISP]           = "TMAIN",
    [PPU_REG_TSUB - PPU_REG_INIDISP]            = "TSUB",
    [PPU_REG_TMAINWM - PPU_REG_INIDISP]         = "TMAINWM",
    [PPU_REG_TSUBWM - PPU_REG_INIDISP]          = "TSUBWM",
    [PPU_REG_CGSWSEL - PPU_REG_INIDISP]         = "CGSWEL",
    [PPU_REG_CGADSUB - PPU_REG_INIDISP]         = "CGADSUB",
    // [PPU_REG_COLDATA]         = 0x2132,
    [PPU_REG_SETINI - PPU_REG_INIDISP]          = "SETINI",
    // [PPU_REG_MPYL]            = 0x2134,
    // [PPU_REG_MPYM]            = 0x2135,
    // [PPU_REG_MPYH]            = 0x2136,
    // [PPU_REG_SLVH]            = 0x2137,
    // [PPU_REG_OAMDATAR]        = 0x2138,
    [PPU_REG_VMDATARL - PPU_REG_INIDISP]        = "VMDATARL",
    [PPU_REG_VMDATARH - PPU_REG_INIDISP]        = "VMDATARH",
    // [PPU_REG_CGDATAR]         = 0x213b,
    // [PPU_REG_OPHCT]           = 0x213c,
    // [PPU_REG_OPVCT]           = 0x213d,
    // [PPU_REG_STAT77]          = 0x213e,
    // [PPU_REG_STAT78]          = 0x213f,
    // [PPU_REG_WMDATA]          = 0x2180,
    // [PPU_REG_WMADDL]          = 0x2181,
    // [PPU_REG_WMADDM]          = 0x2182,
    // [PPU_REG_WMADDH]          = 0x2183,
};


uint8_t (*chr_dot_funcs[])(void *chr_base, uint32_t index, uint32_t dot_h, uint32_t dot_v) = {
    // [COLOR_FUNC_CHR0] = NULL,
    [COLOR_FUNC_CHR4] = chr4_dot,
    [COLOR_FUNC_CHR16] = chr16_dot,
    [COLOR_FUNC_CHR256] = chr256_dot,
};

uint16_t (*pal_col_funcs[])(void *pal_base, uint8_t pallete, uint8_t index) = {
    // [COLOR_FUNC_CHR0] = NULL,
    [COLOR_FUNC_CHR4] = pal4_col,
    [COLOR_FUNC_CHR16] = pal16_col,
    [COLOR_FUNC_CHR256] = pal256_col,
};

// uint32_t                            line_obj_count = 0;
// uint32_t                            line_chr_count = 0;
//struct line_obj_t   line_objs[128];
struct chr16_t *                    obj_chr_base[2];
// void  *             obj_chr_base[2];
// uint8_t             line_objs[128];

// uint32_t

// uint8_t cgram[512];

uint32_t vmadd_increment_shifts[] = {
    [PPU_VMADD_INC1]    = 0,
    [PPU_VMADD_INC32]   = 5,
    [PPU_VMADD_INC64]   = 7,
    [PPU_VMADD_INC128]  = 7,
    // [PPU_VMADD_INC1]    = 0,
    // [PPU_VMADD_INC32]   = 0,
    // [PPU_VMADD_INC64]   = 0,
    // [PPU_VMADD_INC128]  = 0,
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
    ppu_cgram = calloc(1, PPU_CGRAM_SIZE);
    vram = calloc(1, PPU_VRAM_SIZE << 1);
//    line_objects = calloc(256, sizeof(struct dot_objs_t));

//    scanline_obj_tiles = calloc(SCANLINE_DOT_LENGTH, sizeof(struct dot_obj_priorities_t));
    /* worst case scenario where all objs are 32x32 wide */
    // obj_draw_tiles = calloc(MAX_OBJ_COUNT * 16, sizeof(struct draw_tile_t));
    // bg_tiles = calloc(8, sizeof(struct draw_tile_t));
    // main_scanline_tiles = calloc(SCANLINE_DOT_LENGTH, sizeof(struct dot_tiles_t));
    // sub_scanline_tiles = calloc(SCANLINE_DOT_LENGTH, sizeof(struct dot_tiles_t));
    last_draw_scanline = 224;

    backgrounds[0] = (struct background_t){.chr_base = vram, .data_base = (struct bg_sc_data_t *)vram, .pal_base = vram};
    backgrounds[1] = (struct background_t){.chr_base = vram, .data_base = (struct bg_sc_data_t *)vram, .pal_base = vram};
    backgrounds[2] = (struct background_t){.chr_base = vram, .data_base = (struct bg_sc_data_t *)vram, .pal_base = vram};
    backgrounds[3] = (struct background_t){.chr_base = vram, .data_base = (struct bg_sc_data_t *)vram, .pal_base = vram};

    obj_chr_base[0] = (struct chr16_t *)vram;
    obj_chr_base[1] = (struct chr16_t *)vram;
}

void shutdown_ppu()
{
//    free(framebuffer);
    // free(obj_draw_tiles);
    // free(main_scanline_tiles);
    // free(sub_scanline_tiles);
    free(ppu_cgram);
    free(vram);
}

void reset_ppu()
{
    vcounter = 0;
    hcounter = 0;

    // main_screen_bg_count = 0;
    // sub_screen_bg_count = 0;
    // obj_draw_tile_count = 0;


    // backgrounds[0] = (struct background_t){.chr_base = vram, .data_base = (struct bg_sc_data_t *)vram, .pal_base = vram};
    // backgrounds[1] = (struct background_t){.chr_base = vram, .data_base = (struct bg_sc_data_t *)vram, .pal_base = vram};
    // backgrounds[2] = (struct background_t){.chr_base = vram, .data_base = (struct bg_sc_data_t *)vram, .pal_base = vram};
    // backgrounds[3] = (struct background_t){.chr_base = vram, .data_base = (struct bg_sc_data_t *)vram, .pal_base = vram};

    

    mem_regs[PPU_REG_STAT77] = 1;
    mem_regs[PPU_REG_STAT78] = 1;

    mem_regs[PPU_REG_INIDISP] |= 0x80;
    mem_regs[PPU_REG_BGMODE] |= 0x0f;
    mem_regs[PPU_REG_VMAINC] |= 0x0f;

    mem_regs[PPU_REG_MPYL] = 1;
    mem_regs[PPU_REG_MPYM] = 0;
    mem_regs[PPU_REG_MPYH] = 0;
}

uint8_t bg_chr0_dot_col(void *chr_base, uint32_t index, uint32_t size16, uint32_t dot_h, uint32_t dot_v)
{
    return 0;
}

uint8_t chr0_dot(void *chr_base, uint32_t name, uint32_t dot_x, uint32_t dot_y)
{
    return 0;
}

uint8_t chr4_dot(void *chr_base, uint32_t name, uint32_t dot_x, uint32_t dot_y)
{
    struct chr4_t *chr = (struct chr4_t *)chr_base + name;
    dot_x &= 0x07;
    dot_y &= 0x07;
    uint8_t color_index;
    color_index  =  (chr->p01[dot_y] & (0x80 >> dot_x)) && 1;
    color_index |= ((chr->p01[dot_y] & (0x8000 >> dot_x)) && 1) << 1;
    return color_index;
}

uint8_t chr16_dot(void *chr_base, uint32_t name, uint32_t dot_x, uint32_t dot_y)
{
    dot_x &= 0x07;
    dot_y &= 0x07;

    uint16_t mask1 = 0x80 >> dot_x;
    uint16_t mask2 = 0x8000 >> dot_x;

    struct chr16_t *chr = (struct chr16_t *)chr_base + name;
    uint8_t color_index;
    color_index  =  (chr->p01[dot_y] & mask1) && 1;
    color_index |= ((chr->p01[dot_y] & mask2) && 1) << 1;
    color_index |= ((chr->p23[dot_y] & mask1) && 1) << 2;
    color_index |= ((chr->p23[dot_y] & mask2) && 1) << 3;
    return color_index;
}

uint8_t chr256_dot(void *chr_base, uint32_t name, uint32_t dot_x, uint32_t dot_y)
{
    dot_x &= 0x07;
    dot_y &= 0x07;

    uint16_t mask1 = 0x80 >> dot_x;
    uint16_t mask2 = 0x8000 >> dot_x;

    struct chr256_t *chr = (struct chr256_t *)chr_base + name;
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

uint8_t bg7_chr256_dot(void *chr_base, uint32_t name, uint32_t dot_x, uint32_t dot_y)
{
    // struct bg7_sc_data_t *data_base = (struct bg7_sc_data_t *)chr_base;
    // struct pal256_t *pallete = (struct pal256_t *)background->pal_base;
    // uint32_t data_index = (dot_x >> 3) + (dot_y >> 3) * 128;
    // struct bg7_sc_data_t *bg_data = data_base + data_index;

    // uint32_t chr_dot_x = dot_x % 8;
    // uint32_t chr_dot_y = dot_y % 8;
    // uint32_t chr_index = ((uint32_t)bg_data->name << 6) + (chr_dot_v << 3) + chr_dot_h;

    // struct bg7_sc_data_t *chr_data = data_base + chr_index;
    // return pallete->colors[chr_data->chr];
}

uint16_t pal4_col(void *pal_base, uint8_t pallete, uint8_t index)
{
    struct pal4_t *pal = (struct pal4_t *)pal_base;
    return pal[pallete].colors[index];
    // struct col_t color;
    // color.r = color_lut[(packed_color >> COL_DATA_R_SHIFT) & COL_DATA_MASK];
    // color.g = color_lut[(packed_color >> COL_DATA_G_SHIFT) & COL_DATA_MASK];
    // color.b = color_lut[(packed_color >> COL_DATA_B_SHIFT) & COL_DATA_MASK];
    // return color;
}

uint16_t pal16_col(void *pal_base, uint8_t pallete, uint8_t index)
{
    struct pal16_t *pal = (struct pal16_t *)pal_base;
    return pal[pallete].colors[index];
    // struct col_t color;
    // color.r = color_lut[(packed_color >> COL_DATA_R_SHIFT) & COL_DATA_MASK];
    // color.g = color_lut[(packed_color >> COL_DATA_G_SHIFT) & COL_DATA_MASK];
    // color.b = color_lut[(packed_color >> COL_DATA_B_SHIFT) & COL_DATA_MASK];
    // return color;
}

uint16_t pal256_col(void *pal_base, uint8_t pallete, uint8_t index)
{
    struct pal256_t *pal = (struct pal256_t *)pal_base;
    return pal[pallete].colors[index];
    // struct col_t color;
    // color.r = color_lut[(packed_color >> COL_DATA_R_SHIFT) & COL_DATA_MASK];
    // color.g = color_lut[(packed_color >> COL_DATA_G_SHIFT) & COL_DATA_MASK];
    // color.b = color_lut[(packed_color >> COL_DATA_B_SHIFT) & COL_DATA_MASK];
    // return color;
}

uint8_t bg_chr4_dot_col(void *chr_base, uint32_t index, uint32_t size, uint32_t dot_h, uint32_t dot_v)
{
//    uint32_t chr_size = size;
    uint32_t chr_dot_h = dot_h & 0x07;
    uint32_t chr_dot_v = dot_v & 0x07;
    uint8_t color_index;

//    index += (dot_h >> 3);
//    index += (dot_v >> 3) << 4;
    // uint32_t offset = 8 * 4 * index;

    struct chr4_t *chr = (struct chr4_t *)chr_base + index;
    // struct chr4_t *chr = (struct chr4_t *)((uint8_t *)chr_base + offset);
    color_index  =  (chr->p01[chr_dot_v] & (0x80 >> chr_dot_h)) && 1;
    color_index |= ((chr->p01[chr_dot_v] & (0x8000 >> chr_dot_h)) && 1) << 1;
    return color_index;
}

uint8_t bg_chr16_dot_col(void *chr_base, uint32_t index, uint32_t size, uint32_t dot_h, uint32_t dot_v)
{
//    uint32_t chr_size = 1 << (3 + size16);
//    uint32_t chr_size = size;
    uint32_t chr_dot_h = dot_h & 0x07;
    uint32_t chr_dot_v = dot_v & 0x07;
    uint8_t color_index;

//    index += (dot_h >> 3);
//    index += (dot_v >> 3) << 4;

    // uint32_t offset = 8 * 16 * index;

    uint16_t mask1 = 0x80 >> chr_dot_h;
    uint16_t mask2 = 0x8000 >> chr_dot_h;

    struct chr16_t *chr = (struct chr16_t *)chr_base + index;
    // struct chr16_t *chr = (struct chr16_t *)((uint8_t *)chr_base + offset);
    color_index  =  (chr->p01[chr_dot_v] & mask1) && 1;
    color_index |= ((chr->p01[chr_dot_v] & mask2) && 1) << 1;
    color_index |= ((chr->p23[chr_dot_v] & mask1) && 1) << 2;
    color_index |= ((chr->p23[chr_dot_v] & mask2) && 1) << 3;
    return color_index;
}

uint8_t bg_chr256_dot_col(void *chr_base, uint32_t index, uint32_t size, uint32_t dot_h, uint32_t dot_v)
{
//    uint32_t chr_size = 1 << (3 + size16);
    uint32_t chr_dot_h = dot_h & 0x07;
    uint32_t chr_dot_v = dot_v & 0x07;
    uint8_t color_index;

//    index += (dot_h % chr_size) > 7;
//    index += ((dot_v % chr_size) > 7) << 4;

//    index += (dot_h >> 3);
//    index += (dot_v >> 3) << 4;

    // uint32_t offset = 8 * 8 * index;

    uint16_t mask1 = 0x80 >> chr_dot_h;
    uint16_t mask2 = 0x8000 >> chr_dot_h;

   struct chr256_t *chr = (struct chr256_t *)chr_base + index;
    // struct chr256_t *chr = (struct chr256_t *)((uint8_t *)chr_base + offset);
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
    // uint32_t screen = 0;
    uint32_t screen = ((tile_x >> 5) & 0x1) + ((tile_y >> 4) & 0x2);

    tile_x &= 0x1f;
    tile_y &= 0x1f;

    uint32_t data_index = tile_x + (tile_y << 5);
    struct bg_sc_data_t *bg_data = background->data_base[screen] + data_index;

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

    return (struct bg_tile_t) {
        .chr_index      = (bg_data->data & BG_SC_DATA_NAME_MASK) /* + (tile_dot_x > 7) + ((tile_dot_y > 7) << 4) */,
        .pal_index      = (bg_data->data >> BG_SC_DATA_PAL_SHIFT) & BG_SC_DATA_PAL_MASK,
        .priority       = (bg_data->data >> BG_SC_DATA_PRIORITY_SHIFT) & BG_SC_DATA_PRIORITY_MASK,
        .tile_dot_x     = tile_dot_x,
        .tile_dot_y     = tile_dot_y,
    };
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
    uint8_t color_index = bg_chr4_dot_col(background->chr_base, tile.chr_index, background->chr_size, tile.tile_dot_x, tile.tile_dot_y);

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
    uint8_t color_index = bg_chr16_dot_col(background->chr_base, tile.chr_index, background->chr_size, tile.tile_dot_x, tile.tile_dot_y);

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
    uint8_t color_index = bg_chr256_dot_col(background->chr_base, tile.chr_index, background->chr_size, tile.tile_dot_x, tile.tile_dot_y);

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

// uint16_t obj_pal16_col(uint32_t dot_h, uint32_t dot_v, struct obj_t *obj)
// {
// //    struct oam_t *oam_tables = (struct oam_t *)oam;
// //    struct obj_attr1_t *attr1 = oam_tables->table1 + obj->index;
// //    uint32_t chr_name = attr1->fpcn & OBJ_ATTR1_NAME_MASK;
// //    void *chr_base = obj_chr_base[(chr_name & 0x100) != 0];
// //
// ////    uint32_t chr_size = 1 << (3 + size16);
// //    uint32_t chr_dot_h = dot_h % 8;
// //    uint32_t chr_dot_v = dot_v % 8;
// //    uint8_t color_index;
// //
// //    return color_index;
// }

// void update_scanline_obj_tiles(uint16_t line)
// {
//     obj_draw_tile_count = 0;

//     uint16_t chr_names[4];

//     struct dot_tiles_t *dot_tiles = NULL;

//     if(mem_regs[PPU_REG_TMAIN] & PPU_TMAIN_FLAG_OBJ)
//     {
//         dot_tiles = main_scanline_tiles;
//     }
//     else if(mem_regs[PPU_REG_TSUB] & PPU_TSUB_FLAG_OBJ)
//     {
//         dot_tiles = sub_scanline_tiles;
//     }
//     else
//     {
//         return;
//     }

//     for(uint32_t index = 0; index < 128; index++)
//     {
//         union obj_attr1_t *attr1 = oam.tables.table1 + index;
//         struct obj_attr2_t *attr2 = oam.tables.table2 + (index >> 3);

//         uint16_t size_pos = (attr2->size_hpos >> ((index & 0x07) << 1)) & OBJ_ATTR2_DATA_MASK;
//         uint16_t hpos = (uint16_t)attr1->h_pos | ((size_pos & 1) << 8);

//         if(hpos > 0x100)
//         {
//             /* object wrapped around to the other side of the screen */
//             hpos |= 0xff00;
//             // hpos = -hpos;
//         }

//         uint16_t vpos = (uint16_t)attr1->v_pos;
//         uint16_t name = attr1->fpcn & OBJ_ATTR1_NAME_MASK;
//         uint16_t obj_size = cur_obj_sizes[size_pos >> 1];

//         uint16_t pal = (attr1->fpcn >> OBJ_ATTR1_PAL_SHIFT) & OBJ_ATTR1_PAL_MASK;
//         uint8_t priority = (attr1->fpcn >> OBJ_ATTR1_PRI_SHIFT) & OBJ_ATTR1_PRI_MASK;

//         uint16_t obj_end_hpos = hpos + obj_size;
//         uint16_t obj_end_vpos = vpos + obj_size;

//         if(hpos > (PPU_DRAW_END_DOT - PPU_DRAW_START_DOT) || (obj_end_hpos & 0x100) || vpos > line || obj_end_vpos <= line)
//         {
//             /* object is outside screen or doesn't touch this scanline */
//             continue;
//         }

//         int16_t start_dot = hpos;

//         if(start_dot < 0)
//         {
//             start_dot = 0;
//         }

//         int16_t end_dot = start_dot + obj_size;

//         if(end_dot > PPU_DRAW_END_DOT - PPU_DRAW_START_DOT)
//         {
//             end_dot = PPU_DRAW_END_DOT - PPU_DRAW_START_DOT;
//         }

//         uint32_t tile_count = obj_size / TILE_SIZE;
//         int16_t tile_y_index = (line - vpos) / TILE_SIZE;
//         int16_t tile_start_dot_y = vpos + (tile_y_index * TILE_SIZE);

//         if(attr1->fpcn & OBJ_ATTR1_VFLIP)
//         {
//             name += ((tile_count - 1) - tile_y_index) << 4;
//             tile_start_dot_y += TILE_SIZE - 1;
//         }
//         else
//         {
//             name += tile_y_index << 4;
//         }


//         int16_t tile_start_dot_x = hpos;
//         uint32_t first_obj_draw_tile = obj_draw_tile_count;

//         if(attr1->fpcn & OBJ_ATTR1_HFLIP)
//         {
//             name += tile_count - 1;
//             tile_start_dot_x += TILE_SIZE - 1;

//             chr_names[0] = name;
//             chr_names[1] = name - 1;
//             chr_names[2] = name - 2;
//             chr_names[3] = name - 3;
//         }
//         else
//         {
//             chr_names[0] = name;
//             chr_names[1] = name + 1;
//             chr_names[2] = name + 2;
//             chr_names[3] = name + 3;
//         }

//         for(uint32_t tile_index = 0; tile_index < tile_count; tile_index++)
//         {
//             struct draw_tile_t *tile = obj_draw_tiles + obj_draw_tile_count;
//             obj_draw_tile_count++;
//             tile->start_x = tile_start_dot_x;
//             tile->start_y = tile_start_dot_y;
//             tile->pallete = pal;
//             tile->name = chr_names[tile_index];
//             tile->color_func = COLOR_FUNC_CHR16;
//             tile_start_dot_x += TILE_SIZE;
//         }

//         uint32_t dot_count = 0;
//         for(uint32_t dot = start_dot; dot < end_dot; dot++)
//         {
//             struct dot_obj_draw_tiles_t *dot_obj_draw_tiles = dot_tiles[dot].obj_draw_tiles + priority;
//             dot_obj_draw_tiles->draw_tiles[dot_obj_draw_tiles->draw_tile_count] = first_obj_draw_tile + (dot_count >> 3);
//             dot_obj_draw_tiles->draw_tile_count++;
//             dot_count++;
//         }
//     }
// }

// void update_scanline_bg_tiles(uint16_t line, uint16_t dot)
// {
//     bg_tile_count = 0;

//     for(uint32_t background_index = 0; background_index < main_screen_bg_count; background_index++)
//     {
//         struct background_t *background = backgrounds + background_index;

//         int16_t bg_dot_x = ((int16_t)dot + (int16_t)background->offset.offsets[0]);
//         int16_t bg_dot_y = ((int16_t)line + (int16_t)background->offset.offsets[1]);

//         uint32_t tile_x = (bg_dot_x / background->chr_size) & 0x3f;
//         uint32_t tile_y = (bg_dot_y / background->chr_size) & 0x3f;
//         uint32_t screen = ((tile_x >> 5) & 0x1) + ((tile_y >> 4) & 0x2);

//         tile_x &= 0x1f;
//         tile_y &= 0x1f;

//         uint32_t data_index = tile_x + (tile_y << 5);
//         struct bg_sc_data_t *bg_data = background->data_base[screen] + data_index;
//         int16_t tile_dot_x = bg_dot_x & TILE_SIZE;
//         int16_t tile_dot_y = bg_dot_y & TILE_SIZE;
//         uint16_t name = bg_data->data & BG_SC_DATA_NAME_MASK;
//         uint16_t pallete = (bg_data->data >> BG_SC_DATA_PAL_SHIFT) & BG_SC_DATA_PAL_MASK;

//         if(bg_data->data & (BG_SC_DATA_FLIP_MASK << BG_SC_DATA_H_FLIP_SHIFT))
//         {
//             tile_dot_x += TILE_SIZE - 1;
//             name += background->chr_size > TILE_SIZE;
//         }

//         if(bg_data->data & (BG_SC_DATA_FLIP_MASK << BG_SC_DATA_V_FLIP_SHIFT))
//         {
//             tile_dot_y += TILE_SIZE - 1;
//             name += (background->chr_size > TILE_SIZE) << 4;
//         }

//         struct draw_tile_t *tile = bg_tiles + bg_tile_count;
//         bg_tile_count++;

//         tile->start_x = tile_dot_x;
//         tile->start_y = tile_dot_y;
//         tile->name = name;
//         tile->pallete = pallete;
//     }
// }

void ppu_EvalObjsSpriteStep()
{
    uint32_t next_scanline = vcounter == SCANLINE_COUNT ? 0 : (vcounter + 1);

    switch(ppu_sprite_state.substep)
    {
        case 0:
            ppu_reg_oam_addr = ppu_sprite_state.sprite_eval_index << 2;
            ppu_obj_low_attr = oam.tables.table1[ppu_sprite_state.sprite_eval_index];
            ppu_sprite_state.substep = 1; 
        break;

        case 1:
        {
            if(ppu_sprite_state.scanline_sprite_count >= 32)
            {
                mem_regs[PPU_REG_STAT77] |= PPU_STAT77_FLAG_33_RANGE_OVER;
                break;
            }

            uint16_t byte_index = ppu_sprite_state.sprite_eval_index >> 2;
            ppu_reg_oam_addr = PPU_OAM_TABLE2_START | byte_index;
            // uint16_t size_pos = oam.bytes[ppu_reg_oam_addr];
            uint8_t size_pos = oam.tables.table2[ppu_sprite_state.sprite_eval_index >> 3].size_hpos >> ((ppu_sprite_state.sprite_eval_index & 0x7) << 1);
            // size_pos >>= (ppu_obj_eval_index & 0x3) << 1;
            ppu_sprite_state.substep = 0;
            uint32_t obj_index = ppu_sprite_state.sprite_eval_index;
            ppu_sprite_state.sprite_eval_index++;

            uint16_t size = cur_obj_sizes[(size_pos >> PPU_OBJ_ATTR2_SIZE_SHIFT) & PPU_OBJ_ATTR2_SIZE_MASK];

            if(ppu_obj_low_attr.v_pos > next_scanline || ppu_obj_low_attr.v_pos + (size - 1) < next_scanline)
            {
                break;
            }

            /* TODO: add x = 256 bug */
            uint16_t h_pos = ppu_obj_low_attr.h_pos;
            if(size_pos & PPU_OBJ_ATTR2_HPOS_MASK)
            {
                h_pos |= 0xff00;
            }

            if((h_pos + size) & 0x8000)
            {
                break;
            }

            ppu_sprite_state.scanline_sprites[ppu_sprite_state.scanline_sprite_count] = obj_index;
            ppu_sprite_state.scanline_sprite_count++;

            // printf("sprite %d, %d (%d %d) is visible, total is %d\n", obj_index, size_pos, h_pos, ppu_obj_low_attr.v_pos, ppu_scanline_sprite_count);
        }
        break;
    }
}

void ppu_LoadTilesSpriteStep()
{
    uint32_t next_scanline = vcounter == SCANLINE_COUNT ? 0 : (vcounter + 1);

    switch(ppu_sprite_state.substep)
    {
        case 0:
        {
            if(ppu_sprite_state.scanline_sprite_count == 0)
            {
                break;
            }

            ppu_sprite_state.scanline_sprite_count--;
            uint32_t sprite_index = ppu_sprite_state.scanline_sprites[ppu_sprite_state.scanline_sprite_count];
            uint16_t size_pos = oam.tables.table2[sprite_index >> 3].size_hpos >> ((sprite_index & 0x7) << 1);

            // printf("load tiles for sprite %d (%d)\n", ppu_sprite_index, ppu_sprite_size_hpos);

            ppu_sprite_state.sprite_y_pos = oam.tables.table1[sprite_index].v_pos;
            // ppu_sprite_x_pos = (uint16_t)oam.tables.table1[obj_index].h_pos | (((uint16_t)high_byte & 1) << 8);
            ppu_sprite_state.sprite_tile_x = oam.tables.table1[sprite_index].h_pos;
            ppu_sprite_state.sprite_name = oam.tables.table1[sprite_index].fpcn & PPU_OBJ_ATTR1_NAME_MASK;
            ppu_sprite_state.sprite_flip = oam.tables.table1[sprite_index].fpcn;
            ppu_sprite_state.sprite_size = cur_obj_sizes[(size_pos >> PPU_OBJ_ATTR2_SIZE_SHIFT) & PPU_OBJ_ATTR2_SIZE_MASK];
            ppu_sprite_state.sprite_pallete = (oam.tables.table1[sprite_index].fpcn >> PPU_OBJ_ATTR1_PAL_SHIFT) & PPU_OBJ_ATTR1_PAL_MASK;
            ppu_sprite_state.sprite_priority = (oam.tables.table1[sprite_index].fpcn >> PPU_OBJ_ATTR1_PRI_SHIFT) & PPU_OBJ_ATTR1_PRI_MASK;

            ppu_sprite_state.sprite_span_index = next_scanline - ppu_sprite_state.sprite_y_pos;
            uint16_t tile_row_index = ppu_sprite_state.sprite_span_index / PPU_TILE_SIZE;
            uint32_t tile_count = ppu_sprite_state.sprite_size / PPU_TILE_SIZE;
            ppu_sprite_state.sprite_span_index %= PPU_TILE_SIZE;
            // uint16_t first_tile = 0;
            uint32_t first_tile = 0; 
            uint16_t sprite_x = ppu_sprite_state.sprite_tile_x;

            // ppu_sprite_x_pos = ppu_sprite_tile_x;

            if(size_pos & PPU_OBJ_ATTR2_HPOS_MASK)
            {
                /* sign extend */
                ppu_sprite_state.sprite_tile_x |= 0xff00;
            }

            if(tile_count > 1)
            {
                if(size_pos & PPU_OBJ_ATTR2_HPOS_MASK)
                {
                    first_tile = (0xff - sprite_x) / PPU_TILE_SIZE;
                }
                
                if(ppu_sprite_state.sprite_flip & PPU_OBJ_ATTR1_VFLIP)
                {
                    // row_index = (ppu_sprite_size - 1) - row_index;
                    tile_row_index = (tile_count - 1) - tile_row_index;
                    ppu_sprite_state.sprite_span_index = (PPU_TILE_SIZE - 1) - ppu_sprite_state.sprite_span_index;
                }

                ppu_sprite_state.sprite_name += tile_row_index << 4;

                if(ppu_sprite_state.sprite_flip & PPU_OBJ_ATTR1_HFLIP)
                {
                    ppu_sprite_state.sprite_name += tile_count - 1;
                    ppu_sprite_state.sprite_name -= first_tile;
                }
                else
                {
                    ppu_sprite_state.sprite_name += first_tile;
                }

                ppu_sprite_state.sprite_tile_x += first_tile << 3;
                ppu_sprite_state.sprite_size -= first_tile << 3;
            }
            else if(ppu_sprite_state.sprite_flip & PPU_OBJ_ATTR1_VFLIP)
            {
                ppu_sprite_state.sprite_span_index = (PPU_TILE_SIZE - 1) - ppu_sprite_state.sprite_span_index;
            }

            ppu_sprite_state.substep = 1;
        }

        case 1:
        {
            if(ppu_sprite_state.scanline_tile_count >= 34)
            {
                mem_regs[PPU_REG_STAT77] |= PPU_STAT77_FLAG_35_TIME_OVER;
                break;
            }

            ppu_sprite_state.scanline_tile_count++;

            ppu_sprite_state.sprite_size -= 8;
            ppu_sprite_state.sprite_tile = obj_chr_base[ppu_sprite_state.sprite_name >= 0x100] + ppu_sprite_state.sprite_name;
            ppu_sprite_state.sprite_tile_pixel_offset = 0;
            ppu_sprite_state.sprite_tile_pixel_count = PPU_TILE_SIZE;

            if(ppu_sprite_state.sprite_tile_x & 0x8000)
            {
                ppu_sprite_state.sprite_tile_pixel_offset = 0xff - (ppu_sprite_state.sprite_tile_x & 0xff);
                ppu_sprite_state.sprite_tile_pixel_count = PPU_TILE_SIZE - ppu_sprite_state.sprite_tile_pixel_offset;
                ppu_sprite_state.sprite_tile_x = 0;
            }
            else if(ppu_sprite_state.sprite_tile_x + PPU_TILE_SIZE > 255)
            {
                ppu_sprite_state.sprite_tile_pixel_count = 255 - ppu_sprite_state.sprite_tile_x;
            }
        }
        case 2:
        {
            uint32_t plane_index = ppu_sprite_state.substep == 2;
            ppu_sprite_state.sprite_spans[plane_index] = ppu_sprite_state.sprite_tile->pixels[plane_index][ppu_sprite_state.sprite_span_index];

            if(ppu_sprite_state.substep == 2)
            {
                struct line_pixel_t *line_buffer = ppu_line_buffer + ppu_sprite_state.sprite_tile_x;
                // uint32_t plane_shift = ppu_sprite_substep & 0x2;

                if(ppu_sprite_state.sprite_flip & PPU_OBJ_ATTR1_HFLIP)
                {
                    for(uint32_t pixel_index = 0; pixel_index < ppu_sprite_state.sprite_tile_pixel_count; pixel_index++)
                    {
                        uint8_t color = ((ppu_sprite_state.sprite_spans[0] & 0x1) | ((ppu_sprite_state.sprite_spans[0] & 0x100) >> 7));
                        color |= ((ppu_sprite_state.sprite_spans[1] & 0x1) | ((ppu_sprite_state.sprite_spans[1] & 0x100) >> 7)) << 2;

                        if(color != 0 && ppu_sprite_state.sprite_priority >= line_buffer[pixel_index].priority)
                        {
                            line_buffer[pixel_index].pallete = ppu_sprite_state.sprite_pallete;
                            line_buffer[pixel_index].priority = ppu_sprite_state.sprite_priority;
                            line_buffer[pixel_index].color = color;
                        }

                        ppu_sprite_state.sprite_spans[0] >>= 1;
                        ppu_sprite_state.sprite_spans[1] >>= 1;
                    }
                }
                else
                {
                    for(uint32_t pixel_index = 0; pixel_index < ppu_sprite_state.sprite_tile_pixel_count; pixel_index++)
                    {
                        uint8_t color = (((ppu_sprite_state.sprite_spans[0] & 0x80) >> 7) | ((ppu_sprite_state.sprite_spans[0] & 0x8000) >> 14));
                        color |= (((ppu_sprite_state.sprite_spans[1] & 0x80) >> 7) | ((ppu_sprite_state.sprite_spans[1] & 0x8000) >> 14)) << 2;

                        if(color != 0 && ppu_sprite_state.sprite_priority >= line_buffer[pixel_index].priority)
                        {
                            line_buffer[pixel_index].pallete = ppu_sprite_state.sprite_pallete;
                            line_buffer[pixel_index].priority = ppu_sprite_state.sprite_priority;
                            line_buffer[pixel_index].color = color;
                        }

                        ppu_sprite_state.sprite_spans[0] <<= 1;
                        ppu_sprite_state.sprite_spans[1] <<= 1;
                    }
                }

                if(ppu_sprite_state.sprite_size == 0)
                {
                    ppu_sprite_state.substep = 0;
                }
                else
                {
                    ppu_sprite_state.substep = 1;
                    ppu_sprite_state.sprite_tile_x += ppu_sprite_state.sprite_tile_pixel_count;

                    if(ppu_sprite_state.sprite_flip & PPU_OBJ_ATTR1_HFLIP)
                    {
                        ppu_sprite_state.sprite_name--;
                    }
                    else
                    {
                        ppu_sprite_state.sprite_name++;
                    }
                }
            }
            else
            {
                ppu_sprite_state.substep = 2;
            }
        }
        break;
    }
}

void ppu_NoneSpriteStep()
{

}

void ppu_LoadNamesBackgroundStep()
{
    switch(ppu_background_state.substep)
    {
        case 0:
            // if(ppu_background_state.cur_background)
        break;
    }
}

void ppu_LoadTilesBackgroundStep()
{

}

/* TODO: finish background fetch sequence */
uint32_t ppu_Step(int32_t cycle_count)
{
    ppu_cycle_count += cycle_count;
    ppu_scanline_master_cycles += cycle_count;

    uint32_t entered_vblank = 0;
    uint32_t irq_triggered = 0;
    uint32_t step_count = ppu_cycle_count / 4;

    while(step_count)
    {
        step_count--;
        uint32_t dot_length = ppu_dot_length[(hcounter == 323 || hcounter == 327) && (vcounter != 240 || !(mem_regs[PPU_REG_STAT78] & PPU_STAT78_FLAG_FIELD))];
        
        if(!(mem_regs[PPU_REG_INIDISP] & PPU_INIDISP_FLAG_FBLANK) && !(mem_regs[CPU_MEM_REG_HVBJOY] & CPU_HVBJOY_FLAG_VBLANK))
        {
            uint32_t next_sprite_step = PPU_SPRITE_STEP_NONE;
            if(hcounter <= 255)
            {
                next_sprite_step = PPU_SPRITE_STEP_EVAL_OBJS;
            }
            else if(hcounter >= 277 && hcounter <= 339)
            {
                next_sprite_step = PPU_SPRITE_STEP_LOAD_TILES;
            }

            if(ppu_sprite_state.step != next_sprite_step)
            {
                ppu_sprite_state.substep = 0;
            }

            ppu_sprite_state.step = next_sprite_step;

            /*
                (Internal OAM address sprite evaluation)
                https://forums.nesdev.org/viewtopic.php?p=234414#p234414

                (Any research done on writing to oam durring active display?)
                https://forums.nesdev.org/viewtopic.php?t=15108

                (Accessing OAM during H-blank)
                https://forums.nesdev.org/viewtopic.php?t=6758

                (PPU bus activity traces)
                https://forums.nesdev.org/viewtopic.php?t=14467

                (higan CPU emulation mode bug? (attn: byuu or any 65816 g...)
                https://forums.nesdev.org/viewtopic.php?p=173458#p173458

                (5A22 CPU pinout)
                https://board.zsnes.com/phpBB3/viewtopic.php?f=6&t=11230&p=204966#p204966


                So, apparently the oam is readable/writable during the active display
                period (when not in vblank or forced blank).

                -   writes still respect the low byte/high byte toggle, so a write would only
                    go through when an entire word is written. This means that a word write will
                    very likely land on the wrong location (as in, the low byte is written when the
                    address is X, and the high byte is written when the address is X + O, with O > 0,
                    making the entire word land further ahead in the oam than intended) when written
                    outside hblank. During hblank, writes will land on the location pointed by the oam
                    address after the ppu is done with sprite evaluation.
                    
                -   the internal oam address doesn't get incremented during active display, so if it was
                    possible to write two bytes several times before the address gets modified, then the
                    resulting word would get written to the same location in oam.


                Also, as mentioned in the thread on the first link (and other linked threads), the ppu stores
                sprites indexes (or maybe oam adresses?) into an "evaluation buffer", and then reads this buffer
                backwards while fetching tile data. This last part is backed by the tests conducted by
                paulb_nl (https://forums.nesdev.org/viewtopic.php?p=234317#p234317). During sprite evaluation,
                writes to oam affect sprites in increasing order, while during tile fetching they affectt them
                in descending order. Kingizor (https://forums.nesdev.org/viewtopic.php?p=234409#p234409) also
                mentions that once 35's time over happen, tiles of lower indexed sprites are dropped.

                Pixels from tiles are stored in a 256 entry line buffer, where each entry is composed by a 4 bit
                color (not really a color, it's an index into a pallete), a 3 bit pallete index and 2 bit priority.
                PPU1 then sends this to PPU2. The pinout of those chips seem to also confirm this.

            */

            // switch(ppu_sprite_step)
            // {
            //     case PPU_SPRITE_STEP_EVAL_OBJS:
            //         ppu_EvalObjsSpriteStep();
            //     break;

            //     case PPU_SPRITE_STEP_LOAD_TILES:
            //         ppu_LoadTilesSpriteStep();
            //     break;
            // }

            ppu_SpriteStepFunc[ppu_sprite_state.step]();
        }

        if(ppu_cycle_count >= dot_length)
        {
            ppu_cycle_count -= dot_length;
            // sub_dot = 0;
            hcounter++;
            cur_irq_counter--;
//            scanline_cycles += dot_length;

            if(hcounter == SCANLINE_DOT_LENGTH)
            {
                vcounter = (vcounter + 1) % SCANLINE_COUNT;
                hcounter = 0;
                ppu_scanline_master_cycles = 0;

                if(vcounter < last_draw_scanline && !(mem_regs[PPU_REG_INIDISP] & PPU_INIDISP_FLAG_FBLANK))
                {
                    ppu_reg_oam_addr = 0;
                    ppu_sprite_state.sprite_eval_index = 0;
                    ppu_sprite_state.step = PPU_SPRITE_STEP_NONE;
                    ppu_sprite_state.scanline_sprite_count = 0;
                    ppu_sprite_state.scanline_tile_count = 0;
                }

                // if(line_obj_count > 32)
                // {
                //    mem_regs[PPU_REG_STAT77] |= PPU_STAT77_FLAG_33_RANGE_OVER;
                // }

                // if(line_chr_count > 34)
                // {
                //     mem_regs[PPU_REG_STAT77] |= PPU_STAT77_FLAG_35_TIME_OVER;
                // }
            }

            // if(hcounter == 0 /* && vcounter >= PPU_DRAW_START_LINE && vcounter < last_draw_scanline */)
            // {
            //     // update_scanline_obj_tiles(vcounter - PPU_DRAW_START_LINE);
            //     ppu_reg_oam_addr = 0;
            //     ppu_obj_eval_index = 0;
            //     ppu_sprite_step = PPU_SPRITE_STEP_NONE;
            //     ppu_scanline_oam_entry_count = 0;
            // }

            if(vcounter == V_BLANK_END_LINE)
            {
                if(hcounter == 0)
                {
                    mem_regs[CPU_MEM_REG_HVBJOY] &= ~CPU_HVBJOY_FLAG_VBLANK;
                    mem_regs[CPU_MEM_REG_RDNMI] &= ~CPU_RDNMI_BLANK_NMI;
                    mem_regs[PPU_REG_STAT77] &= ~(PPU_STAT77_FLAG_33_RANGE_OVER | PPU_STAT77_FLAG_35_TIME_OVER);
                }
            }
            else
            {
                if(vcounter == last_draw_scanline)
                {
                    if(hcounter == 0)
                    {
                        if(!(mem_regs[CPU_MEM_REG_HVBJOY] & CPU_HVBJOY_FLAG_VBLANK))
                        {
                            entered_vblank = 1;
                            mem_regs[CPU_MEM_REG_RDNMI] |= CPU_RDNMI_BLANK_NMI;
                        }

                        mem_regs[CPU_MEM_REG_HVBJOY] |= CPU_HVBJOY_FLAG_VBLANK;
                        
                        if(mem_regs[CPU_MEM_REG_NMITIMEN] & CPU_NMITIMEN_FLAG_NMI_ENABLE)
                        {
                            assert_nmi(2);
                            deassert_nmi(2);
                        }
                    }
                    else if(hcounter == PPU_OAM_ADDR_RESET_DOT && !(mem_regs[PPU_REG_INIDISP] & PPU_INIDISP_FLAG_FBLANK))
                    {
                        ppu_ReloadOamAddrReg();
                    }
                }
            }

            if(hcounter == PPU_HBLANK_END_DOT)
            {
                mem_regs[CPU_MEM_REG_HVBJOY] &= ~CPU_HVBJOY_FLAG_HBLANK;

                if(vcounter == 0)
                {
                    mem_regs[PPU_REG_STAT78] ^= PPU_STAT78_FLAG_FIELD;
                }
            }
            else if(hcounter == PPU_HBLANK_START_DOT)
            {
                mem_regs[CPU_MEM_REG_HVBJOY] |= CPU_HVBJOY_FLAG_HBLANK;
            }

            if(!cur_irq_counter)
            {
                if(mem_regs[CPU_MEM_REG_NMITIMEN] & (CPU_NMITIMEN_FLAG_HTIMER_EN | CPU_NMITIMEN_FLAG_VTIMER_EN))
                {
                    mem_regs[CPU_MEM_REG_TIMEUP] |= 1 << 7;
                    irq_triggered = 1;
                    irq_hold_timer = 8;
                }

                cur_irq_counter = irq_counter_reload;
            }

            if(irq_hold_timer > 0)
            {
                irq_hold_timer -= cycle_count;
                mem_regs[CPU_MEM_REG_TIMEUP] |= 1 << 7;
            }

            uint8_t hvbjoy = mem_regs[CPU_MEM_REG_HVBJOY];

            if(vcounter >= PPU_DRAW_START_LINE && vcounter < last_draw_scanline && hcounter >= PPU_DRAW_START_DOT && hcounter <= PPU_DRAW_END_DOT)
            {
                uint8_t inidisp = mem_regs[PPU_REG_INIDISP];
                float brightness = cur_brightness;
                uint16_t dot_x = hcounter - PPU_DRAW_START_DOT;
                uint16_t dot_y = vcounter - PPU_DRAW_START_LINE;

                if(mem_regs[PPU_REG_INIDISP] & PPU_INIDISP_FLAG_FBLANK)
                {
                    brightness = 0.0;
                }

                union mode0_cgram_t *mode0_cgram = (union mode0_cgram_t *)ppu_cgram;
                struct dot_t *main_dot = emu_framebuffer + dot_y * FRAMEBUFFER_WIDTH + dot_x;
                // struct dot_t sub_dot = {};

                // *main_dot = cur_backdrop;

                struct dot_color_t dot_color = {.color = backdrop_color};

                if((mem_regs[PPU_REG_TMAIN] & PPU_TMAIN_FLAG_OBJ) || (mem_regs[PPU_REG_TSUB] & PPU_TSUB_FLAG_OBJ))
                {
                    struct line_pixel_t *pixel = ppu_line_buffer + dot_x;

                    if(pixel->color != 0)
                    {
                        dot_color.color = mode0_cgram->obj_colors[pixel->pallete].colors[pixel->color];
                        dot_color.priority = pixel->priority;
                    }

                    pixel->color = 0;
                    pixel->pallete = 0;
                    pixel->priority = 0;
                }

                uint32_t bg_mode = mem_regs[PPU_REG_BGMODE] & PPU_BGMODE_MODE_MASK;
                uint32_t bg3_prio = (bg_mode == PPU_BGMODE_MODE0 || bg_mode == PPU_BGMODE_MODE1) && (mem_regs[PPU_REG_BGMODE] & PPU_BGMODE_FLAG_BG3_PRIO);

                for(uint32_t index = 0; index < main_screen_bg_count; index++)
                {
                    struct background_t *background = main_screen[index];
                    if(background->disabled)
                    {
                        continue;
                    }

                    uint32_t background_index = background - backgrounds;

                    uint16_t chr_size = background->chr_size;
                    uint16_t bg_dot_x = dot_x + background->offset.offsets[0];
                    uint16_t bg_dot_y = dot_y + background->offset.offsets[1];


                    if(bg_dot_x < 0x8000 && bg_dot_y < 0x8000)
                    {
                        struct bg_tile_t tile = bg_tile_entry(bg_dot_x, bg_dot_y, background);
                        uint8_t priority = (2 - (background_index & 0x2)) + tile.priority;

                        if(bg3_prio && background_index == 2 && tile.priority)
                        {
                            priority = 4;
                        }

                        if(priority > dot_color.priority)
                        {
                            // uint16_t color = background->color_func(bg_dot_x, bg_dot_y, background);
                            uint8_t color_index = chr_dot_funcs[background->color_func](background->chr_base, tile.chr_index, tile.tile_dot_x, tile.tile_dot_y);

                            if(color_index > 0)
                            {
                                uint16_t packed_color = pal_col_funcs[background->color_func](background->pal_base, tile.pal_index, color_index);
                                dot_color.color = packed_color;
                                dot_color.priority = priority;

                                if(!bg3_prio)
                                {
                                    break;
                                }
                            }

                            // if(color != 0xffff)
                            // {
                            //     dot_color.color = color;
                            //     dot_color.priority = priority;

                            //     if(!bg3_prio)
                            //     {
                            //         break;
                            //     }
                            // }
                        }
                    }
                }


//                 for(uint32_t index = 0; index < main_screen_bg_count; index++)
//                 {
// //                    struct bg_draw_t *bg_draw = main_screen + index;
//                     struct background_t *background = main_screen[index];
//                     if(background == NULL)
//                     {
//                         continue;
//                     }
//                     uint16_t chr_size = background->chr_size;
//                     uint16_t bg_dot_x = dot_x + background->offset.offsets[0];
//                     uint16_t bg_dot_y = dot_y + background->offset.offsets[1];
//                     // int16_t bg_dot_x = ((int16_t)hcounter);
//                     // int16_t bg_dot_y = ((int16_t)vcounter);

//                     if(bg_dot_x < 0x8000 && bg_dot_y < 0x8000)
//                     {
//                         uint16_t color = background->color_func(bg_dot_x, bg_dot_y, background);

//                         if(color != 0xffff)
//                         {
//                             // main_dot->r = color_lut[(color >> COL_DATA_R_SHIFT) & COL_DATA_MASK];
//                             // main_dot->g = color_lut[(color >> COL_DATA_G_SHIFT) & COL_DATA_MASK];
//                             // main_dot->b = color_lut[(color >> COL_DATA_B_SHIFT) & COL_DATA_MASK];
//                             // main_dot->a = 255;
//                             dot_color.color = color;
//                             break;
//                         }
//                     }
//                 }

                main_dot->r = ppu_color_lut[(dot_color.color >> COL_DATA_R_SHIFT) & COL_DATA_MASK];
                main_dot->g = ppu_color_lut[(dot_color.color >> COL_DATA_G_SHIFT) & COL_DATA_MASK];
                main_dot->b = ppu_color_lut[(dot_color.color >> COL_DATA_B_SHIFT) & COL_DATA_MASK];
                main_dot->a = 255;

                // // struct dot_tiles_t *dot_tiles = NULL;
                // struct dot_t *obj_dot = NULL;

                // if((mem_regs[PPU_REG_TMAIN] & PPU_TMAIN_FLAG_OBJ) || (mem_regs[PPU_REG_TSUB] & PPU_TSUB_FLAG_OBJ))
                // {
                //     obj_dot = main_dot;
                //     struct line_pixel_t *pixel = ppu_line_buffer + dot_x;

                //     if(pixel->color != 0)
                //     {
                //         uint16_t color = mode0_cgram->obj_colors[pixel->pallete].colors[pixel->color];
                //         obj_dot->r = color_lut[(color >> COL_DATA_R_SHIFT) & COL_DATA_MASK];
                //         obj_dot->g = color_lut[(color >> COL_DATA_G_SHIFT) & COL_DATA_MASK];
                //         obj_dot->b = color_lut[(color >> COL_DATA_B_SHIFT) & COL_DATA_MASK];
                //         obj_dot->a = 255;
                //     }

                //     pixel->color = 0;
                //     pixel->pallete = 0;
                //     pixel->priority = 0;
                // }

                // if(mem_regs[PPU_REG_TMAIN] & PPU_TMAIN_FLAG_OBJ)
                // {
                //     dot_tiles = main_scanline_tiles + dot_x;
                //     obj_dot = main_dot;
                // }
                // else if(mem_regs[PPU_REG_TSUB] & PPU_TSUB_FLAG_OBJ)
                // {
                //     dot_tiles = sub_scanline_tiles + dot_x;
                //     obj_dot = &sub_dot;
                // }

                // if(obj_dot)
                // {
                //     for(uint32_t priority = 4; priority > 0;)
                //     {
                //         priority--;
                //         // uint32_t priority = 0;
                //         struct dot_obj_draw_tiles_t *dot_obj_draw_tiles = dot_tiles->obj_draw_tiles + priority;

                //         for(uint32_t dot_draw_tile_index = 0; dot_draw_tile_index < dot_obj_draw_tiles->draw_tile_count; dot_draw_tile_index++)
                //         {
                //             struct draw_tile_t *draw_tile = obj_draw_tiles + dot_obj_draw_tiles->draw_tiles[dot_draw_tile_index];
                //             // int16_t tile_dot_x = abs((int16_t)dot_x - (int16_t)draw_tile->start_x);
                //             // int16_t tile_dot_y = abs((int16_t)dot_y - (int16_t)draw_tile->start_y);
                //             uint16_t tile_dot_x = dot_x - draw_tile->start_x;
                //             uint16_t tile_dot_y = dot_y - draw_tile->start_y;

                //             uint8_t color_index = chr_dot_funcs[draw_tile->color_func](obj_chr_base[draw_tile->name >= 0x100], draw_tile->name, tile_dot_x, tile_dot_y);

                //             if(color_index)
                //             {
                //                 uint16_t color = mode0_cgram->obj_colors[draw_tile->pallete].colors[color_index];
                //                 obj_dot->r = color_lut[(color >> COL_DATA_R_SHIFT) & COL_DATA_MASK];
                //                 obj_dot->g = color_lut[(color >> COL_DATA_G_SHIFT) & COL_DATA_MASK];
                //                 obj_dot->b = color_lut[(color >> COL_DATA_B_SHIFT) & COL_DATA_MASK];
                //                 obj_dot->a = 255;
                //                 priority = 0;
                //                 break;
                //             }
                //         }
                //     }

                //     for(uint32_t priority = 0; priority < 4; priority++)
                //     {
                //         dot_tiles->obj_draw_tiles[priority].draw_tile_count = 0;
                //     }
                // }

                main_dot->r *= brightness;
                main_dot->g *= brightness;
                main_dot->b *= brightness;
            }
        }

        // ppu_cycle_count--;
    }

    // ppu_cycle_count -= step_count * 4;

    return entered_vblank;
}

// void dump_ppu()
// {
//    printf("===================== PPU ========================\n");
//    printf("current dot: (H: %d, V: %d)\n", hcounter, vcounter);
//    printf("v-blank: %d -- h-blank: %d\n", (mem_regs[CPU_MEM_REG_HVBJOY] & CPU_HVBJOY_FLAG_VBLANK) && 1, (mem_regs[CPU_MEM_REG_HVBJOY] & CPU_HVBJOY_FLAG_HBLANK) && 1);
//    printf("--------------------------------------------------\n");
//    printf("BG Mode: Mode %d\n", mem_regs[PPU_REG_BGMODE] & PPU_BGMODE_MODE_MASK);
//    printf("\n");
// }

// void dump_vram(uint32_t start, uint32_t end)
// {
//     if(start > PPU_VRAM_SIZE)
//     {
//         start = PPU_VRAM_SIZE;
//     }

//     if(end > PPU_VRAM_SIZE)
//     {
//         end = PPU_VRAM_SIZE;
//     }

//     if(start > end)
//     {
//         uint32_t temp = start;
//         start = end;
//         end = temp;
//     }

//     start &= 0xfffe;
//     printf("----------- VRAM -----------\n");
//     while(start < end)
//     {
//         uint32_t count = end - start;

//         if(count > 16)
//         {
//             count = 16;
//         }

//         printf("%04x: ", start);

//         for(uint32_t byte = 0; byte < count; byte++)
//         {
//             printf("%02x ", vram[start]);
//             start++;
//         }

//         printf("\n");
//     }

//     printf("----------------------------\n");

// //    for(uint32_t line = 0; line <= lines; line++)
// //    {
// //        for(uint32_t byte = 0; byte < 16 && ; byte++)
// //        {
// //            printf("")
// //        }
// //    }
// }


void inidisp_write(uint32_t effective_address, uint8_t value)
{
    if((mem_regs[PPU_REG_INIDISP] & PPU_INIDISP_FLAG_FBLANK) && !(value & PPU_INIDISP_FLAG_FBLANK))
    {
        ppu_ReloadOamAddrReg();
    }
    mem_regs[PPU_REG_INIDISP] = value;
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
    mem_regs[PPU_REG_OBJSEL] = value;
    uint32_t obj_size_select = (value >> PPU_OBJSEL_SIZE_SHIFT) & PPU_OBJSEL_SIZE_MASK;
    cur_obj_sizes[0] = objsel_size_sel_sizes[obj_size_select][0];
    cur_obj_sizes[1] = objsel_size_sel_sizes[obj_size_select][1];
    uint32_t chr_base = ((value & PPU_OBJSEL_NAME_BASE_MASK) << 13) & 0x7fff;
    uint32_t name_sel = (chr_base + (((value >> PPU_OBJSEL_NAME_SEL_SHIFT) & PPU_OBJSEL_NAME_SEL_MASK) << 12)) & 0x7fff;
    obj_chr_base[0] = (struct chr16_t *)(vram + (chr_base << 1));
    obj_chr_base[1] = (struct chr16_t *)(vram + (name_sel << 1));
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

void ppu_ReloadOamAddrReg()
{
    ppu_reg_oam_addr = (((uint16_t)mem_regs[PPU_REG_OAMADDL]) | (((uint16_t)mem_regs[PPU_REG_OAMADDH] & 0x1) << 8));
    ppu_reg_oam_addr <<= 1;
    ppu_oam_lsb_toggle = 0;
}

void oamadd_write(uint32_t effective_address, uint8_t value)
{
    uint32_t reg = effective_address & 0xffff;
    mem_regs[reg] = value;

    if((mem_regs[PPU_REG_INIDISP] & PPU_INIDISP_FLAG_FBLANK) || (mem_regs[CPU_MEM_REG_HVBJOY] & CPU_HVBJOY_FLAG_VBLANK))
    {
        ppu_ReloadOamAddrReg();
    }
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

    if(latched_counter_reads[index])
    {
        value = value | (ppu2_last_bus_value & 0x7e);
    }

    ppu2_last_bus_value = value;

    latched_counter_reads[index] = !latched_counter_reads[index];

    return value;
}

/*
==================================================================================
==================================================================================
==================================================================================
*/

void oamdataw_write(uint32_t effective_address, uint8_t value)
{
    // if(!(mem_regs[PPU_REG_INIDISP] & PPU_INIDISP_FLAG_FBLANK) && !(mem_regs[CPU_MEM_REG_HVBJOY] & CPU_HVBJOY_FLAG_VBLANK))
    // {
    //     printf("write to oam ignored\n");
    //     return;
    // }

    if(ppu_reg_oam_addr < PPU_OAM_TABLE2_START)
    {
        if(ppu_oam_lsb_toggle)
        {
            oam.bytes[ppu_reg_oam_addr] = ppu_oam_lsb_latch;
            oam.bytes[ppu_reg_oam_addr + 1] = value;
        }
        else
        {
            ppu_oam_lsb_latch = value;
        }
    }
    else
    {
        oam.bytes[ppu_reg_oam_addr + ppu_oam_lsb_toggle] = value;
    }

    // printf("write %x to %d\n", value, ppu_reg_oam_addr >> 1);

    if(((mem_regs[PPU_REG_INIDISP] & PPU_INIDISP_FLAG_FBLANK) || 
        (mem_regs[CPU_MEM_REG_HVBJOY] & CPU_HVBJOY_FLAG_VBLANK)) &&
         ppu_oam_lsb_toggle)
    {
        ppu_reg_oam_addr = (ppu_reg_oam_addr + 2) % PPU_OAM_SIZE;
    }

    ppu_oam_lsb_toggle = !ppu_oam_lsb_toggle;
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
    ppu1_last_bus_value = oam.bytes[ppu_reg_oam_addr + ppu_oam_lsb_toggle];

    if(((mem_regs[PPU_REG_INIDISP] & PPU_INIDISP_FLAG_FBLANK) || 
        (mem_regs[CPU_MEM_REG_HVBJOY] & (CPU_HVBJOY_FLAG_VBLANK | CPU_HVBJOY_FLAG_HBLANK))) &&
        ppu_oam_lsb_toggle)
    {
        ppu_reg_oam_addr = (ppu_reg_oam_addr + 2) % PPU_OAM_SIZE;
    }

    ppu_oam_lsb_toggle = !ppu_oam_lsb_toggle;
    return ppu1_last_bus_value;
}

void update_bg_state()
{
    uint8_t bg_mode = mem_regs[PPU_REG_BGMODE];
    uint8_t through_main = mem_regs[PPU_REG_TMAIN];
    uint32_t last_main_background = 0;
    uint8_t through_sub = mem_regs[PPU_REG_TSUB];

//    struct bg_draw_t main_screen_backgrounds[5];
    struct background_t *main_screen_backgrounds[5];
    struct bg_draw_t sub_backgrounds[5];

    backgrounds[0].chr_size = 8 << ((bg_mode >> PPU_BGMODE_BG1_CHR_SIZE_SHIFT) & PPU_BGMODE_BG_CHR_SIZE_MASK);
    backgrounds[1].chr_size = 8 << ((bg_mode >> PPU_BGMODE_BG2_CHR_SIZE_SHIFT) & PPU_BGMODE_BG_CHR_SIZE_MASK);
    backgrounds[2].chr_size = 8 << ((bg_mode >> PPU_BGMODE_BG3_CHR_SIZE_SHIFT) & PPU_BGMODE_BG_CHR_SIZE_MASK);
    backgrounds[3].chr_size = 8 << ((bg_mode >> PPU_BGMODE_BG4_CHR_SIZE_SHIFT) & PPU_BGMODE_BG_CHR_SIZE_MASK);

    bg_mode &= PPU_BGMODE_MODE_MASK;

    main_screen_bg_count = 0;

    switch(bg_mode)
    {
        case PPU_BGMODE_MODE0:
        {
            union mode0_cgram_t *mode0_cgram = (union mode0_cgram_t *)ppu_cgram;
            backgrounds[0].pal_base = mode0_cgram->bg1_colors;
            backgrounds[0].color_func = COLOR_FUNC_CHR4;
            // backgrounds[0].color_func = bg_pal4_col;

            backgrounds[1].pal_base = mode0_cgram->bg2_colors;
            backgrounds[1].color_func = COLOR_FUNC_CHR4;
            // backgrounds[1].color_func = bg_pal4_col;

            backgrounds[2].pal_base = mode0_cgram->bg3_colors;
            backgrounds[2].color_func = COLOR_FUNC_CHR4;
            // backgrounds[2].color_func = bg_pal4_col;

            backgrounds[3].pal_base = mode0_cgram->bg4_colors;
            backgrounds[3].color_func = COLOR_FUNC_CHR4;
            // backgrounds[3].color_func = bg_pal4_col;

            main_screen_backgrounds[0] = &backgrounds[0];
            main_screen_backgrounds[1] = &backgrounds[1];
            main_screen_backgrounds[2] = &backgrounds[2];
            main_screen_backgrounds[3] = &backgrounds[3];

            last_main_background = 3;
        }
        break;

        case PPU_BGMODE_MODE1:
        {
            union mode12_cgram_t *mode12_cgram = (union mode12_cgram_t *)ppu_cgram;
            backgrounds[0].pal_base = mode12_cgram->bg12_colors;
            backgrounds[0].color_func = COLOR_FUNC_CHR16;
            // backgrounds[0].color_func = bg_pal16_col;

            backgrounds[1].pal_base = mode12_cgram->bg12_colors;
            backgrounds[1].color_func = COLOR_FUNC_CHR16;
            // backgrounds[1].color_func = bg_pal16_col;

            backgrounds[2].pal_base = mode12_cgram->bg3_colors;
            backgrounds[2].color_func = COLOR_FUNC_CHR4;
            // backgrounds[2].color_func = bg_pal4_col;

           main_screen_backgrounds[0] = &backgrounds[0];
           main_screen_backgrounds[1] = &backgrounds[1];
           main_screen_backgrounds[2] = &backgrounds[2];

            // main_screen_backgrounds[0] = &backgrounds[0];
            // main_screen_backgrounds[1] = &backgrounds[0];
            // main_screen_backgrounds[2] = &backgrounds[0];

            last_main_background = 2;
        }
        break;

        case PPU_BGMODE_MODE2:
        case PPU_BGMODE_MODE3:
        case PPU_BGMODE_MODE4:
            return;
        break;

        case PPU_BGMODE_MODE5:
        case PPU_BGMODE_MODE6:
        {
            union mode56_cgram_t *mode56_cgram = (union mode56_cgram_t *)ppu_cgram;
            backgrounds[0].pal_base = mode56_cgram->bg1_colors;
            // backgrounds[0].color_func = bg_pal16_col;
            backgrounds[0].color_func = COLOR_FUNC_CHR16;

            backgrounds[1].pal_base = mode56_cgram->bg2_colors;
            backgrounds[1].color_func = COLOR_FUNC_CHR4;
            // backgrounds[1].color_func = bg_pal4_col;

            main_screen_backgrounds[0] = &backgrounds[0];
            main_screen_backgrounds[1] = &backgrounds[1];

            last_main_background = 1;
        }
        break;

        case PPU_BGMODE_MODE7:
        {
            union mode347_cgram_t *mode7_cgram = (union mode347_cgram_t *)ppu_cgram;
            backgrounds[0].pal_base = &mode7_cgram->bg1_colors;
            backgrounds[0].color_func = COLOR_FUNC_CHR256;
            // backgrounds[0].color_func = bg7_pal256_col;

            main_screen_backgrounds[0] = &backgrounds[0];

//            main_screen_backgrounds[0] = (struct bg_draw_t){.background = &backgrounds[0], .color_func = bg7_pal256_col};
            last_main_background = 0;
        }
        break;
    }

    for(uint32_t index = 0; index <= last_main_background; index++)
    {
        // uint32_t background_index = last_main_background - index;
        uint32_t background_index = index;
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
    mem_regs[PPU_REG_BGMODE] = value;
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
    
    if(emu_IsPPURegOverriden(reg))
    {
        return;
    }

    mem_regs[reg] = value;
    value = (value >> 2) & 0x3f;
    // uint32_t offset = ((((uint32_t)value) << 10) & 0x7fff) << 1;

    uint32_t offset = (((uint32_t)value) << 11) & 0xffff;
    // uint32_t offset = value << 9;
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

    if(emu_IsPPURegOverriden(reg))
    {
        return;
    }

    uint32_t background_index = (reg - PPU_REG_BG12NBA) << 1;
    mem_regs[reg] = value;

    // uint32_t offset = ((((uint32_t)(value & 0x0f)) << 12) & 0x7fff) << 1;

    uint32_t offset = ((uint32_t)(value & 0x0f)) << 13;
    struct background_t *background = backgrounds + background_index;
    // background_index++;
    background->chr_base = vram + offset;
//    value >>= 4;
    offset = ((uint32_t)(value & 0xf0)) << 9;
//    offset = ((uint32_t)value) * 0x2000;
    background = backgrounds + 1;
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
    
    uint32_t background_index = (reg >> 1);
    /* two registers per background (h and v offsets), so bit 1 gives us the
    offset of the background we want to */
    // struct bg_offset_t *bg_offset = &bg_offsets[reg >> 1];
    struct background_t *background = backgrounds + background_index;
    /* for each background, the first register is the horizontal offset, the second
    is vertical offset. */
    uint32_t offset_index = reg & 1;
    // uint32_t sign_mask = 0;
    uint32_t value_mask;
    uint32_t bg_mode = mem_regs[PPU_REG_BGMODE];

    if((bg_mode & PPU_BGMODE_MODE_MASK) == PPU_BGMODE_MODE7)
    {
        value_mask = PPU_MODE7_MAX_UNSIGNED_COORD_VALUE;
    }
    else if(bg_mode & (PPU_BGMODE_BG1_CHR_SIZE_16x16 << (PPU_BGMODE_BG1_CHR_SIZE_SHIFT + background_index)))
    {
        value_mask = PPU_CHAR16_MAX_COORD_VALUE;
    }
    else
    {
        value_mask = PPU_CHAR8_MAX_COORD_VALUE;
    }

    if(background->offset.lsb_written[offset_index])
    {
        background->offset.offsets[offset_index] &= 0x00ff; 
        background->offset.offsets[offset_index] |= ((uint16_t)value << 8);
    }
    else
    {
        background->offset.offsets[offset_index] &= 0xff00;
        background->offset.offsets[offset_index] |= value;
    }

    background->offset.offsets[offset_index] &= value_mask;
    if(background->offset.offsets[offset_index] > PPU_MODE7_MAX_POSITIVE_COORD_VALUE)
    {
        /* sign extend */
        background->offset.offsets[offset_index] |= 0xe000;
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
    // return last_bus_value;
}

/*
==================================================================================
==================================================================================
==================================================================================
*/

void vmainc_write(uint32_t effective_address, uint8_t value)
{
    mem_regs[PPU_REG_VMAINC] = value;
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
    mem_regs[reg] = value;
    vram_addr = ((uint16_t)mem_regs[PPU_REG_VMADDL]) | (((uint16_t)mem_regs[PPU_REG_VMADDH]) << 8);
    vram_addr &= 0x7fff;
    vram_read_prefetch();
}

uint8_t vmadd_read(uint32_t effective_address)
{
    if((effective_address & 0xffff) == PPU_REG_VMADDL)
    {
        return ppu1_last_bus_value;
    }
    
    return last_bus_value;
}

/*
==================================================================================
==================================================================================
==================================================================================
*/

void vmdataw_write(uint32_t effective_address, uint8_t value)
{
    uint32_t write_order = mem_regs[PPU_REG_VMAINC] & 0x80;
    uint32_t reg = effective_address & 0xffff;
    uint32_t write_addr = (vram_addr << 1) | reg == PPU_REG_VMDATAWH;
    uint32_t increment_shift = vmadd_increment_shifts[mem_regs[PPU_REG_VMAINC] & 0x3];

    if((mem_regs[CPU_MEM_REG_HVBJOY] & CPU_HVBJOY_FLAG_VBLANK) || (mem_regs[PPU_REG_INIDISP] & PPU_INIDISP_FLAG_FBLANK))
    {
        if(emu_vram_breakpoint_bitmask[write_addr >> 2] & (EMU_BREAKPOINT_FLAG_WRITE << ((write_addr & 0x3) << 1)))
        {
            struct emu_thread_data_t *thread_data = emu_emulation_thread->data;
            thread_data->status |= EMU_STATUS_BREAKPOINT;
            thread_data->breakpoint_type = BREAKPOINT_TYPE_VRAM_WRITE;
            thread_data->breakpoint_data.vram.address = write_addr;
            thread_data->breakpoint_data.vram.data = value;
            thrd_Switch(emu_emulation_thread, &emu_main_thread);
        }

        // write_addr |= PPU_REG_VMDATAWH == reg;
        vram[write_addr] = value;
        vram_addr += ((write_order == PPU_VMDATA_ADDR_INC_LH && reg == PPU_REG_VMDATAWH) ||
                      (write_order == PPU_VMDATA_ADDR_INC_HL && reg == PPU_REG_VMDATAWL)) << increment_shift;
        vram_addr &= 0x7fff;
        // mem_regs[PPU_REG_VMADDL] = vram_addr & 0xff;
        // mem_regs[PPU_REG_VMADDH] = (vram_addr >> 8) & 0xff;
    }
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
    return last_bus_value;
}

/*
==================================================================================
==================================================================================
==================================================================================
*/

void cgadd_write(uint32_t effective_address, uint8_t value)
{
    uint32_t reg = effective_address & 0xffff;
    mem_regs[reg] = value;
    ppu_reg_cgram_addr = value << 1;
    ppu_cgram_lsb_toggle = 0;
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
    if((mem_regs[CPU_MEM_REG_HVBJOY] & (CPU_HVBJOY_FLAG_HBLANK | CPU_HVBJOY_FLAG_VBLANK)) || 
       (mem_regs[PPU_REG_INIDISP] & PPU_INIDISP_FLAG_FBLANK))
    {

        if(!ppu_cgram_lsb_toggle)
        {
            ppu_cgram_lsb_latch = value;
        }
        else
        {
            ppu_cgram[ppu_reg_cgram_addr] = ppu_cgram_lsb_latch;
            ppu_cgram[ppu_reg_cgram_addr + 1] = value;
            ppu_reg_cgram_addr = (ppu_reg_cgram_addr + 2) % PPU_CGRAM_SIZE;
        }

        ppu_cgram_lsb_toggle = !ppu_cgram_lsb_toggle;

        /* snes programmer manual, vol 1, page A-17 */
        if(ppu_reg_cgram_addr <= 2)
        {
            /* address 0 of cgram is background, so recompute the backdrop color whenever
            the first two bytes of the cgram are modified */
            union mode0_cgram_t *mode0_cgram = (union mode0_cgram_t *)ppu_cgram;
            backdrop_color = mode0_cgram->bg1_colors[0].colors[0];
            // uint16_t backdrop = mode0_cgram->bg1_colors[0].colors[0];

            // cur_backdrop.r = color_lut[(backdrop >> COL_DATA_R_SHIFT) & COL_DATA_MASK];
            // cur_backdrop.g = color_lut[(backdrop >> COL_DATA_G_SHIFT) & COL_DATA_MASK];
            // cur_backdrop.b = color_lut[(backdrop >> COL_DATA_B_SHIFT) & COL_DATA_MASK];
            // cur_backdrop.a = 255;
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
    mem_regs[reg] = value;
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
    mem_regs[reg] = value;
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
    mem_regs[PPU_REG_COLDATA] = value;
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
    mem_regs[PPU_REG_SETINI] = value;
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
    latched_counter_reads[0] = 0;
    latched_counter_reads[1] = 0;
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

    value = ppu_cgram[ppu_reg_cgram_addr + ppu_cgram_lsb_toggle];

    if(((mem_regs[PPU_REG_INIDISP] & PPU_INIDISP_FLAG_FBLANK) || 
        (mem_regs[CPU_MEM_REG_HVBJOY] & (CPU_HVBJOY_FLAG_VBLANK | CPU_HVBJOY_FLAG_HBLANK))) &&
        ppu_cgram_lsb_toggle)
    {
        ppu_reg_cgram_addr = (ppu_reg_cgram_addr + 2) % PPU_CGRAM_SIZE;
    }

    if(ppu_cgram_lsb_toggle)
    {
        value &= ~0x80;
        value |= ppu2_last_bus_value & 0x80;
    }

    ppu2_last_bus_value = value;

    ppu_cgram_lsb_toggle = !ppu_cgram_lsb_toggle;

    // if((mem_regs[CPU_MEM_REG_HVBJOY] & (CPU_HVBJOY_FLAG_HBLANK | CPU_HVBJOY_FLAG_VBLANK)) || 
    //    (mem_regs[PPU_REG_INIDISP] & PPU_INIDISP_FLAG_FBLANK))
    // {
    //     // value = ppu_cgram[ppu_reg_cgram_addr];

    //     // if(ppu_reg_cgram_addr & 1)
    //     // {
    //     //     value |= ppu2_last_bus_value & 0x80;
    //     // }

    //     // ppu2_last_bus_value = value;

    //     // ppu_reg_cgram_addr = (ppu_reg_cgram_addr + 1) % PPU_CGRAM_SIZE;
    // }

    return value;
}

/*
==================================================================================
==================================================================================
==================================================================================
*/

uint8_t stat77_read(uint32_t effective_address)
{
    ppu1_last_bus_value = (mem_regs[PPU_REG_STAT77] & ~0x10) | (ppu1_last_bus_value & 0x10);
    return ppu1_last_bus_value;
}

/*
==================================================================================
==================================================================================
==================================================================================
*/

uint8_t stat78_read(uint32_t effective_address)
{
    ppu2_last_bus_value = (mem_regs[PPU_REG_STAT78] & ~0x20) | (ppu2_last_bus_value & 0x20);
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

    // emu_Log("VRAM prefetch for vram address 0x%04x (0x%04x): %02x %02x", vram_addr, read_address, vram_prefetch[0], vram_prefetch[1]);

    // printf("vram prefetch: ")
}

uint8_t vmdatar_read(uint32_t effective_address)
{
    uint32_t read_order = mem_regs[PPU_REG_VMAINC] & 0x80;
    uint32_t reg = effective_address & 0xffff;
    uint32_t read_addr = (vram_addr << 1) | reg == PPU_REG_VMDATARH;
    uint32_t increment_shift = vmadd_increment_shifts[mem_regs[PPU_REG_VMAINC] & 0x3];

    if((mem_regs[CPU_MEM_REG_HVBJOY] & (CPU_HVBJOY_FLAG_VBLANK | CPU_HVBJOY_FLAG_HBLANK)) || 
       (mem_regs[PPU_REG_INIDISP] & PPU_INIDISP_FLAG_FBLANK))
    {
        ppu1_last_bus_value = vram_prefetch[reg == PPU_REG_VMDATARH];

        if((read_order == PPU_VMDATA_ADDR_INC_LH && reg == PPU_REG_VMDATARH) ||
           (read_order == PPU_VMDATA_ADDR_INC_HL && reg == PPU_REG_VMDATARL))
        {
            vram_read_prefetch();
            vram_addr += 1 << increment_shift;
            vram_addr &= 0x7fff;
            mem_regs[PPU_REG_VMADDL] = vram_addr & 0xff;
            mem_regs[PPU_REG_VMADDH] = (vram_addr >> 8) & 0xff;
        }

        if(emu_vram_breakpoint_bitmask[read_addr >> 2] & (EMU_BREAKPOINT_FLAG_READ << ((read_addr & 0x3) << 1)))
        {
            struct emu_thread_data_t *thread_data = emu_emulation_thread->data;
            thread_data->status |= EMU_STATUS_BREAKPOINT;
            thread_data->breakpoint_type = BREAKPOINT_TYPE_VRAM_READ;
            thread_data->breakpoint_data.vram.address = read_addr;
            thread_data->breakpoint_data.vram.data = ppu1_last_bus_value;
            thrd_Switch(emu_emulation_thread, &emu_main_thread);
        }
    }

    return ppu1_last_bus_value;
}


void update_irq_counter()
{
    uint16_t vtimer = (uint16_t)mem_regs[CPU_MEM_REG_VTIMEL] | ((uint16_t)mem_regs[CPU_MEM_REG_VTIMEH] << 8);
    uint16_t htimer = (uint16_t)mem_regs[CPU_MEM_REG_HTIMEL] | ((uint16_t)mem_regs[CPU_MEM_REG_HTIMEH] << 8);

    switch(mem_regs[CPU_MEM_REG_NMITIMEN] & (CPU_NMITIMEN_FLAG_HTIMER_EN | CPU_NMITIMEN_FLAG_VTIMER_EN))
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
    mem_regs[reg] = value;
    update_irq_counter();
}


void mpy_write(uint32_t effective_address, uint8_t value)
{

}

uint8_t mpy_read(uint32_t effective_address)
{
    uint32_t reg = effective_address & 0xffff;
    ppu1_last_bus_value = mem_regs[reg];
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
    mem_regs[reg] = value;
    wmdata_address = (uint32_t)mem_regs[PPU_REG_WMADDL] | ((uint32_t)mem_regs[PPU_REG_WMADDM] << 8) | ((uint32_t)mem_regs[PPU_REG_WMADDH] << 16);
    wmdata_address = PPU_WMDATA_BASE | (wmdata_address & PPU_WMADDR_MASK);
}

