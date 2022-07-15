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

enum PPU_REGS
{
    PPU_REG_INIDISP         = 0x2100,
    PPU_REG_OBJSEL          = 0x2101,
    PPU_REG_OAMADDL         = 0x2102,
    PPU_REG_OAMADDH         = 0x2103,
    PPU_REG_OAMDATA         = 0x2104,
    PPU_REG_BGMODE          = 0x2105,
    PPU_REG_MOSAIC          = 0x2106,
    PPU_REG_BG1SC           = 0x2107,
    PPU_REG_BG2SC           = 0x2108,
    PPU_REG_BG3SC           = 0x2109,
    PPU_REG_BG4SC           = 0x210a,
    PPU_REG_BG12NBA         = 0x210b,
    PPU_REG_BG34NBA         = 0x210c,
    PPU_REG_BG1H0FS         = 0x210d,
    PPU_REG_BG1H0VS         = 0x210e,
    PPU_REG_BG2H0FS         = 0x210f,
    PPU_REG_BG2H0VS         = 0x2110,
    PPU_REG_BG3H0FS         = 0x2111,
    PPU_REG_BG3H0VS         = 0x2112,
    PPU_REG_BG4H0FS         = 0x2113,
    PPU_REG_BG4H0VS         = 0x2114,
    PPU_REG_VMAINC          = 0x2115,
    PPU_REG_VMADDL          = 0x2116,
    PPU_REG_VMADDH          = 0x2117,
    PPU_REG_VMDATAL         = 0x2118,
    PPU_REG_VMDATAH         = 0x2119,
    PPU_REG_M7SEL           = 0x211a,
    PPU_REG_M7A             = 0x211b,
    PPU_REG_M7B             = 0x211c,
    PPU_REG_M7C             = 0x211d,
    PPU_REG_M7D             = 0x211e,
    PPU_REG_M7X             = 0x211f,
    PPU_REG_M7Y             = 0x2120,
    PPU_REG_CGADD           = 0x2121,
    PPU_REG_CGDATA          = 0x2122,
};

enum PPU_VMDATA_WRITES
{
    PPU_VMDATA_WRITE_HL = 0,
    PPU_VMDATA_WRITE_LH = 1 << 7,
};

void *ppu_regs_pointer(uint32_t effective_address);

void ppu_regs_write(uint32_t effective_address, uint8_t data);

uint8_t ppu_regs_read(uint32_t effective_address);

void reset_ppu();

void step_ppu(uint32_t cycle_count);

void dump_ppu();

void vmadd_write(uint32_t effective_address);

void vmdata_write(uint32_t effective_address);

// void 

#endif // PPU_H
