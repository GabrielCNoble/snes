#include <stdlib.h>
#include <stdio.h>
#include "mem.h"
#include "addr.h"
#include "cpu.h"
#include "ppu.h"
#include "rom.h"
#include "dma.h"

// struct mem_write_t cpu_reg_writes[CPU_REGS_END - CPU_REGS_START] = {

// };

// struct mem_write_t ppu_reg_writes[PPU_REGS_END - PPU_REGS_START] = {

// };
uint8_t *               ram1_regs;
uint8_t *               ram2;
uint8_t *               vram;
struct mem_write_t *    reg_writes;
struct mem_read_t *     reg_reads;

void mem_init()
{
    /* 8KB of wram1 + cpu,ppu,etc regs */
    ram1_regs = calloc(RAM1_REGS_END - RAM1_REGS_START, 1);
    /* 120K of wram2 */
    ram2 = calloc(0x1e000, 1);
    /* 64K of vram */
    vram = calloc(0xffff, 1);
    /* write behavior for all mem mapped registers */
    reg_writes = calloc(RAM1_REGS_END - RAM1_REGS_START, sizeof(struct mem_write_t));
    reg_reads = calloc(RAM1_REGS_END - RAM1_REGS_START, sizeof(struct mem_read_t));

    reg_writes[CPU_REG_MDMAEN].notify = mdmaen_write;
    reg_writes[CPU_REG_HDMAEN].notify = hdmaen_write;

    reg_writes[PPU_REG_VMADDL].notify = vmadd_write;
    reg_writes[PPU_REG_VMADDH].notify = vmadd_write;
    reg_writes[PPU_REG_VMDATAL].notify = vmdata_write;
    reg_writes[PPU_REG_VMDATAH].notify = vmdata_write;

}

void mem_shutdonwn()
{
    free(ram1_regs);
    free(ram2);
    free(vram);
    free(reg_writes);
    free(reg_reads);
}

uint32_t access_location(uint32_t effective_address)
{
    uint32_t offset;
    uint32_t bank;

    offset = effective_address & 0xffff;
    bank = (effective_address >> 16) & 0xff;

    if((bank >= RAM1_REGS_BANK_RANGE0_START && bank <= RAM1_REGS_BANK_RANGE0_START ||
       bank >= RAM1_REGS_BANK_RANGE1_START && bank <= RAM1_REGS_BANK_RANGE1_END) && offset < RAM1_REGS_END)
    {
        if(offset < RAM1_END)
        {
            return ACCESS_RAM1;
        }

        return ACCESS_RAM1_REGS;
    }

    if(bank == RAM1_EXTRA_BANK && offset < RAM1_END)
    {
        return ACCESS_RAM1;
    }

    if(effective_address >= RAM2_START && effective_address < RAM2_END)
    {
        return ACCESS_RAM2;
    }

    return ACCESS_ROM;
}

void *memory_pointer(uint32_t effective_address)
{
//     uint32_t access_location = access_location(effective_address);
//     void *pointer = NULL;
// //     switch(access_location)
// //     {
// //         case ACCESS_LOCATION_REGS:
// //         case ACCESS_LOCATION_WRAM1:
// //         case ACCESS_LOCATION_WRAM2:
// //             pointer = cpu_pointer(effective_address, access_location);
// //         break;
        
// //         case ACCESS_LOCATION_PPU:
// // //            pointer = ppu_pointer(effective_address);
// //         break;
        
// //         case ACCESS_LOCATION_ROM:
// //             pointer = rom_pointer(effective_address);
// //         break;
// //     }
    
//     return pointer;
}

void write_byte(uint32_t effective_address, uint8_t data)
{
    uint32_t access = access_location(effective_address);

    if(access == ACCESS_RAM2)
    {
        uint32_t offset = effective_address - RAM2_START;
        ram2[offset] = data;
    }
    else if(access == ACCESS_RAM1_REGS)
    {
        uint32_t offset = (effective_address & 0xffff) - RAM1_REGS_START;

        if(!reg_writes[offset].invalid)
        {
            ram1_regs[offset] = data;

            if(reg_writes[offset].notify)
            {
                reg_writes[offset].notify(effective_address);
            }
        }
    }
    else if(access == ACCESS_RAM1)
    {
        uint32_t offset = (effective_address & 0xffff) - RAM1_REGS_START;
        ram1_regs[offset] = data;
    }
}

uint8_t read_byte(uint32_t effective_address)
{
    uint32_t access = access_location(effective_address);
    uint8_t data = 0;

    if(access == ACCESS_RAM2)
    {
        uint32_t offset = effective_address - RAM2_START;
        data = ram2[offset];
    }
    else if(access == ACCESS_RAM1_REGS)
    {
        uint32_t offset = (effective_address & 0xffff) - RAM1_REGS_START;

        if(!reg_reads[offset].invalid)
        {
            data = ram1_regs[offset];

            if(reg_reads[offset].notify)
            {
                reg_reads[offset].notify(effective_address);
            }
        }
    }
    else if(access == ACCESS_RAM1)
    {
        uint32_t offset = (effective_address & 0xffff) - RAM1_REGS_START;
        data = ram1_regs[offset];
    }
    else if(access == ACCESS_ROM)
    {
        data = rom_read(effective_address);
    }
    
    // switch(access)
    // {
    //     case ACCESS_LOCATION_REGS:
    //         data = cpu_regs_read(effective_address);
    //     break;
        
    //     case ACCESS_LOCATION_WRAM1:
    //         data = cpu_wram1_read(effective_address);
    //     break;
        
    //     case ACCESS_LOCATION_WRAM2:
    //         data = cpu_wram2_read(effective_address);
    //     break;
        
    //     case ACCESS_LOCATION_PPU:
    //         data = ppu_regs_read(effective_address);
    //     break;
        
    //     case ACCESS_LOCATION_ROM:
    //         data = rom_read(effective_address);
    //     break;
    // }
    
    return data;
}

uint16_t read_word(uint32_t effective_address)
{
    uint16_t data;
    data = (uint16_t)read_byte(effective_address) | ((uint16_t)read_byte(effective_address + 1) << 8);
    return data;
}

void poke(uint32_t effective_address, uint32_t *value)
{
    // unsigned char *memory;

    // memory = memory_pointer(effective_address);

    // if(memory)
    {
        uint8_t byte0 = read_byte(effective_address);
        uint8_t byte1 = read_byte(effective_address + 1);
        uint8_t byte2 = read_byte(effective_address + 2);
        uint8_t byte3 = read_byte(effective_address + 3);

        printf("[%02x:%04x] %02x %02x %02x %02x ", (effective_address >> 16) & 0xff, effective_address & 0xffff, byte0, byte1, byte2, byte3);

        // if(value)
        // {
        //     if(*value & 0x00ff0000)
        //     {
        //         memcpy(memory, value, 3);
        //     }
        //     else if(*value & 0x0000ff00)
        //     {
        //         memcpy(memory, value, 2);
        //     }
        //     else
        //     {
        //         memcpy(memory, value, 1);
        //     }

        //     byte0 = read_byte(effective_address);
        //     byte1 = read_byte(effective_address + 1);
        //     byte2 = read_byte(effective_address + 2);
        //     byte3 = read_byte(effective_address + 3);

        //     printf("-> %02x %02x %02x %02x ", byte0, byte1, byte2, byte3);
        // }

        printf("\n");
    }
}







