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

struct breakpoint_list_t emu_breakpoints[BREAKPOINT_TYPE_LAST];

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
uint32_t                        emu_first_log_entry;
uint32_t                        emu_log_entry_count;

// SDL_Renderer *  renderer = NULL;
// SDL_Texture *   backbuffer_textures[2] = {};
// uint32_t        run_window_thread = 1;
// uint32_t        blit_backbuffer_texture = 0;
uint32_t        get_input = 0;
uint64_t        counter_frequency;
uint64_t        prev_count = 0;
// SDL_atomic_t    blit_semaphore = {0};
uint64_t        master_cycles = 0;
//  uint32_t        mem_refresh_state = 0;
//uint32_t        scanline_cycles = 0;
// int32_t         mem_refresh_cyles = 0;
// int32_t         mem_refresh_start = 538;


uint32_t    interactive_mode = 0;
uint32_t    animated_mode = 0;
FILE *      trace_file;
uint64_t    frame = 0;
float       accum_time = 0;

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
extern uint32_t                 scanline_cycles;
extern uint32_t                 halt;




void set_execution_breakpoint(uint32_t effective_address)
{
    struct breakpoint_list_t *list = &emu_breakpoints[BREAKPOINT_TYPE_EXECUTION];
    struct breakpoint_t *breakpoint = list->breakpoints + list->count;
    list->count++;

    breakpoint->type = BREAKPOINT_TYPE_EXECUTION;
    breakpoint->address = effective_address;

    printf("breakpoint set on address %x\n", effective_address);
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
    list->count++;

    breakpoint->start_address = start_address;
    breakpoint->end_address = end_address;
    breakpoint->type = type;

    switch(type)
    {
        case BREAKPOINT_TYPE_WRAM_READ:
            emu_Log("breakpoint set for WRAM reads from 0x%06x to 0x%06x\n", start_address, end_address);
        break;

        case BREAKPOINT_TYPE_VRAM_READ:
            emu_Log("breakpoint set for VRAM reads from 0x%04x to 0x%04x\n", start_address, end_address);
        break;

        case BREAKPOINT_TYPE_WRAM_WRITE:
            emu_Log("breakpoint set for WRAM writes from 0x%06x to 0x%06x\n", start_address, end_address);
        break;

        case BREAKPOINT_TYPE_VRAM_WRITE:
            emu_Log("breakpoint set for VRAM writes from 0x%04x to 0x%04x\n", start_address, end_address);
        break;
    }

    // if(type == BREAKPOINT_TYPE_WRAM_READ)
    // {
    //     printf("breakpoint set for reads from 0x%x to 0x%x\n", start_address);
    // }
    // else
    // {
    //     printf("breakpoint set for writes to address %x\n", address);
    // }
}

void clear_breakpoints()
{
    // breakpoint_count = 0;
    // emu_wram_read_breakpoint_count = 0;
    // emu_wram_write_breakpoint_count = 0;
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

    emu_emulation_thread = thrd_Create(emu_EmulationThread);
    emu_emulation_thread->data = &emu_emulation_data;

    counter_frequency = SDL_GetPerformanceFrequency();
    prev_count = SDL_GetPerformanceCounter();
    ui_Init();
    init_apu();
    init_ppu();
    init_mem();
    init_dma();
}

