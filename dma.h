#ifndef DMA_H
#define DMA_H

#include <stdint.h>

enum DMA_WRITE_MODES
{
    DMA_WRITE_MODE_BYTE_ONCE = 0,
    DMA_WRITE_MODE_WORDLH_ONCE,
    DMA_WRITE_MODE_BYTE_TWICE,
    DMA_WRITE_MODE_WORDLH_TWICE,
    DMA_WRITE_MODE_DWORDLHLH_ONCE,
};

enum DMA_ADDR_MODES
{
    DMA_ADDR_MODE_FIXED     = 1,
    DMA_ADDR_MODE_INC       = 0,
    DMA_ADDR_MODE_DEC       = 2
};

enum DMA_DIRECTION
{
    DMA_DIRECTION_CPU_PPU = 0,
    DMA_DIRECTION_PPU_CPU,
};

struct dma_t
{
    uint32_t    addr;
    uint32_t    count;
    uint16_t    regs[4];
    uint8_t     channel : 3;
    // uint8_t     write_mode : 3;
    uint8_t     addr_mode : 2;
    uint8_t     direction : 1;
};

void step_dma(uint32_t cycle_count);

void mdmaen_write(uint32_t effective_address);

void hdmaen_write(uint32_t effective_address);

#endif