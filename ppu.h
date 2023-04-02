#ifndef PPU_H
#define PPU_H

#include <stdint.h>

//struct obj_attr_t
//{
//    uint8_t h_pos;
//    uint8_t v_pos;
//    /* f -> flip, p -> priority, c -> color, n-> name. If the arrangement
//    of bit fields in memory wasn't implementation defined this would
//    be a nice use of them.  */
//    uint16_t fpcn;
//};

#define OBJ_ATTR1_HPOS_MASK     0x01
#define OBJ_ATTR1_PAL_SHIFT     0x09
#define OBJ_ATTR1_PAL_MASK      0x07
#define OBJ_ATTR1_NAME_MASK     0x1ff
#define OBJ_ATTR1_HFLIP         0x4000
#define OBJ_ATTR1_VFLIP         0x8000
#define OBJ_ATTR1_PRI_SHIFT     0x0c
#define OBJ_ATTR1_PRI_MASK      0x03
union obj_attr1_t
{
    struct
    {
        uint8_t     h_pos;
        uint8_t     v_pos;
        uint16_t    fpcn;
    };

    uint32_t        word;
};

#define OBJ_ATTR2_HPOS_MASK 0x01
#define OBJ_ATTR2_SIZE_SHIFT 0x01
#define OBJ_ATTR2_SIZE_MASK 0x01
#define OBJ_ATTR2_DATA_MASK 0x03
struct obj_attr2_t
{
    uint16_t size_hpos;
};

#define MAX_OBJ_COUNT 128
#define TILE_SIZE 8
struct obj_t
{
    int16_t     hpos;
    int16_t     vpos;
    uint16_t    name;
    uint8_t     size;
    uint8_t     pal;
//    uint8_t     vflip : 1;
//    uint8_t     hflip : 1;
};

//struct dot_objs_t
//{
//    uint8_t         objects[128];
//    uint16_t        count;
//};

#define OBJ_PRIORITIES 4
//struct dot_obj_priorities_t
//{
//    struct dot_objs_t priorities[OBJ_PRIORITIES];
//};

#define BG_PRIORITIES 2
//struct dot_bg_priorities_t
//{
//    uint8_t           chr_name[BG_PRIORITIES];
//};

//struct tile_dot_priorities_t
//{
//    /* all objects that touch the current scanline */
//    struct dot_priorities_t *       objs;
//
//    /* all tiles of all backgrounds that touch the current dot at the current scanline */
//    struct dot_bg_priorities_t *    bgs[4];
//    uint32_t                        bg_count;
//};

enum COLOR_FUNCS
{
    COLOR_FUNC_CHR0,
    COLOR_FUNC_CHR2,
    COLOR_FUNC_CHR4,
    COLOR_FUNC_CHR8,
};

struct draw_tile_t
{
    uint16_t    name;
    uint16_t    start_x;
    uint8_t     start_y;
    uint8_t     pallete     : 4;
    uint8_t     color_func  : 4;
};

struct bg_draw_tile_t
{
    struct draw_tile_t  tile;
    uint8_t             background;
};

struct dot_obj_tiles_t
{
    uint16_t                tiles[MAX_OBJ_COUNT];
    uint16_t                tile_count;
};

//struct draw_tile_list_t
//{
//    uint16_t *              tiles;
//    uint16_t                tile_count;
//};

struct dot_bg_tiles_t
{
    /* there will be at most 2 tiles overlapping the same dot per priority */
    uint16_t                tiles[2];
    uint16_t                tile_count;
};

struct dot_tiles_t
{
    struct dot_obj_tiles_t  obj_tiles[OBJ_PRIORITIES];
    struct dot_bg_tiles_t   bg_tiles[OBJ_PRIORITIES];
};

#define PPU_OAM_TABLE1_START 0x0000
#define PPU_OAM_TABLE2_START 0x0200
#define PPU_OAM_SIZE 0x220
#define PPU_CGRAM_SIZE 0x200
#define PPU_VRAM_SIZE 0xffff

struct oam_tables_t
{
    union obj_attr1_t table1    [128];
    struct obj_attr2_t table2   [32];
};
union oam_t
{
    struct oam_tables_t             tables;
    uint8_t                         bytes[sizeof(struct oam_tables_t)];
};

