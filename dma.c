#include "dma.h"
#include "cpu.h"
#include "mem.h"
#include "emu.h"
#include <stdio.h>

extern uint8_t *ram1_regs;
int32_t         dma_cycle_count = 0;
// uint8_t active_dma_channels = 0;
struct dma_t    dma_channels[8];
uint8_t         prev_hvbjoy;


uint32_t (*hdma_states[])(int32_t cycle_count) = {
    [HDMA_STATE_IDLE]                           = hdma_idle_state,
    [HDMA_STATE_INIT]                           = hdma_init_state,
    [HDMA_STATE_INIT_CHANNELS]                  = hdma_init_channels_state,
    [HDMA_STATE_INIT_CHANNEL]                   = hdma_init_channel_state,
    [HDMA_STATE_START]                          = hdma_start_state,
    [HDMA_STATE_START_CHANNELS]                 = hdma_start_channels_state,
    [HDMA_STATE_START_CHANNEL_TRANSFER]         = hdma_start_channel_transfer_state,
    [HDMA_STATE_TRANSFER]                       = hdma_transfer_state
};

struct hdma_t   hdma_channels[8];
uint32_t        (*hdma_state)(int32_t cycle_count);
uint32_t        init_hdma_channel_index = 0;
int32_t         hdma_cycle_count = 0;
int32_t         init_channel_cycle_overhead = 0;

uint32_t        active_hdma_channel_index = 0;
uint32_t        enabled_hdma_channel_count = 0;
int32_t         active_channel_cycle_overhead = 0;
uint32_t        last_hdma_line = 0;
// uint8_t         hdma_enabled_channels = 0;
// uint32_t        init_channels = 0;


extern uint16_t     hcounter;
extern uint16_t     vcounter;
extern int32_t      ppu_cycle_count;

extern uint64_t     master_cycles;

extern struct breakpoint_list_t emu_breakpoints[];
extern uint32_t                 emu_dma_breakpoint_bitmask;
extern struct thrd_t            emu_main_thread;
extern struct thrd_t *          emu_emulation_thread;

uint32_t addr_increments[] = {
    [DMA_ADDR_MODE_FIXED]   = 0,
    [DMA_ADDR_MODE_INC]     = 1,
    [DMA_ADDR_MODE_DEC]     = 0xffffffff,
};

uint16_t write_mode_orders[][4] = {
    [DMA_WRITE_MODE_BYTE_ONCE]          = {0, 0, 0, 0},
    [DMA_WRITE_MODE_BYTE_TWICE]         = {0, 0, 0, 0},
    [DMA_WRITE_MODE_WORDLH_ONCE]        = {0, 1, 0, 1},
    [DMA_WRITE_MODE_WORDLH_TWICE]       = {0, 0, 1, 1},
    [DMA_WRITE_MODE_DWORDLHLH_ONCE]     = {0, 1, 2, 3}
};

uint16_t hdma_write_mode_write_counts[] = {
    [DMA_WRITE_MODE_BYTE_ONCE]          = 1,
    [DMA_WRITE_MODE_BYTE_TWICE]         = 2,
    [DMA_WRITE_MODE_WORDLH_ONCE]        = 2,
    [DMA_WRITE_MODE_WORDLH_TWICE]       = 4,
    [DMA_WRITE_MODE_DWORDLHLH_ONCE]     = 4
};

uint16_t hdma_addr_mode_data_addr_regs[][2] = {
    [0] = {CPU_MEM_REG_HDMA0_DIR_ADDRL, CPU_MEM_REG_HDMA0_ATAB_DMA0_BANK},
    [1] = {CPU_MEM_REG_HDMA0_IND_ADDR_DMA0_COUNTL, CPU_MEM_REG_HDMA0_IND_ADDR_BANK}
};

int32_t hdma_addr_mode_init_cycle_overheads[] = {
    [0] = HDMA_INIT_DIR_ADDR_CYCLE_OVERHEAD,
    [1] = HDMA_INIT_IND_ADDR_CYCLE_OVERHEAD
};

int32_t hdma_addr_mode_start_cycle_overheads[] = {
    [0] = HDMA_START_CHANNEL_CYCLE_OVERHEAD,
    [1] = HDMA_START_CHANNEL_CYCLE_OVERHEAD + HDMA_IND_ADDR_START_CYCLE_OVERHEAD,
};

