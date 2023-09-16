#ifndef EMU_H
#define EMU_H

#include "mem.h"
#include "cpu/cpu.h"
#include "ppu.h"
#include "dma.h"
#include "apu.h"
#include "ctrl.h"
#include "thrd.h"

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

#define MAX_BREAKPOINTS 512
enum BREAKPOINT_TYPE
{
    BREAKPOINT_TYPE_EXECUTION = 0,
    BREAKPOINT_TYPE_REGISTER,
    BREAKPOINT_TYPE_MEM_READ,
    BREAKPOINT_TYPE_MEM_WRITE,
    BREAKPOINT_TYPE_VRAM_READ,
    BREAKPOINT_TYPE_VRAM_WRITE,
    BREAKPOINT_TYPE_REG_READ,
    BREAKPOINT_TYPE_REG_WRITE,
    BREAKPOINT_TYPE_DMA,
    BREAKPOINT_TYPE_LAST
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

enum EMU_BREAKPOINT_FLAGS
{
    EMU_BREAKPOINT_FLAG_WRITE   = 1,
    EMU_BREAKPOINT_FLAG_READ    = 1 << 1
};

struct breakpoint_t
{
    uint32_t type;
    uint32_t reg;
    uint32_t start_address;
    uint32_t end_address;
    
    uint32_t address;
    uint32_t value;

    uint32_t trace;

    // union
    // {
        struct
        {
            uint32_t channel;
        }dma;

        // struct
        // {
        //     uint32_t address;
        // }mem;
    // };
}; 

struct breakpoint_list_t 
{
    struct breakpoint_t breakpoints[MAX_BREAKPOINTS];
    uint32_t            count;
};

struct emu_thread_data_t
{
    uint32_t                status;
    struct breakpoint_t *   breakpoint;
    int32_t                 step_cycles;
    uint32_t                breakpoint_type;
    union
    {
        struct
        {
            uint32_t    channel;
            uint32_t    src_address;
            uint32_t    dst_address;
            uint32_t    data;
        }dma;

        struct
        {
            uint32_t    address;
            uint32_t    data;
        }vram;

        struct
        {
            uint32_t    address;
            uint32_t    location;
            uint32_t    data;
        }mem;
    }breakpoint_data;
};


#define EMU_MAX_LOG_ENTRIES 0x20000
struct emu_log_t
{
    char        message[248];
    uint64_t    master_clock;
};

void set_execution_breakpoint(uint32_t effective_address);

void set_register_breakpoint(uint32_t reg, uint32_t value);

void set_read_write_breakpoint(uint32_t type, uint32_t start_address, uint32_t end_address);

void set_dma_breakpoint(uint32_t channel);

void remove_breakpoint(struct breakpoint_t *breakpoint);

void clear_breakpoints();

void blit_backbuffer();

void init_emu();

void shutdown_emu();

void reset_emu();

// uint32_t emu_BreakpointHandler(int32_t step_cycles);

void emu_EmulationThread(struct thrd_t *thread);

uint32_t emu_Step(int32_t step_cycles);

void emu_Log(const char *fmt, ...);

void dump_emu();

void write_trace();


#endif // EMU_H
