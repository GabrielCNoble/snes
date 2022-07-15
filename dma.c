#include "dma.h"
#include "cpu.h"
#include "mem.h"
#include <stdio.h>

extern uint8_t *ram1_regs;

uint8_t active_channels = 0;
uint32_t active_count = 0;
struct dma_t channels[8];

uint32_t addr_increments[] = {
    [DMA_ADDR_MODE_FIXED]   = 0,
    [DMA_ADDR_MODE_INC]     = 1,
    [DMA_ADDR_MODE_DEC]     = 0xffffffff,
};

void step_dma(uint32_t cycle_count)
{
    for(uint32_t channel_index = 0; channel_index < active_count; channel_index++)
    {
        struct dma_t *dma = channels + channel_index;
        uint32_t addr_increment = addr_increments[dma->addr_mode];
        uint32_t effective_address = dma->addr;

        if(dma->direction == DMA_DIRECTION_CPU_PPU)
        {
            for(uint32_t index = 0; index < dma->count; index++)
            {
                uint8_t data = read_byte(effective_address);
                effective_address += addr_increment;
                write_byte(dma->regs[index & 0x3], data);
            }
        }
        else
        {
            for(uint32_t index = 0; index < dma->count; index++)
            {
                uint8_t data = read_byte(dma->regs[index & 0x3]);
                write_byte(effective_address, data);
                effective_address += addr_increment;
            }
        }
    }

    active_channels = 0;
    active_count = 0;
    ram1_regs[CPU_REG_MDMAEN] = 0;
}

void mdmaen_write(uint32_t effective_address)
{
    uint8_t mdmaen = ram1_regs[CPU_REG_MDMAEN];
    uint8_t start_channels = mdmaen & (~active_channels);

    for(uint32_t channel_index = 0; channel_index < 8; channel_index++)
    {
        if(start_channels & 1)
        {
            struct dma_t *channel = channels + active_count;
            active_count++;

            uint32_t reg_channel_index = channel_index << 4;
            uint32_t reg = CPU_REG_DMA0_PARAM | reg_channel_index;

            channel->channel = channel_index;
            // channel->write_mode = ram1_regs[reg] & 0x7;
            uint32_t write_mode = ram1_regs[reg] & 0x7;
            channel->addr_mode = (ram1_regs[reg] >> 3) & 0x3;
            channel->direction = (ram1_regs[reg] >> 7) & 0x1;

            reg = CPU_REG_DMA0_BBUS_ADDR | reg_channel_index;
            uint16_t ppu_reg = 0x2100 | (uint16_t)ram1_regs[reg];

            switch(write_mode)
            {
                case DMA_WRITE_MODE_BYTE_ONCE:
                case DMA_WRITE_MODE_BYTE_TWICE:
                    channel->regs[0] = reg;
                    channel->regs[1] = reg;
                    channel->regs[2] = reg;
                    channel->regs[3] = reg;
                break;

                case DMA_WRITE_MODE_WORDLH_ONCE:
                    channel->regs[0] = reg;
                    channel->regs[1] = reg + 1;
                    channel->regs[2] = reg;
                    channel->regs[3] = reg + 1;
                break;

                case DMA_WRITE_MODE_WORDLH_TWICE:
                    channel->regs[0] = reg;
                    channel->regs[1] = reg;
                    channel->regs[2] = reg + 1;
                    channel->regs[3] = reg + 1;
                break;

                case DMA_WRITE_MODE_DWORDLHLH_ONCE:
                    channel->regs[0] = reg;
                    channel->regs[1] = reg + 1;
                    channel->regs[2] = reg + 2;
                    channel->regs[3] = reg + 3;
                break;
            }

            reg = CPU_REG_DMA0_ATAB_ADDRL | reg_channel_index;
            channel->addr = (uint32_t)ram1_regs[reg];
            channel->addr |= ((uint32_t)ram1_regs[reg + 2]) << 8;
            channel->addr |= ((uint32_t)ram1_regs[reg + 1]) << 16; 

            reg = CPU_REG_HDMA0_ADDR_DMA0_COUNTL | reg_channel_index;
            channel->count = (uint32_t)ram1_regs[reg];
            channel->count |= ((uint32_t)ram1_regs[reg + 1]) << 8;

            if(!channel->count)
            {
                channel->count = 0x10000;
            }

            // printf("start dma on ch %d\n", channel_index);
            // printf("direction: %s\n", channel->direction ? "PPU -> CPU" : "CPU -> PPU");
            // printf("addr mode: ");
            // switch(channel->addr_mode)
            // {
            //     case DMA_ADDR_MODE_FIXED:
            //         printf("fixed");
            //     break;

            //     case DMA_ADDR_MODE_DEC:
            //         printf("dec");
            //     break;

            //     case DMA_ADDR_MODE_INC:
            //         printf("inc");
            //     break;
            // }
            // printf("\n");
            // printf("write mode: ");
            // switch(channel->write_mode)
            // {
            //     case DMA_WRITE_MODE_BYTE_ONCE:
            //         printf("byte, once\n");
            //     break;

            //     case DMA_WRITE_MODE_WORDLH_ONCE:
            //         printf("word, LH, once\n");
            //     break;

            //     case DMA_WRITE_MODE_BYTE_TWICE:
            //         printf("byte, twice\n");
            //     break;

            //     case DMA_WRITE_MODE_WORDLH_TWICE:
            //         printf("word, LH, twice\n");
            //     break;

            //     case DMA_WRITE_MODE_DWORDLHLH_ONCE:
            //         printf("dword, LHLH, once\n");
            //     break;
            // }
            // printf("\n");
            // printf("addr: %x\n", channel->addr);
            // printf("register: %x\n", channel->reg);
            // printf("size: %d bytes\n", channel->count);
        }

        start_channels >>= 1;
    }
}

void hdmaen_write(uint32_t effective_address)
{
    uint8_t hdmaen = ram1_regs[CPU_REG_HDMAEN];
}