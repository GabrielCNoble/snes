#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include "emu.h"
#include "ui.h"
#include "SDL2/SDL.h"
#include "GL/glew.h"

    // uint32_t breakpoint_count = 0;
    // struct breakpoint_t breakpoints[512];

// struct breakpoint_t emu_wram_read_breakpoints[MAX_BREAKPOINTS];
// uint32_t            emu_wram_read_breakpoint_count;

// struct breakpoint_t emu_wram_write_breakpoints[MAX_BREAKPOINTS];
// uint32_t            emu_wram_write_breakpoint_count;

struct breakpoint_range_t
{
    uint32_t start_address;
    uint32_t end_address;
};

struct breakpoint_range_t       emu_address_ranges[MAX_BREAKPOINTS];
struct breakpoint_list_t        emu_breakpoints[BREAKPOINT_TYPE_LAST];
uint32_t                        emu_dma_breakpoint_bitmask = 0;
uint8_t *                       emu_vram_breakpoint_bitmask;

SDL_Window *                    emu_window = NULL;
SDL_GLContext                   emu_context;
uint32_t                        emu_window_width = 1360;
uint32_t                        emu_window_height = 700;
GLuint                          emu_framebuffer_texture = 0;
struct dot_t *                  emu_framebuffer;
struct thrd_t                   emu_main_thread;
struct thrd_t *                 emu_emulation_thread;
struct emu_thread_data_t        emu_emulation_data;
struct emu_log_t *              emu_log_entries;
uint32_t                        emu_last_log_entry;
uint32_t                        emu_log_entry_count;

// SDL_Renderer *  renderer = NULL;
// SDL_Texture *   backbuffer_textures[2] = {};
// uint32_t        run_window_thread = 1;
// uint32_t        blit_backbuffer_texture = 0;
uint32_t        get_input = 0;
// uint64_t        counter_frequency;
// uint64_t        prev_count = 0;
// SDL_atomic_t    blit_semaphore = {0};
uint64_t        master_cycles = 0;
//  uint32_t        mem_refresh_state = 0;
//uint32_t        scanline_cycles = 0;
// int32_t         mem_refresh_cyles = 0;
// int32_t         mem_refresh_start = 538;


uint32_t    interactive_mode = 0;
uint32_t    animated_mode = 0;
FILE *      trace_file;
// uint64_t    frame = 0;
// float       accum_time = 0;

char *breakpoint_register_names[] =
{
    [BREAKPOINT_REGISTER_Y] = "Y",
    [BREAKPOINT_REGISTER_X] = "X"
};

extern struct cpu_state_t cpu_state;

extern uint8_t *                ram1_regs;
extern uint8_t *                ram2;
extern struct mem_write_t *     reg_writes;
extern struct mem_read_t *      reg_reads;
// extern struct dot_t *           framebuffer;
uint32_t                        cur_framebuffer = 0;
// struct dot_t *                  framebuffers[2];
// struct dot_t *                  framebuffer;
extern uint32_t                 cpu_cycle_count;
extern uint8_t                  active_channels;
extern uint16_t                 hcounter;
extern uint16_t                 vcounter;
extern uint32_t                 ppu_scanline_master_cycles;
extern uint32_t                 halt;

uint32_t emu_breakpoint_flag_lut[] = {
    [BREAKPOINT_TYPE_VRAM_READ]     = EMU_BREAKPOINT_FLAG_READ,
    [BREAKPOINT_TYPE_VRAM_WRITE]    = EMU_BREAKPOINT_FLAG_WRITE,
    [BREAKPOINT_TYPE_MEM_READ]      = EMU_BREAKPOINT_FLAG_READ,
    [BREAKPOINT_TYPE_MEM_WRITE]     = EMU_BREAKPOINT_FLAG_WRITE,
    [BREAKPOINT_TYPE_REG_READ]      = EMU_BREAKPOINT_FLAG_READ,
    [BREAKPOINT_TYPE_REG_WRITE]     = EMU_BREAKPOINT_FLAG_WRITE,
};


