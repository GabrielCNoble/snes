#include "apu.h"
#include <stdint.h>

struct apu_io_reg_t io_regs[4];
uint32_t            apu_transfer_state = APU_STATE_IDLE;
uint32_t            end_transfer_timer = 0;

struct apu_state_t apu_state;

/* from Anomie's SPC700 doc */
uint8_t apu_ipl_boot_rom[] = {
    0xcd, 0xef, 0xbd, 0xe8, 
    0x00, 0xc6, 0x1d, 0xd0, 
    0xfc, 0x8f, 0xaa, 0xf4, 
    0x8f, 0xbb, 0xf5, 0x78,
    0xcc, 0xf4, 0xd0, 0xfb, 
    0x2f, 0x19, 0xeb, 0xf4, 
    0xd0, 0xfc, 0x7e, 0xf4, 
    0xd0, 0x0b, 0xe4, 0xf5,
    0xcb, 0xf4, 0xd7, 0x00, 
    0xfc, 0xd0, 0xf3, 0xab, 
    0x01, 0x10, 0xef, 0x7e, 
    0xf4, 0x10, 0xeb, 0xba,
    0xf6, 0xda, 0x00, 0xba, 
    0xf4, 0xc4, 0xf4, 0xdd, 
    0x5d, 0xd0, 0xdb, 0x1f, 
    0x00, 0x00, 0xc0, 0xff
};

void init_apu()
{
    io_regs[0].read = 0xaa;
    io_regs[1].read = 0xbb;
    reset_apu();
} 

void reset_apu()
{
    apu_state.regs[APU_REG_PC].word = 0xfffe;
    apu_state.regs[APU_REG_SP].word = 0x00ff;
    apu_state.reg_psw.c = 0;
    apu_state.reg_psw.z = 0;
    apu_state.reg_psw.i = 0;
    apu_state.reg_psw.h = 0;
    apu_state.reg_psw.b = 0;
    apu_state.reg_psw.p = 0;
    apu_state.reg_psw.v = 0;
    apu_state.reg_psw.n = 0;
}

void step_apu(int32_t cycles)
{
    switch(apu_transfer_state)
    {
        case APU_STATE_IDLE:
            if(io_regs[0].write == 0xcc && io_regs[1].write != 0x00)
            {
                apu_transfer_state = APU_STATE_START_TRANSFER;
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
                apu_transfer_state = APU_STATE_TRANSFER;
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
                    apu_transfer_state = APU_STATE_END_TRANSFER;
                    end_transfer_timer = 0;
                }
                else
                {
                    apu_transfer_state = APU_STATE_START_TRANSFER;
                }
                io_regs[0].read = io_regs[0].write;
            }
        break;

        case APU_STATE_END_TRANSFER:
            end_transfer_timer++;
            if(end_transfer_timer >= 0x3f)
            {
                io_regs[0].write = 0x00;
                apu_transfer_state = APU_STATE_IDLE;
            }
        break;
    }
}

uint8_t apuio_read(uint32_t effective_address)
{
    uint32_t reg = (effective_address & 0xffff) - APU_MEM_REG_IO0;
    return io_regs[reg].read;
}

void apuio_write(uint32_t effective_address, uint8_t data)
{
    uint32_t reg = (effective_address & 0xffff) - APU_MEM_REG_IO0;
    io_regs[reg].write = data;
}
