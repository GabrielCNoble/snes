#ifndef APU_H
#define APU_H
#include <stdint.h>

struct apu_io_reg_t
{
    uint16_t read;
    uint16_t write;
};

enum APU_REGS
{
    APU_REG_IO0     = 0x2140,
    APU_REG_IO1     = 0x2141,
    APU_REG_IO2     = 0x2142,
    APU_REG_IO3     = 0x2143,
};


void init_apu();

void step_apu(int32_t cycles);

uint8_t apuio_read(uint32_t effective_address, uint64_t master_cycles);

void apuio_write(uint32_t effective_address, uint64_t master_cycles, uint8_t data);

#endif