void set_flag_in_range(uint8_t *flag_bytes, uint32_t start_address, uint32_t end_address, uint32_t flag)
{
    // uint32_t mask = type == BREAKPOINT_TYPE_VRAM_READ ? EMU_BREAKPOINT_FLAG_READ : EMU_BREAKPOINT_FLAG_WRITE;
    uint32_t last_byte = end_address >> 2;     
    uint32_t first_flag = start_address & 0x3;
    uint32_t flag_count = ((1 + (end_address - start_address)) << 2) - first_flag;
    for(uint32_t byte_index = start_address >> 2; byte_index < last_byte; byte_index++)
    {
        uint32_t count = (flag_count > 4) ? 4 : flag_count;

        for(uint32_t flags_index = first_flag; flags_index < count; flags_index++)
        {
            emu_vram_breakpoint_bitmask[byte_index] |= flag << (flags_index << 1);
        }

        first_flag = 0;
        flag_count -= count;
    }
}

void clear_flag_in_range(uint8_t *flag_bytes, uint32_t start_address, uint32_t end_address, uint32_t flag)
{
    // uint32_t mask = type == BREAKPOINT_TYPE_VRAM_READ ? EMU_BREAKPOINT_FLAG_READ : EMU_BREAKPOINT_FLAG_WRITE;
    uint32_t last_byte = end_address >> 2;     
    uint32_t first_flag = start_address & 0x3;
    uint32_t flag_count = ((1 + (end_address - start_address)) << 2) - first_flag;
    for(uint32_t byte_index = start_address >> 2; byte_index < last_byte; byte_index++)
    {
        uint32_t count = (flag_count > 4) ? 4 : flag_count;

        for(uint32_t flags_index = first_flag; flags_index < count; flags_index++)
        {
            emu_vram_breakpoint_bitmask[byte_index] &= ~(flag << (flags_index << 1));
        }

        first_flag = 0;
        flag_count -= count;
    }
}

void set_execution_breakpoint(uint32_t effective_address)
{
    struct breakpoint_list_t *list = &emu_breakpoints[BREAKPOINT_TYPE_EXECUTION];
    struct breakpoint_t *breakpoint = list->breakpoints + list->count;
    list->count++;

    breakpoint->type = BREAKPOINT_TYPE_EXECUTION;
    breakpoint->start_address = effective_address;

    emu_Log("execution breakpoint set on address %02x:%04x\n", (effective_address >> 16), effective_address & 0xffff);
}

void set_register_breakpoint(uint32_t reg, uint32_t value)
{
    struct breakpoint_list_t *list = &emu_breakpoints[BREAKPOINT_TYPE_REGISTER];
    struct breakpoint_t *breakpoint = list->breakpoints + list->count;
    list->count++;

    breakpoint->type = BREAKPOINT_TYPE_REGISTER;
    breakpoint->reg = reg;
    breakpoint->value = value;

    printf("breakpoint set on register %s, for value %x\n", breakpoint_register_names[reg], value);
}

void set_read_write_breakpoint(uint32_t type, uint32_t start_address, uint32_t end_address)
{
    struct breakpoint_list_t *list = emu_breakpoints + type;
    struct breakpoint_t *breakpoint = list->breakpoints + list->count;

    if(start_address > end_address)
    {
        uint32_t temp = end_address;
        end_address = start_address;
        start_address = temp;
    }

    breakpoint->start_address = start_address;
    breakpoint->end_address = end_address;
    breakpoint->type = type;

    if(type == BREAKPOINT_TYPE_VRAM_READ || type == BREAKPOINT_TYPE_VRAM_WRITE)
    {
        set_flag_in_range(emu_vram_breakpoint_bitmask, start_address, end_address, emu_breakpoint_flag_lut[type]);

        switch(type)
        {
            case BREAKPOINT_TYPE_VRAM_READ:
                emu_Log("breakpoint set for VRAM reads from 0x%04x to 0x%04x\n", start_address, end_address);
            break;

            case BREAKPOINT_TYPE_VRAM_WRITE:
                emu_Log("breakpoint set for VRAM writes from 0x%04x to 0x%04x\n", start_address, end_address);
            break;
        }
    }
    else
    {
        switch(type)
        {
            case BREAKPOINT_TYPE_MEM_READ:
                emu_Log("breakpoint set for memory reads from 0x%06x to 0x%06x\n", start_address, end_address);
            break;

            case BREAKPOINT_TYPE_MEM_WRITE:
                emu_Log("breakpoint set for memory writes from 0x%06x to 0x%06x\n", start_address, end_address);
            break;

            case BREAKPOINT_TYPE_REG_READ:
                emu_Log("breakpoint set for reads from register 0x%04x\n", start_address);
            break;

            case BREAKPOINT_TYPE_REG_WRITE:
                emu_Log("breakpoint set for writes to register 0x%04x\n", start_address);
            break;
        }
    }

    list->count++;
}

