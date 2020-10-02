#ifndef EMU_H
#define EMU_H

#include "mem.h"
#include "cpu.h"
#include "ppu.h"

/* https://problemkaputt.de/fullsnes.htm */

/*
    SNES master clock: signal generated by the crystal,
    without dividers (21.4772700MHz for NTSC, or 21.2813700MHz for PAL).
    That is 46.561437 ns per cycle. 
    
    Depending on bit 0 of register 420d the cpu will run at either 2.68MHz
    (master clock/8) or 3.58MHz (master clock/6).
    
    At 2.68MHz, the cpu cycle will be 372.486923 ns. At 3.58MHz, it will be
    279.365114 ns.
    
    The ppu also generates its own clock. That's (master clock / 4). So, for
    NTSC that's 5.369317 MHz (186.243427 ns). For PAL, it's 5.320342 MHz
    (187.957841 ns).
    
    Unfortunately SDL's high performance counter has a resolution of 100 ns,
    which is much too coarse for this. The solution then is to step the
    emulation by scanline instead.
*/

void set_breakpoint(uint32_t effective_address);

void reset_emu();

uint32_t step_emu();



#endif // EMU_H
