#ifndef DMOV_H
#define DMOV_H

#include <stdint.h>

enum ACCESSS_LOCATION
{
    ACCESS_LOCATION_PPU = 0,
    ACCESS_LOCATION_APU,
    ACCESS_LOCATION_WRAM1,
    ACCESS_LOCATION_WRAM2,
    ACCESS_LOCATION_REGS,
    ACCESS_LOCATION_ROM,
    ACCESS_LOCATION_CONTROLLER,
    ACCESS_LOCATION_DMA,
};

uint32_t data_access_location(uint32_t effective_address);

void *memory_pointer(uint32_t effective_address);

void write_data(uint32_t effective_address, uint32_t data, uint32_t byte_write);

uint32_t read_data(uint32_t effective_address);


#endif // DMOV_H