void init_dma()
{
    hdma_state = hdma_states[HDMA_STATE_IDLE];
}

void step_dma(int32_t cycle_count)
{
    /* DMA won't run while HDMA is active. HDMA is active during horizontal blanking, outside of
    the vertical blanking period. */
    uint32_t hdma_active = ram1_regs[CPU_MEM_REG_HDMAEN] &&
                            (ram1_regs[CPU_MEM_REG_HVBJOY] & (CPU_HVBJOY_FLAG_HBLANK | CPU_HVBJOY_FLAG_VBLANK)) == CPU_HVBJOY_FLAG_HBLANK;

    struct breakpoint_list_t *list = &emu_breakpoints[BREAKPOINT_TYPE_DMA];

    if(ram1_regs[CPU_MEM_REG_MDMAEN] && !hdma_active)
    {
        dma_cycle_count += cycle_count;
        int32_t byte_count = dma_cycle_count / DMA_CYCLES_PER_BYTE;
        uint8_t active_channels = ram1_regs[CPU_MEM_REG_MDMAEN];
        uint64_t elapsed_cycles = master_cycles;
        if(byte_count)
        {
            dma_cycle_count -= DMA_CYCLES_PER_BYTE * byte_count;

            for(uint32_t channel_index = 0; channel_index < 8; channel_index++)
            {
                // struct breakpoint_t *breakpoint = NULL;

                // for(uint32_t breakpoint_index = 0; breakpoint_index < list->count; breakpoint_index++)
                // {
                //     // struct breakpoint_t *breakpoint = list->breakpoints + breakpoint_index;
                //     if(list->breakpoints[breakpoint_index].dma.channel == channel_index)
                //     {
                //         breakpoint = list->breakpoints + breakpoint_index;
                //         break;
                //     }
                // }

                if(active_channels & (1 << channel_index))
                {
                    struct dma_t *dma = dma_channels + channel_index;
                    /* TODO: this can be easily optimized. The DMA will always read from/write to the
                    same ppu register, so it's worth it to manually read from/write to the register here */

                    if(byte_count > dma->count)
                    {
                        byte_count = dma->count;
                    }

                    if(dma->direction == DMA_DIRECTION_CPU_PPU)
                    {
                        while(byte_count)
                        {
                            elapsed_cycles += 4;
                            uint8_t data = read_byte(dma->addr);

                            if(emu_dma_breakpoint_bitmask & (1 << channel_index))
                            {
                                struct emu_thread_data_t *thread_data = emu_emulation_thread->data;
                                thread_data->status |= EMU_STATUS_BREAKPOINT;
                                thread_data->breakpoint_type = BREAKPOINT_TYPE_DMA;
                                thread_data->breakpoint_data.dma.channel = channel_index;
                                thread_data->breakpoint_data.dma.src_address = dma->addr;
                                thread_data->breakpoint_data.dma.dst_address = dma->regs[dma->cur_reg];
                                thread_data->breakpoint_data.dma.data = data;
                                thrd_Switch(emu_emulation_thread, &emu_main_thread);
                            }

                            // if(breakpoint != NULL)
                            // {
                            //     struct emu_thread_data_t *thread_data = emu_emulation_thread->data;
                            //     thread_data->status |= EMU_STATUS_BREAKPOINT;
                            //     thread_data->breakpoint = breakpoint;

                            //     // breakpoint->dma.channel = channel_index;
                            //     breakpoint->dma.src_address = dma->addr;
                            //     breakpoint->dma.dst_address = dma->regs[dma->cur_reg];
                            //     breakpoint->value = data;
                            //     thrd_Switch(emu_emulation_thread, &emu_main_thread);
                            // }

                            dma->addr += dma->increment;
                            elapsed_cycles += 4;
                            write_byte(dma->regs[dma->cur_reg], data);
                            dma->cur_reg++;
                            dma->count--;
                            byte_count--;
                        }
                    }
                    else
                    {
                        while(byte_count)
                        {
                            elapsed_cycles += 4;
                            uint8_t data = read_byte(dma->regs[dma->cur_reg]);

                            if(emu_dma_breakpoint_bitmask & (1 << channel_index))
                            {
                                struct emu_thread_data_t *thread_data = emu_emulation_thread->data;
                                thread_data->status |= EMU_STATUS_BREAKPOINT;
                                thread_data->breakpoint_type = BREAKPOINT_TYPE_DMA;
                                thread_data->breakpoint_data.dma.channel = channel_index;
                                thread_data->breakpoint_data.dma.src_address = dma->regs[dma->cur_reg];
                                thread_data->breakpoint_data.dma.dst_address = dma->addr;
                                thread_data->breakpoint_data.dma.data = data;
                                thrd_Switch(emu_emulation_thread, &emu_main_thread);
                            }

                            // if(breakpoint != NULL)
                            // {
                            //     struct emu_thread_data_t *thread_data = emu_emulation_thread->data;
                            //     thread_data->status |= EMU_STATUS_BREAKPOINT;
                            //     thread_data->breakpoint = breakpoint;

                            //     breakpoint->dma.channel = channel_index;
                            //     breakpoint->dma.src_address = dma->regs[dma->cur_reg];
                            //     breakpoint->dma.dst_address = dma->addr;
                            //     breakpoint->value = data;
                            //     thrd_Switch(emu_emulation_thread, &emu_main_thread);
                            // }

                            dma->cur_reg++;
                            elapsed_cycles += 4;
                            write_byte(dma->addr, data);
                            dma->addr += dma->increment;
                            dma->count--;
                            byte_count--;
                        }
                    }

                    if(!dma->count)
                    {
                        ram1_regs[CPU_MEM_REG_MDMAEN] &= ~(1 << channel_index);
                    }

                    break;
                }
            }
        }

        if(!ram1_regs[CPU_MEM_REG_MDMAEN])
        {
            dma_cycle_count = 0;
        }
    }
}