void set_dma_breakpoint(uint32_t channel)
{
    if(channel < 8)
    {
        if(!(emu_dma_breakpoint_bitmask & (1 << channel)))
        {
            emu_dma_breakpoint_bitmask |= 1 << channel;
            struct breakpoint_list_t *list = &emu_breakpoints[BREAKPOINT_TYPE_DMA];
            struct breakpoint_t *breakpoint = list->breakpoints + list->count;
            list->count++;

            breakpoint->type = BREAKPOINT_TYPE_DMA;
            breakpoint->dma.channel = channel;
        }

        emu_Log("breakpoint set on dma channel %d", channel);   
    }
}

void remove_breakpoint(struct breakpoint_t *breakpoint)
{
    struct breakpoint_list_t *list = &emu_breakpoints[breakpoint->type];
    uint32_t range_count = 0;

    if(breakpoint->type == BREAKPOINT_TYPE_VRAM_READ || breakpoint->type == BREAKPOINT_TYPE_VRAM_WRITE)
    {
        emu_address_ranges[range_count].start_address = breakpoint->start_address;
        emu_address_ranges[range_count].end_address = breakpoint->end_address;
        range_count++;

        for(uint32_t index = 0; index < list->count; index++)
        {
            struct breakpoint_t *test_breakpoint = list->breakpoints + index;
            if(test_breakpoint != breakpoint)
            {
                for(uint32_t range_index = 0; range_index < range_count; range_index++)
                {
                    if(emu_address_ranges[range_index].end_address >= test_breakpoint->start_address &&
                       emu_address_ranges[range_index].start_address <= test_breakpoint->end_address)
                    {
                        if(emu_address_ranges[range_index].start_address < test_breakpoint->start_address)
                        {
                            uint32_t old_end_address = emu_address_ranges[range_index].end_address;
                            emu_address_ranges[range_index].end_address = test_breakpoint->start_address - 1;
                            /* retest this range to make sure it's not contained in this breakpoint */
                            range_index--;

                            emu_address_ranges[range_count].start_address = test_breakpoint->start_address;
                            emu_address_ranges[range_count].end_address = old_end_address;
                            range_count++;

                        }
                        else if(emu_address_ranges[range_index].end_address > test_breakpoint->end_address)
                        {
                            uint32_t old_start_address = emu_address_ranges[range_index].start_address;
                            emu_address_ranges[range_index].start_address = test_breakpoint->end_address;
                            /* retest this range to make sure it's not contained in this breakpoint */
                            range_index--;
                            
                            emu_address_ranges[range_count].start_address = old_start_address;;
                            emu_address_ranges[range_count].end_address = test_breakpoint->end_address - 1;
                            range_count++;
                        }
                        else
                        {
                            /* range is contained inside thsi breakpoint, so discard it */
                            if(range_index < range_count - 1)
                            {
                                emu_address_ranges[range_index] = emu_address_ranges[range_count - 1];
                                range_index--;
                            }
                            range_count--;
                        }
                    }
                }
            }
        }

        for(uint32_t range_index = 0; range_index < range_count; range_index)
        {
            clear_flag_in_range(emu_vram_breakpoint_bitmask, emu_address_ranges[range_index].start_address, 
                                                             emu_address_ranges[range_index].end_address,
                                                             emu_breakpoint_flag_lut[breakpoint->type]);
        }
    }

    uint32_t breakpoint_index = breakpoint - list->breakpoints;
    if(breakpoint_index < list->count - 1)
    {
        list->breakpoints[breakpoint_index] = list->breakpoints[list->count - 1];
    }

    list->count--;
}