void shutdown_emu()
{
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

        if(!ram1_regs[CPU_REG_MDMAEN])
        {
            if(step_cpu(&data->step_cycles))
            {
                data->status |= EMU_STATUS_END_OF_INSTRUCTION;
            }
        }
        else
        {
            data->status |= EMU_STATUS_END_OF_INSTRUCTION;
        }

        step_dma(data->step_cycles);
        step_hdma(data->step_cycles);
        step_apu(data->step_cycles);
        step_ctrl(data->step_cycles);

        if(step_ppu(data->step_cycles))
        {
            data->status |= EMU_STATUS_END_OF_FRAME;
        }

        if(scanline_cycles >= 538 && scanline_cycles < 578)
        {
            /* dram refresh */
            deassert_rdy(0);
        }
        else
        {
            assert_rdy(0);
        }

        if(ram1_regs[CPU_REG_TIMEUP] & 0x80)
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

uint32_t emu_Step(int32_t step_cycles)
{
    if(!(emu_emulation_data.status & EMU_STATUS_BREAKPOINT))
    {
        /* only modify the amount of cycles to step if we're not handling a breakpoint */
        emu_emulation_data.step_cycles = step_cycles;
        emu_emulation_data.status = 0;
        emu_emulation_data.breakpoint = NULL;
    }

    emu_emulation_data.status &= ~EMU_STATUS_BREAKPOINT;

    thrd_Switch(&emu_main_thread, emu_emulation_thread);

    if(emu_emulation_data.status & EMU_STATUS_END_OF_FRAME)
    {
        frame++;
        uint64_t cur_count = SDL_GetPerformanceCounter();
        accum_time += (float)(cur_count - prev_count) / (float)counter_frequency;
        prev_count = cur_count;

        if(frame >= 60)
        {
            frame = 0;
            accum_time /= 60.0;
            printf("frame time: %f ms\n", accum_time * 1000.0);
            accum_time = 0.0;
        }

        blit_backbuffer();
    }

    if(emu_emulation_data.status & EMU_STATUS_BREAKPOINT)
    {
        struct breakpoint_t *breakpoint = emu_emulation_data.breakpoint;
        switch(breakpoint->type)
        {
            case BREAKPOINT_TYPE_WRAM_READ:
                emu_Log("WRAM read: read from 0x%06x\n", breakpoint->address);        
            break;

            case BREAKPOINT_TYPE_WRAM_WRITE:
                emu_Log("WRAM write: write 0x%02x to 0x%06x\n", breakpoint->value, breakpoint->address);
            break;

            case BREAKPOINT_TYPE_VRAM_READ:
                emu_Log("VRAM read: read from 0x%06x\n", breakpoint->address);        
            break;

            case BREAKPOINT_TYPE_VRAM_WRITE:
                emu_Log("VRAM write: write 0x%02x to 0x%06x\n", breakpoint->value, breakpoint->address);
            break;
        }
    }
    else
    {
        master_cycles += emu_emulation_data.step_cycles;
    }

    if(emu_emulation_data.status & EMU_STATUS_END_OF_INSTRUCTION)
    {
        // for(uint32_t breakpoint_index = 0; breakpoint_index < breakpoint_count; breakpoint_index++)
        // {
        //     struct breakpoint_t *breakpoint = breakpoints + breakpoint_index;

        //     switch(breakpoint->type)
        //     {
        //         case BREAKPOINT_TYPE_EXECUTION:
        //             if(breakpoint->address == EFFECTIVE_ADDRESS(cpu_state.regs[REG_PBR].byte[0], cpu_state.regs[REG_PC].word))
        //             // if(breakpoint->address == cpu_state.instruction_address)
        //             {
        //                 return emu_emulation_data.status | EMU_STATUS_BREAKPOINT;
        //             }
        //         break;

        //         case BREAKPOINT_TYPE_REGISTER:
        //         {
        //             uint32_t reg_value;

        //             switch(breakpoint->reg)
        //             {
        //                 case BREAKPOINT_REGISTER_Y:
        //                     reg_value = cpu_state.regs[REG_Y].word;
        //                 break;

        //                 case BREAKPOINT_REGISTER_YL:
        //                     reg_value = cpu_state.regs[REG_Y].byte[0];
        //                 break;

        //                 case BREAKPOINT_REGISTER_YH:
        //                     reg_value = cpu_state.regs[REG_Y].byte[1];
        //                 break;

        //                 case BREAKPOINT_REGISTER_X:
        //                     reg_value = cpu_state.regs[REG_X].word;
        //                 break;
        //             }

        //             if(breakpoint->value == reg_value)
        //             {
        //                 return emu_emulation_data.status | EMU_STATUS_BREAKPOINT;
        //             }
        //         }
        //         break;
        //     }
        // }

        if(halt)
        {
            halt = 0;
            emu_emulation_data.status |= EMU_STATUS_BREAKPOINT;
        }
    }

    return emu_emulation_data.status;
}

void emu_Log(const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    struct emu_log_t *log_entry = emu_log_entries + emu_log_entry_count;

    emu_log_entry_count++;
    vsnprintf(log_entry->message, sizeof(log_entry->message), fmt, args);
    va_end(args);
}

void dump_emu()
{
    printf("master cycles: %llu\n", master_cycles);
    dump_dma();
    dump_ppu();
    dump_cpu(1);
    printf("\n");
}

void write_trace()
{
//    fprintf(trace_file, "[%llu]: %s\n", master_cycles, instruction_str(cpu_state.instruction_address));
    fprintf(trace_file, "%s\n", instruction_str2(cpu_state.instruction_address));
}