#define SCANLINE_DOT_LENGTH     340
#define SCANLINE_COUNT          262
#define H_BLANK_END_DOT         1
#define H_BLANK_START_DOT       255

/*
    https://problemkaputt.github.io/fullsnes.htm#snestiminghvevents

    Didn't really find this explicitly stated in the official snes programmer
    manual, which is odd, but it's somewhat hinted at on page A-20. The display
    area described there is 256x224 or 256x239. H-blank starts at dot 274 and ends
    at dot 1. So, there's 21 dots that fall outside the left side of the screen...
*/
#define DRAW_START_DOT          0
#define DRAW_END_DOT            255
#define DRAW_START_LINE         1
//#define DRAW_END_LINE           224

#define V_BLANK_END_LINE 0

#define FRAMEBUFFER_WIDTH SCANLINE_DOT_LENGTH
#define FRAMEBUFFER_HEIGHT SCANLINE_COUNT
#define PPU_CYCLES_PER_DOT 4
//#define PPU_OAM_SIZE 0x420
struct dot_t
{
    uint8_t a;
    uint8_t b;
    uint8_t g;
    uint8_t r;
};

struct bg_offset_t
{
    uint16_t    offsets[2];
    uint8_t     lsb_written[2];
};

#define BG_SC_DATA_NAME_MASK    0x3ff
#define BG_SC_DATA_PAL_SHIFT    10
#define BG_SC_DATA_PAL_MASK     0x07
#define BG_SC_DATA_H_FLIP_SHIFT 14
#define BG_SC_DATA_V_FLIP_SHIFT 15
#define BG_SC_DATA_FLIP_MASK    0x01

struct bg_sc_data_t
{
    uint16_t data;
};

struct bg7_sc_data_t
{
    uint8_t name;
    uint8_t chr;
};

struct background_t
{
    struct bg_offset_t      offset;
    struct bg_sc_data_t *   data_base[4];
    void *                  chr_base;
    void *                  pal_base;
    uint16_t                chr_size;

    uint16_t           (*   color_func)(uint32_t dot_h, uint32_t dot_v, struct background_t *background);
};

enum PPU_REGS
{
    /*  */
    PPU_REG_INIDISP         = 0x2100,
    PPU_REG_OBJSEL          = 0x2101,
    PPU_REG_OAMADDL         = 0x2102,
    PPU_REG_OAMADDH         = 0x2103,
    PPU_REG_OAMDATAW        = 0x2104,
    PPU_REG_BGMODE          = 0x2105,
    PPU_REG_MOSAIC          = 0x2106,

    /* specifies the base vram address for background screen data (which
    are lists of characters), and also their size (whether 8x8 or 16x16 dot).
        7-2: screen data base vram address
        1-0: screen size
     */
    PPU_REG_BG1SC           = 0x2107,
    PPU_REG_BG2SC           = 0x2108,
    PPU_REG_BG3SC           = 0x2109,
    PPU_REG_BG4SC           = 0x210a,

    PPU_REG_BG12NBA         = 0x210b,
    PPU_REG_BG34NBA         = 0x210c,

    /* horizontal and vertical offsets for backgrounds 1-4. For modes 0-6,
    it's a 11 bit value from 0 to 1023. The value is the offset between the
    bottom right corner of the background and the top left corner of the display
    area.  */
    PPU_REG_BG1HOFS         = 0x210d,
    PPU_REG_BG1VOFS         = 0x210e,
    PPU_REG_BG2HOFS         = 0x210f,
    PPU_REG_BG2VOFS         = 0x2110,
    PPU_REG_BG3HOFS         = 0x2111,
    PPU_REG_BG3VOFS         = 0x2112,
    PPU_REG_BG4HOFS         = 0x2113,
    PPU_REG_BG4VOFS         = 0x2114,

