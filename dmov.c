#include "dmov.h"
#include "addr.h"
#include "cpu.h"
#include "ppu.h"
#include "rom.h"

uint32_t data_access_location(uint32_t effective_address)
{
    unsigned int temp_address;
    unsigned int temp_bank;

    temp_address = effective_address & 0xffff;

    if(temp_address >= PPU_START_ADDRESS && temp_address <= PPU_END_ADDRESS)
    {
        /* ppu registers... */
        return ACCESS_LOCATION_PPU;
    }
    else if(temp_address >= CPU_REGS_START_ADDRESS && temp_address <= CPU_REGS_END_ADDRESS)
    {
        return ACCESS_LOCATION_REGS;
    }
    else
    {
        if(effective_address >= RAM2_ADDRESS_START && effective_address <= RAM2_ADDRESS_END)
        {
            /* WRAM2... */
            return ACCESS_LOCATION_WRAM2;
        }
        else
        {
            temp_address = effective_address & 0xffff;
            temp_bank = (effective_address & 0x00ff0000) >> 16;
            /* either WRAM1 or ROM access... */
            if((temp_address >= RAM1_START_ADDRESS && temp_address <= RAM1_END_ADDRESS) &&
               ((temp_bank >= 0x00 && temp_bank <= 0x3f) ||
                (temp_bank >= 0x80 && temp_bank <= 0xbf)))
            {
                /* WRAM1... */
                return ACCESS_LOCATION_WRAM1;
            }
            else
            {
                /* ROM... */
                return ACCESS_LOCATION_ROM;
            }
        }
    }

}

void *memory_pointer(uint32_t effective_address)
{
    uint32_t access_location = data_access_location(effective_address);
    void *pointer;
    switch(access_location)
    {
        case ACCESS_LOCATION_REGS:
        case ACCESS_LOCATION_WRAM1:
        case ACCESS_LOCATION_WRAM2:
            pointer = cpu_pointer(effective_address, access_location);
        break;
        
        case ACCESS_LOCATION_PPU:
            pointer = ppu_pointer(effective_address);
        break;
        
        case ACCESS_LOCATION_ROM:
            pointer = rom_pointer(effective_address);
        break;
    }
    
    return pointer;
}

void write_data(uint32_t effective_address, uint32_t data, uint32_t byte_write)
{
    uint32_t access_location = data_access_location(effective_address);
    switch(access_location)
    {
        case ACCESS_LOCATION_REGS:
        case ACCESS_LOCATION_WRAM1:
        case ACCESS_LOCATION_WRAM2:
            cpu_write(effective_address, data, access_location, byte_write);
        break;
        
        case ACCESS_LOCATION_PPU:
            ppu_write(effective_address, data, byte_write);
        break;
        
        case ACCESS_LOCATION_ROM:
            
        break;
    }
}

uint32_t read_data(uint32_t effective_address)
{
    uint32_t access_location = data_access_location(effective_address);
    uint32_t data;
    
    switch(access_location)
    {
        case ACCESS_LOCATION_REGS:
        case ACCESS_LOCATION_WRAM1:
        case ACCESS_LOCATION_WRAM2:
            data = cpu_read(effective_address, access_location);
        break;
        
        case ACCESS_LOCATION_PPU:
            data = ppu_read(effective_address);
        break;
        
        case ACCESS_LOCATION_ROM:
            data = rom_read(effective_address);
        break;
    }
    
    return data;
}