void dump_dma()
{
    printf("===================== DMA/HDMA ===================\n");
    printf("HDMA: ch1: %d, ch2: %d, ch3: %d, ch4: %d, ch5: %d, ch6: %d, ch7: %d, ch8: %d\n",
    ram1_regs[CPU_MEM_REG_HDMAEN] & 1, (ram1_regs[CPU_MEM_REG_HDMAEN] >> 1) & 1, (ram1_regs[CPU_MEM_REG_HDMAEN] >> 2) & 1,
    (ram1_regs[CPU_MEM_REG_HDMAEN] >> 3) & 1, (ram1_regs[CPU_MEM_REG_HDMAEN] >> 4) & 1, (ram1_regs[CPU_MEM_REG_HDMAEN] >> 5) & 1,
    (ram1_regs[CPU_MEM_REG_HDMAEN] >> 6) & 1, (ram1_regs[CPU_MEM_REG_HDMAEN] >> 7) & 1);

    printf("-------------------------------------\n");
    printf("DMA: ch1: %d, ch2: %d, ch3: %d, ch4: %d, ch5: %d, ch6: %d, ch7: %d, ch8: %d\n",
    ram1_regs[CPU_MEM_REG_MDMAEN] & 1, (ram1_regs[CPU_MEM_REG_MDMAEN] >> 1) & 1, (ram1_regs[CPU_MEM_REG_MDMAEN] >> 2) & 1,
    (ram1_regs[CPU_MEM_REG_MDMAEN] >> 3) & 1, (ram1_regs[CPU_MEM_REG_MDMAEN] >> 4) & 1, (ram1_regs[CPU_MEM_REG_MDMAEN] >> 5) & 1,
    (ram1_regs[CPU_MEM_REG_MDMAEN] >> 6) & 1, (ram1_regs[CPU_MEM_REG_MDMAEN] >> 7) & 1);
    if(ram1_regs[CPU_MEM_REG_MDMAEN])
    {
        struct dma_t *channel = NULL;
        uint32_t channel_index;
        for(channel_index = 0; channel_index < 8; channel_index++)
        {
            if(ram1_regs[CPU_MEM_REG_MDMAEN] & (1 << channel_index))
            {
                channel = dma_channels + channel_index;
                break;
            }
        }
        printf("-------------------------------------\n");
        printf("current channel: %d\n", channel_index + 1);
        printf("src: %06x, dst: %04x\n", channel->addr, channel->regs[channel->cur_reg]);
        printf("count: %d bytes\n", channel->count);
    }
    printf("\n");
}

