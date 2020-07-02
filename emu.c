#include "emu.h"

void reset_emu()
{
    reset_cpu();
    reset_ppu();
}

void step_emu()
{
    uint32_t cpu_cycles = step_cpu();
    step_ppu(cpu_cycles);
}