void clear_breakpoints()
{
    for(uint32_t type = 0; type < BREAKPOINT_TYPE_LAST; type++)
    {
        emu_breakpoints[type].count = 0;
    }
    printf("breakpoints cleared\n");
}

void blit_backbuffer()
{
    glBindTexture(GL_TEXTURE_2D, emu_framebuffer_texture);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, FRAMEBUFFER_WIDTH, FRAMEBUFFER_HEIGHT, GL_RGBA, GL_UNSIGNED_BYTE, emu_framebuffer);
}

void init_emu()
{
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 4);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);

    emu_window = SDL_CreateWindow("snes", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, emu_window_width, emu_window_height, SDL_WINDOW_OPENGL);
    emu_context = SDL_GL_CreateContext(emu_window);
    SDL_GL_MakeCurrent(emu_window, emu_context);
    SDL_GL_SetSwapInterval(1);
    glewExperimental = GL_TRUE;
    GLenum status = glewInit();
    if(status != GLEW_OK)
    {
        printf("couldn't initialize glew (%d)\n%s\n", status, glewGetErrorString(status));
        exit(-1);
    }

    glGenTextures(1, &emu_framebuffer_texture);
    glBindTexture(GL_TEXTURE_2D, emu_framebuffer_texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 0);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, FRAMEBUFFER_WIDTH, FRAMEBUFFER_HEIGHT, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);

    emu_framebuffer = calloc(FRAMEBUFFER_WIDTH * FRAMEBUFFER_HEIGHT, sizeof(struct dot_t));
    emu_log_entries = calloc(EMU_MAX_LOG_ENTRIES, sizeof(struct emu_log_t));

    emu_vram_breakpoint_bitmask = calloc(1, PPU_VRAM_SIZE >> 2);

    trace_file = fopen("./trace.log", "w");
    if(trace_file == NULL)
    {
        printf("couldn't open trace file\n");
    }

    // counter_frequency = SDL_GetPerformanceFrequency();
    // prev_count = SDL_GetPerformanceCounter();
    ui_Init();
    apu_Init();
    init_ppu();
    init_mem();
    init_dma();
}

void shutdown_emu()
{
    emu_FlushLog();
    fclose(trace_file);
    free(emu_framebuffer);
    // free(framebuffers[1]);
    ui_Shutdown();
    shutdown_ppu();
    shutdown_mem();
}

void reset_emu()
{
    master_cycles = 0;
    reset_cpu();
    reset_ppu();
    apu_Reset();

    if(emu_emulation_thread)
    {
        thrd_Destroy(emu_emulation_thread);
    }

    emu_emulation_thread = thrd_Create(emu_EmulationThread);
    emu_emulation_thread->data = &emu_emulation_data;
    memset(&emu_emulation_data, 0, sizeof(emu_emulation_data));
}

// uint32_t emu_BreakpointHandler(int32_t step_cycles)
// {

// }