    PPU_REG_VMAINC          = 0x2115,
    PPU_REG_VMADDL          = 0x2116,
    PPU_REG_VMADDH          = 0x2117,
    PPU_REG_VMDATAWL        = 0x2118,
    PPU_REG_VMDATAWH        = 0x2119,
    PPU_REG_M7SEL           = 0x211a,
    PPU_REG_M7A             = 0x211b,
    PPU_REG_M7B             = 0x211c,
    PPU_REG_M7C             = 0x211d,
    PPU_REG_M7D             = 0x211e,
    PPU_REG_M7X             = 0x211f,
    PPU_REG_M7Y             = 0x2120,
    PPU_REG_CGADD           = 0x2121,
    PPU_REG_CGDATAW         = 0x2122,
    PPU_REG_W12SEL          = 0x2123,
    PPU_REG_W34SEL          = 0x2124,
    PPU_REG_WCOLOBJSEL      = 0x2125,
    PPU_REG_W1L             = 0x2126,
    PPU_REG_W1R             = 0x2127,
    PPU_REG_W2L             = 0x2128,
    PPU_REG_W2R             = 0x2129,
    PPU_REG_WBGLOG          = 0x212a,
    PPU_REG_WCOLOBJLOG      = 0x212b,
    PPU_REG_TMAIN           = 0x212c,
    PPU_REG_TSUB            = 0x212d,
    PPU_REG_TMAINWM         = 0x212e,
    PPU_REG_TSUBWM          = 0x212f,
    PPU_REG_CGSWSEL         = 0x2130,
    PPU_REG_CGADSUB         = 0x2131,
    PPU_REG_COLDATA         = 0x2132,
    PPU_REG_SETINI          = 0x2133,
    PPU_REG_MPYL            = 0x2134,
    PPU_REG_MPYM            = 0x2135,
    PPU_REG_MPYH            = 0x2136,
    PPU_REG_SLVH            = 0x2137,
    PPU_REG_OAMDATAR        = 0x2138,
    PPU_REG_VMDATARL        = 0x2139,
    PPU_REG_VMDATARH        = 0x213a,
    PPU_REG_CGDATAR         = 0x213b,
    PPU_REG_OPHCT           = 0x213c,
    PPU_REG_OPVCT           = 0x213d,
    PPU_REG_STAT77          = 0x213e,
    PPU_REG_STAT78          = 0x213f,
    PPU_REG_WMDATA          = 0x2180,
    PPU_REG_WMADDL          = 0x2181,
    PPU_REG_WMADDM          = 0x2182,
    PPU_REG_WMADDH          = 0x2183,
};

#define PPU_WMDATA_BASE 0x7e0000
#define PPU_WMADDR_MASK 0x1ffff
// #define PPU_WMDATA_RAM1_END 0x2000

enum PPU_VMDATA_ADDR_INCS
{
    PPU_VMDATA_ADDR_INC_HL = 0,
    PPU_VMDATA_ADDR_INC_LH = 1 << 7,
};

enum PPU_STAT77_FLAGS
{
    PPU_STAT77_FLAG_33_RANGE_OVER = 1 << 6,
    PPU_STAT77_FLAG_35_TIME_OVER = 1 << 7,
};

enum PPU_STAT78_FLAGS
{
    PPU_STAT78_FLAG_FIELD = 1 << 7
};

enum PPU_CHR_BITDEPTHS
{
    PPU_CHR_BITDEPTH_0,
    PPU_CHR_BITDEPTH_2,
    PPU_CHR_BITDEPTH_4,
    PPU_CHR_BITDEPTH_8,
};

#define PPU_VMAINC_SC_INCREMENT_MASK    0x03
#define PPU_VMAINC_BITDEPTH_SHIFT       0x02
#define PPU_VMAINC_BITDEPTH_MASK        0x03

enum PPU_INIDISP_FLAGS
{
    PPU_INIDISP_FLAG_FBLANK = 1 << 7
};

enum PPU_VMADD_INC
{
    PPU_VMADD_INC1 = 0,
    PPU_VMADD_INC32,
    PPU_VMADD_INC64,
    PPU_VMADD_INC128,
};

enum PPU_SETINI_FLAGS
{
    PPU_SETINI_FLAG_INTERLACE   = 1,
    PPU_SETINI_FLAG_OBJV_SEL    = 1 << 1,
    PPU_SETINI_FLAG_BGV_SEL     = 1 << 2,
};

