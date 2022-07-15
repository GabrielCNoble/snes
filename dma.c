#include "dma.h"
#include "cpu.h"
#include <stdio.h>

uint8_t prev_mdmaen = 0;
extern uint8_t *ram1_regs;

void step_dma(uint32_t cycle_count)
{
    
}

void mdmaen_write(uint32_t effective_address)
{
    uint8_t mdmaen = ram1_regs[CPU_REG_MDMAEN];
    uint8_t start_channels = mdmaen ^ prev_mdmaen;

    for(uint32_t index = 0; index < 8; index++)
    {
        if(start_channels & 1)
        {
            printf("start dma channel %d\n", index);
        }

        start_channels >>= 1;
    }

    prev_mdmaen = mdmaen;
}

void hdmaen_write(uint32_t effective_address)
{
    uint8_t hdmaen = ram1_regs[CPU_REG_HDMAEN];
}