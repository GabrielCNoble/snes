#include "apu.h"
#include <stdint.h>

struct apu_io_reg_t io_regs[4];

void init_apu()
{

}

void step_apu(int32_t cycles)
{

}

uint8_t apuio_read(uint32_t effective_address)
{
    uint32_t reg = (effective_address & 0xffff) - APU_REG_IO0;
    return io_regs[reg].read;
}

void apuio_write(uint32_t effective_address, uint8_t data)
{
    uint32_t reg = (effective_address & 0xffff) - APU_REG_IO0;
    io_regs[reg].read = data;
}
