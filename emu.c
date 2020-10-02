#include "emu.h"

uint32_t breakpoint_count = 0;
uint32_t breakpoints[512];

extern struct cpu_state_t cpu_state;

void set_breakpoint(uint32_t effective_address)
{
    breakpoints[breakpoint_count] = effective_address & 0x00ffffff;
    breakpoint_count++;
}

void reset_emu()
{
    reset_cpu();
}

uint32_t step_emu()
{
    uint32_t cpu_cycles = step_cpu();
    step_ppu(cpu_cycles);
    
    for(uint32_t breakpoint_index = 0; breakpoint_index < breakpoint_count; breakpoint_index++)
    {
        if(breakpoints[breakpoint_index] == EFFECTIVE_ADDRESS(cpu_state.reg_pbrw.reg_pbr, cpu_state.reg_pc))
        {
            return 0;
        }
    }
    
    return 1;
}
