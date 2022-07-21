#include <stdlib.h>
#include <stdio.h>
#include "mem.h"
#include "addr.h"
#include "cpu.h"
#include "ppu.h"
#include "cart.h"
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

void init_mem()
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

    reg_writes[CPU_REG_MDMAEN].write = mdmaen_write;
    reg_writes[CPU_REG_HDMAEN].write = hdmaen_write;

    reg_writes[PPU_REG_VMADDL].write = vmadd_write;
    reg_writes[PPU_REG_VMADDH].write = vmadd_write;
    reg_writes[PPU_REG_VMDATAWL].write = vmdata_write;
    reg_writes[PPU_REG_VMDATAWH].write = vmdata_write;
    reg_writes[PPU_REG_OAMADDL].write = oamadd_write;
    reg_writes[PPU_REG_OAMADDH].write = oamadd_write;
    reg_writes[PPU_REG_OAMDATA].write = oamdata_write;

    reg_reads[PPU_REG_SLVH].read = slhv_read;
    reg_reads[PPU_REG_OPHCT].read = opct_read;
    reg_reads[PPU_REG_OPVCT].read = opct_read;
    reg_reads[PPU_REG_VMDATARL].read = vmdata_read;
    reg_reads[PPU_REG_VMDATARH].read = vmdata_read;

}

void shutdown_mem()
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

    if(((bank >= RAM1_REGS_BANK_RANGE0_START && bank <= RAM1_REGS_BANK_RANGE0_END) ||
        (bank >= RAM1_REGS_BANK_RANGE1_START && bank <= RAM1_REGS_BANK_RANGE1_END)) && offset < RAM1_REGS_END)
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

    return ACCESS_CART;
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

        if(reg_writes[offset].write)
        {
            reg_writes[offset].write(effective_address, data);
        }
        else
        {
            ram1_regs[offset] = data;
        }
    }
    else if(access == ACCESS_RAM1)
    {
        uint32_t offset = (effective_address & 0xffff) - RAM1_REGS_START;
        ram1_regs[offset] = data;
    }
    else if(access == ACCESS_CART)
    {
        cart_write(effective_address, data);
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

        if(reg_reads[offset].read)
        {
            data = reg_reads[offset].read(effective_address);
        }
        else
        {
            data = ram1_regs[offset];
        }
    }
    else if(access == ACCESS_RAM1)
    {
        uint32_t offset = (effective_address & 0xffff) - RAM1_REGS_START;
        data = ram1_regs[offset];
    }
    else if(access == ACCESS_CART)
    {
        data = cart_read(effective_address);
    }

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







