#ifndef EMU_H
#define EMU_H

#include "mem.h"
#include "cpu/cpu.h"
#include "ppu.h"
#include "dma.h"
#include "apu.h"
#include "ctrl.h"

/*
    Just a small heads-up regarding comments explaining snes aspects. Much of the stuff explained here is explicitly state in many
    sources I checked, but some parts that should ALSO be explicitly stated (at least in my opinion, since they're crucial details)
    are left as an exercise to the reader to figure out, which is downright bullshit.

    Some of the details are backed by official documentation, and I try my best to leave no stones unturned when explaining. Sometimes
    it may even seem excessive.

    Some of the details are conjecture on my part of how it probably works, given all the documentation I found. If I got something
    wrong, please let me know!
*/

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
*/

#define ADDRESS_BUFFER_SIZE 64

enum BREAKPOINT_REGISTER
{
    BREAKPOINT_REGISTER_X,
    BREAKPOINT_REGISTER_YL,
    BREAKPOINT_REGISTER_YH,
    BREAKPOINT_REGISTER_Y,
    BREAKPOINT_REGISTER_A,
    BREAKPOINT_REGISTER_D,
    BREAKPOINT_REGISTER_PC,
    BREAKPOINT_REGISTER_S
};

enum BREAKPOINT_TYPE
{
    BREAKPOINT_TYPE_EXECUTION = 0,
    BREAKPOINT_TYPE_REGISTER,
    BREAKPOINT_TYPE_READ,
    BREAKPOINT_TYPE_WRITE,
};

enum EMU_STATUS
{
    EMU_STATUS_END_OF_INSTRUCTION   = 1,
    EMU_STATUS_BREAKPOINT           = 1 << 1,
    EMU_STATUS_END_OF_FRAME         = 1 << 2,
};

enum EMU_STATES
{
    EMU_STATE_HALT = 0,
    EMU_STATE_RUN,
    EMU_STATE_STEP,
};

struct breakpoint_t
{
    uint32_t type;
    uint32_t reg;
    uint32_t address;
    uint32_t value;
};

void set_execution_breakpoint(uint32_t effective_address);

void set_register_breakpoint(uint32_t reg, uint32_t value);

void set_read_write_breakpoint(uint32_t type, uint32_t address);

void clear_breakpoints();

void blit_backbuffer();

void init_emu();

void shutdown_emu();

void reset_emu();

uint32_t step_emu(int32_t step_cycles);

void dump_emu();

void write_trace();


#endif // EMU_H
