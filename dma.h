#ifndef DMA_H
#define DMA_H

#include <stdint.h>

void step_dma(uint32_t cycle_count);

void mdmaen_write(uint32_t effective_address);

void hdmaen_write(uint32_t effective_address);

#endif