uint32_t hdma_idle_state(int32_t cycle_count)
{
    uint8_t enabled_channels = ram1_regs[CPU_MEM_REG_HDMAEN];

    if(ram1_regs[CPU_MEM_REG_HDMAEN])
    {
        hdma_cycle_count = 0;

        if(vcounter == HDMA_INIT_LINE && hcounter >= HDMA_INIT_DOT)
        {
            hdma_state = hdma_states[HDMA_STATE_INIT];
        }
        else if((ram1_regs[CPU_MEM_REG_HVBJOY] & (CPU_HVBJOY_FLAG_HBLANK | CPU_HVBJOY_FLAG_VBLANK)) == CPU_HVBJOY_FLAG_HBLANK)
        {
            hdma_state = hdma_states[HDMA_STATE_START];
        }
        else
        {
            return 0;
        }

        hdma_state(cycle_count);
    }

    return 0;
}

uint32_t hdma_init_state(int32_t cycle_count)
{
    hdma_cycle_count += cycle_count;

    if(hdma_cycle_count >= HDMA_INIT_CYCLE_OVERHEAD)
    {
        init_hdma_channel_index = 0;
        hdma_cycle_count -= HDMA_INIT_CYCLE_OVERHEAD;
        hdma_state = hdma_states[HDMA_STATE_INIT_CHANNELS];
        hdma_state(0);
    }

    return 0;
}

uint32_t hdma_init_channels_state(int32_t cycle_count)
{
    hdma_cycle_count += cycle_count;
    for(; init_hdma_channel_index < 8; init_hdma_channel_index++)
    {
        if(ram1_regs[CPU_MEM_REG_HDMAEN] & (1 << init_hdma_channel_index))
        {
            uint32_t channel_reg_index = init_hdma_channel_index << 4;
            uint32_t reg = CPU_MEM_REG_DMA0_PARAM | channel_reg_index;
            uint32_t indirect = (ram1_regs[CPU_MEM_REG_DMA0_PARAM | reg] & DMA_INDIRECT_ADDR_MASK) && 1;

            init_channel_cycle_overhead = hdma_addr_mode_init_cycle_overheads[indirect];
            hdma_state = hdma_states[HDMA_STATE_INIT_CHANNEL];

            if(!hdma_state(0))
            {
                return 0;
            }
        }
    }

    hdma_state = hdma_states[HDMA_STATE_IDLE];
    return 1;
}

// void step_hdma_channel_data_regs(uint32_t channel_index)
// {
//     uint32_t channel_reg_index = channel_index << 4;
//     uint32_t reg = CPU_MEM_REG_DMA0_PARAM | channel_reg_index;
//     uint32_t indirect = (ram1_regs[CPU_MEM_REG_DMA0_PARAM | reg] & DMA_INDIRECT_ADDR_MASK) && 1;
//     uint32_t table_reg = CPU_MEM_REG_HDMA0_DIR_ADDRL | channel_reg_index;
//     uint32_t line_count_reg = CPU_MEM_REG_HDMA0_CUR_LINE_COUNT | channel_reg_index;
//     /* first byte of a table entry is the line count, so advance one byte forward
//     in the current entry to point at the actual table data */
//     table_addr++;

//     if(indirect)
//     {
//         uint32_t cur_addr_reg = CPU_MEM_REG_HDMA0_IND_ADDR_DMA0_COUNTL | channel_reg_index;
//         /* in indirect mode, the table data is a pointer to the data, so fetch that and
//         put it in the indirect address register */
//         ram1_regs[cur_addr_reg] = read_byte((uint32_t)table_addr | table_bank);
//         ram1_regs[cur_addr_reg + 1] = read_byte(((uint32_t)table_addr + 2) | table_bank);
//     }

//     /* this is a pointer to the data of the current table entry. When a direct
//     table is used, this points to the actual data. When a indirect table is used,
//     this points to the pointer that points to the data. Either way, this gets
//     incremented during the transfer. */
//     table_reg = CPU_MEM_REG_HDMA0_DIR_ADDRL | channel_reg_index;
//     ram1_regs[table_reg] = (uint8_t)table_addr;
//     ram1_regs[table_reg + 1] = (uint8_t)(table_addr >> 8);
// }

