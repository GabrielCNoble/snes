#include "apu.h"
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

// struct apu_io_reg_t io_regs[4];

typedef uint32_t (*apu_uop_func_t)(uint32_t arg);

struct apu_uop_t
{
    apu_uop_func_t      func;
    uint32_t            arg;
};

struct apu_inst_t
{
    struct apu_uop_t    uops[24];
};

// struct apu_port_t           apu_ports[4];
uint32_t                    apu_transfer_state = APU_STATE_IDLE;
uint32_t                    end_transfer_timer = 0;
uint8_t *                   apu_ram;
struct apu_io_reg_funcs_t   apu_io_regs[APU_IO_REG_COUNT] = {
    [APU_IO_REG_TEST]       = { .read = apu_IoTestRead,     .write = apu_IoTestWrite },
    [APU_IO_REG_CONTROL]    = { .read = apu_IoControlRead,  .write = apu_IoControlWrite },
    [APU_IO_REG_PORT0]      = { .read = apu_IoPortRead,     .write = apu_IoPortWrite },
    [APU_IO_REG_PORT1]      = { .read = apu_IoPortRead,     .write = apu_IoPortWrite },
    [APU_IO_REG_PORT2]      = { .read = apu_IoPortRead,     .write = apu_IoPortWrite },
    [APU_IO_REG_PORT3]      = { .read = apu_IoPortRead,     .write = apu_IoPortWrite },
    [APU_IO_REG_AUX04]      = {},
    [APU_IO_REG_AUX05]      = {},
    [APU_IO_REG_T0DIV]      = { .read = apu_IoTimerDivRead, .write = apu_IoTimerDivWrite },
    [APU_IO_REG_T1DIV]      = { .read = apu_IoTimerDivRead, .write = apu_IoTimerDivWrite },
    [APU_IO_REG_T2DIV]      = { .read = apu_IoTimerDivRead, .write = apu_IoTimerDivWrite },
    [APU_IO_REG_T0OUT]      = { .read = apu_IoTimerOutRead, .write = apu_IoTimerOutWrite },
    [APU_IO_REG_T1OUT]      = { .read = apu_IoTimerOutRead, .write = apu_IoTimerOutWrite },
    [APU_IO_REG_T2OUT]      = { .read = apu_IoTimerOutRead, .write = apu_IoTimerOutWrite },
};

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


struct apu_inst_t apu_instructions[APU_OPCODE_LAST] = {
    [APU_OPCODE_FETCH] = {

    }
};

void apu_Init()
{
    // io_regs[0].read = 0xaa;
    // io_regs[1].read = 0xbb;

    apu_ram = calloc(1, APU_RAM_SIZE);

    apu_Reset();

    apu_state.ports[0].read = 0xaa;
    apu_state.ports[1].read = 0xbb;
} 

void apu_Reset()
{
    for(uint32_t offset = 0; offset < APU_RAM_SIZE;)
    {
        for(uint32_t index = 0; index < 32; index++)
        {
            apu_ram[offset] = 0x00;
            offset++;
        }

        for(uint32_t index = 0; index < 32; index++)
        {
            apu_ram[offset] = 0xff;
            offset++;
        }
    }

    memcpy(apu_ram + APU_ROM_START, apu_ipl_boot_rom, sizeof(apu_ipl_boot_rom));

    apu_state.regs[APU_REG_PC].word = 0xfffe;
    apu_state.regs[APU_REG_SP].word = 0x01ff;
    apu_state.reg_psw.c = 0;
    apu_state.reg_psw.z = 0;
    // apu_state.reg_psw.i = 0;
    apu_state.reg_psw.h = 0;
    // apu_state.reg_psw.b = 0;
    apu_state.reg_psw.p = 0;
    apu_state.reg_psw.v = 0;
    apu_state.reg_psw.n = 0;

    apu_state.ports[0].read = 0;
    apu_state.ports[0].write = 0;
    apu_state.ports[1].read = 0;
    apu_state.ports[1].write = 0;
    apu_state.ports[2].read = 0;
    apu_state.ports[2].write = 0;
    apu_state.ports[3].read = 0;
    apu_state.ports[3].write = 0;

    apu_state.regs[APU_REG_INST].word = APU_OPCODE_FETCH;
}

