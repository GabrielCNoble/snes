#ifndef PPU_H
#define PPU_H

#include <stdint.h>

#pragma pack(push, 1)
struct obj_attr_t
{
    uint8_t h_pos;
    uint8_t v_pos;
    /* f -> flip, p -> priority, c -> color, n-> name. If the arrangement
    of bit fields in memory wasn't implementation defined this would 
    be a nice use of them.  */
    uint16_t fpcn;
};
#pragma pack(pop)

void *ppu_regs_pointer(uint32_t effective_address);

void ppu_regs_write(uint32_t effective_address, uint8_t data);

uint8_t ppu_regs_read(uint32_t effective_address);

void reset_ppu();

void step_ppu(uint32_t cycle_count);

void dump_ppu();

#endif // PPU_H