uint32_t hdma_init_channel_state(int32_t cycle_count)
{
    hdma_cycle_count += cycle_count;

    if(hdma_cycle_count >= init_channel_cycle_overhead)
    {
        hdma_cycle_count -= init_channel_cycle_overhead;
        uint32_t channel_reg_index = init_hdma_channel_index << 4;
        uint32_t param_reg = CPU_MEM_REG_DMA0_PARAM | channel_reg_index;
        uint32_t indirect = (ram1_regs[param_reg] & DMA_INDIRECT_ADDR_MASK) && 1;

        uint32_t table_reg = CPU_MEM_REG_HDMA0_ATAB_DMA0_ADDRL | channel_reg_index;
        uint32_t line_count_reg = CPU_MEM_REG_HDMA0_CUR_LINE_COUNT | channel_reg_index;
        uint16_t table_addr = (uint16_t)ram1_regs[table_reg] | ((uint16_t)ram1_regs[table_reg + 1] << 8);
        uint32_t table_bank = (uint32_t)ram1_regs[table_reg + 2] << 16;

        /* this is a pointer to the data of the current table entry. When a direct
        table is used, this points to the actual data. When a indirect table is used,
        this points to the pointer that points to the data. Either way, this gets
        incremented during the transfer. */
        table_reg = CPU_MEM_REG_HDMA0_DIR_ADDRL | channel_reg_index;
        ram1_regs[table_reg] = (uint8_t)table_addr;
        ram1_regs[table_reg + 1] = (uint8_t)(table_addr >> 8);

        // ram1_regs[line_count_reg] = read_byte((uint32_t)table_addr | table_bank);
        // /* first byte of a table entry is the line count, so advance one byte forward
        // in the current entry to point at the actual table data */
        // table_addr++;

        // /* this is a pointer to the data of the current table entry. When a direct
        // table is used, this points to the actual data. When a indirect table is used,
        // this points to the pointer that points to the data. Either way, this gets
        // incremented during the transfer. */
        // table_reg = CPU_MEM_REG_HDMA0_DIR_ADDRL | channel_reg_index;
        // ram1_regs[table_reg] = (uint8_t)table_addr;
        // ram1_regs[table_reg + 1] = (uint8_t)(table_addr >> 8);

        hdma_state = hdma_states[HDMA_STATE_INIT_CHANNELS];

        return 1;
    }

    return 0;
}

uint32_t hdma_start_state(int32_t cycle_count)
{
    hdma_cycle_count += cycle_count;
    if(hdma_cycle_count >= HDMA_START_CYCLE_OVERHEAD)
    {
        active_hdma_channel_index = 0;
        hdma_cycle_count -= HDMA_START_CYCLE_OVERHEAD;

        if(last_hdma_line != vcounter)
        {
            last_hdma_line = vcounter;

            hdma_state = hdma_states[HDMA_STATE_START_CHANNELS];

            for(uint32_t channel_index = 0; channel_index < 8; channel_index++)
            {
                hdma_channels[channel_index].cur_reg = 0;
            }

            hdma_state(0);
        }
    }

    return 0;
}

uint32_t hdma_start_channels_state(int32_t cycle_count)
{
    hdma_cycle_count += cycle_count;
    for(; active_hdma_channel_index < 8; active_hdma_channel_index++)
    {
        struct hdma_t *channel = hdma_channels + active_hdma_channel_index;

        if(ram1_regs[CPU_MEM_REG_HDMAEN] & (1 << active_hdma_channel_index) && channel->cur_reg == 0)
        {
            uint32_t channel_reg_index = active_hdma_channel_index << 4;
            uint32_t reg = CPU_MEM_REG_DMA0_PARAM | channel_reg_index;
            uint32_t write_mode = ram1_regs[reg] & DMA_PARAM_WRITE_MODE_MASK;

            channel->indirect = (ram1_regs[reg] & HDMA_PARAM_INDIRECT) && 1;
            channel->last_reg = hdma_write_mode_write_counts[write_mode];

            reg = CPU_MEM_REG_DMA0_BBUS_ADDR | channel_reg_index;
            uint16_t ppu_reg = DMA_BASE_REG | (uint16_t)ram1_regs[reg];

            channel->regs[0] = ppu_reg + write_mode_orders[write_mode][0];
            channel->regs[1] = ppu_reg + write_mode_orders[write_mode][1];
            channel->regs[2] = ppu_reg + write_mode_orders[write_mode][2];
            channel->regs[3] = ppu_reg + write_mode_orders[write_mode][3];

            active_channel_cycle_overhead = hdma_addr_mode_start_cycle_overheads[channel->indirect];

            hdma_state = hdma_states[HDMA_STATE_START_CHANNEL_TRANSFER];

            if(!hdma_state(0))
            {
                return 0;
            }
        }
    }

    hdma_state = hdma_states[HDMA_STATE_IDLE];
    return 1;
}

