#ifndef PPU_H
#define PPU_H

#include <stdint.h>

struct obj_attr_t
{
    uint8_t h_pos;
    uint8_t v_pos;
    /* f -> flip, p -> priority, c -> color, n-> name. If the arrangement
    of bit fields in memory wasn't implementation defined this would
    be a nice use of them.  */
    uint16_t fpcn;
};

#define SCANLINE_DOT_LENGTH 340
#define SCANLINE_COUNT 262
#define H_BLANK_END_DOT 1
#define H_BLANK_START_DOT 274

#define V_BLANK_END_LINE 0

#define FRAMEBUFFER_WIDTH SCANLINE_DOT_LENGTH
#define FRAMEBUFFER_HEIGHT SCANLINE_COUNT
#define PPU_CYCLES_PER_DOT 4
#define PPU_OAM_SIZE 0x220

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

enum PPU_REGS
{
    /*  */
    PPU_REG_INIDISP         = 0x2100,
    PPU_REG_OBJSEL          = 0x2101,
    PPU_REG_OAMADDL         = 0x2102,
    PPU_REG_OAMADDH         = 0x2103,
    PPU_REG_OAMDATA         = 0x2104,
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
    PPU_REG_CGDATA          = 0x2122,
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
    PPU_REG_COLDATA         = 0x2132,
    PPU_REG_SETINI          = 0x2133,
    PPU_REG_SLVH            = 0x2137,
    PPU_REG_OPHCT           = 0x213c,
    PPU_REG_OPVCT           = 0x213d,
    PPU_REG_VMDATARL        = 0x2139,
    PPU_REG_VMDATARH        = 0x213a
};

enum PPU_VMDATA_ADDR_INCS
{
    PPU_VMDATA_ADDR_INC_HL = 0,
    PPU_VMDATA_ADDR_INC_LH = 1 << 7,
};

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

enum PPU_OBJSEL_SIZE_SEL
{
    PPU_OBJSEL_SIZE_SEL_8_16 = 0,
    PPU_OBJSEL_SIZE_SEL_8_32,
    PPU_OBJSEL_SIZE_SEL_8_64,
    PPU_OBJSEL_SIZE_SEL_16_32,
    PPU_OBJSEL_SIZE_SEL_16_64,
    PPU_OBJSEL_SIZE_SEL_32_64,
};

#define PPU_BGMODE_MODE_MASK 0x07
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

void init_ppu();

void shutdown_ppu();

void reset_ppu();

struct reg_write_t *queue_write(uint16_t reg, uint64_t cycle, uint8_t value);

void apply_writes();

uint32_t step_ppu(int32_t cycle_count);

void mode0_draw();

void dump_ppu();

void inidisp_write(uint32_t effective_address, uint8_t value);

uint8_t slhv_read(uint32_t effective_address);

uint8_t opct_read(uint32_t effective_address);

void vmadd_write(uint32_t effective_address, uint8_t value);

void vmdata_write(uint32_t effective_address, uint8_t value);

uint8_t vmdata_read(uint32_t effective_address);

void oamadd_write(uint32_t effective_address, uint8_t value);

void oamdata_write(uint32_t effective_address, uint8_t value);

void bgmode_write(uint32_t effective_address, uint8_t value);

void bgsc_write(uint32_t effective_address, uint8_t value);

void bgoffs_write(uint32_t effective_address, uint8_t value);

void coldata_write(uint32_t effective_address, uint8_t value);



#endif // PPU_H