#define PPU_OBJSEL_SIZE_SHIFT       0x05
#define PPU_OBJSEL_SIZE_MASK        0x07
#define PPU_OBJSEL_NAME_BASE_MASK   0x07
#define PPU_OBJSEL_NAME_SEL_SHIFT   0x03
#define PPU_OBJSEL_NAME_SEL_MASK    0x03

enum PPU_OBJSEL_SIZE_SEL
{
    PPU_OBJSEL_SIZE_SEL_8_16 = 0,
    PPU_OBJSEL_SIZE_SEL_8_32,
    PPU_OBJSEL_SIZE_SEL_8_64,
    PPU_OBJSEL_SIZE_SEL_16_32,
    PPU_OBJSEL_SIZE_SEL_16_64,
    PPU_OBJSEL_SIZE_SEL_32_64,
};

#define PPU_BGMODE_MODE_MASK            0x07
#define PPU_BGMODE_BG_CHR_SIZE_MASK     0x01
#define PPU_BGMODE_BG1_CHR_SIZE_SHIFT   0x04
#define PPU_BGMODE_BG2_CHR_SIZE_SHIFT   0x05
#define PPU_BGMODE_BG3_CHR_SIZE_SHIFT   0x06
#define PPU_BGMODE_BG4_CHR_SIZE_SHIFT   0x07
enum PPU_BGMODE_MODES
{
    PPU_BGMODE_MODE0 = 0,
    PPU_BGMODE_MODE1,
    PPU_BGMODE_MODE2,
    PPU_BGMODE_MODE3,
    PPU_BGMODE_MODE4,
    PPU_BGMODE_MODE5,
    PPU_BGMODE_MODE6,
    PPU_BGMODE_MODE7,
    PPU_BGMODE_LAST
};

enum PPU_TMAIN_FLAGS
{
    PPU_TMAIN_FLAG_BG1 = 1,
    PPU_TMAIN_FLAG_BG2 = 1 << 1,
    PPU_TMAIN_FLAG_BG3 = 1 << 2,
    PPU_TMAIN_FLAG_BG4 = 1 << 3,
    PPU_TMAIN_FLAG_OBJ = 1 << 4,
};

enum PPU_TSUB_FLAGS
{
    PPU_TSUB_FLAG_BG1 = 1,
    PPU_TSUB_FLAG_BG2 = 1 << 1,
    PPU_TSUB_FLAG_BG3 = 1 << 2,
    PPU_TSUB_FLAG_BG4 = 1 << 3,
    PPU_TSUB_FLAG_OBJ = 1 << 4,
};

/*
    addressing goes from [<chr base address> + name * 8] to
    [from <chr base address> + name * 8 + 7].
*/
struct chr2_t
{
    uint16_t p01[8];
};

struct chr4_t
{
    uint16_t p01[8];
    uint16_t p23[8];
};

struct chr8_t
{
    uint16_t p01[8];
    uint16_t p23[8];
    uint16_t p45[8];
    uint16_t p67[8];
};

#define COL_DATA_MASK  0x1f
#define COL_DATA_R_SHIFT 0
#define COL_DATA_G_SHIFT 5
#define COL_DATA_B_SHIFT 10


struct pal4_t
{
    uint16_t colors[4];
};

struct pal16_t
{
    uint16_t colors[16];
};

//struct pal16e_t
//{
//    struct
//    {
//        uint8_t r;
//        uint8_t g;
//        uint8_t b;
//    } colors[16];
//};

struct pal256_t
{
    uint16_t colors[256];
};

//struct col_entry_t
//{
//    uint8_t color_index;
//    uint8_t pal_index;
//};

struct bg_tile_t
{
    uint16_t    chr_index;
    uint16_t    pal_index;
    int16_t     tile_dot_x;
    int16_t     tile_dot_y;
    uint16_t    hflip : 1;
    uint16_t    vflip : 1;
};

struct bg7_tile_t
{
    uint8_t chr_name;
    uint8_t chr_data;
};

 struct col_t
 {
     uint8_t r;
     uint8_t g;
     uint8_t b;
 };

struct mode0_cgram_t
{
    struct pal4_t       bg1_colors[8];
    struct pal4_t       bg2_colors[8];
    struct pal4_t       bg3_colors[8];
    struct pal4_t       bg4_colors[8];
    struct pal16_t      obj_colors[8];
};