void apu_Step(int32_t cycles)
{
    switch(apu_transfer_state)
    {
        case APU_STATE_IDLE:
            if(apu_state.ports[0].read == 0xcc && apu_state.ports[1].read != 0x00)
            {
                apu_transfer_state = APU_STATE_START_TRANSFER;
                apu_state.ports[0].write = 0xcc;
            }
            else
            {
                apu_state.ports[0].write = 0xaa;
                apu_state.ports[1].write = 0xbb;
            }
        break;

        case APU_STATE_START_TRANSFER:
            if(apu_state.ports[0].read == 0x00)
            {
                apu_state.ports[0].write = 0x00;
                apu_transfer_state = APU_STATE_TRANSFER;
            }
        break;

        case APU_STATE_TRANSFER:
            uint8_t status = apu_state.ports[0].read - apu_state.ports[0].write;
            if(status == 1)
            {
                apu_state.ports[0].write = apu_state.ports[0].read;
            }
            else if(status > 1)
            {
                if(apu_state.ports[1].read == 0x00)
                {
                    apu_transfer_state = APU_STATE_END_TRANSFER;
                    end_transfer_timer = 0;
                }
                else
                {
                    apu_transfer_state = APU_STATE_START_TRANSFER;
                }
                apu_state.ports[0].write = apu_state.ports[0].read;
            }
        break;

        case APU_STATE_END_TRANSFER:
            end_transfer_timer++;
            if(end_transfer_timer >= 0x3f)
            {
                apu_state.ports[0].read = 0x00;
                apu_transfer_state = APU_STATE_IDLE;
            }
        break;
    }
}

uint8_t apu_ReadByte(uint16_t address)
{
    if(address >= APU_IO_REGS_START && address <= APU_IO_REGS_END)
    {
        uint32_t reg_index = address - APU_IO_REGS_START;

        if(apu_io_regs[reg_index].read != NULL)
        {
            return apu_io_regs[reg_index].read(address);
        }
    }

    return apu_ram[address];
}

void apu_WriteByte(uint16_t address, uint8_t value)
{
    if(address >= APU_IO_REGS_START && address <= APU_IO_REGS_END)
    {
        uint32_t reg_index = address - APU_IO_REGS_START;
        
        if(apu_io_regs[reg_index].read != NULL)
        {
            apu_io_regs[reg_index].write(address, value);
        }
    }

    /* writes to io registers also affect ram */
    if(address < APU_ROM_START)
    {
        apu_ram[address] = value;
    }
}

void apu_IoTestWrite(uint16_t address, uint8_t value)
{

}

uint8_t apu_IoTestRead(uint16_t address)
{

}

void apu_IoControlWrite(uint16_t address, uint8_t value)
{
    if(value & APU_IO_CONTROL_REG_FLAG_PC10)
    {
        apu_state.ports[0].read = 0;
        apu_state.ports[0].write = 0;
        apu_state.ports[1].read = 0;
        apu_state.ports[1].write = 0;
    }

    if(value & APU_IO_CONTROL_REG_FLAG_PC23)
    {
        apu_state.ports[2].read = 0;
        apu_state.ports[2].write = 0;
        apu_state.ports[3].read = 0;
        apu_state.ports[3].write = 0;
    }
}

uint8_t apu_IoControlRead(uint16_t address)
{

}

void apu_IoDspAddrWrite(uint16_t address, uint8_t value)
{

}

uint8_t apu_IoDspAddrRead(uint16_t address, uint8_t value)
{

}

void apu_IoDspDataWrite(uint16_t address, uint8_t value)
{

}

uint8_t apu_IoDspDataRead(uint16_t address)
{

}

void apu_IoPortWrite(uint16_t address, uint8_t value)
{
    uint32_t port_index = address - APU_IO_REGS_START;
    apu_state.ports[port_index].write = value;
}

uint8_t apu_IoPortRead(uint16_t address)
{
    uint32_t port_index = address - APU_IO_REGS_START;
    return apu_state.ports[port_index].read;
}

void apu_IoTimerDivWrite(uint16_t address, uint8_t value)
{

}

uint8_t apu_IoTimerDivRead(uint16_t address)
{

}

void apu_IoTimerOutWrite(uint16_t address, uint8_t value)
{

}

uint8_t apu_IoTimerOutRead(uint16_t address)
{

}



uint8_t apuio_read(uint32_t effective_address)
{
    uint32_t reg = (effective_address & 0xffff) - APU_MEM_REG_IO0;
    return apu_state.ports[reg].write;
}

void apuio_write(uint32_t effective_address, uint8_t data)
{
    uint32_t reg = (effective_address & 0xffff) - APU_MEM_REG_IO0;
    apu_state.ports[reg].read = data;
}