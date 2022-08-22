#include <stdlib.h>
#include <stdio.h>
#include "mem.h"
#include "addr.h"
#include "cpu/cpu.h"
#include "ppu.h"
#include "cart.h"
#include "ctrl.h"
#include "dma.h"
#include "apu.h"

// struct mem_write_t cpu_reg_writes[CPU_REGS_END - CPU_REGS_START] = {

// };

// struct mem_write_t ppu_reg_writes[PPU_REGS_END - PPU_REGS_START] = {

// };
uint8_t *               ram1_regs;
uint8_t *               ram2;
uint8_t                 last_bus_value;
// uint8_t *               vram;
struct mem_write_t *    reg_writes;
struct mem_read_t *     reg_reads;

void init_mem()
{
    /* 8KB of wram1 + cpu,ppu,etc regs */
    ram1_regs = calloc(RAM1_REGS_END - RAM1_REGS_START, 1);
    /* 120K of wram2 */
    ram2 = calloc(0x1e000, 1);
    /* 64K of vram */
    // vram = calloc(0xffff, 1);
    /* write behavior for all mem mapped registers */
    reg_writes = calloc(RAM1_REGS_END - RAM1_REGS_START, sizeof(struct mem_write_t));
    reg_reads = calloc(RAM1_REGS_END - RAM1_REGS_START, sizeof(struct mem_read_t));

    reg_writes[CPU_REG_MDMAEN].write = mdmaen_write;
    reg_writes[CPU_REG_HDMAEN].write = hdmaen_write;
    reg_writes[CPU_REG_JOYA].write = ctrl_write;
    reg_writes[CPU_REG_NMITIMEN].write = nmitimen_write;
    reg_writes[CPU_REG_HTIMEL].write = vhtime_write;
    reg_writes[CPU_REG_HTIMEH].write = vhtime_write;
    reg_writes[CPU_REG_VTIMEL].write = vhtime_write;
    reg_writes[CPU_REG_VTIMEH].write = vhtime_write;
    reg_writes[CPU_REG_WRMPYB].write = wrmpyb_write;
    reg_writes[CPU_REG_WRDIVB].write = wrdivb_write;

    reg_reads[CPU_REG_JOYA].read = ctrl_read;
    reg_reads[CPU_REG_JOYB].read = ctrl_read;
    reg_reads[CPU_REG_TIMEUP].read = timeup_read;
    reg_reads[CPU_REG_RDNMI].read = rdnmi_read;
    reg_reads[CPU_REG_HVBJOY].read = hvbjoy_read;


//    reg_reads[CPU_REG_JOYB].read = io_read;

    reg_writes[PPU_REG_VMADDL].write = vmadd_write;
    reg_writes[PPU_REG_VMADDH].write = vmadd_write;
    reg_writes[PPU_REG_VMDATAWL].write = vmdata_write;
    reg_writes[PPU_REG_VMDATAWH].write = vmdata_write;
    reg_writes[PPU_REG_OAMADDL].write = oamadd_write;
    reg_writes[PPU_REG_OAMADDH].write = oamadd_write;
    reg_writes[PPU_REG_OAMDATA].write = oamdata_write;
    reg_writes[PPU_REG_BGMODE].write = bgmode_write;
    reg_writes[PPU_REG_TMAIN].write = tmain_write;
    reg_writes[PPU_REG_TSUB].write = tsub_write;
    reg_writes[PPU_REG_OBJSEL].write = objsel_write;

    reg_writes[PPU_REG_BG1SC].write = bgsc_write;
    reg_writes[PPU_REG_BG2SC].write = bgsc_write;
    reg_writes[PPU_REG_BG3SC].write = bgsc_write;
    reg_writes[PPU_REG_BG4SC].write = bgsc_write;

    reg_writes[PPU_REG_BG12NBA].write = bgnba_write;
    reg_writes[PPU_REG_BG34NBA].write = bgnba_write;

    reg_writes[PPU_REG_VMAINC].write = vmainc_write;
    reg_writes[PPU_REG_SETINI].write = setinit_write;

    reg_writes[PPU_REG_COLDATA].write = coldata_write;
    reg_writes[PPU_REG_CGADD].write = cgadd_write;
    reg_writes[PPU_REG_CGDATAW].write = cgdata_write;
    reg_reads[PPU_REG_CGDATAR].read = cgdata_read;

    reg_writes[PPU_REG_INIDISP].write = inidisp_write;
    reg_writes[PPU_REG_BG1HOFS].write = bgoffs_write;
    reg_writes[PPU_REG_BG1VOFS].write = bgoffs_write;
    reg_writes[PPU_REG_BG2HOFS].write = bgoffs_write;
    reg_writes[PPU_REG_BG2VOFS].write = bgoffs_write;
    reg_writes[PPU_REG_BG3HOFS].write = bgoffs_write;
    reg_writes[PPU_REG_BG3VOFS].write = bgoffs_write;
    reg_writes[PPU_REG_BG4HOFS].write = bgoffs_write;
    reg_writes[PPU_REG_BG4VOFS].write = bgoffs_write;

    reg_writes[APU_REG_IO0].write = apuio_write;
    reg_writes[APU_REG_IO1].write = apuio_write;
    reg_writes[APU_REG_IO2].write = apuio_write;
    reg_writes[APU_REG_IO3].write = apuio_write;

    reg_writes[PPU_REG_WMADDL].write = wmadd_write;
    reg_writes[PPU_REG_WMADDM].write = wmadd_write;
    reg_writes[PPU_REG_WMADDH].write = wmadd_write;
    reg_writes[PPU_REG_WMDATA].write = wmdata_write;
    reg_reads[PPU_REG_WMDATA].read = wmdata_read;

    reg_reads[PPU_REG_SLVH].read = slhv_read;
    reg_reads[PPU_REG_OPHCT].read = opct_read;
    reg_reads[PPU_REG_OPVCT].read = opct_read;
    reg_reads[PPU_REG_VMDATARL].read = vmdata_read;
    reg_reads[PPU_REG_VMDATARH].read = vmdata_read;
    reg_reads[APU_REG_IO0].read = apuio_read;
    reg_reads[APU_REG_IO1].read = apuio_read;
    reg_reads[APU_REG_IO2].read = apuio_read;
    reg_reads[APU_REG_IO3].read = apuio_read;

}

