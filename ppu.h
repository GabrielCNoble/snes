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

void *ppu_pointer(uint32_t effective_address);

void ppu_write(uint32_t effective_address, uint32_t data, uint32_t byte_write);

uint32_t ppu_read(uint32_t effective_address);

void step_ppu(uint32_t cycle_count);

#endif // PPU_H