uint32_t hdma_start_channel_transfer_state(int32_t cycle_count)
{
    hdma_cycle_count += cycle_count;

    if(hdma_cycle_count >= active_channel_cycle_overhead)
    {
        hdma_cycle_count -= active_channel_cycle_overhead;
        uint32_t channel_reg_index = active_hdma_channel_index << 4;
        uint32_t reg = CPU_MEM_REG_DMA0_PARAM | channel_reg_index;
        uint32_t indirect = (ram1_regs[CPU_MEM_REG_DMA0_PARAM | reg] & DMA_INDIRECT_ADDR_MASK) && 1;
        uint32_t table_reg = CPU_MEM_REG_HDMA0_DIR_ADDRL | channel_reg_index;
        uint32_t line_count_reg = CPU_MEM_REG_HDMA0_CUR_LINE_COUNT | channel_reg_index;

        struct hdma_t *channel = hdma_channels + active_hdma_channel_index;

//        if(channel)

        hdma_state = hdma_states[HDMA_STATE_TRANSFER];
        uint32_t done_for_line = hdma_state(0);

        if(!(ram1_regs[line_count_reg] & 0x7f))
        {
            uint16_t table_addr = (uint16_t)ram1_regs[table_reg] | ((uint16_t)ram1_regs[table_reg + 1] << 8);
            uint32_t table_bank = (uint32_t)ram1_regs[table_reg + 2] << 16;

            ram1_regs[line_count_reg] = read_byte((uint32_t)table_addr | table_bank);

            if(ram1_regs[line_count_reg] & 0x7f)
            {
                /* first byte of a table entry is the line count, so advance one byte forward
                in the current entry to point at the actual table data */
                table_addr++;

                if(indirect)
                {
                    uint32_t cur_addr_reg = CPU_MEM_REG_HDMA0_IND_ADDR_DMA0_COUNTL | channel_reg_index;
                    /* in indirect mode, the table data is a pointer to the data, so fetch that and
                    put it in the indirect address register */
                    ram1_regs[cur_addr_reg] = read_byte((uint32_t)table_addr | table_bank);
                    ram1_regs[cur_addr_reg + 1] = read_byte(((uint32_t)table_addr + 2) | table_bank);
                }

                /* this is a pointer to the data of the current table entry. When a direct
                table is used, this points to the actual data. When a indirect table is used,
                this points to the pointer that points to the data. Either way, this gets
                incremented during the transfer. */
                table_reg = CPU_MEM_REG_HDMA0_DIR_ADDRL | channel_reg_index;
                ram1_regs[table_reg] = (uint8_t)table_addr;
                ram1_regs[table_reg + 1] = (uint8_t)(table_addr >> 8);
            }
        }

        return 1;
    }

    return 0;
}

