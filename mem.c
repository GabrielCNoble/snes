#include <stdlib.h>
#include <stdio.h>
#include "mem.h"
#include "addr.h"
#include "cpu/cpu.h"
#include "ppu.h"
#include "cart.h"
#include "ctrl.h"
#include "emu.h"
#include "dma.h"
#include "apu.h"
#include "thrd.h"

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

// extern struct breakpoint_t  emu_wram_read_breakpoints[];
// extern uint32_t             emu_wram_read_breakpoint_count;

// extern struct breakpoint_t  emu_wram_write_breakpoints[];
// extern uint32_t             emu_wram_write_breakpoint_count;

extern struct breakpoint_list_t emu_breakpoints[];

extern struct thrd_t        emu_main_thread;
extern struct thrd_t *      emu_emulation_thread;

void init_mem()
{
    /* 8KB of wram1 + cpu,ppu,etc regs */
    ram1_regs = calloc(RAM1_REGS_END - RAM1_REGS_START, 2);
    /* 120K of wram2 */
    ram2 = calloc(0x1000000, 1);
    /* 64K of vram */
    // vram = calloc(0xffff, 1);
    /* write behavior for all mem mapped registers */
    reg_writes = calloc(RAM1_REGS_END - RAM1_REGS_START, sizeof(struct mem_write_t));
    reg_reads = calloc(RAM1_REGS_END - RAM1_REGS_START, sizeof(struct mem_read_t));

    reg_writes[PPU_REG_INIDISP].write = inidisp_write;
    reg_reads[PPU_REG_INIDISP].read = inidisp_read;

    reg_writes[PPU_REG_OBJSEL].write = objsel_write;
    reg_reads[PPU_REG_OBJSEL].read = objsel_read;

    reg_writes[PPU_REG_OAMADDL].write = oamadd_write;
    reg_reads[PPU_REG_OAMADDL].read = oamadd_read;

    reg_writes[PPU_REG_OAMADDH].write = oamadd_write;
    reg_reads[PPU_REG_OAMADDH].read = oamadd_read;

    reg_writes[PPU_REG_OAMDATAW].write = oamdataw_write;
    reg_reads[PPU_REG_OAMDATAW].read = oamdataw_read;

    reg_writes[PPU_REG_BGMODE].write = bgmode_write;
    reg_reads[PPU_REG_BGMODE].read = bgmode_read;

    reg_writes[PPU_REG_MOSAIC].write = mosaic_write;
    reg_reads[PPU_REG_MOSAIC].read = mosaic_read;

    reg_writes[PPU_REG_BG1SC].write = bgsc_write;
    reg_reads[PPU_REG_BG1SC].read = bgsc_read;

    reg_writes[PPU_REG_BG2SC].write = bgsc_write;
    reg_reads[PPU_REG_BG2SC].read = bgsc_read;

    reg_writes[PPU_REG_BG3SC].write = bgsc_write;
    reg_reads[PPU_REG_BG3SC].read = bgsc_read;

    reg_writes[PPU_REG_BG4SC].write = bgsc_write;
    reg_reads[PPU_REG_BG4SC].read = bgsc_read;

    reg_writes[PPU_REG_BG12NBA].write = bgnba_write;
    reg_reads[PPU_REG_BG12NBA].read = bgnba_read;

    reg_writes[PPU_REG_BG34NBA].write = bgnba_write;
    reg_reads[PPU_REG_BG34NBA].read = bgnba_read;

    reg_writes[PPU_REG_BG1HOFS].write = bgoffs_write;
    reg_reads[PPU_REG_BG1HOFS].read = bgoffs_read;

    reg_writes[PPU_REG_BG1VOFS].write = bgoffs_write;
    reg_reads[PPU_REG_BG1VOFS].read = bgoffs_read;

    reg_writes[PPU_REG_BG2HOFS].write = bgoffs_write;
    reg_reads[PPU_REG_BG2HOFS].read = bgoffs_read;

    reg_writes[PPU_REG_BG2VOFS].write = bgoffs_write;
    reg_reads[PPU_REG_BG2VOFS].read = bgoffs_read;

    reg_writes[PPU_REG_BG3HOFS].write = bgoffs_write;
    reg_reads[PPU_REG_BG3HOFS].read = bgoffs_read;

    reg_writes[PPU_REG_BG3VOFS].write = bgoffs_write;
    reg_reads[PPU_REG_BG3VOFS].read = bgoffs_read;

    reg_writes[PPU_REG_BG4HOFS].write = bgoffs_write;
    reg_reads[PPU_REG_BG4HOFS].read = bgoffs_read;

    reg_writes[PPU_REG_BG4VOFS].write = bgoffs_write;
    reg_reads[PPU_REG_BG4VOFS].read = bgoffs_read;

    reg_writes[PPU_REG_VMAINC].write = vmainc_write;
    reg_reads[PPU_REG_VMAINC].read = vmainc_read;

    reg_writes[PPU_REG_VMADDL].write = vmadd_write;
    reg_reads[PPU_REG_VMADDL].read = vmadd_read;

    reg_writes[PPU_REG_VMADDH].write = vmadd_write;
    reg_reads[PPU_REG_VMADDH].read = vmadd_read;

    reg_writes[PPU_REG_VMDATAWL].write = vmdataw_write;
    reg_reads[PPU_REG_VMDATAWL].read = vmdataw_read;

    reg_writes[PPU_REG_VMDATAWH].write = vmdataw_write;
    reg_reads[PPU_REG_VMDATAWH].read = vmdataw_read;

    reg_writes[PPU_REG_M7SEL].write = m7sel_write;
    reg_reads[PPU_REG_M7SEL].read = m7sel_read;

    reg_writes[PPU_REG_M7A].write = mrot_write;
    reg_reads[PPU_REG_M7A].read = mrot_read;

    reg_writes[PPU_REG_M7B].write = mrot_write;
    reg_reads[PPU_REG_M7B].read = mrot_read;

    reg_writes[PPU_REG_M7C].write = mrot_write;
    reg_reads[PPU_REG_M7C].read = mrot_read;

    reg_writes[PPU_REG_M7D].write = mrot_write;
    reg_reads[PPU_REG_M7D].read = mrot_read;

    reg_writes[PPU_REG_M7X].write = mpos_write;
    reg_reads[PPU_REG_M7X].read = mpos_read;

    reg_writes[PPU_REG_M7Y].write = mpos_write;
    reg_reads[PPU_REG_M7Y].read = mpos_read;

    reg_writes[PPU_REG_CGADD].write = cgadd_write;
    reg_reads[PPU_REG_CGADD].read = cgadd_read;

    reg_writes[PPU_REG_CGDATAW].write = cgdataw_write;
    reg_reads[PPU_REG_CGDATAW].read = cgdataw_read;

    reg_writes[PPU_REG_W12SEL].write = wsel_write;
    reg_reads[PPU_REG_W12SEL].read = wsel_read;

    reg_writes[PPU_REG_W34SEL].write = wsel_write;
    reg_reads[PPU_REG_W34SEL].read = wsel_read;

    reg_writes[PPU_REG_WCOLOBJSEL].write = wobjcolsel_write;
    reg_reads[PPU_REG_WCOLOBJSEL].read = wobjcolsel_read;

    reg_writes[PPU_REG_W1L].write = wlr_write;
    reg_reads[PPU_REG_W1L].read = wlr_read;

    reg_writes[PPU_REG_W1R].write = wlr_write;
    reg_reads[PPU_REG_W1R].read = wlr_read;

    reg_writes[PPU_REG_W2L].write = wlr_write;
    reg_reads[PPU_REG_W2L].read = wlr_read;

    reg_writes[PPU_REG_W2R].write = wlr_write;
    reg_reads[PPU_REG_W2R].read = wlr_read;

    reg_writes[PPU_REG_WBGLOG].write = wbglog_write;
    reg_reads[PPU_REG_WBGLOG].read = wbglog_read;

    reg_writes[PPU_REG_WCOLOBJLOG].write = wcolobjlog_write;
    reg_reads[PPU_REG_WCOLOBJLOG].read = wcolobjlog_read;

    reg_writes[PPU_REG_TMAIN].write = tmainsub_write;
    reg_reads[PPU_REG_TMAIN].read = tmainsub_read;

    reg_writes[PPU_REG_TSUB].write = tmainsub_write;
    reg_reads[PPU_REG_TSUB].read = tmainsub_read;

    reg_writes[PPU_REG_TMAINWM].write = tmainsubwm_write;
    reg_reads[PPU_REG_TMAINWM].read = tmainsubwm_read;

    reg_writes[PPU_REG_TSUBWM].write = tmainsubwm_write;
    reg_reads[PPU_REG_TSUBWM].read = tmainsubwm_read;

    reg_writes[PPU_REG_CGSWSEL].write = cgswsel_write;
    reg_reads[PPU_REG_CGSWSEL].read = cgswsel_read;

    reg_writes[PPU_REG_CGADSUB].write = cgadsub_write;
    reg_reads[PPU_REG_CGADSUB].read = cgadsub_read;

    reg_writes[PPU_REG_CGSWSEL].write = cgswsel_write;
    reg_reads[PPU_REG_CGSWSEL].read = cgswsel_read;

    reg_writes[PPU_REG_COLDATA].write = coldata_write;
    reg_reads[PPU_REG_COLDATA].read = coldata_read;

    reg_writes[PPU_REG_CGSWSEL].write = cgswsel_write;
    reg_reads[PPU_REG_CGSWSEL].read = cgswsel_read;

    reg_writes[PPU_REG_SETINI].write = setinit_write;
    reg_reads[PPU_REG_SETINI].read = setinit_read;

    reg_reads[PPU_REG_SLVH].read = slhv_read;

    reg_reads[PPU_REG_CGDATAR].read = cgdatar_read;

    reg_reads[PPU_REG_OPHCT].read = opct_read;

    reg_reads[PPU_REG_OPVCT].read = opct_read;

    reg_reads[PPU_REG_STAT77].read = stat77_read;

    reg_reads[PPU_REG_STAT78].read = stat78_read;


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

    reg_writes[APU_REG_IO0].write = apuio_write;
    reg_writes[APU_REG_IO1].write = apuio_write;
    reg_writes[APU_REG_IO2].write = apuio_write;
    reg_writes[APU_REG_IO3].write = apuio_write;

    reg_writes[PPU_REG_WMADDL].write = wmadd_write;
    reg_writes[PPU_REG_WMADDM].write = wmadd_write;
    reg_writes[PPU_REG_WMADDH].write = wmadd_write;
    reg_writes[PPU_REG_WMDATA].write = wmdata_write;
    reg_reads[PPU_REG_WMDATA].read = wmdata_read;

    reg_reads[PPU_REG_VMDATARL].read = vmdatar_read;
    reg_reads[PPU_REG_VMDATARH].read = vmdatar_read;

    reg_reads[PPU_REG_OAMDATAR].read = oamdatar_read;

    reg_reads[PPU_REG_MPYL].read = mpy_read;
    reg_reads[PPU_REG_MPYM].read = mpy_read;
    reg_reads[PPU_REG_MPYH].read = mpy_read;

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

    if(effective_address >= RAM2_START && effective_address <= RAM2_END)
    {
        return ACCESS_RAM2;
    }

    return ACCESS_CART;
}