void emu_EmulationThread(struct thrd_t *thread)
{
    while(1)
    {
        struct emu_thread_data_t *data = thread->data;
        data->status = 0;

        if(!ram1_regs[CPU_MEM_REG_MDMAEN])
        {
            if(cpu_Step(&data->step_master_cycles))
            {
                data->status |= EMU_STATUS_END_OF_INSTRUCTION;
            }
        }
        else
        {
            data->status |= EMU_STATUS_END_OF_INSTRUCTION;
        }

        step_dma(data->step_master_cycles);
        step_hdma(data->step_master_cycles);
        apu_Step(data->step_master_cycles);
        step_ctrl(data->step_master_cycles);

        if(step_ppu(data->step_master_cycles))
        {
            data->status |= EMU_STATUS_END_OF_FRAME;
        }

        if(ppu_scanline_master_cycles >= 538 && ppu_scanline_master_cycles < 578)
        {
            /* dram refresh */
            deassert_rdy(0);
        }
        else
        {
            assert_rdy(0);
        }

        if(ram1_regs[CPU_MEM_REG_TIMEUP] & 0x80)
        {
            assert_irq(1);
        }
        else
        {
            deassert_irq(1);
        }

        thrd_Switch(thread, &emu_main_thread);
    }
}

uint32_t emu_Step(int32_t master_cycle_count)
{
    if(!(emu_emulation_data.status & EMU_STATUS_BREAKPOINT))
    {
        /* only modify the amount of cycles to step if we're not handling a breakpoint */
        emu_emulation_data.step_master_cycles = master_cycle_count;
        emu_emulation_data.status = 0;
        emu_emulation_data.breakpoint = NULL;
    }

    emu_emulation_data.status &= ~EMU_STATUS_BREAKPOINT;

    thrd_Switch(&emu_main_thread, emu_emulation_thread);

    if(emu_emulation_data.status & EMU_STATUS_END_OF_FRAME)
    {
        blit_backbuffer();
    }

    /* this might not work if a large value is passed to step_cpu, because it may finish
    several instructions before returning. Then again, nothing else will properly work correctly
    as well */
    if(emu_emulation_data.status & EMU_STATUS_END_OF_INSTRUCTION)
    {
        struct breakpoint_list_t *list = &emu_breakpoints[BREAKPOINT_TYPE_EXECUTION];

        for(uint32_t breakpoint_index = 0; breakpoint_index < list->count; breakpoint_index++)
        {
            struct breakpoint_t *breakpoint = list->breakpoints + breakpoint_index;

            if(breakpoint->start_address == cpu_state.instruction_address)
            {
                emu_emulation_data.status |= EMU_STATUS_BREAKPOINT;
                emu_emulation_data.breakpoint_type = BREAKPOINT_TYPE_EXECUTION;
                emu_emulation_data.breakpoint = breakpoint;
                break;
            }
        }
    }

    if(emu_emulation_data.status & EMU_STATUS_BREAKPOINT)
    {
        struct breakpoint_t *breakpoint = emu_emulation_data.breakpoint;
        struct breakpoint_list_t *list = &emu_breakpoints[emu_emulation_data.breakpoint_type];

        switch(emu_emulation_data.breakpoint_type)
        {
            case BREAKPOINT_TYPE_EXECUTION:
                emu_Log("EXECUTION at 0x%06x\n", breakpoint->start_address);        
            break;

            case BREAKPOINT_TYPE_MEM_READ:
                emu_Log("MEM read: read 0x%02x from 0x%06x\n", emu_emulation_data.breakpoint_data.mem.data, 
                                                               emu_emulation_data.breakpoint_data.mem.address);        
            break;

            case BREAKPOINT_TYPE_MEM_WRITE:
                emu_Log("MEM write: write 0x%02x to 0x%06x\n", emu_emulation_data.breakpoint_data.mem.data, 
                                                               emu_emulation_data.breakpoint_data.mem.address);
            break;

            case BREAKPOINT_TYPE_REG_READ:
                emu_Log("REG read: read 0x%02x from 0x%04x\n", breakpoint->value, breakpoint->start_address);        
            break;

            case BREAKPOINT_TYPE_REG_WRITE:
                emu_Log("REG write: write 0x%02x to 0x%04x\n", breakpoint->value, breakpoint->start_address);
            break;

            default:
                for(uint32_t index = 0; index < list->count; index++)
                {
                    struct breakpoint_t *breakpoint = list->breakpoints + index;

                    switch(breakpoint->type)
                    {
                        case BREAKPOINT_TYPE_VRAM_READ:
                            if(breakpoint->start_address <= emu_emulation_data.breakpoint_data.vram.address && 
                               breakpoint->end_address >= emu_emulation_data.breakpoint_data.vram.address)
                            {
                                emu_emulation_data.breakpoint = breakpoint;
                                emu_Log("VRAM read: read 0x%02x from 0x%06x\n", emu_emulation_data.breakpoint_data.vram.data, 
                                                                                emu_emulation_data.breakpoint_data.vram.address);        
                                index = list->count;
                            }
                        break;

                        case BREAKPOINT_TYPE_VRAM_WRITE:
                            if(breakpoint->start_address <= emu_emulation_data.breakpoint_data.vram.address && 
                               breakpoint->end_address >= emu_emulation_data.breakpoint_data.vram.address)
                            {
                                emu_emulation_data.breakpoint = breakpoint;
                                emu_Log("VRAM write: write 0x%02x to 0x%06x\n", emu_emulation_data.breakpoint_data.vram.data, 
                                                                                emu_emulation_data.breakpoint_data.vram.address);
                                index = list->count;
                            }
                        break;

                        case BREAKPOINT_TYPE_DMA:
                            if(breakpoint->dma.channel == emu_emulation_data.breakpoint_data.dma.channel)
                            {
                                emu_emulation_data.breakpoint = breakpoint;
                                emu_Log("DMA transfer: channel %d, move %02x from %04x to %04x", emu_emulation_data.breakpoint_data.dma.channel, 
                                                                                                 emu_emulation_data.breakpoint_data.dma.data, 
                                                                                                 emu_emulation_data.breakpoint_data.dma.src_address, 
                                                                                                 emu_emulation_data.breakpoint_data.dma.dst_address);
                                index = list->count;
                            }
                        break;
                    }
                }
            break;
        }

        if(emu_emulation_data.breakpoint->trace)
        {
            emu_emulation_data.status &= ~EMU_STATUS_BREAKPOINT;
        }
    }
    else
    {
        master_cycle_count += emu_emulation_data.step_master_cycles;
    }

    return emu_emulation_data.status;
}