uint32_t hdma_transfer_state(int32_t cycle_count)
{
    struct hdma_t *channel = hdma_channels + active_hdma_channel_index;
    uint32_t channel_reg_index = active_hdma_channel_index << 4;
    uint32_t line_count_reg = CPU_MEM_REG_HDMA0_CUR_LINE_COUNT | channel_reg_index;
    uint32_t line_count = ram1_regs[line_count_reg];
    uint32_t continuous_mode = line_count & 0x80;
    line_count &= 0x7f;

    if(continuous_mode || !line_count)
    {
        hdma_cycle_count += cycle_count;

        uint32_t byte_count = hdma_cycle_count / DMA_CYCLES_PER_BYTE;

        if(byte_count)
        {
            hdma_cycle_count -= DMA_CYCLES_PER_BYTE * byte_count;
            uint32_t data_addr_reg = hdma_addr_mode_data_addr_regs[channel->indirect][0] | channel_reg_index;
            uint32_t data_bank_reg = hdma_addr_mode_data_addr_regs[channel->indirect][1] | channel_reg_index;

            uint16_t data_addr = (uint16_t)ram1_regs[data_addr_reg] | ((uint16_t)ram1_regs[data_addr_reg] << 8);
            uint32_t data_bank = (uint32_t)ram1_regs[data_bank_reg] << 16;
            uint32_t write_count = channel->last_reg - channel->cur_reg;

            if(byte_count > write_count)
            {
                byte_count = write_count;
            }

            while(write_count)
            {
                uint8_t data = read_byte((uint32_t)data_addr | data_bank);
                write_byte(channel->regs[channel->cur_reg], data);
                data_addr++;
                channel->cur_reg++;
                write_count--;
            }

            ram1_regs[data_addr_reg] = (uint8_t)data_addr;
            ram1_regs[data_addr_reg + 1] = (uint8_t)(data_addr >> 8);
            // ram1_regs[line_count_reg] = line_count;

            if(channel->cur_reg == channel->last_reg)
            {
                // if(channel->indirect)
                // {
                //     /* adjust the table pointer to point to the next entry on the indirect table */
                //     uint16_t cur_table_addr_reg = CPU_MEM_REG_HDMA0_DIR_ADDRL | channel_reg_index;
                //     uint16_t cur_table_addr = (uint16_t)ram1_regs[cur_table_addr_reg] | ((uint16_t)ram1_regs[cur_table_addr_reg] << 8);
                //     /* now it points at the line count byte */
                //     cur_table_addr += 2;

                //     ram1_regs[cur_table_addr_reg] = (uint8_t)cur_table_addr;
                //     ram1_regs[cur_table_addr_reg + 1] = (uint8_t)(cur_table_addr >> 8);
                // }
                // else
                // {
                //     /* nothing to be done when a direct table is used */
                // }

                if(continuous_mode)
                {
                    line_count--;
                }

                hdma_state = hdma_states[HDMA_STATE_START_CHANNELS];
            }
        }
    }
    else
    {
        line_count--;
    }

    ram1_regs[line_count_reg] = continuous_mode | line_count;

    return 0;
}

void step_hdma(int32_t cycle_count)
{
    hdma_state(cycle_count);
}

void dma_reg_list(uint16_t write_mode, uint16_t reg, uint16_t *reg_list)
{
    reg_list[0] = reg + write_mode_orders[write_mode][0];
    reg_list[1] = reg + write_mode_orders[write_mode][1];
    reg_list[2] = reg + write_mode_orders[write_mode][2];
    reg_list[3] = reg + write_mode_orders[write_mode][3];
}

void mdmaen_write(uint32_t effective_address, uint8_t value)
{
    ram1_regs[CPU_MEM_REG_MDMAEN] = value;

    for(uint32_t channel_index = 0; channel_index < 8; channel_index++)
    {
        if(value & 1)
        {
            struct dma_t *channel = dma_channels + channel_index;
            uint32_t reg_channel_index = channel_index << 4;
            uint32_t reg = CPU_MEM_REG_DMA0_PARAM | reg_channel_index;

            uint32_t write_mode = ram1_regs[reg] & 0x7;
            channel->increment = addr_increments[(ram1_regs[reg] >> 3) & 0x3];
            channel->direction = (ram1_regs[reg] >> 7) & 0x1;
            channel->cur_reg = 0;

            reg = CPU_MEM_REG_DMA0_BBUS_ADDR | reg_channel_index;
            uint16_t ppu_reg = DMA_BASE_REG | (uint16_t)ram1_regs[reg];

            channel->regs[0] = ppu_reg + write_mode_orders[write_mode][0];
            channel->regs[1] = ppu_reg + write_mode_orders[write_mode][1];
            channel->regs[2] = ppu_reg + write_mode_orders[write_mode][2];
            channel->regs[3] = ppu_reg + write_mode_orders[write_mode][3];

            reg = CPU_MEM_REG_HDMA0_ATAB_DMA0_ADDRL | reg_channel_index;
            channel->addr = (uint32_t)ram1_regs[reg];
            channel->addr |= ((uint32_t)ram1_regs[reg + 1]) << 8;
            channel->addr |= ((uint32_t)ram1_regs[reg + 2]) << 16;

            reg = CPU_MEM_REG_HDMA0_IND_ADDR_DMA0_COUNTL | reg_channel_index;
            channel->count = (uint32_t)ram1_regs[reg];
            channel->count |= ((uint32_t)ram1_regs[reg + 1]) << 8;

            if(!channel->count)
            {
                channel->count = 0x10000;
            }
        }

        value >>= 1;
    }
}

void hdmaen_write(uint32_t effective_address, uint8_t value)
{
    ram1_regs[CPU_MEM_REG_HDMAEN] = value;
    last_hdma_line = 0xffff;
}

void dma_param(uint32_t effective_address, uint8_t value)
{

}