struct mode12_cgram_t
{
    union
    {
        struct pal4_t       bg3_colors[8];
        struct pal16_t      bg12_colors[8];
    };
    struct pal16_t      obj_colors[8];
};

// struct mode3_cgram_t
// {
//     struct pal4_t       bg2_colors[8];
//     struct pal256_t     bg1_colors;
// };

struct mode56_cgram_t
{
    union
    {
        struct pal4_t   bg2_colors[8];
        struct pal16_t  bg1_colors[8];
    };

    struct pal16_t      obj_colors[8];
};

struct mode7_cgram_t
{
    union
    {
        struct pal256_t     bg1_colors;
        struct pal16_t      obj_colors[8];
    };
};

struct bg_draw_t
{
    struct background_t *   background;
    uint16_t           (*   color_func)(uint32_t dot_h, uint32_t dot_v, struct background_t *background);
};

enum PPU_BGMODE_FLAGS
{
    PPU_BGMODE_FLAG_BG3_PRIO      = 1 << 3,
};

enum PPU_BGMODE_CHR_SIZES
{
    PPU_BGMODE_BG1_CHR_SIZE_16x16 = 1 << 4,
    PPU_BGMODE_BG2_CHR_SIZE_16x16 = 1 << 5,
    PPU_BGMODE_BG3_CHR_SIZE_16x16 = 1 << 6,
    PPU_BGMODE_BG4_CHR_SIZE_16x16 = 1 << 7,
};

struct reg_write_t
{
    // struct reg_write_t *    next;
    uint64_t                cycle;
    uint16_t                reg;
    uint8_t                 value;
};

union counters_t
{
    struct
    {
        uint16_t hcounter;
        uint16_t vcounter;
    };

    uint32_t     counters;
};

void init_ppu();

void shutdown_ppu();

void reset_ppu();

uint8_t bg_chr0_dot_col(void *chr_base, uint32_t index, uint32_t size, uint32_t dot_h, uint32_t dot_v);

uint8_t chr0_dot(void *chr_base, uint32_t name, uint32_t dot_x, uint32_t dot_y);



uint8_t bg_chr2_dot_col(void *chr_base, uint32_t index, uint32_t size, uint32_t dot_h, uint32_t dot_v);

uint8_t chr2_dot(void *chr_base, uint32_t name, uint32_t dot_x, uint32_t dot_y);

struct col_t pal4_col(void *pal_base, uint8_t pallete, uint8_t index);

uint8_t bg_chr4_dot_col(void *chr_base, uint32_t index, uint32_t size, uint32_t dot_h, uint32_t dot_v);

uint8_t chr4_dot(void *chr_base, uint32_t name, uint32_t dot_x, uint32_t dot_y);

struct col_t pal16_col(void *pal_base, uint8_t pallete, uint8_t index);

uint8_t bg_chr8_dot_col(void *chr_base, uint32_t index, uint32_t size, uint32_t dot_h, uint32_t dot_v);

uint8_t chr8_dot(void *chr_base, uint32_t name, uint32_t dot_x, uint32_t dot_y);

struct col_t pal256_col(void *pal_base, uint8_t pallete, uint8_t index);

uint8_t bg7_chr8_dot_col(void *chr_base, uint32_t index, uint32_t dot_h, uint32_t dot_v);

struct bg_tile_t bg_tile_entry(uint32_t dot_h, uint32_t dot_v, struct background_t *background);

uint16_t bg_pal4_col(uint32_t dot_h, uint32_t dot_v, struct background_t *background);

uint16_t bg_pal16_col(uint32_t dot_h, uint32_t dot_v, struct background_t *background);

uint16_t bg_pal256_col(uint32_t dot_h, uint32_t dot_v, struct background_t *background);

uint16_t bg7_pal256_col(uint32_t dot_h, uint32_t dot_v, struct background_t *background);

//uint16_t obj_pal16_col(uint32_t dot_h, uint32_t dot_v, struct obj_t *obj);

//uint8_t

void update_objs();

void update_line_objs(uint16_t line);

void clear_scanline_objs();

void update_scanline_objs(uint16_t line);

