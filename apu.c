#include "apu.h"
#include <stdint.h>

struct apu_io_reg_t io_regs[4];
uint32_t            apu_state = APU_STATE_IDLE;
uint32_t            end_transfer_timer = 0;

void init_apu()
{
    io_regs[0].read = 0xaa;
    io_regs[1].read = 0xbb;
}

void step_apu(int32_t cycles)
{
    switch(apu_state)
    {
        case APU_STATE_IDLE:
            if(io_regs[0].write == 0xcc && io_regs[1].write != 0x00)
            {
                apu_state = APU_STATE_START_TRANSFER;
                io_regs[0].read = 0xcc;
            }
            else
            {
                io_regs[0].read = 0xaa;
                io_regs[1].read = 0xbb;
            }
        break;

        case APU_STATE_START_TRANSFER:
            if(io_regs[0].write == 0x00)
            {
                io_regs[0].read = 0x00;
                apu_state = APU_STATE_TRANSFER;
            }
        break;

        case APU_STATE_TRANSFER:
            uint8_t status = io_regs[0].write - io_regs[0].read;
            if(status == 1)
            {
                io_regs[0].read = io_regs[0].write;
            }
            else if(status > 1)
            {
                if(io_regs[1].write == 0x00)
                {
                    apu_state = APU_STATE_END_TRANSFER;
                    end_transfer_timer = 0;
                }
                else
                {
                    apu_state = APU_STATE_START_TRANSFER;
                }
                io_regs[0].read = io_regs[0].write;
            }
        break;

        case APU_STATE_END_TRANSFER:
            end_transfer_timer++;
            if(end_transfer_timer >= 0x3f)
            {
                io_regs[0].write = 0x00;
                apu_state = APU_STATE_IDLE;
            }
        break;
    }
}

uint8_t apuio_read(uint32_t effective_address)
{
    uint32_t reg = (effective_address & 0xffff) - APU_REG_IO0;
    return io_regs[reg].read;
}

void apuio_write(uint32_t effective_address, uint8_t data)
{
    uint32_t reg = (effective_address & 0xffff) - APU_REG_IO0;
    io_regs[reg].write = data;
}