void emu_Log(const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    struct emu_log_t *log_entry;

    log_entry = emu_log_entries + (emu_log_entry_count + emu_last_log_entry) % EMU_MAX_LOG_ENTRIES;
    
    if(emu_log_entry_count < EMU_MAX_LOG_ENTRIES)
    {
       emu_log_entry_count++;
    }
    else
    {
        fprintf(trace_file, "[%lld]: %s", log_entry->master_clock, log_entry->message);
        emu_last_log_entry = (emu_last_log_entry + 1) % EMU_MAX_LOG_ENTRIES;
    }

    vsnprintf(log_entry->message, sizeof(log_entry->message), fmt, args);
    log_entry->master_clock = master_cycles;
    va_end(args);
}

void emu_FlushLog()
{
    if(emu_log_entry_count < EMU_MAX_LOG_ENTRIES)
    {
        for(uint32_t index = 0; index < emu_log_entry_count; index++)
        {
            struct emu_log_t *log_entry = emu_log_entries + index;
            fprintf(trace_file, "[%lld]: %s", log_entry->master_clock, log_entry->message);
        }
    }
    else
    {
        for(uint32_t index = 0; index < EMU_MAX_LOG_ENTRIES; index++)
        {
            uint32_t entry_index = (emu_last_log_entry + index) % EMU_MAX_LOG_ENTRIES;
            struct emu_log_t *log_entry = emu_log_entries + entry_index;
            fprintf(trace_file, "[%lld]: %s", log_entry->master_clock, log_entry->message);
        }
    }
}

// void dump_emu()
// {
//     printf("master cycles: %llu\n", master_cycles);
//     dump_dma();
//     dump_ppu();
//     dump_cpu(1);
//     printf("\n");
// }

void write_trace()
{
//    fprintf(trace_file, "[%llu]: %s\n", master_cycles, instruction_str(cpu_state.instruction_address));
    // fprintf(trace_file, "%s\n", instruction_str2(cpu_state.instruction_address));
}