void update_scanline_obj_tiles(uint16_t line);

void update_scanline_bg_tiles(uint16_t line, uint16_t dot);

uint32_t step_ppu(int32_t cycle_count);

void dump_ppu();

void dump_vram(uint32_t start, uint32_t end);


void    inidisp_write(uint32_t effective_address, uint8_t value);
uint8_t inidisp_read(uint32_t effective_address);


void    objsel_write(uint32_t effective_address, uint8_t value);
uint8_t objsel_read(uint32_t effective_address);


void    oamadd_write(uint32_t effective_address, uint8_t value);
uint8_t oamadd_read(uint32_t effective_address);


void    oamdataw_write(uint32_t effective_address, uint8_t value);
uint8_t oamdataw_read(uint32_t effective_address);

void    update_bg_state();
void    bgmode_write(uint32_t effective_address, uint8_t value);
uint8_t bgmode_read(uint32_t effective_address);


void    mosaic_write(uint32_t effective_address, uint8_t value);
uint8_t mosaic_read(uint32_t effective_address);


void    bgsc_write(uint32_t effective_address, uint8_t value);
uint8_t bgsc_read(uint32_t effective_address);


void    bgnba_write(uint32_t effective_address, uint8_t value);
uint8_t bgnba_read(uint32_t effective_address);


void    bgoffs_write(uint32_t effective_address, uint8_t value);
uint8_t bgoffs_read(uint32_t effective_address);


void    vmainc_write(uint32_t effective_address, uint8_t value);
uint8_t vmainc_read(uint32_t effective_address);


void    vmadd_write(uint32_t effective_address, uint8_t value);
uint8_t vmadd_read(uint32_t effective_address);


void    vmdataw_write(uint32_t effective_address, uint8_t value);
uint8_t vmdataw_read(uint32_t effective_address);


void    m7sel_write(uint32_t effective_address, uint8_t value);
uint8_t m7sel_read(uint32_t effective_address);


void    mrot_write(uint32_t effective_address, uint8_t value);
uint8_t mrot_read(uint32_t effective_address);


void    mpos_write(uint32_t effective_address, uint8_t value);
uint8_t mpos_read(uint32_t effective_address);


void    cgadd_write(uint32_t effective_address, uint8_t value);
uint8_t cgadd_read(uint32_t effective_address);


void    cgdataw_write(uint32_t effective_address, uint8_t value);
uint8_t cgdataw_read(uint32_t effective_address);


void    wsel_write(uint32_t effective_address, uint8_t value);
uint8_t wsel_read(uint32_t effective_address);


void    wobjcolsel_write(uint32_t effective_address, uint8_t value);
uint8_t wobjcolsel_read(uint32_t effective_address);


void    wlr_write(uint32_t effective_address, uint8_t value);
uint8_t wlr_read(uint32_t effective_address);


void    wbglog_write(uint32_t effective_address, uint8_t value);
uint8_t wbglog_read(uint32_t effective_address);


void    wcolobjlog_write(uint32_t effective_address, uint8_t value);
uint8_t wcolobjlog_read(uint32_t effective_address);


void    tmain_write(uint32_t effective_address, uint8_t value);
uint8_t tmain_read(uint32_t effective_address);


void    tsub_write(uint32_t effective_address, uint8_t value);
uint8_t tsub_read(uint32_t effective_address);

uint8_t slhv_read(uint32_t effective_address);

uint8_t opct_read(uint32_t effective_address);



uint8_t oamdatar_read(uint32_t effective_address);









void vram_read_prefetch();

uint8_t vmdatar_read(uint32_t effective_address);



void coldata_write(uint32_t effective_address, uint8_t value);

void update_irq_counter();

void vhtime_write(uint32_t effective_address, uint8_t value);




uint8_t cgdata_read(uint32_t effective_address);

uint8_t stat77_read(uint32_t effective_address);

void setinit_write(uint32_t effective_address, uint8_t value);

uint8_t mpy_read(uint32_t effective_address);

void wmdata_write(uint32_t effective_address, uint8_t value);

uint8_t wmdata_read(uint32_t effective_address);

void wmadd_write(uint32_t effective_address, uint8_t value);


#endif // PPU_H