void write_byte(uint32_t effective_address, uint8_t data)
{
    uint32_t access = access_location(effective_address);

    struct breakpoint_list_t *mem_write_breakpoints = &emu_breakpoints[BREAKPOINT_TYPE_MEM_WRITE];

    for(uint32_t breakpoint_index = 0; breakpoint_index < mem_write_breakpoints->count; breakpoint_index++)
    {
        struct breakpoint_t *breakpoint = mem_write_breakpoints->breakpoints + breakpoint_index;
        if(breakpoint->start_address <= effective_address && effective_address <= breakpoint->end_address)
        {
            struct emu_thread_data_t *thread_data = emu_emulation_thread->data;
            // breakpoint->value = data;
            // breakpoint->address = effective_address;
            thread_data->breakpoint = breakpoint;
            thread_data->status |= EMU_STATUS_BREAKPOINT;
            thread_data->breakpoint_type = BREAKPOINT_TYPE_MEM_WRITE;
            thread_data->breakpoint_data.mem.address = effective_address;
            thread_data->breakpoint_data.mem.location = access;
            thread_data->breakpoint_data.mem.data = data;
            thrd_Switch(emu_emulation_thread, &emu_main_thread);
            break;
        }
    }

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

//    write_address_buffer[write_address_count] = effective_address;
//    write_address_count++;
}

uint8_t read_byte(uint32_t effective_address)
{
    uint32_t access = access_location(effective_address);
    uint8_t data = 0;

//    read_address_buffer[read_address_count] = effective_address;
//    read_address_count++;

    struct breakpoint_list_t *mem_read_breakpoints = &emu_breakpoints[BREAKPOINT_TYPE_MEM_READ];

    for(uint32_t breakpoint_index = 0; breakpoint_index < mem_read_breakpoints->count; breakpoint_index++)
    {
        struct breakpoint_t *breakpoint = mem_read_breakpoints->breakpoints + breakpoint_index;
        if(breakpoint->start_address <= effective_address && effective_address <= breakpoint->end_address)
        {
            struct emu_thread_data_t *thread_data = emu_emulation_thread->data;
            // breakpoint->address = effective_address;
            thread_data->breakpoint = breakpoint;
            thread_data->status |= EMU_STATUS_BREAKPOINT;
            thread_data->breakpoint_type = BREAKPOINT_TYPE_MEM_READ;
            thread_data->breakpoint_data.mem.address = effective_address;
            thread_data->breakpoint_data.mem.location = access;
            thrd_Switch(emu_emulation_thread, &emu_main_thread);
            break;
        }
    }

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