void shutdown_mem()
{
    free(ram1_regs);
    free(ram2);
    // free(vram);
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

        return ACCESS_REGS;
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
    last_bus_value = data;
    if(access == ACCESS_RAM2)
    {
        uint32_t offset = effective_address - RAM2_START;
        ram2[offset] = data;
    }
    else if(access == ACCESS_REGS)
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
    else if(access == ACCESS_REGS)
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

    last_bus_value = data;

    return data;
}

uint8_t peek_byte(uint32_t effective_address)
{
    uint32_t access = access_location(effective_address);
    uint8_t data = 0;

    if(access == ACCESS_RAM2)
    {
        uint32_t offset = effective_address - RAM2_START;
        data = ram2[offset];
    }
    else if(access == ACCESS_REGS)
    {
        uint32_t offset = (effective_address & 0xffff) - RAM1_REGS_START;
        data = ram1_regs[offset];
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

uint16_t peek_word(uint32_t effective_address)
{
    uint16_t data;
    data = (uint16_t)peek_byte(effective_address) | ((uint16_t)peek_byte(effective_address + 1) << 8);
    return data;
}

void poke(uint32_t effective_address, uint32_t *value)
{
    // unsigned char *memory;

    // memory = memory_pointer(effective_address);

    // if(memory)
    {
        uint8_t byte0 = peek_byte(effective_address);
        uint8_t byte1 = peek_byte(effective_address + 1);
        uint8_t byte2 = peek_byte(effective_address + 2);
        uint8_t byte3 = peek_byte(effective_address + 3);

        printf("[%02x:%04x] %02x %02x %02x %02x ", (effective_address >> 16) & 0xff, effective_address & 0xffff, byte0, byte1, byte2, byte3);

         if(value)
         {
             uint32_t write_value = *value;
             if(write_value & 0xff000000)
             {
                 write_byte(effective_address, write_value);
                 write_value >>= 8;
                 write_byte(effective_address + 1, write_value);
                 write_value >>= 8;
                 write_byte(effective_address + 2, write_value);
                 write_value >>= 8;
                 write_byte(effective_address + 3, write_value);
             }
             else if(write_value & 0x00ff0000)
             {
                 write_byte(effective_address, write_value);
                 write_value >>= 8;
                 write_byte(effective_address + 1, write_value);
                 write_value >>= 8;
                 write_byte(effective_address + 2, write_value);
             }
             else if(write_value & 0x0000ff00)
             {
                 write_byte(effective_address, write_value);
                 write_value >>= 8;
                 write_byte(effective_address + 1, write_value);
             }
             else
             {
                 write_byte(effective_address, write_value);
             }

             byte0 = peek_byte(effective_address);
             byte1 = peek_byte(effective_address + 1);
             byte2 = peek_byte(effective_address + 2);
             byte3 = peek_byte(effective_address + 3);

             printf("-> %02x %02x %02x %02x ", byte0, byte1, byte2, byte3);
         }

        printf("\n");
    }
}







